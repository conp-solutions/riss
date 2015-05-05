#!/bin/bash  

TLIM=$1
MLIM=$2  # in M !!!! All others are in K 

BIN=`realpath $3`

num_params=$(($#-4))
PARAMS=${@:4:$num_params}
FILE=`realpath "${@: -1}"`

MD5SUM=`echo "$FILE" "$PARAMS" | md5sum | tr [:blank:] _`

ARAM=`free -k | awk '{ if( NR == 2 ) print $4 }'`
AMEM=`df -l /tmp | awk '{ if( NR == 2 ) print $4 }'`
FSIZE=`du -schk $FILE | awk '{ if( NR == 2 ) print $1 }'`
BSIZE=`du -schk $BIN | awk '{ if( NR == 2 ) print $1 }'`

NMEM=$(($FSIZE + $BSIZE))

DEST=`mktemp -d`

if [ ! -d "tmp" ]; then
  mkdir tmp
fi

exec > >(tee tmp/$MD5SUM.log)

echo "------------------------------------------------------------------------------------------------------"
echo "  Working directory       | $DEST"
echo "  Used binary             | $BIN"
echo "  Used Formula            | $FILE"
echo "------------------------------------------------------------------------------------------------------"
echo "  Available memory (RAM)  | $(($ARAM / 1024))M"
echo "  Memory limit            | $(($MLIM))M"
echo "------------------------------------------------------------------------------------------------------"
echo "  Available memory (/tmp) | $(($AMEM / 1024))M"
echo "  Binary size             | $(($BSIZE / 1024))M"
echo "  Filesize                | $(($FSIZE / 1024))M"
echo "  Approx. needed memory   | $(($NMEM / 1024))M"
echo "------------------------------------------------------------------------------------------------------"

# check if the binary is statically linked

file $BIN | grep 'statically linked'

if [ "$?" = "1" ]
then
  file $BIN
  echo "ERROR: $BIN is not a statically linked binary (check with 'file <binary>')."
  exit 1
fi

echo "  Used parameters         | $PARAMS"
echo "  Time limit              | $(($TLIM))s"

DIF=$(($AMEM - $NMEM))

if [ "$DIF" -le 0 ] # maybe better if the limit is > 0
then
  file $BIN
  echo "ERROR: There is not enough memory (/tmp) available. $(($AMEM))K free, $(($NMEM))K needed"
  rm -R $DEST
  exit 1
fi

if [ "$(($ARAM - ($MLIM * 1024)))" -le 0 ] # maybe better if the limit is > 0
then
  file $BIN
  echo "ERROR: There is not enough memory (RAM) available. $(($ARAM))K free, $(($MLIM * 1024))K needed"
  rm -R $DEST
  exit 1
fi

cp $BIN $DEST
cp $FILE $DEST

mkdir $DEST'/tmp'

TMPFILE=$DEST'/tmp/'$MD5SUM.tmp.dat

# switch to the copied binary
BIN=$DEST'/'$(basename "$BIN")
FILE=$DEST'/'$(basename "$FILE")

PERF="perf stat -o $TMPFILE.perf -B -e cache-references,cache-misses,cycles,stalled-cycles-frontend,stalled-cycles-backend,instructions,branches,branch-misses"

#perf stat -B ./runsolver -M $MLIM -w $TMPFILE.watch -o $TMPFILE.out  $BIN $FILE $PARAMS 
$PERF ./runsolver -C $TLIM -M $MLIM -w $TMPFILE.watch -o $TMPFILE.out  $BIN $FILE $PARAMS;
#$PERF ./runlim -k -r $TLIM -s $MLIM -1 $TMPFILE.out -2 $TMPFILE.err $BIN $FILE /dev/null $PARAMS 2> $TMPFILE.runlim2;

rm $BIN
rm $FILE

cp -R $DEST/* . # maybe remove the tmp folder
rm -R $DEST

DEST=`pwd`'/'$(basename "$DEST")
echo "------------------------------------------------------------------------------------------------------"
echo "  Output written to       | `pwd`/tmp/$MD5SUM*"
echo "------------------------------------------------------------------------------------------------------"
