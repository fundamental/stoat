
class CallgraphNode
    def initialize(id, mangled)
        @id              = id
        @mangled         = mangled
        @demangled       = nil
        @demangled_short = nil
        @definition      = nil
        @attr            = Set.new
    end

    def to_s
        "<CGnode:#{id} #{mangled} #{attr.to_a}>"
    end

    def add_attr(sym)
        @attr << sym
    end

    def add_map(sym, value)
        @attr << [sym, value]
    end

    def get_map(sym)
        @attr.each do |x|
            if(x.kind_of?(Array) && sym == x[0])
                return x[1]
            end
        end
        nil
    end

    def has?(sym)
        @attr.each do |x|
            if(sym == x)
                return true
            end
        end
        return false
    end

    def match?(pattern)
        if(pattern.include?"*")
            pat = Regexp.new(pattern)
            return pat.match(mangled) || pat.match(demangled_short)
        else
            pat = pattern
            return pat == mangled || pat == demangled_short
        end
    end
    
    def match_any?(plist)
        plist.each do |p|
            if(match?(p))
                return true
            end
        end
        false
    end

    attr_accessor :id
    attr_accessor :attr
    attr_reader :mangled
    attr_accessor :demangled
    attr_accessor :demangled_short
end

class CallgraphEdge
    def initialize(id, src, dest)
        @id       = id
        @src      = src
        @dest     = dest
        @attr     = Set.new
        @suppress = false
    end
    def to_s
        "<CGedge:#{id} #{src} #{dest} #{attr.to_a}>"
    end
    attr_accessor :id
    attr_accessor :src
    attr_accessor :dest
    attr_accessor :attr
end

class Callgraph
    def initialize()
        @node_list           = Array.new
        @edge_list           = Array.new
        @edge_map            = nil
        @inverse_edge_map    = nil
        @mangled_map         = Hash.new
        @edge_cache          = Hash.new
    end

    def get_edge_count()
        cnt = 0
        @edge_map.each do |a, b|
            cnt += b.length
        end
        cnt
    end

    def to_s()
        "Callgraph:\n" + 
            "\tNodes: #{@node_list.length}\n" +
            "\tEdges: #{@edge_list.length}"
    end

    def full_to_s()
        result = "Callgraph:\n Nodes:"
        @node_list.each do |node|
            result += "\n\t" + node.to_s
        end
        result += "\n Edges:"
        @edge_list.each do |edge|
            result += "\n\t" + edge.to_s
        end
        result
    end

    #Declare the existance of a function
    #
    #@param mangled [String] mangled function name
    def declare(mangled)
        if(@mangled_map.has_key? mangled)
                return
        end
        #it does not exist
        @mangled_map[mangled] = @node_list.length
        @node_list << CallgraphNode.new(@node_list.length, mangled)
    end

    attr_reader :node_list
    def [](mangled)
        if(@mangled_map.has_key? mangled)
            return @node_list[@mangled_map[mangled]]
        end
        throw "Unknown Symbol '#{mangled}'"
    end

    def rebuild_edge_cache()
        def push_to_array(a,b,c)
            if(a.has_key?(b))
                a[b] << c
            else
                a[b] = [c]
            end
        end
        @edge_map = Hash.new
        @inverse_edge_map = Hash.new
        @edge_list.each do |edge|
            if(!edge.attr.include?(:suppress))
                push_to_array(@edge_map, edge.src, edge.dest)
                push_to_array(@inverse_edge_map, edge.dest, edge.src)
            end
        end
    end

    def children(id)
        if(@edge_map == nil) #time to start populating this cache
            rebuild_edge_cache
        end
        if(@edge_map.has_key?(id))
            @edge_map[id]
        else
            []
        end
    end
    
    def parents(id)
        if(@inverse_edge_map == nil) #time to start populating this cache
            rebuild_edge_cache
        end
        if(@inverse_edge_map.has_key?(id))
            @inverse_edge_map[id]
        else
            []
        end
    end

    def has_id_link?(src,dest)
        return @edge_cache.has_key?([src,dest])
    end

    def has_link?(a,b)
        has_id_link?(self[a].id, self[b].id)
    end

    def strict_link(a,b)
        src  = self[a].id
        dest = self[b].id

        if(@edge_cache.has_key?([src,dest]))
            return
        end
        @edge_cache[[src,dest]] = @edge_list.length
        @edge_list << CallgraphEdge.new(@edge_list.length, src, dest)
    end

    # Add Implicit Calls Between Constructor Variants
    def add_constructor_chains()
        @node_list.each do |node|
            sym = node.mangled
            if /D1Ev$/.match sym
                sym_mod = sym.gsub(/D1Ev$/, "D2Ev")
                declare sym
                declare sym_mod
                strict_link(sym, sym_mod)
                self[sym].add_attr :body
            end
        end
    end

    # Add Implicit Calls Between Destructor Variants
    def add_destructor_chains()
        @node_list.each do |node|
            sym = node.mangled
            if /C1E/.match sym
                sym_mod = sym.gsub(/C1E/, "C2E")
                declare sym
                declare sym_mod
                strict_link(sym, sym_mod)
                self[sym].add_attr :body
            end
        end
    end

    # Add Virtual Method Calls To Callgraph
    #
    # @param vtables [Hash] vtable method offsets and their corresponding
    #                       methods
    def add_virtual_methods(vtables)
        vtables.each do |cs, value|
            if(value)
            value.each do |id, method|
                css = cs.gsub(/.variant.*$/,"")
                virtual_method = "#{css}$vtable#{id}"
                declare virtual_method
                if(method != "(none)" && method != "__cxa_pure_virtual")
                    #puts "vtable link #{virtual_method} -> #{method}"
                    declare method
                    strict_link(virtual_method, method)
                    self[virtual_method].add_attr :body
                end
            end
            end
        end
    end

    def has_method?(method)
        @mangled_map.has_key?(method)
    end

    # Add Plausible Virtual Method Calls Down the Class Hierarchy
    def add_subclass_calls(classes, vtables)
        classes.each do |sub, supers|
            currently_processed = 100
            iter  = 0
            vtoff = nil
            #puts "-------------------------------"
            #pp sub
            #pp supers
            supers.reverse.each do |super_|
                sp = super_.split "+"
                supname = sp[0]
                offset = 0
                if(sp.length == 2)
                    offset = sp[1].to_i
                end
                #puts "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
                #puts "working on #{supname}-#{offset}"
                (offset..currently_processed).each do |vt_id|
                    #vtoff = 0
                    vt = nil
                    vt2 = nil
                    vt_id2 = vt_id-offset
                    #puts "vt_id2=#{vt_id2}"
                    if(vt_id == 0)
                        if(vtables.has_key?(sub))
                            vt = vtables[sub]
                        end
                    else
                        if(vtables.has_key?(sub+".variant"+vt_id.to_s))
                            vt = vtables[sub+".variant"+vt_id.to_s]
                        end
                    end
                    if(vt_id2 == 0)
                        if(vtables.has_key?(supname))
                            vt2 = vtables[supname]
                        end
                    else
                        if(vtables.has_key?(supname+".variant"+vt_id2.to_s))
                            vt2 = vtables[supname+".variant"+vt_id2.to_s]
                        end
                    end
                    #if(vtables.has_key?(supname))
                    #    vt2 = vtables[supname]
                    #end
                    if(vt && vt2)

                        #pp vt
                        #pp vt.keys.min
                        #puts "Oh my head"
                        #pp vt2
                        #pp vt2.length
                        vtoff = vt.keys.min
                        vtoff2 = vt2.keys.min
                        vt.length.times do |x|
                            testing = "#{supname}$vtable#{x+vtoff2}"
                            source = "#{sub}$vtable#{x+vtoff}"
                            if((has_method?(testing) || has_method?(source)) && x<vt2.length)
                                #puts "#{testing} -> #{source}"
                                declare testing
                                declare source
                                strict_link(testing,source)
                                self[testing].add_attr :body
                                self[source].add_attr :body
                            end
                        end
                        vtoff += vt.length
                        iter += 1
                    end
                end
                currently_processed = offset-1
            end
        end
    end

    #Apply Suppression Files to Callgraph Edges
    def apply_suppression(suppression_list)
        suppression_list.each do |x|
            call   = x[0]
            callee = x[1]
            call_list = []
            callee_list = []
            @node_list.each do |node|
                if(node.match?(call))
                    call_list << node.id
                end
                if(node.match?(callee))
                    callee_list << node.id
                end
            end
            call_list.each do |c1|
                callee_list.each do |c2|
                    if(@edge_cache.has_key?([c1,c2]))
                        edge = @edge_list[@edge_cache[[c1,c2]]]
                        edge.attr << :suppress
                    end
                end
            end
        end
    end

    # Apply List of Known Safe Functions
    #
    # @param white_list a list of mangled/short mangled strings or regular
    #                   expressions
    def apply_whitelist(white_list)
        #Add WhiteList Information
        #whitelist_wild = Array.new
        #white_list.each do |x|
        #    if(x.include? "*")
        #        whitelist_wild.push(Regexp.new(x))
        #    end
        #end

        #def wild_match(wild_list, str)
        #    wild_list.each do |x|
        #        if(x.match(str))
        #            return true
        #        end
        #    end
        #    return false
        #end
        @node_list.each do |node|
            if(!(node.has?(:realtime) || node.has?(:"non-realtime")) &&
               node.match_any?(white_list))
                node.add_attr :realtime
                node.add_map(:reason, :reason_user_w)
            end
        end
    end

    # Apply List of Known Un-Safe Functions
    #
    # @param black_list a list of mangled/short mangled strings or regular
    #                   expressions
    def apply_blacklist(black_list)
        @node_list.each do |node|
            if(!(node.has?(:realtime) || node.has?(:"non-realtime")) && 
               node.match_any?(black_list))
                node.add_attr :"non-realtime"
                node.add_map(:reason, :reason_user_b)
            end
        end
    end

    def save_file()
        f = File.open(options.dump_file, 'w+')
        map = Hash.new
        map["callgraph"] = callgraph
        map["whitelist"] = Hash.new
        map["blacklist"] = Hash.new
        map["has_code"]  = Set.new
        property_list.each do |key, val|
            if(val.realtime_p)
                map["whitelist"][key] = reasons[val.reason][0]
            end
            if(val.non_realtime_p)
                map["blacklist"][key] = reasons[val.reason][0]
            end
            if(val.has_body_p)
                map["has_code"] << key
            end
        end

        f.puts YAML.dump map
        f.close
    end

    #def init_known_source_attr()
    #    #@node_list.each_with_index do |node, i|
    #    #    node.add_attr :has_body
    #    #end
    #    #callgraph2 = Hash.new
    #    #callgraph.each do |key,value|
    #    #    property_list[key].has_body_p = true
    #    #    if(!value.include? "nil")
    #    #        callgraph2[key] = value.uniq
    #    #    end
    #    #end
    #    #callgraph = callgraph2
    #    #callgraph.do_nothing
    #end

    def update_demangled_cache(rtosc_information)
        #Identify all symbols that are observed in the callgraph
        symbol_list = Set.new
        @node_list.each do |node|
            if(!node.demangled)
                symbol_list << node.mangled
            end
        end

        #$stderr.puts "Demangling #{symbol_list.length} Symbols..."
        demangled_symbols = Hash.new
        demangled_short   = Hash.new

        demangle(symbol_list, demangled_symbols, demangled_short, rtosc_information)
        @node_list.each do |node|
            if(!node.demangled)
                node.demangled       = demangled_symbols[node.mangled]
                node.demangled_short = demangled_short[node.mangled]
            end
        end
    end

    #pp demangled_short
end

def shorten_name(name)
    # replace template arguments
    name = name.gsub(/ (?<args> <
                        (?: (?> [^<>]+ ) | \g<args>)*
                            > ) /x, '<>')
    # remove return type
    name = name.sub(/^(?!operator).* (?=[^<])/, '')
    # remove leading namespaces (keep the last two components of the name)
    name = name.sub(/(\w+(<>)?::)*(\w+(<>)?::)/, '\3')
    # insert line breaks
    if(name =~ /^_Z\w{20,}_$/)
        name = name.chars.each_slice(20).map(&:join).join("\n")
    else
        name = name.gsub('::', "::\n")
    end
    name
end

#Generate a shortened demangled name by removing the function arguments
def remove_arguments(sym_)
    return sym_ if sym_ =~ /\$vtable\d+$/

    paren = 0
    finished = false

    sym = ""
    sym_.reverse.each_char do |x|
        if finished
            sym << x
        end
        if x == ')'
            paren = paren + 1
        end

        if(x == '(' && paren == 1)
            finished = 1
        elsif(x == '(')
            paren = paren - 1
        end
    end
    sym.reverse
end

#Translate from mangled symbols into a list of demangled items
def demangle(symbol_list, demangled_symbols, demangled_short, rtosc)
    #$stderr.puts "Demangling #{symbol_list.length} Symbols..."
    f = File.new("stoat_tmp.txt", "w")
    f.write(symbol_list.to_a.join("\n"))
    f.close
    demangled_list = `cat stoat_tmp.txt | c++filt`.split("\n")
    `rm stoat_tmp.txt`
    #$stderr.puts "Resulting in #{demangled_list.length} Items..."
    tmp = 0
    symbol_list.each do |x|
        demangled_symbols[x] = demangled_list[tmp]
        tmp = tmp + 1
    end

    demangled_symbols.each do |key, value|
        m = remove_arguments(value)
        if(m.empty?)
            demangled_short[key] = value
        else
            demangled_short[key] = m
        end
    end
    #Use Rtosc Symbols to get a better demangling
    rtosc.each do |x|
        key = x["func"]
        val = x["name"]
        if(demangled_short.include?(key))
            demangled_short[key] = val
        end
    end
end
