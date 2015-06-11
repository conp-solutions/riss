#!/bin/bash
#
# This script builds all targets and runs the fuzzer on the solvers. 
# 

# exits the script immediately if a command exits with a non-zero status
set -e

# directory of this script
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

echo "Build all targets"
# rm -r build # TODO: remove this line
# mkdir build
# cd build
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# make all

echo "Make fuzzer"
cd $DIR/tools/checker
make

echo "Run fuzzer on Riss"
n=10
./fuzzcheck.sh $DIR/build/bin/riss $n  >/dev/null 2>/dev/null

echo "Run fuzzer on Pcasso"
./fuzzcheck.sh $DIR/build/bin/pcasso $n  >/dev/null 2>/dev/null

echo "Run fuzzer on Pfolio"
./fuzzcheck.sh $DIR/build/bin/pfolio $n  >/dev/null 2>/dev/null
