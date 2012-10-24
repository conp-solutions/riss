#!/bin/sh

#
# modified version of runcnfuzz. can also find false UNSAT
# however, cannot minimized those bugs
#

die () {
  echo "*** runcnfuzz: $*" 1>&2
  exit 1
}
prg=""
qbf=""
ign=no
while [ $# -gt 0 ]
do
  case $1 in
    -h) echo "usage: runcnfuzz [-h][-q][-i] <prg> [<prgopts>...]";exit 0;;
    -q) qbf="-q";;
    -i) ign=yes;;
    -*) die "invalid command line option";;
    *) prg="$*"; break;;
  esac
  shift
done
[ x"$prg" = x ] && die "no program specified"
cnf=/tmp/runcnfuzz-$$.cnf
sol=/tmp/runcnfuzz-$$.sol
log=runcnfuzz-$$.log
rm -f $log
trap "rm -f $cnf $sol;exit 1" 2 9 15
i=0
echo "[runcnfuzz] running $prg"
echo "[runcnfuzz] logging $log"
[ x"$qbf" = x ] || echo "[runcnfuzz] generating QBF formulas"
[ $ign = yes ] && echo "[runcnfuzz] ignoring witness and zero exit code"
while true
do
  rm -f $cnf
  ./cnfuzz $qbf $CNFUZZOPTIONS > $cnf
  seed=`grep 'c seed' $cnf|head -1|awk '{print $NF}'`
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  printf "%d %d %d %d\r" "$i" "$seed" $head
  i=`expr $i + 1`
  rm -f $sol
  $prg $cnf 1>$sol 2>/dev/null
  res=$?
  case $res in
    10) 
      [ x"$qbf" = x ] || continue
      [ $ign = yes ] && continue
      ./precochk $cnf $sol >/dev/null 2>/dev/null
      [ $? = 10 ] && continue
      ;;
    20) 
      ./lgl $cnf 2> /dev/null 1> /dev/null
      lstat=$?
      if [ $lstat -ne "20" ]
      then
        head="`awk '/p cnf /{print $3, $4}' $cnf`"
        echo "[runcnfuzz] FALSE UNSAT bug-$seed $head"
        mv $cnf bug-$seed.cnf
        echo $seed >> $log
      fi
      continue;;
    0) [ $ign = yes ] && continue;;
  esac
  head="`awk '/p cnf /{print $3, $4}' $cnf`"
  echo "[runcnfuzz] bug-$seed $head"
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
  echo "[runcnfuzz] $red $head"
  # rm -f $bug
done

