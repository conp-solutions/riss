rm -f /tmp/verify_$$.cnf

#echo "run $1 with $2" >> tmp.dat

timeout 90 $1 $2 > /tmp/verify_$$.cnf 2> /dev/null;
status=$?

#echo "finish with state= $status" >> tmp.dat

if [ "$status" -eq "124" ]
then
	# we got a timeout here!
	rm -f /tmp/verify_$$.cnf
	exit 124
fi

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
#	echo "exit with $status" >> tmp.dat

     if [ "$status" -ne "20" ]
     then
			exit $status # not sure what happened during solving the problem
     fi

     checkOutput=$(timeout 120 ./proofcheck -backward $2 /tmp/verify_$$.cnf)
     lstat=$?

     if [ "$lstat" -eq "124" ]
     then
        # we got a timeout here!
				mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        rm -f /tmp/verify_$$.cnf
        exit 124
     fi
		
		 if [ "$lstat" -ne "0" ]
     then
        # verification failed
        rm -f /tmp/verify_$$.cnf
        exit 25
     fi 
     
     checkOutput=$(timeout 120 ./drat-trim $2 /tmp/verify_$$.cnf)
     lstat=$?
     rm -f /tmp/verify_$$.cnf

     if [ "$lstat" -eq "124" ]
     then
        # we got a timeout here!
	mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        exit 124
     fi
		
		 # instead of looking for exit code, grep for matching pattern
		 echo $checkOutput | grep "s VERIFIED" > /dev/null
		 gstat=$?
		 
		 echo "gstat: $gstat output: $checkOutput"
		
		 if [ "$gstat" -ne "0" ]
     then
        # verification failed
        exit 26
     fi 

		# UNSAT has been verified succesfully
    exit 20
fi

