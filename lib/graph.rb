module GraphRender

    #Convert Number to 2 Nibble Hex Format
    #
    def self.to_2hex(x)
        s = x.to_s 16
        if(s.length == 1)
            "0" + s
        else
            s
        end
    end


    #Generate a random color for a graph edge
    #
    #@return [String] a color
    def self.random_color
        r = (rand * 255 * 255).to_i % ("CE".to_i 16)
        g = (rand * 255 * 255).to_i % ("DD".to_i 16)
        b = (rand * 255 * 255).to_i % ("EC".to_i 16)
        "#" + ([r,g,b].map{|x| to_2hex(x)}.join)
    end

    #TODO make this process faster
    def self.transitive_closure(callgraph, root, known = Set.new([]))
        if(known.include?(root))
            return Set.new([])
        end
        result = Set.new(callgraph.parents(root))
        result << root

        result2 = Set.new
        result.map do |x|
            if(!(known.include?(x) || result2.include?(x)))
                result2 = result2.union(
                    transitive_closure(callgraph, x, result2.union([root]).union(known)))
            end
        end
        result2.union result
    end


    Color_Contradicted = "red"
    Color_Unsafe       = "black"
    Color_Safe         = "green"
    Color_Deduced      = "yellow"


    def self.to_graph(deductions, callgraph, graphfile, minimal_graph, shorten)
        g = GraphViz::new("G", "rankdir"=>"LR")
        important_nodes = Set.new
        color_nodes     = Hash.new
        puts "Coloring Deductions..."
        deductions.each_with_index do |deduction,i|
            if(deduction.contradicted_p)
                color_nodes[i] ||= Color_Contradicted
                deduction.contradicted_by.each do |x|
                    color_nodes[x] = Color_Unsafe
                end
            elsif(deduction.realtime_p)
                color_nodes[i] ||= "green"
                callgraph.children(i).each do |x|
                    d = deductions[x]
                    if(d.realtime_p && !d.contradicted_p)
                        color_nodes[x] = "green"
                    end
                end
            end
        end

        #For a minimal graph *only* include the realtime functions that eventually
        #call a non-realtime or ambigious function
        if(minimal_graph)
            puts "Minimizing Graph..."
            important_nodes = Set.new

            color_nodes.each do |call,color|
                if(color == "black")
                    important_nodes = important_nodes.union(
                        transitive_closure(callgraph, call, important_nodes))
                end
            end
        else
            important_nodes = color_nodes.keys
        end



        puts "Generating Graph Nodes..."
        argh      = Hash.new
        node_list = Hash.new
        important_nodes.each do |key|
            if(color_nodes.has_key?(key))
                val = color_nodes[key]
                name = callgraph.node_list[key].demangled_short
                if(shorten)
                    name = shorten_name(name)
                    argh[key] = name
                end
                node_list[key] = g.add_nodes(name, "color"=> val,
                                             :shape=>:box)
            end
        end

        puts "Generating Graph Links..."
        dont_repeat = Hash.new
        important_nodes.each do |src|
            callgraph.children(src).each do |dest|
                if(important_nodes.include?(dest) && node_list.has_key?(src) &&
                  node_list.has_key?(dest))
                    if(deductions[dest].non_realtime_p)
                        #if(!dont_repeat.has_key?([argh[src],argh[dest]]))
                            g.add_edges(node_list[src], node_list[dest],
                                        "color"=>(random_color), "style"=>"bold")
                            dont_repeat[[argh[src],argh[dest]]] = true
                        #end
                    else
                        #if(!dont_repeat.has_key?([argh[src],argh[dest]]))
                            g.add_edges(node_list[src], node_list[dest],
                                        "color"=>(random_color), "style"=>"dashed")
                            dont_repeat[[argh[src],argh[dest]]] = true
                        #end
                    end
                end
            end
        end
        g.output(:png => graphfile)
    end
end
