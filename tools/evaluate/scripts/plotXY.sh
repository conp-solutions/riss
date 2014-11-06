#!/bin/bash
#
# plot XY plto from data in $2 into $1.eps and $1.pdf
# 
# uses the data from the two specified columns
#
# usage: ./plotSortCol.sh outfile file printXCol printYCol


unset LOCALE
unset LANG

file=$2
outfile=$1

sep="$"

echo "plot col $3 vs $4 from file $2 into $1.eps and $1.pdf"
head -n 7 $2

#extract head
head=$(head -n 1 $file)

#
# create plt file
#
echo "set terminal postscript eps color enhanced" > tmp.plt
#echo "set terminal postscript eps" > tmp.plt
echo "set key left top" >> tmp.plt
echo "set output \"$outfile.eps\"" >> tmp.plt

col=$3
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
echo "set xlabel \"$cHead\"" >> tmp.plt

col=$4
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
echo "set ylabel \"$cHead\"" >> tmp.plt

#echo "set xrange [1:]" >> tmp.plt
#echo "set yrange [1:]" >> tmp.plt
#echo "set xtic 2" >> tmp.plt
echo "set size 0.7,0.7" >> tmp.plt
echo "set logscale x" >> tmp.plt
echo "set logscale y" >> tmp.plt
echo "set title \"\"" >> tmp.plt

echo "f(x)=x" >> tmp.plt

# setup very first plot line
col=$3
#echo "col = $col"
cHead=`echo "$head" | awk -v x=$col '{print $x}'`
line=`echo "plot \"$file\" using $3:$4 title \"instance\" w p, f(x) notitle"`

# add last line also to plot file
echo $line >> tmp.plt

# plot
cat tmp.plt | gnuplot

# convert to pdf
epstopdf $outfile.eps

