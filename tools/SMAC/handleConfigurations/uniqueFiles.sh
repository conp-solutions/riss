#
#
# if executed in the directory "smac-output" the script creates a file that contains all the best configurations of the last SMAC run
#
#

tmp=/tmp/uniq_$$

sort -k 8 -k 9 -g $1 > $tmp
less $tmp

lastData=""

cat $tmp | while read CONFIG
do
	# get data of current line
	fn=`echo $CONFIG | awk '{print $1}'`
	bn=`basename $fn`
	thisData="`awk '{print $8,$9}'` $bn"
	
	# check whether the whole procedure is initialized
	if [ "$lastData" == "" ]
	then
		echo $CONFIG
		lastData=$thisData
		continue
	fi

	# compare two lines
	if [ "$lastData" != "$thisData" ]
	then
		echo $CONFIG
		lastData=$thisData
		continue
	else
		echo "REJECT $CONFIG"
	fi


done

# clean up
rm -f $tmp

exit

