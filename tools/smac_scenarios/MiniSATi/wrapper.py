#!/usr/bin/env python2.7
# encoding: utf-8

import sys, os, time, re
from subprocess import Popen, PIPE

# Read in first 5 arguments.
instance = sys.argv[1]
specifics = sys.argv[2]
cutoff = int(float(sys.argv[3]) + 1)
runlength = int(sys.argv[4])
seed = int(sys.argv[5])

# Read in parameter setting and build a param_name->param_value map.
params = sys.argv[6:]
print str(params)
configMap = dict((name, value) for name, value in zip(params[::2], params[1::2]))

# Construct the call string to sat-solver.
minisati_binary = os.path.dirname(os.path.realpath(__file__))+"/minisati"
cmd = "%s %s -cpu-lim=%d" %(minisati_binary, instance, cutoff)      
for name, value in configMap.items():
    # remove ' characters
    value = value.replace("'","")
    # handle false and positive separately
    if (value == "false"):
        cmd += " -no%s" %(name)
    elif (value == "true"):
        cmd += " %s" %(name)
    else:
        cmd += " %s=%s" %(name,  value)
    
# Execute the call and track its runtime.
print(cmd)
start_time = time.time()
io = Popen(cmd.split(" "), stdout=PIPE, stderr=PIPE)
(stdout_, stderr_) = io.communicate()
runtime = time.time() - start_time

# Very simply parsing of Spear's output. Note that in practice we would check the found solution to guard against bugs.
status = "CRASHED"
if (re.search('s UNKNOWN', stdout_)): 
    status = 'TIMEOUT'
if (re.search('s SATISFIABLE', stdout_)):
    status = 'SAT'
if (re.search('s UNSATISFIABLE', stdout_)):
    status = 'UNSAT'
    
# Output result for SMAC.
print("Result for SMAC: %s, %s, 0, 0, %s" % (status, str(runtime), str(seed)))  
