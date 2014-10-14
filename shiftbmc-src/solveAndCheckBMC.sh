#!/bin/bash
#
#
#

echo "run with PID $$"

#
# buggy version
#
# ../shiftbmc-abc -bmc_v -bmc_v -bmc_dd -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_r -bmc_s -bmc_l -bmc_p UNHIDE:ELS:BVE:VERBOSE:DEBUG -bmc_d -bmc_x -bmc_y $1 15 > /tmp/shiftbmc-plain-model 
../shiftbmc-abc -bmc_v -bmc_dd -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_r -bmc_s -bmc_l -bmc_p AUTO -bmc_d -bmc_x -bmc_y $1 15 > /tmp/shiftbmc-plain-model 
stat=$? 

echo "model"
cat /tmp/shiftbmc-plain-model
 
cat /tmp/shiftbmc-plain-model | grep -v "^u" | ./aigsim -m -c $1
vstat=$?

echo "BMC1: $stat Check1: $vstat"
result=$(($stat + 3*$vstat))
exit $result


#
# good version
#
../shiftbmc-abc -bmc_v -bmc_dd -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_r -bmc_s -bmc_l -bmc_p UNHIDE:ELS -bmc_d -bmc_x -bmc_y $1 15 > /tmp/shiftbmc-plain-model 
stat=$? 
 
cat /tmp/shiftbmc-plain-model | grep -v "^u" | ./aigsim -m -c $1
vstat=$?

echo "BMC1: $stat Check1: $vstat"
result2=$(($stat + 3*$vstat))

exit $(($result + 4*result2))
