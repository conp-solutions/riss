#!/bin/bash

#
# merges two csv files (space separated) into a single one, based on the first column, adds only lines where a join partner is found
#
# usage: ./merge2csv.sh a.csv b.csv c.csv
#
# merge a.csv and b.csv into c.csv, where the columns of a.csv are first
#


f=$1
g=$2
h=$3

cat $f | while read line
do

head=`echo $line | awk '{print $1}'`
tail=`cat $g | grep "$head"`

if [ "$tail" == "" ]
then 
	echo "nothing found"
	continue
fi

newtail=` echo "$tail" | awk '{ for(i=2; i<=NF; i++) {printf("%s ", $i)} } '`

echo "$line $newtail" >> $h

#echo $line
done
