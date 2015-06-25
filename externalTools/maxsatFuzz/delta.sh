#
# fuzzer
#

f=$1

o=$2
tool=$3

callTime=$(date +%s)

# get reference exit code
echo "$(($(date +%s)-$callTime)): initial call"
$tool $f
ec=$?
echo "$(($(date +%s)-$callTime)): ref exit code: $ec"

tmp=/tmp/tmp_$$.wcnf
tmp2=/tmp/tmp_$$_2.wcnf
cp $f $tmp

sep="$"
zero="0"

# number of lines
lines=`cat $f | wc -l`
lines=$(($lines-1))

interval=$(($lines / 10))

calls=0

while [ "$interval" -ge "1" ]
do

#reinit lines variable
lines=`cat $f | wc -l`
lines=$(($lines-1))
while [ "$lines" -ge "$(($interval+1))" ]
do
	awk "{ if( NR == 0 || (NR < $lines || NR > $lines + $interval) ) print $sep$zero; }" $tmp > $tmp2

	calls=$(($calls+1))
	echo "$(($(date +%s)-$callTime)): call $calls (`cat $tmp2 | wc -l` vs `cat $f | wc -l`)"
	$tool $tmp2
	ec2=$?
	echo "$(($(date +%s)-$callTime)): exit code: $ec2, ref $ec"

	# use new file
	if [ "$ec2" == "$ec" ]
	then
		cp $tmp2 $tmp
		cp $tmp $o
	fi

  # dec lines
  lines=$(($lines-$interval))

done

  interval=$(($interval / 2))
  echo "set interval to $interval"
done

mv $tmp $o

rm -f $tmp2
