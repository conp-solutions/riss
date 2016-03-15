#
#
# fir brief check for small bugs in the program by running over the whole set of bugs once
#
#

errorFile=$1

# on the machine where SMAC is running
#
# for f in RunGroup-2014-02-17--*/log-warn0.txt ; do echo $f; grep "./riss " $f  | tail -n 1 >> errors1702.txt ; done
#


#
# turn the file received from the SMAC runs into a file of lines to be checked
#
awk '{ p = 0; s="" ; for( i = 1 ; i <= NF; ++ i ) { if( $i == "./riss" ) p=1; if (p==1) s = (s " " $i); } print s; }' $errorFile > /tmp/filteredErrors_$$.txt

#
# run each command with the CSSC model check script
#
while read line
do
	timeout -k 5 60 ./modelCheckedCSSC.sh $line 2> /dev/null > /dev/null
	stat=$?
	if [ $stat -eq "1" ]  # no bug (any more)
	then
		continue
	fi
	if [ $stat -eq "124" ]  # instance times out
	then
		continue
	fi
	echo "$line"
	echo "exit code: $stat"
	echo ""
	sleep 5
done < /tmp/filteredErrors_$$.txt
