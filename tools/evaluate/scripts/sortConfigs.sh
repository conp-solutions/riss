#
# sort the configurations of the given CSV files in the order they contribute most to the given portfolio of previously selected configurations
#
# usage: ./sortConfigs.sh S1.csv ... Sn.csv
#



#
# algorithm
#
i=0 # counter
tmp=/tmp/joinTimes_$$
tmp2=/tmp/joinTimes2_$$
tmp3=/tmp/joinTimes3_$$
ref=/tmp/joinRef_$$
rm -rf $tmp $tmp2 $tmp3

allConfigs=$*
orderedConfigs=""

echo "configs:  $allConfigs"

# select the first best configuration
solved=0
bestConfig=""
newConfigs=""
for f in $allConfigs
do
	# get number of solved instances
	s=`grep " ok " $f | wc -l`
	# if this is the best configuration, the use it
	if [ "$s" -gt "$solved" ]
	then
		# add current best to next iterations configs
		newConfigs="$newConfigs $bestConfig"
		# store curently best config
		bestConfig=$f
		solved=$s
	else
		# add current config to next iterations configs
		newConfigs="$newConfigs $f"
	fi

done

orderedConfigs="$orderedConfigs $bestConfig"
echo "best: $bestConfig with $solved"
echo "consider next: $newConfigs"

bestFile=/tmp/bestFile_$$

# store the current best file sorted to be able to always join with it
awk '{ if( NR == 1 ) print $1,$2,$3,$4}' $bestConfig > $bestFile 								# init file with instance and time
awk '{ if( NR > 1 ) print $1,$2,$3,$4}'  $bestConfig | sort -k 1b,1 >> $bestFile	# add the actual data

#
# iterate until done
#
iteration=0
lastCommonlyBest=0
while [ "$newConfigs" != "" ]
do
	iteration=$(($iteration+1))
	allConfigs=$newConfigs
	newConfigs=""
	thisRoundConfig=""
	commonlySolved=0
	for f in $allConfigs
	do
		echo $f
		awk '{ if( NR == 1 ) print $1,$2,$3,$4}' $f > $tmp
		awk '{ if( NR > 1 ) print $1,$2,$3,$4}'  $f | sort -k 1b,1 >> $tmp

		# create VBS
		# first, combine the two files properly
		join $bestFile $tmp > $tmp2
		# next, select always the better configuration		
		awk '{ if( NR == 1 ) print "Instance Status.vbs ExitCode.vbs RealTime.vbs"
			else {
			if( $4 == "-" ) print $1,$5,$6,$7;
			else if( $7 == "-" ) print $1,$2,$3,$4;
			else if( $7 < $4 ) print $1,$5,$6,$7;
			else print $1,$2,$3,$4; } }' $tmp2 > $tmp3
		# stats
		solvedSAT=`grep " ok 10 " $tmp3 | wc -l `
		solvedUNSAT=`grep " ok 20 " $tmp3 | wc -l `
		echo "$(( $solvedSAT + $solvedUNSAT )) == $solvedSAT + $solvedUNSAT"
		
		if [ "$(( $solvedSAT + $solvedUNSAT ))" -gt "$commonlySolved" ] 
		then
			commonlySolved=$(( $solvedSAT + $solvedUNSAT ))	# store new intermediate best result
			echo "temporarily select $f"
			if [ "$thisRoundConfig" != "" ]
			then
				newConfigs="$newConfigs $thisRoundConfig" 		# add old best config to next iteration configs
			fi
			thisRoundConfig=$f															# store current config as this rounds config
		else
			newConfigs="$newConfigs $f"				 							# add current config to cofigs for next iteration
		fi
		
	done
	
	# if there is not improvement any more, stop!
	if [ "$lastCommonlyBest" -ge "$commonlySolved" ]
	then
		break # stop because no improvement is given
	fi
	lastCommonlyBest=$commonlySolved	# memorize current VBS number of instances, to be able to early-abort
	
	# handle the currently selected configuration
	if [ "x$thisRoundConfig" != "x" ]
	then
		# checked all configs, now build the best file with the next selected config. ...
		# first, combine the two files properly
		awk '{ if( NR == 1 ) print $1,$2,$3,$4}' $thisRoundConfig > $tmp
		awk '{ if( NR > 1 ) print $1,$2,$3,$4}'  $thisRoundConfig | sort -k 1b,1 >> $tmp
		join $bestFile $tmp > $tmp2
		# next, select always the better configuration		
		awk '{ if( NR == 1 ) print "Instance Status.vbs ExitCode.vbs RealTime.vbs"
			else {
			if( $4 == "-" ) print $1,$5,$6,$7;
			else if( $7 == "-" ) print $1,$2,$3,$4;
			else if( $7 < $4 ) print $1,$5,$6,$7;
			else print $1,$2,$3,$4; } }' $tmp2 > $bestFile
		# stats
		solvedSAT=`grep " ok 10 " $tmp3 | wc -l `
		solvedUNSAT=`grep " ok 20 " $tmp3 | wc -l `
		orderedConfigs="$orderedConfigs $thisRoundConfig"
		echo ""
		echo "this iterations($iteration) best: "
		echo "$orderedConfigs :  $(( $solvedSAT + $solvedUNSAT )) == $solvedSAT + $solvedUNSAT"
		echo ""
	fi
	
done


exit 1


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

		# select always the better configuration		
		awk '{ if( NR == 1 ) print "Instance Status.vbs ExitCode.vbs RealTime.vbs"
			else {
			if( $4 == "-" ) print $1,$5,$6,$7;
			else if( $7 == "-" ) print $1,$2,$3,$4;
			else if( $7 < $4 ) print $1,$5,$6,$7;
			else print $1,$2,$3,$4; } }' $tmp3 > $tmp
	
		cat $tmp | wc -l
	fi

done

echo "wrote output into $outfile"
mv $tmp $outfile
