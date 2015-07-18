#!/bin/bash
#
# This script builds all targets and runs the fuzzer on the solvers. 
#
# It is intended to be used as a script for a CI-runner which will provide a
# clear git repository where this script will be executed.

# exits the script immediately if a command exits with a non-zero status
set -e

# solvers for fuzzchecking
solver=('riss-simp' 'riss-core -config=Riss427:BMC_FULL' 'riss-core -config=CSSC2014' 'pfolio' 'pcasso -model -threads=2')
params="-mem-lim=2048"

# directory of this script (in repo/scripts)
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
# get absolute path for root directory of the repository
repo_dir="$(dirname "$script_dir")"

echo "Build all targets"
cd $repo_dir

if [ ! -d "build" ]; then
    mkdir build
fi

# test debug and release version
build_types=('Debug' 'Release')

# store exit code for fuzzchecker-runs to provide that debug and release are both build once
exitcode=0
# write error msg for fuzz-runs that will be displayed at the bottom of the log
# but for building errors (there the script exits immediately)
error_msg=""

for build_type in "${build_types[@]}"
do
    cd $repo_dir/build
    cmake -DCMAKE_BUILD_TYPE=$build_type ..
    make all

    # make fuzzerchecker (does nothing in second run)
    cd $repo_dir/tools/checker
    make

    echo "Start fuzzchecking"
    n_runs=150

    # check each solver
    for s in "${solver[@]}"
    do
        echo "Run fuzzer with $n_runs cnfs on $s"

        # write wrapper script to call solver with parameters
        echo "$repo_dir/build/bin/$s $params \$1" > solver-wrapper.sh
        echo "exit \$?" >> solver-wrapper.sh
        chmod +x solver-wrapper.sh
        
        # run fuzzcheck as background process and wait for it
        ./fuzzcheck.sh $repo_dir/tools/checker/solver-wrapper.sh $n_runs > /dev/null 2> /dev/null &
        wait $!

        # count lines of log for this run (in this file the name of the bugs-files can be found)
        # the log file has the pid of the fuzzcheck-process
        bugs=`cat runcnfuzz-$!.log | wc -l`

        if ! [ $bugs -eq 1 ]
            then
                if [ "$error_msg" == "" ]; then
                    error_msg="fuzzer found bug for: $s ($build_type)"
                else
                    error_msg="$error_msg, $s ($build_type)"
                fi
            exitcode=1
        fi
    done

    echo $error_msg
done

exit $exitcode
