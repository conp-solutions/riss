#!/bin/bash

# do a XY plot for two files
#
# ./plotXYfiles.sh out param-1.csv col1 param-2.csv col2
#
# TODO: set fail value!
#

failValue=3601
echo "plotXY use fail value $failValue"

#
# join algorithm
#
i=0 # counter
tmp=/tmp/joinTimes_$$
tmp2=/tmp/joinTimes2_$$
tmp3=/tmp/joinTimes3_$$
sep="$"
one="1"
rm -rf $tmp

#
# join all files, keep header, sort body
#
for f in $2 $4; 
do 

	if [ "$i" -eq "0" ]
	then
		awk -v col=$3 '{ if( NR == 1 ) print $1,$col}' $f > $tmp # init file with instance and time
		awk -v col=$3 -v fv=$failValue "{ if( NR > 1 ) {if( $sep$3 != \"-\" ) print $sep$one,$sep$3; else print $sep$one, fv } }" $f | sort -k 1b,1  >> $tmp
		
		head -n 1 $tmp
		i=1 #switch mode
	else

		awk -v col=$5 '{ if( NR == 1 ) print $1,$col}' $f > $tmp2 
		awk -v col=$5 -v fv=$failValue "{ if( NR > 1 ) {if( $sep$5 != \"-\" ) print $sep$one,$sep$5; else print $sep$one, fv } }"  $f | sort -k 1b,1 >> $tmp2
	
		# join, rename
		join $tmp $tmp2 > $tmp3
		mv $tmp3 $tmp
		
		head -n 1 $tmp
	
	fi

	less $tmp

done


#call the plot script with the new file
echo "call plotXY script to plot col col 2 and 3 of ..."
#head -n 7 $tmp
echo "failed: "
grep "$failValue" $tmp | wc -l
#tail -n 7 $tmp

# run simple XY plot, always plotting col 2 and 3 (there should not be more)
./plotXY.sh $1 $tmp 2 3

# delete all temporary files!
rm -f $tmp $tmp2 $tmp3

