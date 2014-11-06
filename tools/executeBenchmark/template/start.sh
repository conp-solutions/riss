#!/bin/bash

# get project data from overview.dat file
TLIM=`cat overview.dat | awk '/^timeout/ { print $2 }'`
SLIM=`cat overview.dat | awk '/^spacelimit/ { print $2 }'`
thr=`cat overview.dat | awk '/^threads/ { print $2 }'`
atlas=`cat overview.dat | awk '/^atlas/ { print $2 }'`

#binary name
BIN=riss4
#name of the project
PROJNAME=riss4

#echo "threads: $thr, tlim: $TLIM"

ct=$TLIM
#echo "cpu time: $ct"

TimeH=$((ct / 3600))
TimeM=$((ct / 60))
TimeM=$((TimeM - TimeH * 60 + 5))

spacelimit=$((SLIM)) # no extra space!


echo $TLIM > data.txt
echo $SLIM >> data.txt
echo $BIN >> data.txt
echo $PROJNAME >> data.txt

DBFILE=db/$PROJNAME.csv
rm -f jobNumbers

PARAMS=`cat params.txt`
if [ "$PARAMS" == "NOPARAM" ]
then
    PARAMS=""
fi


cat files.txt | while read f
do
    if [ "$f" == "" ]; then
	continue
	fi
    
    KEY=`./getUniqueKey.sh "$f" "$PARAMS"`
    if [ -f "$DBFILE" ] && grep -q ^"$KEY" "$DBFILE"; then
	    :
	    else
	# on atlas :
	if [ "$atlas" -eq "1" ]
	then 
		bsub -W "$TimeH:$TimeM" -n $thr -M $spacelimit -u "" runJob.sh $TLIM $SLIM $BIN "$PARAMS" "$f" >> jobNumbers
	else
		# on vulcan or any other system that does not use the LFS batch queue system
		./runJob.sh $TLIM $SLIM $BIN "$PARAMS" "$f" 
	fi
	
	fi
done

if [ "$atlas" -eq "1" ]
then 
	# on atlas :
	bsub -J "$PROJNAME" ./wait.sh 
fi

