#!/bin/bash

#
#
# EVALUATE THE DATA THAT HAS BEEN PRODUCED IN THE EXPERIMENT
#
#

PROJECT=`cat data.txt | head -n4 | tail -n1`
BIN=`cat data.txt | head -n3 | tail -n1`
TLIMIT=`cat data.txt | head -n1`

# rm -f data.csv

#
#
# main script
#
#

./extract > data.csv


# evaluate each file
i=0
cat params.txt | grep -v ^$ | while read PARAMS
do
  if [ "$PARAMS" == "NOPARAM" ]; then
      PARAMS=""
  fi

	echo "work with params: $PARAMS"

  let "i=$i+1"
  echo "param $i"


  cat files.txt | while read f
  do
    if [ "$f" == "" ]; then
			continue
		fi

    # adopted to minisats needs
    TMPE=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.err
    TMPO=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.out
    TMPR=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.runlim
    TMPFILE=tmp_$$.tmp

#    cat $TMPO > $TMPFILE
#    cat $TMPE >> $TMPFILE
#    cat $TMPR >> $TMPFILE
    ./extract  $f $TMPO $TMPE $TMPR >> data.csv

    done
done 

rm -f tmp_$$.tmp
