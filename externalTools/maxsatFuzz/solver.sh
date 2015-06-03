#
#
#

param="-solverConfig=MAXSAT:INCSOLVE -preConfig=plain_BVE:plain_ELS"

tmp=/tmp/out_$$

./open-wbo-riss $param $1  2> /dev/null >$tmp
e1=$?
result=$( cat $tmp | grep "^o" | tail -n 1 | awk '{print $2}')


./open-wbo $1 2> /dev/null > $tmp
e2=$?
ref=$( cat $tmp  | grep "^o" | tail -n 1 | awk '{print $2}') 

rm -f $tmp

echo "result: $result with $e1, ref: $ref with $e2"

if [ "$ref" == "$result" ]
then
	exit 2
else
	if [ "$e1" == "$e2" ]
	then
		exit $e1
	fi
	exit 57
fi
