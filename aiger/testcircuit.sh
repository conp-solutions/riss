#!/bin/sh

#
# run circuit with bmc and give output
#

file=$1
shift

# run bmc
# ./shiftbmc $file 20 -bmc_np -bmc_m -bmc_v -bmc_p -bmc_s -bmc_l -bmc_d -bmc_x -enabled_cp3 -cp3_stats -dense -cp3_dense_forw -bve > /tmp/testcircuit_$$
./shiftbmc $file 20 -bmc_np -bmc_m -bmc_v -bmc_p -bmc_s -bmc_l -bmc_x -enabled_cp3 -cp3_stats -bve > /tmp/testcircuit_$$
ec=$?

#
# not finding a bug counts as solving the problem with a valid solution
#
if [ "$ec" -ne "1" ]
then
	exit 0
fi

# verify
cat /tmp/testcircuit_$$ | ./aigsim -m -c $file
vc=$?

# cleanup
rm -f /tmp/testcircuit_$$

echo "vc:$vc"
exit $vc
