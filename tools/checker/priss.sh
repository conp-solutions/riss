#
#
# wrapper script for riss to pass parameters to it
#
# argument: 1) a cnf file 2) [optional] a file for the drat proof
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param="-threads=4 -ps -no-pc -config=RESTART"

#
# select between printing a proof and not printing a proof
#
if [ "x$2" == "x" ] 
then
	# run riss without printing a proof
	./priss $1 -mem-lim=2048 /tmp/glucose-out-$$ $param #> /dev/null 2> /dev/null
	status=$?
else
	# run riss with printing a DRUP/DRAT proof
	echo "generate proof"
	./priss $1 -mem-lim=2048 /tmp/glucose-out-$$ $param -drup=$2 -proofFormat="" -verb-proof=0 #> /dev/null 2> /dev/null
	status=$?
fi
cat /tmp/glucose-out-$$
rm -f /tmp/glucose-out-$$
exit $status
