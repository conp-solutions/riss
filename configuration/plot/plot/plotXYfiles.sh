#!/bin/bash

# do a XY plot for two files
#
# ./plotXYfiles.sh out param-1.csv col1 param-2.csv col2
#


# merge
rm -f /tmp/compareSH_$$.csv
offset=`head -n 2 $2 | tail -n 1  | awk 'BEGIN {FS=" "} ; END{print NF}'`
./merge2csv.sh $2 $4 /tmp/compareSH_$$.csv
one=1
offset=$(($offset - $one))
echo "offset: $offset"

file="/tmp/compareSH_$$.csv"
timeout=$3

#calcuate real column values for merged file
col1=$3
col2=$5
offsetCol=$(($col2+$offset))

echo "plot in new file $file col $col1 and col $offsetCol"

# run simple XY plot
./plotXY.sh $file $1 $col1 $offsetCol


