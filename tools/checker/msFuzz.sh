#!/bin/sh

#
# create cnf files via fuzzing, check with verifier, apply cnfdd if required
#
#
#prg=$1 here not necesary, since maxsatCheck.sh is called anyways
#

# this can be initialized to a max. size of an unreduced buggy formula
bestcls=-1

cnf=/tmp/runcnfuzz-$$.cnf
sol=/tmp/runcnfuzz-$$.sol
log=runcnfuzz-$$.log
rm -f $log $cnf $sol
echo "[runcnfuzz] running $prg"
echo "[runcnfuzz] logging $log"
echo "run $1 $2" > $log
echo "[runcnfuzz] run with pid $$"
i=0

# set limit for number of instances to be solved
limit=1
if [ "x$2" != "x" ]
then
	limit=$2
fi
test=0

while [ "$test" -lt "$limit" ]
do
  rm -f $cnf; 
  ./wcnfuzz $CNFUZZOPTIONS > $cnf
  seed=`grep 'c seed' $cnf|head -1|awk '{print $NF}'`
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  printf "%d %d %d %d\r" "$i" "$seed" $head
  i=`expr $i + 1`
  rm -f $sol

	# set limit for number of instances to be solved
	if [ "x$2" != "x" ]
	then
		test=$(($test+1))
	fi
  
  thiscls=`awk '/p cnf /{print $4}' $cnf`
  if [ $bestcls -ne "-1" ]
  then
  	if [ $bestcls -le $thiscls ]
  	then
#  		echo "reject new bug with $thiscls clauses"
  		continue
	  fi
  fi
  
  ./maxsatCheck.sh $cnf > /dev/null 2> /dev/null
  res=$?
#  echo "result $res"
	if [ "$res" -eq "0" -o "$res" -ge 3 ]
	then
		continue # 0, and high numbers have nothing to do with the preprocessor!
	fi
#  case $res in
#    136)
#    	echo "timeout3 with $seed" > $log
#    	mv $cnf timeout-$seed.cnf
#      continue
#      ;;
#    0) # good case
#      continue
#      ;;
#  esac
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  echo "[runcnfuzz] bug-$seed $head"
	  echo "result $res"
  echo $seed >> $log
  
  #
  # consider only bugs that are smaller then the ones we found already
  #
  if [ $bestcls -eq "-1" ]
  then
  	bestcls=$thiscls
  	echo "init bestcls with $bestcls"
  elif [ $bestcls -le $thiscls ]
  then
  	echo "reject new bug with $thiscls clauses"
  	continue
  fi
 	bestcls=$thiscls # store better clause count!
 	echo "set bestcls to $bestcls"
  red=red-$seed.cnf
  bug=bug-$seed.cnf
  mv $cnf $bug
  #if [ x"$qbf" = x ]
  #then
    ./cnfdd $bug $red ./maxsatCheck.sh 1>/dev/null 2>/dev/null
  #else
  #  qbfdd.py -v -v -o $red $bug 1>/dev/null 2>/dev/null
  #fi
  head="`awk '/p cnf /{print $3, $4}' $red`"
  echo "[runcnfuzz] $red $head"
  # rm -f $bug
done

echo ""
echo "did $test out of $limit tests"

