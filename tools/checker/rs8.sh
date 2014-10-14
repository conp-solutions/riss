#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#



param="-rnd-seed=9207562  -learnDecP=100 -K=0.8 -no-cce -no-bve_totalG -no-longConflict -lhbr=0 -ccmin-mode=2 -bve_strength -no-inprocess -no-3resolve -rMax=-1 -init-act=0 -all_strength_res=0 -bve_red_lits=0 -no-symm -enabled_cp3 -firstReduceDB=4000 -no-cp3_uhdTrans -no-xor -updLearnAct -rnd-freq=0 -cp3_str_limit=300000000 -init-pol=0 -var-decay-e=0.95 -var-decay-b=0.95 -alluiphack=0 -bve -no-bve_unlimited -unhide -bve_heap_updates=2 -cp3_strength -cp3_uhdUHTE -no-cp3_randomized -up -no-cp3_uhdNoShuffle -incReduceDB=300 -R=1.4 -minSizeMinimizingClause=30 -hack=1 -cp3_uhdIters=3 -subsimp -gc-frac=0.2 -no-bve_force_gates  -szLBDQueue=50 -bve_gates -specialIncReduceDB=1000 -minLBDMinimizingClause=6 -szTrailQueue=5000 -cla-decay=0.999 -phase-saving=2 -no-probe -dense -no-bva -no-sls -minLBDFrozenClause=30 -hack-cost -rtype=0 -cp3_bve_limit=25000000 -bve_cgrow=0 -no-ee -no-hte -cp3_uhdUHLE=1 -no-otfss -no-rew -cp3_bve_heap=0 -no-laHack -bve_BCElim -cp3_sub_limit=300000000 -no-fm -cp3_call_inc=100"

./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
