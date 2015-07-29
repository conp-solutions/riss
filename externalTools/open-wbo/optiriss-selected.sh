#!/bin/bash
# Norbert Manthey, 2015
# optiriss script, to run in selected configuration

#
# run optiriss in selected configuration
#
./optiriss -solverConfig=INCSOLVE -algorithm=0 -incremental=1 $1

