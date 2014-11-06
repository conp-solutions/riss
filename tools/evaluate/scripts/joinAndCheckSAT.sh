#
# normalize data in data files, removing all invalid exit codes!
#
# store result in $1
#


outfile=$1

#
# algorithm
#
i=0 # counter
tmp=/tmp/joinSat_$$
tmp2=/tmp/joinSat2_$$
tmp3=/tmp/joinSat3_$$
rm -rf $tmp

#
# join all files, keep header, sort body
#
for f in *.csv; 
do 
	echo "work on $f"
	if [ "$i" -eq "0" ]
	then
		awk '{ if( NR == 1 ) print $1,$3}' $f > $tmp # init file with instance and time
		awk '{ if( NR > 1 ) print $1,$3}'  $f | sort -k 1b,1 >> $tmp
		
		head -n 1 $tmp
		i=1 #switch mode
	else

		awk '{ if( NR == 1 ) print $1,$3}' $f > $tmp2 
		awk '{ if( NR > 1 ) print $1,$3}'  $f | sort -k 1b,1 >> $tmp2
	
		# join, rename
		join $tmp $tmp2 > $tmp3
		mv $tmp3 $tmp
		
		head -n 1 $tmp
	
	fi

done

echo "wrote output into $outfile"
mv $tmp $outfile

echo "combine run times ... "


#
# check for each line whether all agree, or some results 
#
echo "valid-check ..."
awk '{ if( NR > 1 ) { fw=0;fe=0;t=0; for (i=2; i<=NF; i++) { if( t == 0 && ($i == 10 || $i == 20) ) {t=$i;fw=i} else if( ($i == 10 && $t == 20) || ($i == 20 && $t == 10) ) {t=1; if(fe==0)fe=i} }; if ( t == 1 ) print $0, "-errors-",$fw,$fe;  } }' $outfile
#tot=0; for (i=1; i<=NF; i++) tot += $i; print	tot/NF;


