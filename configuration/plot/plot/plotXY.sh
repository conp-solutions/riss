#!/bin/bash

# plot a csv file into outfile.eps and outfile.pdf
# orders all lines accordinng to the value of the specified column
# plots all specified columns
#
# usage: ./plotSortCol.sh file outfile printXCol printYCol

unset LOCALE
unset LANG

file=${1}
outfile=$2

sep="$"

#echo "parameters:"

paramCount=0
for i
do
#  echo $i
  paramCount=$((paramCount+1))
done
 
echo "Parameters: $paramCount"

#extract head

head=$(head -n 1 $file)

# sort without the headline!

n=`wc -l $file|cut -d' ' -f1`
let n=$n-1

# create plt file

echo "set terminal postscript eps color enhanced" > tmp.plt
#echo "set terminal postscript eps" > tmp.plt
echo "set key left top" >> tmp.plt
echo "set output \"$outfile.eps\"" >> tmp.plt

col=$3
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
echo "set xlabel \"$cHead-1\"" >> tmp.plt

col=$4
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
echo "set ylabel \"$cHead-2\"" >> tmp.plt

#echo "set xrange [90:]" >> tmp.plt
#echo "set logscale y" >> tmp.plt
#echo "set xtic 2" >> tmp.plt
echo "set title \"\"" >> tmp.plt

echo "f(x)=x" >> tmp.plt

# setup very first plot line
col=$3
#echo "col = $col"
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
line=`echo "plot \"$file\" using $3:$4 title \"instance\" w p, f(x)"`

# add last line also to plot file
echo $line >> tmp.plt
#echo "cHead= $cHead"
#echo "line=  $line"

# plot
cat tmp.plt | gnuplot

# convert to pdf
epstopdf $outfile.eps

