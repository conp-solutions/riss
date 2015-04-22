rm -f /tmp/verify_$$.cnf

# get file and all remaining parameters
file=$1
shift

# echo "run $file with $*"

timeout 10 $* $file > /tmp/verify_$$.cnf 2> /dev/null;
status=$?

# echo "finish with state= $status" # >> tmp.dat

# completed job with model
if [ "$status" -eq "0" ]
then
	cat /tmp/verify_$$.cnf | ./aigsim -m -c $file
	vstat=$?
	rm -f /tmp/verify_$$.cnf
	if [ "$vstat" -eq "0" ]
	then
		exit 10
	else
		exit 15
	fi
else
	# if the instance is unsatisfiable or another error occurred, report it
	# echo "exit with $status" >> tmp.dat
    exit 25
fi

