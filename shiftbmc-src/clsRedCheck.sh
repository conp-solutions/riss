
#
#
# run shift BMC with preprocessing and check whether the number of clauses increases
#
#


#
# run ShiftBMC
#
../shiftbmc-abc -bmc_r -bmc_v -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_p -bmc_s -bmc_l -bmc_d -bmc_x -bmc_y -enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -probe -no-pr-vivi -pr-bins -all_strength_res=3  -cp3_stats $1 1 > /tmp/out_$$ 2> /tmp/err_$$ 

# extact [1] clauses
c=`awk '/\[shift-bmc\] \[1\] transition-formula stats/ {print $5}' /tmp/err_$$`

echo $c

# extact [4] clauses
d=`awk '/\[shift-bmc\] \[4\] transition-formula stats/ {print $5}' /tmp/err_$$`
echo $d;

rm -f /tmp/out_$$ rm -f /tmp/err_$$

# return if number increased!
if [ "$c" -lt "$d" ]
then
	exit 1
else
	exit 2
fi
# if if fails, return another exit code!
exit 0
