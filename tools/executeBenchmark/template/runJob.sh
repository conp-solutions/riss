#!/bin/bash

TLIM=$1
SLIM=$2
BIN=$3
num_params=$(($#-4))
PARAMS=${@:4:$num_params}
FILE="${@: -1}"

TMPFILE=tmp/`./getHashKey.sh "$FILE" "$PARAMS"`.tmp.dat

# measure time, use runlim to feed it with limits
before=`date +%s`
# with bash (does not kill process properly!)
#./runlim -k -r $TLIM -s $SLIM ./solver.sh "bin/$BIN $FILE $PARAMS $TMPFILE.out $TMPFILE.err" 2> $TMPFILE.runlim;
# with python
./runlim -k -r $TLIM -s $SLIM python solver.py "bin/$BIN $FILE /dev/null $PARAMS $TMPFILE.out $TMPFILE.err" 2> $TMPFILE.runlim2;
# remove all sample lines from the runlim tool
cat $TMPFILE.runlim2 | grep -v "sample:" > $TMPFILE.runlim

after=`date +%s`
let dif=$after-$before

pid=`cat $TMPFILE.runlim | awk '/main pid/ {print $4}'`
#echo "kill pid $pid"
#kill -9 -P $pid

sleep 2
echo "run for $dif seconds"

#cat $TMPFILE.runlim >> $TMPFILE.err
echo "wallclock seconds: $dif" >> "$TMPFILE.err"


sleep 1 
