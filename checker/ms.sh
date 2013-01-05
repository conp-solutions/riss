#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

# set this to the right values!
#param="-enabled_cp3 -subsimp -cp3_naive_strength -inprocess "
param="-enabled_cp3 -subsimp -cp3_threads=2 -inprocess -cp3_par_strength"
#param="-enabled_cp3 -ee -log=0 -no-cp3_extBlocked -cp3_extNgtInput -no-inprocess -cp3_eagerGates"

#
# run the program
#
./minisat $1 /tmp/minisat-out $param > /dev/null 2> /dev/null

status=$?

cat /tmp/minisat-out

exit $status
