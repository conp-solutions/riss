#!/bin/bash

#
#
# EVALUATE THE DATA THAT HAS BEEN PRODUCED IN THE EXPERIMENT
#
#

PROJECT=`cat data.txt | head -n4 | tail -n1`
BIN=`cat data.txt | head -n3 | tail -n1`
TLIMIT=`cat data.txt | head -n1`

f="$*"

echo "file: $f"

#
#
# main script
#
#

i=0
cat params.txt | grep -v ^$ | while read PARAMS
do
  if [ "$PARAMS" == "NOPARAM" ]; then
      PARAMS=""
  fi
  let "i=$i+1"
done

#
# TODO: add all new statistic extractions to the below line!
#

# print headline into the file
echo "Instance Status ExitCode RealTime CpuTime Memory CreatedNodes UnsolvedNodes SplitSolvedNodes \
RetriedNodes TreeHeight EvaLevel LoadSplits \
duplicates blockedThreads sharedUnsat level1clauses level1clausesEff leve0clauses leve0clausesEff sharedUnaryClauses sharedBinaryClauses sharedGlueClauses \
currentLevelClauses previousLevelClauses minimizedClauses minimizedWorsenings totallyShared sharedMinusDeleted"  >> check.csv


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


  if [ "$f" == "" ]; then
		break
	fi

  # adopted to minisats needs
  TMPE=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.err
  TMPO=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.out
  TMPR=tmp/`./getHashKey.sh "$f" "$PARAMS"`.tmp.dat.runlim
  TMPFILE=tmp_check.tmp

  cat $TMPO >> $TMPFILE
  cat $TMPE > $TMPFILE
  cat $TMPR >> $TMPFILE
  ./csvExtractSingle.sh $TMPFILE $TLIMIT $f >> check.csv

done 

rm -f tmp_$$.tmp
