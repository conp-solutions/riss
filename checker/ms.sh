#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

./minisat $1 /tmp/minisat-out > /dev/null -enabled_cp3 -subsimp > /dev/null # 2> /dev/null
status=$?

cat /tmp/minisat-out

exit $status
