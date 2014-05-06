#!/bin/bash
# Norbert Manthey, 2014
#
# Script to execute Riss with the option to generate a DRAT proof
# Requires the binary to be build with DRAT support
#
# usage: ./Riss427DRAT.sh <input.cnf> [TMPDIR] [DRAT]
#

# getParameters
file=$1
tmpDir=$2
doDRAT=$3

#
# handle DRAT case
#
drup=""
if [ "x$doDRAT" != "x" ]
then
	drup="-drup=$tmpDir/drat_$$.proof -verb-proof=0"
fi

#
# use riss to solve the formula
#
echo "c call riss with: -config=riss3g $file $drup -no-dense" 
./rissDRAT -config=riss3g $file $drup  -no-dense
exitCode=$?

#
# print the DRAT proof, if answer is UNSAT, and if DRAT is requested
#
if [ "x$doDRAT" != "x" -a "$exitCode" -eq "20" ]
then
	cat $tmpDir/drat_$$.proof
fi

# remove the file, if it exists or not
rm -f $tmpDir/drat_$$.proof

#
# return the received exit code
#
exit $exitCode
