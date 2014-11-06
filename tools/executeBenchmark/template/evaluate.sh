#!/bin/bash

#
#
# EVALUATE THE DATA THAT HAS BEEN PRODUCED IN THE EXPERIMENT
#
#

PROJECT=`cat data.txt | head -n4 | tail -n1`
BIN=`cat data.txt | head -n3 | tail -n1`
TLIMIT=`cat data.txt | head -n1`

rm -f data.csv

#
#
# main script
#
#

i=0
cat params.txt | grep -v ^$ | while read PARAMS
do
  if [ "$PARAMS" == "NOPARAMS" ]; then
      PARAMS=""
  fi

  let "i=$i+1"
  rm -f db/param-$i.csv

done

#
# TODO: add all new statistic extractions to the below line!
#

# print headline into the file
echo "Instance Status ExitCode RealTime CpuTime Memory Decisions Conflicts Ratio UpClauses DuplicateClauses Dups1 Dup2 DupsMore UpLits/sec" >> data.csv


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

    cat $TMPO > $TMPFILE
    #cat $TMPE > $TMPFILE
    cat $TMPR >> $TMPFILE
    ./csvExtractSingle.sh $TMPFILE $TLIMIT $f >> data.csv

    done
done 

rm -f tmp_$$.tmp
