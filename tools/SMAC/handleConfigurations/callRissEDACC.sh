#!/bin/bash
# Norbert Manthey, 2014
#
# script to call Riss with EDACC parameter format
#
# usage: ./callRissEDACC.sh <input.cnf> <parameters>
#

# get input CNF
file=$1
shift

# convert parameters
params=`python mapEDACCparams.py $*`

echo "params: $params"

# call riss
./riss $params $file
# exit with the exit code of riss
exit $?
