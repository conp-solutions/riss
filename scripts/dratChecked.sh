#!/bin/bash
#SBATCH --time=2:00:00
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --exclusive
#SBATCH --ntasks-per-node=1
#SBATCH --partition=sandy
#SBATCH --mem-per-cpu=1900M
#SBATCH -A p_sat
#SBATCH --cpus-per-task 4
#SBATCH --mail-type=end
#SBATCH --mail-user=norbert.manthey@tu-dresden.de

#
# SLURM setup: 8 cores of 1 node, access to the whole node (16 cores), Process will have the whole memory of the node
#

# Norbert Manthey, 2014
#
# This script runs a Riss-based SAT solver and checks the output of the solver (DRAT proof)
# CPU time, memory limits, and file size limits can be specified
# The solution will not be printed, but the result whether the solution is correct will be printed
#
# Usage: ./dratChecked.sh <input.cnf>
#
# This script requires the binary drat-trim, verify and runlim to be in the directory of the caller


#
# used solver
#
satSolver=./priss
solverParameters="-threads=4 -ps -no-pc -pv=0 -config=RESTART"

#
# resource limits
#
solveTime=900  # 30 minutes
verifyTime=3600 # 60 minutes
memoryLimit=18874368 # 18 GB
maxFileSize=16777216 # 16 GB  # 20971520 20 GB  # in kilo bytes


#
#
# ACTUAL ALGORITHM
#
#

file=$1
if [ "x$1" = "x" ]; then
  echo "USAGE: ./dratChecked.sh <input.cnf>"
  exit 1
fi

#
# tmp files
#
bn=`basename $file`
solverOutput=/tmp/dratChecked_solverOutput_$$
solveRunlim=tmp/$bn-solver
verifyRunlim=tmp/$bn-verify
err=tmp/$bn.err
out=tmp/$bn.out

#
# apply memory limit
# apply file size limit
#
ulimit -f $maxFileSize
ulimit -m $memoryLimit

#
# show resources of the system
#
rm -f $out $err # begin log from beginning (overwrite old logs)
echo "c solvedFile: $file" >> $out
echo "c date: `date`" >> $out
echo "c usedSolver: $satSolver" >> $out
echo "c usedParameter: $solverParameters" >> $out
echo "c solveTimeLimit: $solveTime" >> $out
echo "c verifyTimeLimit: $verifyTime" >> $out
echo "c memoryLimit: $memoryLimit" >> $out
echo "c maxFileSize: $maxFileSize" >> $out
echo "c taskset: `taskset -p $$`" >> $out
echo "c host: `hostname`" >> $out
echo "c" >> $out ; echo "c" >> $err

echo "c check resources" >> $out
df -h | while read line
do
	echo "c DF-OUTPUT: $line" >> $out
done
echo "c" >> $out ; echo "c" >> $err

#
# solve the file and log the output
#
echo "c execute ./runlim -o $solveRunlim.runlim2 -k -r $solveTime -s $memoryLimit  $satSolver $file $solverParameters -drup=$solverOutput -proofFormat=\"\"" >> $out
solverStart=`date +%s`
./runlim -o $solveRunlim.runlim2 -k -r $solveTime -s $memoryLimit  $satSolver $file $solverParameters -drup=$solverOutput -proofFormat="" 2>> $err >> $out
solverExitCode=$?
solverEnd=`date +%s`
echo "c" >> $out ; echo "c" >> $err
#
# print solving stats
#
echo "c SOLVER-STATS exitcode: $solverExitCode"  >> $out
echo "c SOLVER-STATS walltime: $(( $solverEnd - $solverStart))"  >> $out
echo "c SOLVER-STATS outputSize: `ls -lh $solverOutput`"  >> $out
echo "c SOLVER-STATS outputLines:  `cat $solverOutput | wc -l`"  >> $out
echo "c SOLVER-STATS outputDLines: `grep ^d $solverOutput | wc -l`"  >> $out
echo "c" >> $out ; echo "c" >> $err
# cleanup to large file
grep -v "sample:" $solveRunlim.runlim2 > $solveRunlim.runlim
rm -f $solveRunlim.runlim2

echo "c" >> $out ; echo "c" >> $err
echo "c proof (partial)" >> $out
head -n 7 $solverOutput | while read line
do
	echo "c proof: $line" >> $out
done
echo "c ... " >> $out
echo "c proof head" >> $out
tail -n 7 $solverOutput | while read line
do
	echo "c proof: $line" >> $out
done
echo "c" >> $out ; echo "c" >> $err

#
# verify the UNSAT output
#
if [ "$solverExitCode" -eq "20" ]
then
	verifierStart=`date +%s`
	./runlim -o $verifyRunlim.runlim2 -k -r $verifyTime -s $memoryLimit ./drat-trim $file $solverOutput 2>> $err >> $out
	verifierCode=$?
	verifierEnd=`date +%s`
	echo "c" >> $out ; echo "c" >> $err
	#
	# print verification stats
	#
	echo "c VERIFIER-STATS exitcode: $verifierCode"  >> $out
	echo "c VERIFIER-STATS walltime: $(( $verifierEnd - $verifierStart))"  >> $out
	echo "c" >> $out ; echo "c" >> $err
	# cleanup to large file
	grep -v "sample:" $verifyRunlim.runlim2 > $verifyRunlim.runlim
	rm -f $verifyRunlim.runlim2
fi


#
# CLEANUP unused and large files
#
rm -f $solverOutput

echo "c" >> $out ; echo "c" >> $err
echo "c" >> $out ; echo "c" >> $err
echo "c check cleanup: ls -lh $solverOutput: `ls -lh $solverOutput`" >> $out 2> $err

#
# tell the caller that everything terminated nicely
#
echo "c exit with 0" >> $out
echo "c exit with 0" >> $err
exit 0

