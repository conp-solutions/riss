#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#


param="-rnd-seed=9207562  -rtype=0 -no-bve_force_gates -minLBDFrozenClause=15 -no-bve_totalG -bve_cgrow=0 -cp3_cce_steps=2000000 -up -enabled_cp3 -szLBDQueue=30 -cp3_cce_level=3 -cp3_uhdUHLE=1 -cce -all_strength_res=0 -no-ee -no-tabu -hlaTop=-1 -cp3_sub_limit=3000000 -bve_gates -bve_strength -szTrailQueue=5000 -bve_heap_updates=2 -R=1.2 -alluiphack=0 -no-bva -gc-frac=0.2 -cla-decay=0.995 -no-cp3_randomized  -no-hte -firstReduceDB=16000 -phase-saving=2 -no-sls -specialIncReduceDB=1000 -no-cp3_uhdTrans -cp3_uhdUHTE -hlaLevel=5 -hlaevery=1 -no-longConflict -dense -rMax=-1 -no-dyn -hack=0 -incReduceDB=300 -laHack -cp3_cce_sizeP=40 -cp3_uhdIters=3 -ccmin-mode=2 -no-cp3_uhdNoShuffle -minSizeMinimizingClause=30 -cp3_bve_limit=25000000 -no-inprocess -bve_red_lits=0 -cp3_call_inc=100 -no-bve_unlimited -cp3_str_limit=300000000 -hlabound=4096 -no-probe -K=0.8 -bve -unhide -cp3_strength -cp3_bve_heap=0 -var-decay-e=0.85 -var-decay-b=0.85 -rnd-freq=0.005 -minLBDMinimizingClause=6 -subsimp -no-rew -no-3resolve -bve_BCElim "


./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
