#!/bin/bash

# runs the solver with hooking malloc functions
# tries to display memory usage over time
# tries to analyze object life cycles over the runtime

# get current date and time
DATE="`date +%y%m%d%H%M%S`"

#ensure library is there
make

#remove old files
rm -f allocs.data free.dat
rm -f lifetime.dat lifetime_size.dat memory_time.dat
rm -f *.png

#copy solver here
cp ../../riss .

export LD_PRELOAD="./myMem.so"

# run solver with parameter
./riss ${1} ${2}

export LD_PRELOAD=""

# show allocs and deletes
echo "allocs: `wc -l allocs.dat`"
echo "frees:  `wc -l free.dat`"

# todo: write python scripts that evaluate allocs.dat and free.dat!
python evaluate_data.py 4000

# todo: do some ploting!
cat plot.plt | gnuplot

# todo compress all ascii files!
tar czvf data.tar.gz *.dat
rm -f *.dat

#create folder to store data ins
mkdir $DATE
mv *.eps $DATE
mv data.tar.gz $DATE
mv riss $DATE
