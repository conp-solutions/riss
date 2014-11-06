#!/bin/bash

#
# plot an XY plot for all given CSV files with the 2 columns, adds the prefix to the resulting file
#
# usage ./plotCactusFiles.sh out-prefix col1 col2 files
# result: one XY plot with the two columns per given file
#
#


prefix=$1
colone=$2
coltwo=$3
shift
shift
shift

#
# iterate over all parameters
#
for f in $*
do
	plotname=`basename $f .csv`
	./plotXY.sh "$prefix$plotname" $f $colone $coltwo
done

