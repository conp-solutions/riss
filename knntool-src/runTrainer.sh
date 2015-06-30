#!/bin/bash
set -e
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./trainer -g -t 7200 times.dat features.csv dataset
