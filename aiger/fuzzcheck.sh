#!/bin/sh

#
# create cnf files via fuzzing, check with verifier, apply cnfdd if required
#

prg=$1
mkdir -p bugs

cnf=/tmp/runcnfuzz-$$.cnf
sol=/tmp/runcnfuzz-$$.sol
log=runcnfuzz-$$.log
rm -f $log $cnf $sol
echo "[runcnfuzz] running $prg"
echo "[runcnfuzz] logging $log"
echo "[runcnfuzz] run with pid $$"
i=0
while true
do
  rm -f $cnf; 
  # create circuit
  ./aigfuzz  -S -s -b -z -m > $cnf
  seed=`grep 'c seed' $cnf|head -1|awk '{print $NF}'`
  head="`awk '{if(NR==1) print $2, $4, $5}' $cnf`"
  printf "%d %d %d %d %d\r" "$i" "$seed" $head
  i=`expr $i + 1`
  
  # generate another circuit, if this one does not match size expectations
	ands=`head -n 1 $cnf | awk '{print $2}'`;
	latches=`head -n 1 $cnf | awk '{print $4}'`;
	outputs=`head -n 1 $cnf | awk '{print $5}'`;
  bads=`head -n 1 $cnf | awk '{print $7}'`;
	if [ $ands -gt "100" -o $latches -lt 2 -o $latches -gt 20 -o $outputs -gt 1 ]
	then
	  continue;
	fi
  
  echo "use: A $ands   L $latches  O $outputs   B $bads"
  rm -f $sol
#  echo "check ..."
  ./toolCheck.sh $cnf $* # > /dev/null 2> /dev/null
  res=$?
#  echo "result $res"
  case $res in
    10) 
      continue
      ;;
    20) 
      continue
      ;;
    15)
    ;;
    25)
      mv $cnf bugs/error-$i-$res.aig
      continue
    ;;
  esac
  
  # reject everything with more outputs!
	# accept circuit with blimc for being "valid"
	./blimc -m $FILE 2> /dev/null
	lstat=$?
	if [ "$lstat" -eq "1" ]
	then
		continue
	fi
  
  head="`awk '{if(NR==1) print $2, $4, $5}' $cnf`"
  echo "[runcnfuzz] bug-$i-$res $head"
  echo $seed >> $log
  bug=bugs/bug-$i-$res.cnf
  mv $cnf $bug
  echo ""
  #if [ x"$qbf" = x ]
  #then
  #  ./cnfdd $bug $red ./toolCheck.sh $prg 1>/dev/null 2>/dev/null
  #else
  #  qbfdd.py -v -v -o $red $bug $prg 1>/dev/null 2>/dev/null
  #fi
  head="`awk '{if(NR==1) print $2, $4, $5}' $bug`"
#  echo "[runcnfuzz] $red $head"
  # rm -f $bug
done

