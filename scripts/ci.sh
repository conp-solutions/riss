#!/bin/bash
#
# This script builds all targets and runs the fuzzer on the solvers. Therefore it
# creates the build folder for Debug-builds and the release folder for Release-builds.
#
# It is intended to be used as a script for a CI-runner but can be used for local runs too.

# exits the script immediately if a command exits with a non-zero status
set -e

# solvers with their specific parameters
solver=('riss-simp' 'riss-core -config=Riss427:BMC_FULL' 'riss-core -config=CSSC2014' 'pfolio' 'pcasso -model -threads=2')

# parameters that are the same for all solvers
params="-mem-lim=2048"

# directory of this script (in repo/scripts)
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
# get absolute path for root directory of the repository
repo_dir="$(dirname "$script_dir")"

echo "Build all targets"
cd $repo_dir

# make directories for debug and release builds
if [ ! -d "build" ]; then
    mkdir build
fi
if [ ! -d "release" ]; then
    mkdir release
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
    # different build folder for debug build and release build
    # splitting the two builds speeds up local building
    if [ "$build_type" == "Release" ]; then
        build_folder="$repo_dir/release"
    else
        build_folder="$repo_dir/build"
    fi

    cd $build_folder

    # make all
    echo "$build_type build"
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
        echo "$build_folder/bin/$s $params \$1" > solver-wrapper.sh
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

    # display error message for all fuzz-runs
    echo $error_msg

    # delete wrapper script
    rm solver-wrapper.sh
done

exit $exitcode
