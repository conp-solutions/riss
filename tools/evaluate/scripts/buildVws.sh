#
# create the VWS (worst) from all the given csv files (uses column 4)
#
# usage: ./buildVws.sh outfile S1.csv ... Sn.csv
#


outfile=$1
shift

#
# algorithm
#
i=0 # counter
tmp=/tmp/joinTimes_$$
tmp2=/tmp/joinTimes2_$$
tmp3=/tmp/joinTimes3_$$
rm -rf $tmp $tmp2 $tmp3

#
# join all files, keep header, sort body
#
for f in $*; 
do 
	echo "work on $f"
	if [ "$i" -eq "0" ]
	then
		awk '{ if( NR == 1 ) print $1,$2,$3,$4}' $f > $tmp # init file with instance and time
		awk '{ if( NR > 1 ) print $1,$2,$3,$4}'  $f | sort -k 1b,1 >> $tmp
		
		head -n 1 $tmp
		i=1 #switch mode
	else

		awk '{ if( NR == 1 ) print $1,$2,$3,$4}' $f > $tmp2 
		awk '{ if( NR > 1 ) print $1,$2,$3,$4}'  $f | sort -k 1b,1 >> $tmp2
	
		# join, rename
		join $tmp $tmp2 > $tmp3

		# select always the worse configuration		
		awk '{ if( NR == 1 ) print "Instance Status.vws ExitCode.vws RealTime.vws"
			else {
			if( $4 == "-" ) print $1,$2,$3,$4;
			else if( $7 == "-" ) print $1,$5,$6,$7;
			else if( $7 > $4 ) print $1,$5,$6,$7;
			else print $1,$2,$3,$4; } }' $tmp3 > $tmp
	
		cat $tmp | wc -l
	fi

done

echo "wrote output into $outfile"
mv $tmp $outfile
rm -rf $tmp $tmp2 $tmp3
