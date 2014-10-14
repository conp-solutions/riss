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

     rm -f /tmp/verify_$$.cnf

     if [ "$status" -ne "20" ]
     then
	exit $status
     fi


     timeout 90 ./lgl $2 2> /dev/null 1> /dev/null
     lstat=$?

     if [ "$status" -eq "124" ]
     then
        # we got a timeout here!
	mkdir -p lglTimeout # collect instances where lingeling performs badly
        mv $2 lglTimeout
        exit 124
     fi

     if [ $lstat -ne "20" ]
     then
       exit 25
     fi
    exit 20
fi

