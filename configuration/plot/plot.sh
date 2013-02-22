#!/bin/bash

#
#
# This script plots data about experiments, based on the parameter
# If only one experiment is specified, data for this experiment is plotted
# If you specifiy two parameters, the two experiments are compared to each other
#
#

#get number of experiments to compare
first="1"
last=`cat overview.dat | awk '/lastExperiment/ { print $2 }'`

if [ ! $1 ]; then
	echo "no run specified, abort."
	exit 1
fi

echo "run with pid $$"

mkdir -p pdf

if [ $1 ]; then
	first=$1
	
	if [ ! $2 ]; then
		echo "evaluate run $first"
		file1=../$first/data.csv
		# plot data about a single run, specified by the first parameter
		cd plot;

			# cactus-plot(real time)
			./plotCactusFiles.sh ../pdf/solved-$first $file1 4
			rm -f ../pdf/solved-$first.eps

			# cactus-plot(real time)
			./plotCactusFiles.sh ../pdf/memory-$first $file1 6
			rm -f ../pdf/memory-$first.eps

			# x-y real time vs cpu time
			./plotXYfiles.sh ../pdf/real-cpu-XY-$first $file1 4 $file1 5
			rm -f ../pdf/real-cpu-XY-$first.eps

			# x-y real time vs level of evaluated result
			./plotXYfiles.sh ../pdf/time-eva-XY-$first $file1 4 $file1 12
			rm -f ../pdf/time-eva-$first.eps

			echo "colored x-y time, eva level, unsolved nodes"
			# x-y created nodes, unsolved nodes
			awk '{print $4,$12,$8}' $file1 > /tmp/plot_$$.dat
			./coloredXY.sh ../pdf/time-evaXYcolor-$first /tmp/plot_$$.dat "time" "solutionLevel"
			rm -f ../pdf/time-evaXYcolor-$first.eps
			
			echo "colored x-y tree height"
			# x-y tree height, solved nodes
			python binData.py $file1 8 11 > /tmp/plot_$$.dat
			./coloredXY.sh ../pdf/treeHeight-Unsolved-XY-$first /tmp/plot_$$.dat "Unsolved" "Height"
			rm -f ../pdf/treeHeight-Unsolved-XY-$first

			echo "colored x-y tree number of nodes"
			# x-y created nodes, unsolved nodes
			python binData.py $file1 7 8 > /tmp/plot_$$.dat
			./coloredXY.sh ../pdf/unsolvedNodesXY-$first /tmp/plot_$$.dat "Created" "Unsolved"
			rm -f ../pdf/unsolvedNodesXY-$first.eps
			
	
		cd ..
		exit
	fi
	
fi

if [ $2 ]; then
	last=$2
fi

echo "compare run $first and run $last"

file1=../$first/data.csv
file2=../$last/data.csv


#plot data
cd plot;

	# x-y time comparison of the two files and column 4 (real time)
	./plotXYfiles.sh ../pdf/timeXY-$first-vs-$last $file1 4 $file2 4
	rm -f ../pdf/timeXY-$first-vs-$last.eps

	# cactus-plot of the two files and column 4 (real time)
	./plotCactusFiles.sh ../pdf/solved-$first-vs-$last $file1 4 $file2 4
	rm -f ../pdf/solved-$first-vs-$last.eps
	
	./plotXYfilesColor.sh ../pdf/solved-packed-$first-vs-$last $file1 4 $file2 4 50
	rm -f ../pdf/solved-packed-$first-vs-$last.eps

# done
cd ..;
