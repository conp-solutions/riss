#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

echo "it"
time ./riss3g -enabled_cp3 -ee -cp3_stats -quiet -cp3_ee_it -cp3-exit-pp   $1 2> /tmp/out-it > /dev/null;
if [ "$?" -ne "0" ]
then
  # tell about program error!
  exit 2
fi

awk '/^c \[STAT\] EE/ {print $8}' /tmp/out-it
ee1=`awk '/^c \[STAT\] EE/ {print $8}' /tmp/out-it`

echo "rec"
time ./riss3g -enabled_cp3 -ee -cp3_stats -quiet -no-cp3_ee_it -cp3-exit-pp $1 2> /tmp/out-rec > /dev/null;
if [ "$?" -ne "0" ]
then
  # tell about program error!
  exit 3
fi


awk '/^c \[STAT\] EE/ {print $8}' /tmp/out-rec
ee2=`awk '/^c \[STAT\] EE/ {print $8}' /tmp/out-rec`

echo "it: $ee1  rec: $ee2"

# numbers have to be equal!
if [ "$ee1" -eq "$ee2" ]
then
  exit 1
else
	exit 0
fi

