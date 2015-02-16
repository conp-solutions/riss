#
#
# wrapper script for riss to pass parameters to it
#
# argument: 1) a cnf file 
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param="-enabled_cp3 -cp3_stats -bce -bce-bcm"

#
# select between printing a proof and not printing a proof
#
# run riss without printing a proof
./riss $1 -mem-lim=2048 /tmp/riss-out-$$ $param -drup=/tmp/proof-riss-$? #> /dev/null 2> /dev/null
status=$?
cat /tmp/riss-out-$$
#
# if unsat, the solver furthermore emits a proof
#
if [ "$status" -eq "20" ]
then
	cat /tmp/proof-riss-$?
fi
rm -f /tmp/riss-out-$$ /tmp/proof-riss-$?
exit $status
