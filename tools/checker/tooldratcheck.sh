rm -f /tmp/verify_$$.cnf

#echo "run $1 with $2" >> tmp.dat

timeout 90 $1 $2 > /tmp/verify_$$.cnf 2> /dev/null;
status=$?


proofcheckParams=" -bwc-pseudoParallel -splitLoad=2 -threads=2 -backward "

removecommand="rm -f"
removecommand="ls"

echo "run with PID $?"

#echo "finish with state= $status" >> tmp.dat

if [ "$status" -eq "124" ]
then
	# we got a timeout here!
	$removecommand /tmp/verify_$$.cnf
	exit 124
fi

# cat /tmp/verify.cnf
if [ "$status" -eq "10" ]
then
	./verify SAT /tmp/verify_$$.cnf $2 # 2>&1 > /dev/null
	vstat=$?
	$removecommand /tmp/verify_$$.cnf
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

     checkOutput=$(timeout 120 ./proofcheck $proofcheckParams $2 /tmp/verify_$$.cnf -cores=/tmp/cores-$$.cnf -lemmas=/tmp/lemmas-$$.cnf)
     lstat=$?

     if [ "$lstat" -eq "124" ]
     then
        # we got a timeout here!
				mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 124
     fi
		
     if [ "$lstat" -ne "0" ]
     then
        # verification failed
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 25
     fi 
     
     checkOutput=$(timeout 120 ./drat-trim $2 /tmp/verify_$$.cnf)
     lstat=$?

     if [ "$lstat" -eq "124" ]
     then
        # we got a timeout here!
	      mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 124
     fi
		
		 # instead of looking for exit code, grep for matching pattern
		 echo $checkOutput | grep "s VERIFIED" > /dev/null
		 gstat=$?
		 
		 echo "gstat: $gstat output: $checkOutput"
		
		 if [ "$gstat" -ne "0" ]
     then
        # verification failed
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 26
     fi 
     
     #
     # shrink proof (and hopefully make it invalid)
     #
     cat /tmp/verify_$$.cnf | awk '{ if ($1 == "o" || NR % 5 != 4) print $0;} END {print "0"} ' > /tmp/verify_$$-2.cnf
     mv /tmp/verify_$$-2.cnf /tmp/verify_$$.cnf
     
     echo ""
     echo "shrink solver output and test again"
     echo ""
     
     checkOutput=$(timeout 120 ./proofcheck $proofcheckParams $2 /tmp/verify_$$.cnf)
     pstat=$?
     echo "proofcheck stopped with $pstat"

     if [ "$pstat" -eq "124" ]
     then
        # we got a timeout here!
				mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 124
     fi
		
		
		 ./drat-trim /tmp/core.cnf /tmp/lemmas.cnf -S
		
     checkOutput=$(timeout 120 ./drat-trim $2 /tmp/verify_$$.cnf -S )
     lstat=$?

     if [ "$lstat" -eq "124" ]
     then
        # we got a timeout here!
	      mkdir -p verificationTimeout # collect instances where lingeling performs badly
        mv $2 verificationTimeout
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 124
     fi
		
		 # instead of looking for exit code, grep for matching pattern
		 echo $checkOutput | grep "s VERIFIED" > /dev/null
		 gstat=$?
		 
		 echo "gstat: $gstat (1=valid) output: $pstat (0=valid)"
		
		 # proofcheck said valid, drat-trim said invalid
		 if [ "$gstat" -ne "0" -a "$pstat" -eq "0" ]
     then
        # verification failed
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 27
     fi

		 # proofcheck said invalid, drat-trim said valid
		 if [ "$gstat" -ne "1" -a "$pstat" -ne "0" ]
     then
        # verification failed
        $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
        exit 28
     fi
     
     
     #
     # if a core file has been written (might not have been the case due to trivial unsat)
     #
     if [ -f /tmp/cores-$$.cnf ]
     then
     
		   #
		   # check whether extracted cores and lemmas are correctly unsatisfiable
		   #
		   $removecommand /tmp/verify_$$.cnf 
		   checkOutput=$(timeout 120 ./drat-trim /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf)
		   lstat=$?

		   # delete none necessary files
			 $removecommand /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
			 
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
		      $removecommand /tmp/verify_$$.cnf /tmp/cores-$$.cnf /tmp/lemmas-$$.cnf
		      exit 29
		   fi 
     
    fi

		# UNSAT has been verified succesfully
    exit 20
fi

