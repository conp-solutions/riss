#!/bin/sh

#
# create cnf files via fuzzing, check with verifier, apply cnfdd if required
#

prg=$1

if [ "x" == "x$prg" ]
then
  echo "no program given!"
fi

cnf=/tmp/fuzzDrup-$$.cnf
sol=/tmp/fuzzDrup-$$.sol
log=fuzzDrup-$$.log
rm -f $log $cnf $sol
echo "[fuzzDrup] running $prg"
echo "[fuzzDrup] logging $log"
echo "[fuzzDrup] run with pid $$"
i=0
while true
do
  rm -f $cnf; 
  ./cnfuzz $CNFUZZOPTIONS > $cnf
  seed=`grep 'c seed' $cnf|head -1|awk '{print $NF}'`
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  
#
#  choose one of the two!
#
  printf "%d %d %d %d\r" "$i" "$seed" $head
#  echo "$i $seed $head"
  i=`expr $i + 1`
  rm -f $sol
  $prg $cnf  > /dev/null 2> /dev/null ;
  res=$?
#  echo "result $res"
  case $res in
    0) 
      continue
      ;;
    1)
    ;;
  esac
  echo "saw return code    $res            "
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  echo "[fuzzDrup] bug-$seed $head"
  echo $seed >> $log
  red=red-$seed.cnf
  bug=bug-$seed.cnf
  mv $cnf $bug
  #if [ x"$qbf" = x ]
  #then
    ./cnfdd $bug $red $prg 1>/dev/null 2>/dev/null
  #else
  #  qbfdd.py -v -v -o $red $bug $prg 1>/dev/null 2>/dev/null
  #fi
  head="`awk '/p cnf /{print $3, $4}' $red`"
  echo "[fuzzDrup] $red $head"
  # rm -f $bug
done

