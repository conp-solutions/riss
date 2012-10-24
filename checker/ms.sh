#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

./minisat ~/cnf/sudoku76.cnf /tmp/minisat-out > /dev/null 2> /dev/null
status=$?

cat /tmp/minisat-out

exit $status
