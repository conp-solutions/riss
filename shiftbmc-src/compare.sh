#!/bin/sh

#
# create cnf files via fuzzing, check with verifier, apply cnfdd if required
#

prg1="./aigbmc -bmc_m -bmc_l -bmc_s -bmc_p -bmc_d -enabled_cp3 -bve -unhide -3res -bva -ee -dense -cp3_dense_forw -bmc_x -bmc_y"
prg2="./aigbmc -bmc_m -bmc_l -bmc_s -bmc_p -bmc_d -enabled_cp3 -bve -unhide -3res -bva -ee -dense -cp3_dense_forw"

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
  rm -f $sol
#  echo "check ..."
p1s=`date +%s`
  ./toolCheck.sh $cnf $prg1 # > /dev/null 2> /dev/null
  res=$?
p1e=`date +%s`  
  
p2s=`date +%s`
  ./toolCheck.sh $cnf $prg2 # > /dev/null 2> /dev/null
p2e=`date +%s`  

echo "c 1: $(( $p1e - $p1s))    -- 2: $(( $p2e - $p2s))"
  
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

