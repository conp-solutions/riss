#!/bin/bash
set -e
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./trainer -o database -f features.csv -t times.dat
