#!/bin/bash

#
# plot a cactus plot with the into file $1 from file $2 col $3; file $3 col $4; ...
#
# usage ./plotCactusFiles.sh out file1.csv col1 file2.csv col2 file3.csv col4
# result: cactus plot of the specified colums in out.eps and out.pdf
#
#

outfile=${1}
sep="$"
exitCol="3"
# split file into its rows


# plot all the lines from the parameters
n=0
filename=""
for i
do
	n=$[$n+1]
	if [ "$n" -lt "2" ] 
	then 
		echo "n=$n"
		continue;
	fi

	offset=$( echo "$n % 2" | bc )

	# only use file-col
	if [ "$offset" -eq "0" ] 
	then 
		filename=$i
		continue;
	fi

	echo "n=$n; file=$filename/data.csv col= $i"
	cutcol=$i

	np=$[$n+1]
	col=$( echo "$np / 2" | bc )

	echo "process col $col"

	m=`wc -l $filename/data.csv | cut -d' ' -f1`
	let m=$m-1
	echo "lines: $m"
	tail -n $m $filename/data.csv > tmp.dat

  head -n 1 $filename/data.csv | awk "{ print \"$filename\" }" > tmpHead$col.dat
	cat tmp.dat | awk "{ if( $sep$cutcol + 0 > 0 && $sep$cutcol < 7200 && ($sep$exitCol == 10 || $sep$exitCol == 20) ) print $sep$cutcol }" | sort -g > tmp$col.dat
done

# do actual plot

echo "used overall cols:$col"
cols=$[$col-1]
# create plt file

echo "set terminal postscript eps color enhanced" > tmp.plt
#echo "set terminal postscript eps" > tmp.plt
echo "set key left top" >> tmp.plt
echo "set output \"$outfile.eps\"" >> tmp.plt
echo "set xlabel \"solved instances\"" >> tmp.plt
echo "set ylabel \"time in seconds\"" >> tmp.plt
#echo "set xrange [70:]" >> tmp.plt
#echo "set logscale x" >> tmp.plt
#echo "set logscale y" >> tmp.plt
#echo "set xtic 2" >> tmp.plt
echo "set size 0.7,0.7" >> tmp.plt
echo "set style line 1 lt rgb \"#FF0000\"    lw 3.5 pt 1" >> tmp.plt
echo "set style line 2 lt rgb \"#000000\"  lw 3.5     " >> tmp.plt
echo "set style line 3 lt rgb \"#0000FF\"   lw 3.5 pt 2" >> tmp.plt
echo "set style line 4 lt rgb \"#999999\"   lw 4 pt 1" >> tmp.plt
echo "set style line 5 lt rgb \"#000000\"  lw 3.5 pt 4" >> tmp.plt
echo "set style line 6 lt rgb \"#006400\" lw 2.5 pt 6" >> tmp.plt
echo "set style line 7 lt rgb \"#FFA500\" lw 2.5 pt 7" >> tmp.plt
echo "set style line 8 lt rgb \"#0000FF\"   lw 2.5 pt 8" >> tmp.plt
echo "set style line 9 lt rgb \"#999999\"   lw 3 pt 9" >> tmp.plt
echo "set style line 10 lt rgb \"#000000\" lw 2.5 pt 0" >> tmp.plt
echo "set style line 11 lt rgb \"#FF0000\"   lw 2" >> tmp.plt
echo "set style line 12 lt rgb \"#8A2BE2\" lw 2 pt 11" >> tmp.plt
echo "set style line 13 lt rgb \"#0000FF\"  lw 2 pt 12" >> tmp.plt
echo "set style line 14 lt rgb \"#999999\"  lw 2.5 pt 13" >> tmp.plt
echo "set style line 15 lt rgb \"#000000\" lw 2 pt 8" >> tmp.plt
echo "set style line 16 lt rgb \"#006400\" lw 1 pt 8" >> tmp.plt
echo "set style line 17 lt rgb \"#FFA500\" lw 1 pt 9" >> tmp.plt
echo "set style line 18 lt rgb \"#0000FF\"  lw 1 pt 9" >> tmp.plt
echo "set style line 19 lt rgb \"#999999\"  lw 2" >> tmp.plt
echo "set style line 20 lt rgb \"#000000\" lw 1" >> tmp.plt
#echo "set size ratio 1.5" >> tmp.plt
echo "set title \"\"" >> tmp.plt
#echo "f(x)=900" >> tmp.plt
#echo "plot f(x) title \"SAT Race\" with lines linestyle 2, \\"  >> tmp.plt
#echo "\"Beleg.csv\" using 1 title \"Baseline\" with linespoints linestyle 1, \\" >> tmp.plt

echo "set title \"\"" >> tmp.plt
echo "plot \\"  >> tmp.plt

for col in `seq 2 $cols`
do
	des=$(($col-1))
	lines=$(($col+1))
	cHead=`cat tmpHead$col.dat`
	echo "current head: $cHead"
	echo "    \"tmp$col.dat\" using 1 title \"$cHead-$col\" with linespoints linestyle $lines, \\" >> tmp.plt
done

des=$(($cols+1))
lines=$(($cols+3))


cHead=`cat tmpHead$des.dat`
echo "current head: $cHead"
echo "    \"tmp$des.dat\" using 1 title \"$cHead-$des\" with linespoints linestyle $lines" >> tmp.plt


# plot
cat tmp.plt | gnuplot

# convert to pdf
epstopdf $outfile.eps

# clean up
#for col in `seq 2 $(($cols+1))`
#do
#	rm -f tmp$col.dat
#done
# rm -f tmp.plt
