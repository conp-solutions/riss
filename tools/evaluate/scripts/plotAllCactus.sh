#!/bin/bash

#
# plot a cactus plot with the into file $1 all csv files in the directory with col $2
#
# usage ./plotCactusFiles.sh out col
# result: cactus plot of the specified colum in out.eps and out.pdf
#
#


outname=$1
col=$2

str=""

for f in *.csv
do
	str="$str $f $col"
done

echo "final plot string: $str"

./plotCactusFiles.sh $outname $str
