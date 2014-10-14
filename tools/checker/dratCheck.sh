#
#
# run the SAT solver and check its outcome
# SAT: verify the model
# UNSAT: check the printed proof
#
# usage: ./dratCheck.sh <tool/script> <input.cnf>
#
#

tmpModel=/tmp/verify_$$.cnf
tmpProof=/tmp/proof_$$.cnf

rm -f $tmpModel $tmpProof

#echo "run $1 with $2" >> tmp.dat

#
# run solver with script
# third parameter is file for drup proof
#
timeout -k 9 90 $1 $2 $tmpProof > $tmpModel #2> /dev/null;
status=$?

#echo "finish with state= $status" >> tmp.dat
echo "tool exit code: $status"

if [ "$status" -eq "124" ]
then
	# we got a timeout here!
	rm -f $tmpModel $tmpProof
	exit 124
fi

#
# check the model
#
if [ "$status" -eq "10" ]
then
	echo "check model..."
	./verify SAT $tmpModel $2 # 2>&1 > /dev/null
	vstat=$?
	rm -f $tmpModel $tmpProof
	if [ "$vstat" -eq "1" ]
	then
		exit 10
	else
		exit 15
	fi
else
# if the instance is unsatisfiable or another error occurred, report it
#	echo "exit with $status" >> tmp.dat

     rm -f $tmpModel

     if [ "$status" -ne "20" ]
     then
			exit $status
     fi

#
#
# check the DRAT proof
#
#
#		# use lgl as fall back, as long as drat-trim is not working properly
#	        rm -f $tmpModel $tmpProof
#		timeout -k 9 90  ./lgl $2
#		lec=$?
#
#		if [ "$status" -eq "124" ]
#		then
#		        # we got a timeout here!
#		        exit 124
#		fi
#		if [ "$status" -eq "20" ]
#		then
#		        # we got a timeout here!
#		        exit 20
#		fi
#		exit 25

		#
		# use drat trim to check the proof
		#
		echo "check proof ... (`cat $tmpProof | wc -l`)"
		echo "head: `head $tmpProof`"
		echo "tail: `tail $tmpProof`"
#		timeout -k 9 180 python drup-check.py $2 $tmpProof
		timeout -k 9 90 ./drat-trim $2 $tmpProof > $tmpModel
		lstat=$?

		echo "checker output (ec: $lstat)"
		cat $tmpModel
		echo "grep: <`grep ^s $tmpModel`>"
		
		# trivial unsat, then exit code 20 is alright!
		if [ "`grep ^s $tmpModel`" == "s VERIFIED" ]
		then
			rm -f $tmpProof $tmpModel
			exit 20
		fi
		

		echo "proof stat: $lstat"

		if [ "$status" -eq "124" ]
		then
      # timeout!
   		rm -f $tmpProof
			exit 124
		fi

		# check proof result
		if [ $lstat -ne "0" -a $lstat -ne "20" ]
		then
			# cat $tmpProof
			rm -f $tmpProof $tmpModel
			exit 25
		fi
		rm -f $tmpProof $tmpModel
    exit 20
fi

