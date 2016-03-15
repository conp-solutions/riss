def get_solver_callstring(instance_filename, seed, param_hashmap)
	solver_callstring = "./riss  #{instance_filename}  -config= "
#        solver_callstring +=" # -rnd-seed=#{seed} "  # ignore random seed, do not use it at all to stay deterministic!
        for param_name in param_hashmap.keys
	        param_value = param_hashmap[param_name]
	        # convert usual minisat syle BoolOption parameters from yes/no into -param and -no-param
	        if param_value == "yes" 
	        then solver_callstring += " -#{param_name}"
	        elsif param_value == "no" 
	        then solver_callstring += " -no-#{param_name}"
	        # otherwise, use simple int and double
	        else solver_callstring += " -#{param_name}=#{param_value}"
	        end
	end
	# print call to screen
	# puts " #{solver_callstring}"
	return solver_callstring
end
