rm -f /tmp/verify_$$.cnf

# get file and all remaining parameters
num_params=$(($#-1))
PARAMS=${@:1:$num_params}
FILE="${@: -1}"

# echo "run $file with $*"

$PARAMS $FILE > /tmp/verify_$$.cnf 2> /dev/null;
status=$?

# accept circuit with blimc for being "valid"
./blimc -m $FILE 2> /dev/null
lstat=$?
if [ "$status" -eq "1" ]
then
  exit 1
fi

# echo "finish with state= $status" # >> tmp.dat

# completed job with model
if [ "$status" -eq "0" ]
then
	cat /tmp/verify_$$.cnf | ./aigsim -m -c $FILE
	vstat=$?
	rm -f /tmp/verify_$$.cnf
	if [ "$vstat" -eq "0" ]
	then
		exit 10
	else
	  # model s wrong!
		exit 15
	fi
else
  exit 25
fi

exit 0
