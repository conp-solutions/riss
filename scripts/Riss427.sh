#!/bin/bash
# Norbert Manthey, 2014
#
# Script to execute Riss with the option to generate a DRAT proof
# Requires the binary to be build with DRAT support
#
# usage: ./Riss427.sh <input.cnf> [TMPDIR] [DRAT]
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
	# disable fm, because it does not support DRAT proofs
	drup="-drup=$tmpDir/drat_$$.proof -verb-proof=0 -no-dense -no-fm"
fi

#
# use riss to solve the formula
#
echo "c call riss with: -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense $f $drup" 
./rissDRAT -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense $file $drup
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
