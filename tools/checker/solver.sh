#
#
# wrapper script for riss to pass parameters to it
#
# argument: 1) a cnf file 
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param="-v"

#
# select between printing a proof and not printing a proof
#
# run riss without printing a proof
../../riss $1 -mem-lim=2048 /tmp/riss-out-$$ $param #> /dev/null 2> /dev/null
status=$?
cat /tmp/riss-out-$$
rm -f /tmp/riss-out-$$
exit $status
