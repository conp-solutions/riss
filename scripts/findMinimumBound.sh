#!/bin/bash
# findMinimumBound.sh, Norbert Manthey, 2014
#
# take an AIG circuit and find the least bound so that the circuit is not solved by unit propagation any more
#
# Usage: findMinimumBound.sh <input.aig> MaxBound 
# 
# requires the binary shiftbmc-abc and riss to be in the same (calling) directory
#


input=$1
bound=$2
shift;shift;

#
# check input
#
if [ "x$input" = "x" ]; then
  echo "USAGE: findMinimumBound.sh <input.aig> MaxBound " 1>&2
  exit 1
fi

if [ "x$bound" = "x" ]; then
  echo "c set defaultbound to 10" 1>&2
  bound=10
fi

#
# ALGORITHM
#

tmpOut=/tmp/minBound_$$

solutionBound=$bound
# loop over the bounds
for (( i=1; i<=$bound; i++ ))
do
	echo "check bound $i" 1>&2
	# generate formula
	./shiftBMC-dumpCNF.sh $input $i $tmpOut > /dev/null 2> /dev/null
	# check whether the formula can be solved by unit propagation
	./riss -cpu-lim=180 $tmpOut -quiet -maxConflicts=0 > /dev/null 2> /dev/null
	exitCode=$?
	
	if [ "$exitCode" -ne "20" ]; then
		solutionBound=$i;
		break;
	fi
done

#
# print solution
#
echo "s minBound: $solutionBound"

#
# cleanup
#
rm -f $tmpOut
exit 0
