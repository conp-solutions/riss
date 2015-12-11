#
# join all the run times from the available .csv files
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
		awk '{ if( NR == 1 ) print $0}' $f > $tmp # init file with instance and time
		awk '{ if( NR > 1 ) print $0}'  $f | sort -k 1b,1 >> $tmp
		
		head -n 1 $tmp
		i=1 #switch mode
	else

		awk '{ if( NR == 1 ) print $0}' $f > $tmp2 
		awk '{ if( NR > 1 ) print $0}'  $f | sort -k 1b,1 >> $tmp2
	
		# join, rename
		join $tmp $tmp2 > $tmp3
		mv $tmp3 $tmp
		
		head -n 1 $tmp
		cat $tmp | wc -l
	fi

done

echo "wrote output into $outfile"
mv $tmp $outfile
