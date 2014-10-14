rm -f /tmp/verify_$$.cnf

echo "run $1 with $2" >> tmp.dat

$1 $2 > /tmp/verify_$$.cnf 2> /dev/null;
status=$?

echo "finish with state= $status" >> tmp.dat

# cat /tmp/verify.cnf
if [ "$status" -eq "10" ]
then
	./verify SAT /tmp/verify_$$.cnf $2 # 2>&1 > /dev/null
	vstat=$?
	rm -f /tmp/verify_$$.cnf
	if [ "$vstat" -eq "1" ]
	then
		exit 10
	else
		exit 15
	fi
else
	# if the instance is unsatisfiable or another error occurred, report it
	echo "exit with $status" >> tmp.dat
	
     rm -f /tmp/verify_$$.cnf

     exit 25
     # works only with SAT formulas, but then its much faster!

     ./lgl $2 2> /dev/null 1> /dev/null
     lstat=$?
     if [ $lstat -ne "20" ]
     then
       exit 25
     fi
    exit 20
fi

