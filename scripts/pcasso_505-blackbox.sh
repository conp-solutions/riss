#!/bin/bash
# pcasso_505-blackbox.sh, Norbert Manthey, 2015
#
# solve CNF formula $1 with pcasso and $2 threads 
#
# Assumption: solver binary is located in this directory (change variable otherwise!)
#
binary=./pcasso_505/build/bin/pcasso

# resource independent parameters
params="-auto -checkModel -no-use-priss -riss-config=INCSIMP -crosslink -pcasso-com-config=usual -load-split -model -work-conflicts=-1 -work-timeout=-1 -split-mode=2 -split-timeout=1024 -presel-fac=0.1 -presel-min=64-presel-max=1024 -fail-lit=2 -nec-assign=2 -num-iterat=3 -con-resolv=1 -bin-const -la-heur=4 -presel-heur=2-clause-learn=2 -dir-prior=3 -child-count=7 -shrk-clause -var-eq=3-split-method=1 -split-depth=0 -dseq -h-acc=3 -h-maxcl=7 -h-cl-wg=5-h-upper=10900 -h-lower=0.1 -shpool-size=15000-shclause-size=2 -stop-children -adp-preselS=7 -sort-split -verbosity=0 -verb=0 -g-priss-config=best4 $*"

# input
file=$1
worker=$2

shift; 
shift;


# resource dependent parameters
if [ "$worker" -eq "8" ]  # 8 cores?
then
 # 8 = 3 + 5
 # 3 in global priss, 5 workers with 1 for each worker
 params="-threads=5 -g-priss-threads=3 $params"
elif [ "$worker" -eq "64" ]  # use 32 cores, ignore hyperthreads (SAT Race specifics)
then
 # 32 = 8 + 24
 # 8 in global priss, 24 workers with 1 for each worker
 params="-threads=24 -g-priss-threads=8 $params"
else
 echo "c ERROR: no configuration for the given number of resources found ($worker computing resources)"
 exit 3
fi

# call the binary, solve the formula with the given parameters, the exit code will be forwarded
$binary $file $params
