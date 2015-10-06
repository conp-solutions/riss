# checksmac.sh, Norbert Manthey, 2015
#
# run smac with a solver pcs file on some scenario
#
# if a faulty configuration has been found, the configuration is minimized, reported, and added as forbidden configuration to the configuration file
#
# usage: ./checksmac.sh <solver-pcs-file> <solver-dir-name> <number-of-fix-attempts>
#
# example call: ./checksmac.sh riss5 riss5.pcs 1
#
# known problems:
# - script will try to exclude continous parameters for a single value, which cannot be handled by SMAC any more.
#

# parameter
DIRNAME=$1                       # directory of the solver
PARAMFILE=solvers/$DIRNAME/$2    # configuration file of the solver that should be used
FIXATTEMPTS=$3                   # number of tries to fix the solver, if it its configuration is still faulty

# make a backup copy of the original configuration
cp $PARAMFILE $PARAMFILE.old.$$.pcs

# output
SMACOUTPUT=/tmp/smacoutput_$$
SMACERROR=/tmp/smacerror_$$
REDUCEOUTPUT=/tmp/reduceoutput_$$

# SMAC run parameters (might be specified from the command line?)
#SMAC=./configurators/smac3/smac  # more recent version, output does not fit to older version - TODO: adapt greps and awks
SMAC=./configurators/smac-v2.04.01-master-415/smac
SCENARIOFILE=./scenarios/regressiontests-5s-1h.txt
WALLCLOCKLIMIT=4000
SMACMODE=ROAR   # ROAD or SMAC

# memorize directory
PWD=`pwd`

# start actual working loop
echo "run configuration fixing in dir $DIRNAME with PCS file $PARAMFILE and $FIXATTEMPTS fix iterations"
echo "SMAC run parameters: scenario: $SCENARIOFILE wall clock: $WALLCLOCKLIMIT mode: $SMACMODE"
for (( i=1; i <= $FIXATTEMPTS; i++ ))
do
  echo "fix iteration $i"


	# go to base directory (again)
	cd $PWD
	# call smac (again)
	$SMAC --numRun 0 --wallClockLimit $WALLCLOCKLIMIT --doValidation false --abortOnCrash true --executionMode $SMACMODE --scenarioFile $SCENARIOFILE --algo "ruby ../scripts/generic_silent_solver_wrapper.rb $DIRNAME" --paramfile $PARAMFILE > $SMACOUTPUT  2> $SMACERROR
	smacexitcode=$?

	#echo "smac exited with $?"
	#echo "output:"
	#cat $SMACOUTPUT
	#echo "error:"
	#cat $SMACERROR

	# check for error
	#echo "grep for bug"
	grep "Target Algorithm Run Reported Crashed" $SMACERROR  # > /dev/null
	foundNoError=$?
	#echo "found error: $foundNoError"

	# we found an error in the file
	if [ "$foundNoError" -eq 0 ]
	then
		echo "found an error - run configuration reduction"

#		echo "output:"
#		cat $SMACOUTPUT
		
		#echo ""
		FAILTIME=`grep " Algorithm Reported: Result for ParamILS" $SMACOUTPUT | tail -n 1 | awk '{print $9}' | sed  's/\./,/' | awk -v f=$FAILFACTOR '{ print $1 ; }'`
		echo "FAILTIME: $FAILTIME"
#		grep "Failed Run Detected Call: " $SMACOUTPUT | tail -n 1
	
		FAULTYCALL=`grep "Failed Run Detected Call: " $SMACOUTPUT | tail -n 1 | awk -v ftime=$FAILTIME '{ for(i=10; i<=NF; i++) if( i != 15 ) printf("%s ", $i ); else printf("%lf ", $i * 2.0);}'`
#		echo "faulty call: $FAULTYCALL"
		FAULTYINSTANCE=`echo $FAULTYCALL | awk '{print $4}'`
		echo "faulty instance: `pwd`/$FAULTYINSTANCE"
		
#		exit 0
		
		# reduce faulty call parameters
		python shrinkFaultyParams.py $FAULTYCALL > $REDUCEOUTPUT # TODO convert into SMAC's pcs format to disable these options
		echo "reduced call: " 
		grep "^final faulty options: " $REDUCEOUTPUT
	
		# add faulty configuration to used solver specification
		echo "" >> $PARAMFILE
		echo "# found faulty configuration on file `pwd`/$FAULTYINSTANCE within timeout $FAILTIME (minimized with double)"  >> $PARAMFILE
		grep "^final faulty options: " $REDUCEOUTPUT | awk '{ for(i=4; i<=NF; i++) printf("%s ", $i ) }'                    >> $PARAMFILE
	
	else
	  # in the last iteration we did not find a problem, hence quit
	  echo "did not find a problem, exit fix loops"
	  break
	fi

done

# print number of iterations
echo "fix iterations: $i ( out of $FIXATTEMPTS )"

# remove intermediate files
ls $SMACOUTPUT $SMACERROR $REDUCEOUTPUT



