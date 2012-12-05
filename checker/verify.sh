#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

./minisat $1 /tmp/minisat-out -enabled_cp3 -bve > /dev/null # 2> /dev/null
status=$?

if [ $status = 10 ]
then 
    ./verify SAT /tmp/minisat-out $1
else 
    ./verify UNSAT /tmp/minisat-out $1
fi
exit $?
