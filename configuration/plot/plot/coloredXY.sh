
#
#
# plot x y plot with third coloumn as color
# usage: ./coloredXY.sh dataFile outFile xLabel yLabel
#
#

echo "set terminal postscript eps color enhanced" > tmp.plt
echo "set view map" >> tmp.plt
echo "set nokey" >> tmp.plt
echo "set output \"$1.eps\"" >> tmp.plt
echo "set logscal x" >> tmp.plt
echo "set logscal y" >> tmp.plt
echo "set xlabel \"$3\"" >> tmp.plt

# set log scale to the colors!
echo "set log cb" >> tmp.plt

echo "set ylabel \"$4\"" >> tmp.plt

echo "set title \"frequency\"" >> tmp.plt

echo "splot '$2' u 1:2:(0.0):3 with points palette ps 2 pt 7" >> tmp.plt

# plot
cat tmp.plt | gnuplot

# convert to pdf
epstopdf $1.eps
