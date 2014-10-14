# ShiftBMC, Norbert Manthey, 2014
# run ShiftBMC with shifting as unrolling, Coprocessor as CNF simplifier and ABC as circuit simplifier, Priss 4.27 as SAT solver with 4 threads
#
# parameter 1: instance
# parameter 2: maxbound (optional!)

./shiftbmc-abc $1 $2 -bmc_pr 4 -bmc_v -bmc_ad /tmp/ -bmc_np -bmc_m -bmc_p AUTO -bmc_s -bmc_l -bmc_d -bmc_x -bmc_y -bmc_mp AUTO -bmc_mf 1
