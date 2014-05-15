#!/bin/bash
# ShiftBMC-dumpCNF.sh, Norbert Manthey, 2014
#
# take an AIG circuit and dump the CNF formula to solve the specified bound
#
# Usage: shiftBMC-dumpCNF.sh <input.aig> bound <output.cnf> [shiftBMC-arguments]
# 
# requires the binary shiftbmc-abc to be in the same (calling) directory
#


input=$1
bound=$2
output=$3
shift; shift; shift;


tmpOut=/tmp/shiftBMC-dump_$$
#
# check input
#
if [ "x$input" = "x" ]; then
  echo "USAGE: shiftBMC-dumpCNF.sh <input.aig> bound <output.cnf>"
  exit 1
fi

if [ "$bound" -eq "0" ]; then
	echo "the bound needs to be greater than 0!"
  echo "USAGE: shiftBMC-dumpCNF.sh <input.aig> bound <output.cnf>"
  exit 1
fi

#
# call shiftBMC to dump the formula
#
echo "call shiftBMC with: ./shiftbmc-abc -bmc_r -bmc_np -bmc_m  -bmc_s -bmc_l -bmc_d -bmc_x -bmc_y -bmc_mf 1 -bmc_outCNF $tmpOut $input $bound $*"
./shiftbmc-abc -bmc_r -bmc_np -bmc_m  -bmc_s -bmc_l -bmc_d -bmc_x -bmc_y -bmc_mf 1 -bmc_outCNF $tmpOut $input $bound $*

#
# add p cnf line to formula
#
echo "extract p cnf line"
cls=`grep -v "^c" $tmpOut | wc -l`
var=`grep -v "^c" $tmpOut | awk ' { for(i=1;i<=NF;i++) { if ($i > max) max = $i; if ( -$i > max ) max = -$i;} } END { print max } '`

echo "p cnf $var $cls"

#
# write the final CNF
#
echo "write final formula"
echo "p cnf $var $cls" > $output
cat $tmpOut >> $output

#
# clean up
#
rm -f $tmpOut

