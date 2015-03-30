class Deductions
    Reasons = {
        reason_user_w:  ["Realtime (Whitelist)", :green],
        reason_user_b:  ["NonRealtime (Blacklist)", :red],
        reason_code_w:  ["Realtime (Annotation)", :green],
        reason_code_b:  ["NonRealtime (Annotation)", :red],
        reason_rtosc_w: ["Rtosc Safe", :green],
        reason_rtosc_b: ["Rtosc Unsafe", :red],
        reason_deduced: ["Deduced Realtime", :yellow],
        reason_none:    ["No Deduction has occured", :default],
        reason_nocode:  ["Assumed Unsafe", :red],
    }

    class DeductionChain
        attr_accessor :deduction_source, :reason, :realtime_p, :non_realtime_p, :has_body_p, :contradicted_p, :contradicted_by


        def initialize
            @deduction_source = nil
            @reason           = :reason_none
            @realtime_p       = false
            @non_realtime_p   = false
            @contradicted_p   = false
            @contradicted_by  = Set.new
        end
    end

    def self.deduce_safe_iter(deductions, callgraph, to_check)
        check_next_time = []
        to_check.each do |id|
            deduction = deductions[id]
            if(!deduction.contradicted_p && deduction.realtime_p())
                callgraph.children(id).each do |x|
                    if(deductions[x].non_realtime_p)
                        check_next_time << x
                        deduction.contradicted_p = true
                        deduction.contradicted_by << x
                        do_stuff = true
                    elsif(!deductions[x].realtime_p)
                        check_next_time << x
                        deductions[x].realtime_p = true
                        deductions[x].deduction_source = callgraph.node_list[id].id
                        deductions[x].reason = :reason_deduced
                        do_stuff = true
                    end
                end
            end
        end
        check_next_time
    end

    def self.deduce_safe(deductions, callgraph)
        check_list = []
        deductions.each_with_index do |d, i|
            check_list << i
        end

        while check_list.length > 0
            check_list = deduce_safe_iter(deductions, callgraph, check_list)
        end
    end

    def self.node_name(node)
        if(node.demangled == node.mangled)
            node.demangled
        else
            node.demangled + " " + node.mangled
        end
    end

    def self.dump_errors(deductions, callgraph)
        puts "\n\n"

        error_count = 0
        callgraph.node_list.each do |node|
            deduction = deductions[node.id]
            if(deduction.contradicted_p)
                error_count = error_count+1
                puts "Error ##{error_count}:"
                puts node_name(node)
                #pp deduction
                puts "##The Deduction Chain:".bold
                next_prop = deduction.deduction_source
                while(next_prop) do
                    node = callgraph.node_list[next_prop]
                    string, color = Reasons[deductions[next_prop].reason]
                    puts " - #{node_name(node)} : #{string.colorize(color)}"
                    next_prop = deductions[next_prop].deduction_source
                end
                puts "##The Contradiction Reasons:".bold
                deduction.contradicted_by.each do |x|
                    node = callgraph.node_list[x]
                    string, color = Reasons[deductions[x].reason]
                    puts " - #{node_name(node)} : #{string.colorize(color)}"
                end
                puts "\n\n"
            end
        end

        puts "Total of #{error_count} error(s)"
        error_count
    end

    def self.add_attr_safety(callgraph)
        callgraph.node_list.each do |node|
            if(node.has? :realtime)
                node.add_map(:reason, :reason_code_w)
            elsif(node.has? :"non-realtime")
                node.add_map(:reason, :reason_code_b)
            end
        end
    end

    def self.add_rtosc_safety(callgraph, rtosc_information)
        rtosc_information.each do |x|
            begin 
                fn = x["func"]
                if(callgraph.has_method?(fn) &&
                   !(x["meta"].include?("non-realtime")))
                    #puts "Rtosc NON  info #{fn}"
                    callgraph[fn].add_attr :realtime
                    callgraph[fn].add_map(:reason, :reason_rtosc_w)
                elsif(callgraph.has_method?(fn))
                    #puts "Rtosc REAL info #{fn}"
                    callgraph[fn].add_attr :"non-realtime"
                    callgraph[fn].add_map(:reason, :reason_rtosc_b)
                end
            rescue ArgumentError
            end
        end
    end

    def self.add_no_body_safety(callgraph)
        callgraph.node_list.each do |node|
            if(!(node.has?(:body) || node.has?(:realtime) || node.has?(:"non-realtime")))
                node.add_attr :"non-realtime"
                node.add_map(:reason, :reason_nocode)
            end
        end
    end

    def self.setup(callgraph)
        d = []
        callgraph.node_list.each do |node|
            #puts node.to_s
            dd = DeductionChain.new
            dd.realtime_p = node.has?(:realtime)
            dd.non_realtime_p = node.has?(:"non-realtime")
            if(node.has?(:realtime) || node.has?(:"non-realtime"))
                dd.reason = node.get_map :reason
            end
            #pp dd
            d << dd
        end
        d
    end
end
