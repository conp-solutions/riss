#!/bin/sh

#
# create cnf files via fuzzing, check with verifier, apply cnfdd if required
#

prg=$1

cnf=/tmp/runcnfuzz-$$.cnf
sol=/tmp/runcnfuzz-$$.sol
log=runcnfuzz-$$.log
rm -f $log $cnf $sol
echo "[runcnfuzz] running $prg"
echo "[runcnfuzz] logging $log"
i=0
while true
do
  rm -f $cnf
  ./cnfuzz $CNFUZZOPTIONS > $cnf
  seed=`grep 'c seed' $cnf|head -1|awk '{print $NF}'`
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  printf "%d %d %d %d\r" "$i" "$seed" $head
  i=`expr $i + 1`
  rm -f $sol
  ./toolCheck.sh $prg $cnf > /dev/null 2> /dev/null
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
    ;;
  esac
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  echo "[runcnfuzz] bug-$seed $head"
  echo $seed >> $log
  red=red-$seed.cnf
  bug=bug-$seed.cnf
  mv $cnf $bug
  #if [ x"$qbf" = x ]
  #then
  #  ./cnfdd $bug $red ./toolCheck.sh $prg 1>/dev/null 2>/dev/null
  #else
  #  qbfdd.py -v -v -o $red $bug $prg 1>/dev/null 2>/dev/null
  #fi
  head="`awk '/p cnf /{print $3, $4}' $bug`"
  echo "[runcnfuzz] $bug $head"
  # rm -f $bug
done

