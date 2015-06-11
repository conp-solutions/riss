#!/bin/bash
#
# This script builds all targets and runs the fuzzer on the solvers. 
# 

# exits the script immediately if a command exits with a non-zero status
set -e

# get absolute path for root directory of this repository.
# directory of this script (in repo/scripts)
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
repo="$(dirname "$script_dir")"

echo "Build all targets"
cd $repo
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make all

echo "Make fuzzer"
cd $repo/tools/checker
make

n=10
echo "Run fuzzer on Riss"
./fuzzcheck.sh $repo/build/bin/riss $n  >/dev/null 2>/dev/null

echo "Run fuzzer on Pcasso"
./fuzzcheck.sh $repo/build/bin/pcasso $n  >/dev/null 2>/dev/null

echo "Run fuzzer on Pfolio"
./fuzzcheck.sh $repo/build/bin/pfolio $n  >/dev/null 2>/dev/null
