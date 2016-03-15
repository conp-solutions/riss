#!/bin/bash

#
# run shiftbmc-abc on file (param 1) until bound (param 2)
#
# 
#

file=$1
bound=$2

./shiftbmc-abc -bmc_r -bmc_v -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_p AUTO -bmc_s -bmc_l -bmc_d -bmc_x -bmc_y -bmc_mp AUTO -bmc_rpc INCSOLVE:INCSIMP:-cp3_iinp_cons=50000 100000 -bmc_ac "dc2:dc2:&get;&scorr;&put" -bmc_p BMC_FULL:-bve_early:-cp3_stats:-cp3_bve_limit=50000000:-all_strength_res=4:-pr-vivi:-cp3_Abva:-3resolve $file $bound
ec=$?

exit $ec
