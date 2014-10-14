#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#


param="-rnd-seed=9207562  -no-3resolve -no-symm -no-bve_unlimited -hack=0 -cp3_bve_heap=1 -no-cp3_ee_it -alluiphack=2 -bve_heap_updates=2 -no-cp3_uhdNoShuffle -no-inprocess -no-sls -no-hte -init-act=0 -unhide -subsimp -up -minSizeMinimizingClause=30 -rnd-freq=0 -ee -cla-decay=0.995 -updLearnAct -init-pol=0 -cp3_uhdUHLE=1 -no-cce -dense -bve_BCElim -no-fm -no-bve_gates -no-xor -ee_reset -incReduceDB=300 -enabled_cp3 -no-bve_strength -gc-frac=0.2 -no-bva -no-cp3_randomized -no-otfss -learnDecP=66 -cp3_str_limit=3000000 -rfirst=100 -cp3_bve_limit=25000000 -no-cp3_strength -no-rew -cp3_call_inc=100 -var-decay-e=0.99 -var-decay-b=0.99 -ccmin-mode=2 -phase-saving=2 -rtype=2 -cp3_ee_limit=1000000 -specialIncReduceDB=1000 -no-ee_sub  -cp3_uhdUHTE -firstReduceDB=4000 -cp3_uhdIters=1 -no-longConflict -cp3_ee_bIter=400000000 -no-cp3_uhdTrans -cp3_sub_limit=3000000 -all_strength_res=0 -bve_red_lits=1 -no-bve_totalG -no-probe -no-laHack -bve_cgrow=0 -lhbr=0 -minLBDMinimizingClause=9 -minLBDFrozenClause=30 -bve -rinc=3"


./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
