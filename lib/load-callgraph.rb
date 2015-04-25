#Modifies mapa s.t. it includes the mapb info
def merge_maps(mapa, mapb)
    if mapb
        mapb.each do |key, val|
            if(key == 82 || key == "82" || val==82 || val=="82")
                exit(1)
            end
            if(mapa.has_key?(key) && mapa[key].is_a?(Array))
                mapa[key].concat val
            else
                mapa[key] = val
            end
        end
    end
end


def load_callgraph(opt, library, files)
    callgraph = Hash.new
    function_props = Hash.new
    class_high = Hash.new
    vtable_information = Hash.new
    rtosc_information = Array.new

    #Gather All Information from bitcode files
    files.each do |f|
        $stderr.puts "Parsing '#{f}'..."

        cg = Thread.new {
            `#{opt} -load #{library} --extract-callgraph < #{f} > /dev/null 2> stoat_callgraph.txt`
        }
        an = Thread.new {
            `#{opt} -load #{library} --extract-annotations < #{f} > /dev/null 2> stoat_annotations.txt`
        }
        ch = Thread.new {
            `#{opt} -load #{library} --extract-class-hierarchy < #{f} > /dev/null 2> stoat_class.txt`
        }
        vt = Thread.new {
            `#{opt} -load #{library} --extract-vtables < #{f} > /dev/null 2> stoat_vtable.txt`
        }
        rt = Thread.new {
            `#{opt} -load #{library} --extract-rtosc < #{f} > /dev/null 2> stoat_rtosc.txt`
        }
        cg.join
        an.join
        ch.join
        vt.join
        rt.join
        ncallgraph          = YAML.load_file "stoat_callgraph.txt"
        nfunc               = YAML.load_file "stoat_annotations.txt"
        class_nhigh         = YAML.load_file "stoat_class.txt"
        vtable_ninformation = YAML.load_file "stoat_vtable.txt"
        rtosc_ninformation  = YAML.load_file "stoat_rtosc.txt"

        alias_map = create_alias_map(class_nhigh)
        dealias(alias_map, class_nhigh, ncallgraph)

        merge_maps(callgraph, ncallgraph)
        merge_maps(function_props, nfunc)
        merge_maps(class_high, class_nhigh)
        merge_maps(vtable_information, vtable_ninformation)
        if(rtosc_ninformation)
            rtosc_information.concat rtosc_ninformation
        end
    end

    `rm stoat_callgraph.txt`
    `rm stoat_annotations.txt`
    `rm stoat_class.txt`
    `rm stoat_vtable.txt`
    `rm stoat_rtosc.txt`

    alias_map = create_alias_map(class_high)
    dealias(alias_map, class_high, callgraph)

    cg = Callgraph.new

    #Declare All Known Symbols
    callgraph.each do |a,b|
        cg.declare a
        cg[a].add_attr :body
        b.each do |bb|
            if(bb != "nil")
                cg.declare bb
            end
        end
    end
    
    #Declare All Known Edges
    callgraph.each do |a,b|
        b.each do |bb|
            if(bb != "nil")
                cg.strict_link(a,bb)
            end
        end
    end

    #Add attributes
    function_props.each do |fn, attrs|
        attrs.each do |attr|
            if(attr)
                cg[fn].add_attr attr.to_sym
            end
        end
    end

    return [cg, class_high, vtable_information, rtosc_information]
end

def create_alias_map(class_hierarchy)
    class_high = class_hierarchy
    if(!class_high)
        return nil
    end
    alias_map = Hash.new
    class_high.each do |key, val|
        val.each do |x|
            if(/^alias/.match x)
                #puts "alias '#{key}'->'#{x[6,x.length]}'"
                alias_map[x[6,x.length]] = key
            end
        end
        val.delete_if { |x| /^alias/.match x }
    end
    alias_map
end

def dealias(alias_map, class_high, callgraph)
    if(!alias_map)
        return
    end
    class_high.each do |key, val|
        replace = []
        val.each do |x|
            sp = x.split("+")
            if(alias_map.has_key?(sp[0]))
                if(sp.length == 1)
                    replace << alias_map[sp[0]]
                else
                    replace << alias_map[sp[0]]+"+"+sp[1]
                end
            else
                replace << x
            end
        end
        class_high[key] = replace.uniq
    end

    callgraph.each do |parent, children|
        replace = []
        children.each do |child|
            parts = child.split '$'
            if(parts.length == 2)
                if(alias_map.has_key?(parts[0]))
                    parts[0] = alias_map[parts[0]]
                end
                if((/^class/.match parts.join("$")) ||
                   (/^struct/.match parts.join("$")))
                    replace << parts.join("$")
                else
                    replace << parts.join("$")
                end
            else
                replace << child
            end
        end
        callgraph[parent] = replace
    end
end

