#
#
# connect to spock-neu (141.76.34.76) and start the config environment to check 
#    - the current riss binary    : riss
#    - the parameter specification: riss4.pcs
#
#    against the framework
#
#

machine="141.76.34.76"

#
# reduce cost for upload
#
strip riss

echo "upload ..."
scp riss riss4.pcs 141.76.34.76:/scratch/CSSC/solvers/riss4/
scp ~/projects/SAT2013/CSSC/scenarios/* 141.76.34.76:/scratch/CSSC/scenarios/
scp ~/projects/SAT2013/CSSC/instances/instancelists/* 141.76.34.76:/scratch/CSSC/instances/instancelists/


echo "run ..."
#ssh 141.76.34.76 "cd /scratch/CSSC; ./configurators/smac-v2.04.01-master-415/smac --numRun 0 --executionMode ROAR --wallClockLimit 100 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/regressiontests-25s-2h.txt --algo \"ruby ../scripts/generic_solver_wrapper.rb riss4\" --paramfile solvers/riss4/riss4.pcs"
ssh 141.76.34.76 "cd /scratch/CSSC; ./configurators/smac-v2.04.01-master-415/smac --logLevel WARN --numRun 0 --executionMode ROAR --wallClockLimit 100 --doValidation false --abortOnCrash true --scenarioFile ./scenarios/tests-25s-6h.txt --algo \"ruby ../scripts/generic_solver_wrapper.rb riss4\" --paramfile solvers/riss4/riss4.pcs"
