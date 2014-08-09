require 'optparse'
require 'ostruct'
require 'set'
require 'yaml'
require 'pp'


#Modifies mapa s.t. it includes the mapb info
def merge_maps(mapa, mapb)
    mapb.each do |key, val|
        if(mapa.has_key? key)
            mapa[key].concat val
        else
            mapa[key] = val
        end
    end
end


options = OpenStruct.new
options.whitelist = []
options.blacklist = []
options.unmangled = OpenStruct.new
options.unmangled.whitelist = []
options.unmangled.blacklist = []
options.root = "./"
options.dir  = []
options.recursive = false

OptionParser.new do |opts|
      opts.banner = "Usage: example.rb [options] FILE"

      opts.on("-w", "--whitelist FILE",
              "Define a Mangled Whitelist File") do |list|
          options.whitelist << list
      end

      opts.on("-b", "--blacklist FILE",
              "Define a Mangled Blacklist File") do |list|
          options.blacklist << list
      end

      opts.on("-r", "--recursive DIR",
              "Enable Recursive Search Mode") do |dir|
          options.recursive = true
          options.root = dir
      end
end.parse!

white_list = []
options.whitelist.each do |x|
    xxx = File.read(x).split
    white_list.concat xxx
end

#If there are unmangled files, then mangle them and add them to the end of the
#mangled lists

#For each one of the bitcode files, run the pass based preprocessor
if(!options.recursive)
    $stderr.puts "This Program Must Process Input Recursively At this time"
    exit 1
end
files = `find #{options.root} -type f | grep -e "\\.bc$"`.split
#p files
callgraph = Hash.new
function_props = Hash.new

files.each do |f|
    p "running #{f} file..."
    `opt -load ./src/libfoo.so --dummy1 < #{f} > /dev/null 2> sfpv_output.txt`
    #puts File.read("sfpv_output.txt")
    ncallgraph = YAML.load_file "sfpv_output.txt"


    `opt -load ./src/libfoo.so --dummy2 < #{f} > /dev/null 2> sfpv_output.txt`
    #puts File.read("sfpv_output.txt")
    nfunc = YAML.load_file "sfpv_output.txt"

    if(ncallgraph)
        merge_maps(callgraph, ncallgraph)
    end
    if(nfunc)
        merge_maps(function_props, nfunc)
    end
end

symbol_list = Set.new
callgraph.each do |key,val|
    symbol_list << key
    val.each do |x|
        symbol_list << x
    end
end

puts "Demangling #{symbol_list.length} Symbols..."
f = File.new("tmp_thing.txt", "w")
f.write(symbol_list.to_a.join("\n"))
f.close
demangled_list = `cat tmp_thing.txt | c++filt`.split("\n")
demangled_symbols = Hash.new
#puts "Resulting in #{demangled_list.length} Items..."
tmp = 0
symbol_list.each do |x|
    demangled_symbols[x] = demangled_list[tmp]
    tmp = tmp + 1
end

demangled_short = Hash.new
demangled_symbols.each do |key, value|
    m = /(\S+)\(/.match(value)
    if(m)
        #puts "#{value} -> #{m[1]}"
        demangled_short[key] = m[1]
    else
        demangled_short[key] = value
    end
end

#pp demangled_short


reason_user_w  = "The Function Was Declared Realtime By A Whitelist"
reason_user_b  = "The Function Was Declared NonRealtime By A Blacklist"
reason_code_w  = "The Function Was Declared Realtime By A Code Annotation"
reason_code_b  = "The Function Was Declared NonRealtime By A Code Annotation"
reason_deduced = "The Function Was Deduced To Need To Be RealTime As It Was Called By A Realtime Function"
reason_none    = "No Deduction has occured"
reason_nocode  = "No Code Or Annotations, So The Function is Assumed Unsafe"

class DeductionChain
    attr_accessor :deduction_source, :reason, :realtime_p, :non_realtime_p, :has_body_p, :contradicted_p, :contradicted_by


    def initialize
        @deduction_source = nil
        @reason           = "No Deduction has occured"
        @realtime_p       = false
        @non_realtime_p   = false
        @has_body_p       = false
        @contradicted_p   = false
        @contradicted_by  = Set.new
    end
end

property_list = Hash.new
symbol_list.each do |x|
    property_list[x] = DeductionChain.new
end

puts "Doing Property List Stuff"

#Add information about finding source
callgraph2 = Hash.new
callgraph.each do |key,value|
    property_list[key].has_body_p = true
    if(!value.include? "nil")
        callgraph2[key] = value
    end
end
callgraph = callgraph2

#Add Anything That's On the function_props list
function_props.each do |key, value|
    if(property_list.include? key)
        if(value.include? 'realtime')
            property_list[key].realtime_p = true
            property_list[key].reason     = reason_code_w
        elsif(value.include? 'non-realtime')
            property_list[key].non_realtime_p = true
            property_list[key].reason         = reason_code_b
        end
    end
end

#Add WhiteList information
property_list.each do |key, value|
    if(!value.realtime_p &&
       !value.non_realtime_p)
        if(white_list.include?(key) || white_list.include?(demangled_short[key]))
            value.realtime_p = true
        end
    end
end



#Add no source stuff
property_list.each do |key, value|
    if(!value.has_body_p && !value.realtime_p && !value.non_realtime_p)
        value.non_realtime_p = true
        value.reason         = reason_nocode
    end
end

#Perform Deductions
do_stuff = true
while do_stuff
    do_stuff = false
    property_list.each do |key, value|
        if(!value.contradicted_p)
            if(value.realtime_p() && callgraph.include?(key))
                callgraph[key].each do |x|
                    if(property_list[x].non_realtime_p)
                        value.contradicted_p = true
                        value.contradicted_by << x
                        do_stuff = true
                    elsif(!property_list[x].realtime_p)
                        property_list[x].realtime_p = true
                        property_list[x].deduction_source = key
                        property_list[x].reason = reason_deduced
                        do_stuff = true
                    end
                end
            end
        end
    end
end



#pp symbol_list
pp callgraph
#puts "\n\n\n\n\n"
#pp function_props
#pp property_list

#Print realtime stuff
puts "Realtime stuff:"
property_list.each do |key, value|
    if(value.realtime_p)
        pp demangled_symbols[key]
    end
end

puts "\n\n"

property_list.each do |key, value|
    if(value.contradicted_p)
        pp demangled_symbols[key]
        pp value
        puts "The Contradiction Reasons: "
        value.contradicted_by.each do |x|
            puts " - #{demangled_symbols[x]} : #{property_list[x].reason}"
        end
        puts "\n\n\n"
    end
end


require "graphviz"
g = GraphViz::new( "G" )
color_nodes = Hash.new
property_list.each do |key,val|
    if(val.contradicted_p)
        color_nodes[key] ||= "red"
        val.contradicted_by.each do |x|
            color_nodes[x] = "black"
        end
    elsif(val.realtime_p)
        color_nodes[key] ||= "green"
    end
end


node_list = Hash.new
color_nodes.each do |key,val|
    node_list[key] = g.add_node(demangled_short[key], "color"=> val)
end

callgraph.each do |src, dests|
    dests.each do |dest|
        if(node_list.include?(src) && node_list.include?(dest))
            g.add_edges(node_list[src], node_list[dest])
        end
    end
end
g.output( :png => "sfpv_graphics.png" )

#p options
#p ARGV
