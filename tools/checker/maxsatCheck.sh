rm -f /tmp/msbva_$$.cnf
rm -f /tmp/shuffle_$$.cnf


cp $1 /tmp/shuffle_$$.cnf

echo "run on /tmp/shuffle_$$.cnf"
#echo "run on /tmp/shuffle_$$.cnf" >> tmp.dat
timeout -k 9 10 ./msuncore -r bin-core-dis -incr /tmp/shuffle_$$.cnf 2> /dev/null > /tmp/output1_$$
exitC1=$?


# cat /tmp/output1_$$

echo "exit1: $exitC1" 

if [ "$exitC1" -eq "134" ]
then 
	rm -f /tmp/output1_$$
	exit 134
fi
if [ "$exitC1" -eq "139" ]
then 
	rm -f /tmp/output1_$$
	exit 138
fi
if [ "$exitC1" -eq "124" ]
then 
	rm -f /tmp/output1_$$
	exit 142
fi

solution1=`cat /tmp/output1_$$ | grep "^o" | tail -n 1 | awk '{print $2}'`
rm -f /tmp/output1_$$


timeout 10 ./mprocessor /tmp/shuffle_$$.cnf -dimacs=/tmp/msbva_$$.cnf -enabled_cp3 -cp3_stats -probe -bve -bce -bva -rate -bce-cle -ee -unhide -cce -hte -fm -xor -dense -cp3_dense_debug=2
exitC2=$?
echo "exit2: $exitC2" 

if [ "$exitC1" -eq "20" -a "$exitC2" -eq "20" ]
then
	rm -f /tmp/output2_$$
	rm -f /tmp/verify_$$.cnf
	rm -f /tmp/shuffle_$$.cnf
	exit 0
fi

if [ "$exitC2" -eq "134" ]
then 
  rm -f /tmp/shuffle_$$.cnf
	rm -f /tmp/msbva_$$.cnf
	exit 135
fi
if [ "$exitC2" -eq "139" ]
then 
  rm -f /tmp/shuffle_$$.cnf
	rm -f /tmp/msbva_$$.cnf
	exit 141
fi
if [ "$exitC2" -eq "124" ]
then 
	rm -f /tmp/output1_$$
	exit 143
fi

if [ "x$2" != "x" ]
then
  cat /tmp/msbva_$$.cnf
fi

timeout -k 9 10 ./msuncore -r bin-core-dis -incr /tmp/msbva_$$.cnf 2> /dev/null > /tmp/output2_$$
exitC3=$?

# cat /tmp/output2_$$

echo "exit3: $exitC3" 
rm -f /tmp/msbva_$$.cnf
rm -f /tmp/shuffle_$$.cnf

if [ "$exitC1" -eq "20" -a "$exitC3" -eq "20" ]
then
	rm -f /tmp/output2_$$
	rm -f /tmp/verify_$$.cnf
	rm -f /tmp/shuffle_$$.cnf
	exit 0
fi

if [ "$exitC3" -eq "134" ]
then 
	rm-f /tmp/output2_$$
	exit 136
fi
if [ "$exitC3" -eq "139" ]
then 
	rm -f /tmp/output2_$$
	exit 140
fi
if [ "$exitC3" -eq "124" ]
then 
	rm -f /tmp/output1_$$
	exit 144
fi

solution2=`cat /tmp/output2_$$ | grep "^o" | tail -n 1 | awk '{print $2}'`
rm -f /tmp/output2_$$
rm -f /tmp/verify_$$.cnf
rm -f /tmp/shuffle_$$.cnf

echo "$solution1 vs $solution2"

if [ "$solution1" -eq "$solution2" ]
then
	exit 0
elif [ "$solution1" -gt "$solution2" ]
then
	exit 1
elif [ "$solution1" -lt "$solution2" ]
then
	exit 2
fi

exit 3
