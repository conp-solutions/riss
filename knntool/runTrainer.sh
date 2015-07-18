#!/bin/bash
set -e
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./trainer -g -t 1006500 work300.src workf300.src dataset --igr
