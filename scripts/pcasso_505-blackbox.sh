#!/bin/bash
# pcasso.sh, Norbert Manthey, 2015
#
# solve CNF formula $1 with pcasso and $2 threads 
#
# Assumption: solver binary is located in this directory (change variable otherwise!)
#
binary=./pcasso

# resource independent parameters
params="-auto -checkModel -use-priss -priss-config=epsilon -crosslink -pcasso-com-config=small -load-split -model -work-conflicts=-1 -work-timeout=-1 -split-mode=2 -split-timeout=1024 -presel-fac=0.1 -presel-min=64-presel-max=1024 -fail-lit=2 -nec-assign=2 -num-iterat=3 -con-resolv=1 -bin-const -la-heur=4 -presel-heur=2-clause-learn=2 -dir-prior=3 -child-count=7 -shrk-clause -var-eq=3-split-method=1 -split-depth=0 -dseq -h-acc=3 -h-maxcl=7 -h-cl-wg=5-h-upper=10900 -h-lower=0.1 -shpool-size=15000-shclause-size=2 -stop-children -adp-preselS=7 -sort-split -verbosity=0 -verb=0 -g-priss-config=best4"


# resource dependent parameters
if [ "$2" -eq "8" ]  # 8 cores?
then
 # 8 = 2 + (3 * 2)
 # 2 in global priss, 3 workers with 2 for each worker
 params="-g-priss-threads=2 -threads=2 -priss-threads=2 $params"
elif [ "$2" -eq "64" ]  # use 32 cores, ignore hyperthreads (SAT Race specifics)
then
 # 32 = 6 + (13 * 2)
 # 6 in global priss, 13 workers with 2 for each worker
 params="-g-priss-threads=6 -threads=13 -priss-threads=2 $params"
else
 echo "c ERROR: no configuration for the given number of resources found ($2 computing resources)"
 exit 3
fi

# call the binary, solve the formula with the given parameters, the exit code will be forwarded
$binary $1 $params
