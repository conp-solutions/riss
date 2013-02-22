#!/bin/bash

# do a colored XY plot for two files with a certain bin size (if not specified, no bins are used)
#
# ./plotXYfiles.sh out param-1.csv col1 param-2.csv col2 bins
#

echo "do colored XY plot with bins: $6"

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

# bin the data
python binData.py $file $col1 $offsetCol $6 > /tmp/plot_$$.dat

# plot the data
./coloredXY.sh $1 /tmp/plot_$$.dat "Configuration 1" "Configuration 2" "Frequency"




