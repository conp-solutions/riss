#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param="-rnd-seed=9207562  -no-cp3_res3_reAdd -hack=0 -no-bve_unlimited -dense -no-up -cp3_bva_Vlimit=3000000 -bve -cp3_res_inpInc=2000 -no-longConflict -cp3_res_eagerSub -no-ee -unhide -specialIncReduceDB=1000 -bve_cgrow=-1 -no-cp3_bva_compl -rtype=2 -rfirst=1000 -cp3_sub_limit=400000000 -enabled_cp3 -phase-saving=2 -laHack -bve_totalG -3resolve -cp3_bva_dupli -rinc=2 -bve_cgrow_t=10000 -cp3_call_inc=50 -bve_heap_updates=1 -ccmin-mode=2 -cp3_bva_limit=12000000 -hlabound=1024 -no-bve_gates -no-probe -cp3_uhdUHLE=0 -sls-rnd-walk=2000 -no-bve_strength -no-bve_BCElim -cp3_randomized -minLBDFrozenClause=50 -sls -hlaevery=8 -bva -cp3_ptechs=u3sghpvwc -gc-frac=0.3 -tabu -subsimp -rnd-freq=0.005 -cp3_bva_subOr -cp3_res3_ncls=100000 -cp3_uhdNoShuffle -cla-decay=0.995 -no-inprocess -cp3_uhdIters=1 -cp3_bva_push=2 -bve_red_lits=1 -hlaLevel=3 -no-rew -alluiphack=2 -cp3_bve_heap=1 -no-hte -var-decay-e=0.99 -var-decay-b=0.99 -all_strength_res=4 -firstReduceDB=4000 -no-cp3_res_bin -cp3_bva_incInp=20000 -cp3_res3_steps=100000 -no-dyn -minLBDMinimizingClause=6 -cp3_strength -cp3_uhdUHTE -no-cce -sls-adopt-cls -cp3_str_limit=300000000 -sls-ksat-flips=-1 -cp3_uhdTrans -cp3_bve_limit=2500000 -minSizeMinimizingClause=15 -incReduceDB=200 -hlaTop=-1"



./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

echo "solved"

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
