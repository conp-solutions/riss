#!/bin/bash
# cp3.sh, Norbert Manthey, 2013
#
# solve CNF formula $1 by simplifying first with coprocessor, then run a SAT solver, and finally, reconstruct the model
#

#
# usage
#
if [ "x$1" = "x" ]; then
  echo "USAGE: cp.sh <input CNF> [arguments for cp3]"
  exit 1
fi

#
# variables for the script
#

file=$1											# first argument is CNF instance to be solved
shift												# reduce the parameters, removed the very first one. remaining $@ parameters are arguments

# binary of the used SAT solver
satsolver=glucose_static						# name of the binary (if not in this directory, give relative path as well)

# default parameters for preprocessor
cpParams="-enabled_cp3 -cp3_stats -enabled_cp3 -cp3_stats -up -subsimp -bve -no-bve_gates -no-bve_strength -bve_red_lits=1 -cp3_bve_heap=1 -bve_heap_updates=1 -bve_totalG -bve_cgrow_t=1000 -bve_cgrow=10 "

# some temporary files 
undo=/tmp/undo_$$				# path to temporary file that stores cp3 undo information
tmpCNF=/tmp/tmpCNF_$$		# path to temporary file that stores cp3 simplified formula
model=/tmp/model_$$			# path to temporary file that model of the preprocessor (stdout)
realModel=/tmp/model_$$			# path to temporary file that model of the SAT solver (stdout)
echo "c undo: $undo tmpCNF: $tmpCNF model: $model realModel: $realModel"  1>&2

ppStart=0
ppEnd=0
solveStart=0
solveEnd=0

#
# run coprocessor with parameters added to this script
# and output to stdout of the preprocessor is redirected to stderr
#
ppStart=`date +%s`
#echo "call: ./coprocessor $file $realModel -enabled_cp3 -undo=$undo -dimacs=$tmpCNF $cpParams $@"
./coprocessor $file $realModel -enabled_cp3 -undo=$undo -dimacs=$tmpCNF $cpParams $@  1>&2
exitCode=$?
ppEnd=`date +%s`
echo "c preprocessed $(( $ppEnd - $ppStart)) seconds with exit code $exitCode" 1>&2
echo "c preprocessed $(( $ppEnd - $ppStart)) seconds with exit code $exitCode"

# solved by preprocessing
if [ "$exitCode" -eq "10" -o "$exitCode" -eq "20" ]
then 
	echo "c solved by preprocessor" 1>&2
else
	echo "c not solved by preprocessor -- do search" 1>&2
	if [ "$exitCode" -eq "0" ]
	then
		#
		# exit code == 0 -> could not solve the instance
		# dimacs file will be printed always
		# exit code could be 10 or 20, depending on whether coprocessor could solve the instance already
		#
	
		#
		# run your favorite solver (output is expected to look like in the SAT competition, s line and v line(s) )
		# and output to stdout of the sat solver is redirected to stderr
		#
		solveStart=`date +%s`
		./$satsolver $tmpCNF $model 1>&2
		exitCode=$?
		solveEnd=`date +%s`
		echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2
	
		#
		# undo the model
		# coprocessor can also handle "s UNSATISFIABLE"
		#
		echo "c post-process with cp3" 1>&2
		./coprocessor -post -undo=$undo -model=$model $cpParams > $realModel
	
		#
		# verify final output if SAT?
		#
		if [ "$exitCode" -eq "10" ]
		then
			echo "c verify model ..." 1>&2
#			./verify SAT $realModel $file
		fi
	else
		#
		# preprocessor returned some unwanted exit code
		#
		echo "c preprocessor has been unable to solve the instance" 1>&2
		#
		# run sat solver on initial instance
		# and output to stdout of the sat solver is redirected to stderr
		#
		solveStart=`date +%s`
		./$satsolver $file $realModel  1>&2
		exitCode=$?
		solveEnd=`date +%s`
		echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2
	fi
fi

#
# print times
#

echo "c pp-time: $(( $ppEnd - $ppStart)) solve-time: $(( $solveEnd - $solveStart ))" 1>&2

#
# print solution
#
cat $realModel;

#
# remove tmp files
#
rm -f $undo $undo.map $tmpCNF $model $realModel;

#
# return with correct exit code
#
exit $exitCode
