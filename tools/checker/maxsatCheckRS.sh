rm -f /tmp/msbva_$$.cnf
rm -f /tmp/shuffle_$$.cnf


infile=$1
shift

#
# convert CNF into WCNF
#
./msTool $infile -method=CnfToWcnf -s -p > /tmp/shuffle_$$.cnf

echo "run on /tmp/shuffle_$$.cnf" >> tmp.dat
timeout 20 ./open-wbo_static /tmp/shuffle_$$.cnf 2> /dev/null > /tmp/output1_$$
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

solution1=`cat /tmp/output1_$$ | grep "^o" | tail -n 1 | awk '{print $2}'`
rm -f /tmp/output1_$$


timeout 20 ./mprocessor /tmp/shuffle_$$.cnf -dimacs=/tmp/msbva_$$.cnf $*
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

timeout 20 ./open-wbo_static /tmp/msbva_$$.cnf 2> /dev/null > /tmp/output2_$$
exitC3=$?

if [ "$exitC1" -eq "20" -a "$exitC3" -eq "20" ]
then
	rm -f /tmp/output2_$$
	rm -f /tmp/verify_$$.cnf
	rm -f /tmp/shuffle_$$.cnf
	exit 0
fi

echo "exit3: $exitC3" 
rm -f /tmp/msbva_$$.cnf
rm -f /tmp/shuffle_$$.cnf

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
