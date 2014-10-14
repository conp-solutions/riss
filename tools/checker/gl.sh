#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param="-hack=1"


./glucose $1 /tmp/glucose-out $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out

exit $status
