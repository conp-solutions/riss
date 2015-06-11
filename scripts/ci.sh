#!/bin/bash
#
# This script builds all targets and runs the fuzzer on the solvers. 
#
# It is intended to be used as a script for a CI-runner which will provide a
# clear git repository where this script will be executed.

# exits the script immediately if a command exits with a non-zero status
set -e

# solvers for fuzzchecking
solver=('riss' 'pcasso' 'pfolio')

# directory of this script (in repo/scripts)
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
# get absolute path for root directory of the repository
repo_dir="$(dirname "$script_dir")"

echo "Build all targets"
cd $repo_dir
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make all

echo "Make fuzzer"
cd $repo_dir/tools/checker
make

echo "Start fuzzchecking"
n_runs=100

# check each solver
for s in "${solver[@]}"
do
    echo "Run fuzzer with $n_runs cnfs on $s"
    
    # run fuzzcheck as background process and wait for it
    ./fuzzcheck.sh $repo_dir/build/bin/$s $n_runs > /dev/null 2> /dev/null &
    wait $!

    # count lines of log for this run (in this file the seed of the bugs can be found)
    bugs=`cat runcnfuzz-$!.log | wc -l`

    if ! [ $bugs -eq 1 ]
        then
            echo "fuzzer found bugs for $s"
            exit 1
    fi
done
