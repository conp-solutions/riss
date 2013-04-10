#!/bin/bash
# qp.sh, Norbert Manthey, 2013
#
# solve (Q)CNF formula $1 by simplifying first with qprocessor, then run a QBF solver
#

#
# usage
#
if [ "x$1" = "x" ]; then
  echo "USAGE: qp.sh <input CNF> [arguments for qbf solver]"
  exit 1
fi

#
# variables for the script
#

file=$1											# first argument is CNF instance to be solved
shift												# reduce the parameters, removed the very first one. remaining $@ parameters are arguments

# binary of the used qbf solver
qbfsolver=depqbf						# name of the binary (if not in this directory, give relative path as well)

# default parameters for preprocessor
cp3params="-enabled_cp3 -cp3_stats -up -subsimp -bva -no-cp3_strength -probe"

# some temporary files 
tmpCNF=/tmp/cp3_tmpCNF_$$		# path to temporary file that stores cp3 simplified formula
echo "c undo: $undo tmpCNF: $tmpCNF model: $model realModel: $realModel" 1>&2

ppStart=0
ppEnd=0
solveStart=0
solveEnd=0

#
# run coprocessor with parameters added to this script
# and output to stdout of the preprocessor is redirected to stderr
#
ppStart=`date +%s`
./qprocessor $file $realModel -enabled_cp3 -dimacs=$tmpCNF $cp3params  1>&2
exitCode=$?
ppEnd=`date +%s`
echo "c preprocessed $(( $ppEnd - $ppStart)) seconds with exit code $exitCode" 1>&2

# solve instance
if [ "$exitCode" -eq "0" ]
then
		#
		# exit code == 0 -> could not solve the instance
		# dimacs file will be printed always
		# exit code could be 10 or 20, depending on whether coprocessor could solve the instance already
		#
	
		#
		# run qbf solver and output to stdout
		#
		#
		solveStart=`date +%s`
		./$qbfsolver $tmpCNF  $@
		exitCode=$?
		solveEnd=`date +%s`
		echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2
	
else
		if [ "$exitCode" -eq "20" ]
		then
			echo "c preprocessor found UNSAT"  1>&2
		  echo "UNSAT"
		fi
		#
		# preprocessor returned some unwanted exit code
		#
		echo "c preprocessor has been unable to solve the instance"  1>&2
		#
		# run qbf solver on initial instance
		# and output to stdout of the qbf solver is redirected to stderr
		#
		solveStart=`date +%s`
		./$qbfsolver $file 1>&2
		exitCode=$?
		solveEnd=`date +%s`
		echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2
fi


#
# print times
#
echo "c pp-time: $(( $ppEnd - $ppStart)) solve-time: $(( $solveEnd - $solveStart ))" 1>&2


#
# remove tmp files
#
rm -f $undo $undo.map $tmpCNF $model $realModel  1>&2

#
# return with correct exit code
#
exit $exitCode
