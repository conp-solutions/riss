#!/bin/bash
# Norbert Manthey, 2015
# optiriss script, to run in selected incomplete configuration

#
# run optiriss in selected incomplete configuration
#
./optiriss -solverConfig=INCSOLVE -algorithm=0 -incremental=1  -incomplete $1

