#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

param=""

choice=3  # currently can range from 1 to 14

#default:
if [ "$choice" -eq 1 ]
then
param="-rnd-seed=9207562  -no-rew -no-cp3_uhdTrans -no-bve_totalG -bve_red_lits=0 -bve_heap_updates=2 -no-sls -no-probe -minSizeMinimizingClause=30 -cp3_uhdUHLE=1 -cp3_sub_limit=300000000 -bve_gates -no-bve_force_gates -bve -no-bva -R=1.4 -K=0.8 -subsimp -gc-frac=0.2 -cp3_bve_limit=25000000 -cp3_bve_heap=0 -rtype=0 -rMax=-1 -firstReduceDB=4000 -bve_strength -var-decay-e=0.95  -var-decay-b=0.95 -specialIncReduceDB=1000 -minLBDFrozenClause=30 -hack=1 -cp3_uhdUHTE -no-cce -unhide -rnd-freq=0 -no-inprocess -incReduceDB=300 -cp3_str_limit=300000000 -bve_BCElim -dense -no-cp3_randomized -cla-decay=0.999 -up -phase-saving=2 -no-longConflict -no-laHack -cp3_strength -hack-cost -no-cp3_uhdNoShuffle -szTrailQueue=5000 -szLBDQueue=50 -minLBDMinimizingClause=6 -enabled_cp3 -no-ee -cp3_uhdIters=3  -ccmin-mode=2 -no-bve_unlimited -bve_cgrow=0 -alluiphack=0 -all_strength_res=0 -no-hte -cp3_call_inc=100 -no-3resolve"
fi

#CSSC-IBM-300s-2day:
if [ "$choice" -eq 2 ]
then
param="-rnd-seed=9207562  -incReduceDB=300 -no-cce -no-3resolve -up -no-sls -enabled_cp3 -no-bve_totalG -minSizeMinimizingClause=30 -firstReduceDB=4000 -no-cp3_randomized  -gc-frac=0.1 -no-ee -dense -cp3_bve_heap=0 -bve_strength -szLBDQueue=50 -cp3_bve_limit=25000000 -cla-decay=0.999 -no-bve_unlimited -no-unhide -hack=0 -cp3_call_inc=100 -ccmin-mode=2 -bve_heap_updates=2 -bve_gates -R=1.4 -K=0.8 -rnd-freq=0.005 -no-hte -szTrailQueue=5000 -specialIncReduceDB=1000 -no-rew -minLBDFrozenClause=30 -bve_red_lits=1 -var-decay-e=0.85 -var-decay-b=0.85 -subsimp -no-inprocess -cp3_sub_limit=300000 -cp3_strength -cp3_str_limit=300000000 -no-bve_force_gates -bve_BCElim -bve -no-bva -alluiphack=0 -all_strength_res=0 -rMax=-1 -phase-saving=2 -minLBDMinimizingClause=3 -no-longConflict -bve_cgrow=0 -rtype=0 -no-probe -no-laHack"
fi

#CSSC-CircuitFuzz-300s-2day:
if [ "$choice" -eq 3 ]
then
param="-rnd-seed=9207562  -no-probe -rfirst=1000 -sls -bve_strength -cp3_bve_limit=2500000 -phase-saving=1 -bve_heap_updates=2 -no-laHack -hack=1 -cp3_bva_Vlimit=3000000 -cp3_ptechs=u3svghpwc -cp3_cce_level=1 -sls-ksat-flips=20000000 -bve -cp3_bva_incInp=200 -cp3_cce_sizeP=40 -cp3_uhdUHTE -no-rew -all_strength_res=3 -hte -no-cp3_bva_subOr -rinc=2 -cp3_randomized -bve_unlimited -unhide -rtype=2 -minLBDFrozenClause=50 -no-hack-cost -cp3_uhdNoShuffle -no-3resolve -cce -sls-rnd-walk=500 -minSizeMinimizingClause=15 -no-inprocess -cla-decay=0.995 -cp3_cce_steps=2000000 -cp3_bva_limit=12000000 -subsimp -specialIncReduceDB=900 -up -incReduceDB=300 -bva -no-ee -cp3_bva_push=1 -cp3_hte_steps=214748 -cp3_bve_heap=0 -no-bve_totalG -enabled_cp3 -rnd-freq=0.005 -var-decay-e=0.99 -ccmin-mode=2 -dense -cp3_call_inc=50 -gc-frac=0.3 -sls-adopt-cls -bve_BCElim -cp3_uhdUHLE=0 -bve_red_lits=1 -no-cp3_uhdTrans -no-cp3_strength -bve_cgrow=20 -cp3_str_limit=400000000 -cp3_uhdIters=3 -no-longConflict -minLBDMinimizingClause=6 -no-bve_gates -cp3_sub_limit=300000000 -firstReduceDB=4000 -cp3_bva_dupli -no-cp3_bva_compl -alluiphack=2 "
fi

#CSSC-GI-300s-2day:
if [ "$choice" -eq 4 ]
then
param="-rnd-seed=9207562  -hack=0 -alluiphack=2 -szTrailQueue=4000 -szLBDQueue=30 -cla-decay=0.995 -minSizeMinimizingClause=30 -minLBDFrozenClause=15 -no-longConflict -incReduceDB=300 -var-decay-b=0.99 -var-decay-e=0.99 -rtype=0 -minLBDMinimizingClause=9 -firstReduceDB=4000 -rnd-freq=0 -gc-frac=0.1 -specialIncReduceDB=1100 -phase-saving=0 -no-laHack -no-enabled_cp3 -rMax=-1 -R=1.4 -K=0.7 -ccmin-mode=2"
fi

#CSSC-BMC08-300s-2day:
if [ "$choice" -eq 5 ]
then
param="-rnd-seed=9207562  -rtype=0 -no-bve_force_gates -minLBDFrozenClause=15 -no-bve_totalG -bve_cgrow=0 -cp3_cce_steps=2000000 -up -enabled_cp3 -szLBDQueue=30 -cp3_cce_level=3 -cp3_uhdUHLE=1 -cce -all_strength_res=0 -no-ee -no-tabu -hlaTop=-1 -cp3_sub_limit=3000000 -bve_gates -bve_strength -szTrailQueue=5000 -bve_heap_updates=2 -R=1.2 -alluiphack=0 -no-bva -gc-frac=0.2 -cla-decay=0.995 -no-cp3_randomized  -no-hte -firstReduceDB=16000 -phase-saving=2 -no-sls -specialIncReduceDB=1000 -no-cp3_uhdTrans -cp3_uhdUHTE -hlaLevel=5 -hlaevery=1 -no-longConflict -dense -rMax=-1 -no-dyn -hack=0 -incReduceDB=300 -laHack -cp3_cce_sizeP=40 -cp3_uhdIters=3 -ccmin-mode=2 -no-cp3_uhdNoShuffle -minSizeMinimizingClause=30 -cp3_bve_limit=25000000 -no-inprocess -bve_red_lits=0 -cp3_call_inc=100 -no-bve_unlimited -cp3_str_limit=300000000 -hlabound=4096 -no-probe -K=0.8 -bve -unhide -cp3_strength -cp3_bve_heap=0 -var-decay-e=0.85 -var-decay-b=0.85 -rnd-freq=0.005 -minLBDMinimizingClause=6 -subsimp -no-rew -no-3resolve -bve_BCElim "
fi

#CSSC-LABS-300s-2day:
if [ "$choice" -eq 6 ]
then
param="-rnd-seed=9207562  -no-cp3_res3_reAdd -hack=0 -no-bve_unlimited -dense -no-up -cp3_bva_Vlimit=3000000 -bve -cp3_res_inpInc=2000 -no-longConflict -cp3_res_eagerSub -no-ee -unhide -specialIncReduceDB=1000 -bve_cgrow=-1 -no-cp3_bva_compl -rtype=2 -rfirst=1000 -cp3_sub_limit=400000000 -enabled_cp3 -phase-saving=2 -laHack -bve_totalG -3resolve -cp3_bva_dupli -rinc=2 -bve_cgrow_t=10000 -cp3_call_inc=50 -bve_heap_updates=1 -ccmin-mode=2 -cp3_bva_limit=12000000 -hlabound=1024 -no-bve_gates -no-probe -cp3_uhdUHLE=0 -sls-rnd-walk=2000 -no-bve_strength -no-bve_BCElim -cp3_randomized -minLBDFrozenClause=50 -sls -hlaevery=8 -bva -cp3_ptechs=u3sghpvwc -gc-frac=0.3 -tabu -subsimp -rnd-freq=0.005 -cp3_bva_subOr -cp3_res3_ncls=100000 -cp3_uhdNoShuffle -cla-decay=0.995 -no-inprocess -cp3_uhdIters=1 -cp3_bva_push=2 -bve_red_lits=1 -hlaLevel=3 -no-rew -alluiphack=2 -cp3_bve_heap=1 -no-hte -var-decay-e=0.99 -var-decay-b=0.99 -all_strength_res=4 -firstReduceDB=4000 -no-cp3_res_bin -cp3_bva_incInp=20000 -cp3_res3_steps=100000 -no-dyn -minLBDMinimizingClause=6 -cp3_strength -cp3_uhdUHTE -no-cce -sls-adopt-cls -cp3_str_limit=300000000 -sls-ksat-flips=-1 -cp3_uhdTrans -cp3_bve_limit=2500000 -minSizeMinimizingClause=15 -incReduceDB=200 -hlaTop=-1"
fi

#CSSC-SWV-300s-2days:
if [ "$choice" -eq 7 ]
then
param="-rnd-seed=9207562  -no-unhide -szTrailQueue=6000 -no-sls -no-probe -gc-frac=0.3 -dense -bve -no-bva -minSizeMinimizingClause=30 -no-laHack -up -rMax=40000  -cla-decay=0.999 -no-bve_totalG -bve_cgrow=10 -subsimp -cp3_str_limit=300000000 -cp3_bve_limit=2500000 -no-cce -R=2.0 -K=0.95 -rtype=0 -cp3_strength -cp3_call_inc=200 -no-bve_BCElim -phase-saving=2 -minLBDMinimizingClause=6 -no-inprocess -no-bve_gates -alluiphack=2 -all_strength_res=5 -var-decay-e=0.7 -var-decay-b=0.7 -rMaxInc=1.5 -longConflict -cp3_sub_limit=300000 -specialIncReduceDB=1000 -rnd-freq=0 -minLBDFrozenClause=15 -enabled_cp3 -ccmin-mode=2 -no-bve_unlimited -incReduceDB=450 -hack=0 -firstReduceDB=16000 -no-ee -no-cp3_randomized -szLBDQueue=30 -no-rew -no-hte -cp3_bve_heap=0 -bve_strength -bve_red_lits=0 -bve_heap_updates=2 -no-3resolve "
fi

#RISS3GEXT
#default:
if [ "$choice" -eq 8 ]
then
param="-rnd-seed=9207562  -learnDecP=100 -K=0.8 -no-cce -no-bve_totalG -no-longConflict -lhbr=0 -ccmin-mode=2 -bve_strength -no-inprocess -no-3resolve -rMax=-1 -init-act=0 -all_strength_res=0 -bve_red_lits=0 -no-symm -enabled_cp3 -firstReduceDB=4000 -no-cp3_uhdTrans -no-xor -updLearnAct -rnd-freq=0 -cp3_str_limit=300000000 -init-pol=0 -var-decay-e=0.95 -var-decay-b=0.95 -alluiphack=0 -bve -no-bve_unlimited -unhide -bve_heap_updates=2 -cp3_strength -cp3_uhdUHTE -no-cp3_randomized -up -no-cp3_uhdNoShuffle -incReduceDB=300 -no-vmtf -R=1.4 -minSizeMinimizingClause=30 -hack=1 -cp3_uhdIters=3 -subsimp -gc-frac=0.2 -no-bve_force_gates  -szLBDQueue=50 -bve_gates -specialIncReduceDB=1000 -minLBDMinimizingClause=6 -szTrailQueue=5000 -cla-decay=0.999 -phase-saving=2 -no-probe -dense -no-bva -no-sls -minLBDFrozenClause=30 -hack-cost -rtype=0 -cp3_bve_limit=25000000 -bve_cgrow=0 -no-ee -no-hte -cp3_uhdUHLE=1 -no-otfss -no-rew -cp3_bve_heap=0 -no-laHack -bve_BCElim -cp3_sub_limit=300000000 -no-fm -cp3_call_inc=100"
fi

#CSSC-IBM-300s-2day:
if [ "$choice" -eq 9 ]
then
pram="-rnd-seed=9207562  -updLearnAct -szTrailQueue=6000 -rnd-freq=0.5 -no-cp3_randomized -no-cp3_res3_reAdd -cp3_res3_steps=100000 -hack-cost -bve_cgrow_t=10000 -no-dyn -no-bve_BCElim -cp3_cce_level=1 -sym-clLearn -pr-csize=4 -cp3_hte_steps=2147483 -init-act=2 -hlabound=-1 -no-ee -cp3_rew_ratio -no-unhide -sym-ratio=0.3 -sym-consT=1000 -firstReduceDB=2000 -K=0.7 -3resolve -var-decay-e=0.85 -var-decay-b=0.85 -sym-prop -rtype=0 -pr-viviL=5000000 -no-fm -bve -no-xor -sls-ksat-flips=2000000000 -sls-adopt-cls -no-otfss -minSizeMinimizingClause=30 -lhbr=4 -learnDecP=100 -cp3_cce_inpInc=60000 -cp3_hte_inpInc=60000 -sls-rnd-walk=4000 -rMax=-1 -no-pr-probe -inprocess -alluiphack=2 -cp3_rew_Vlimit=1000000 -no-bve_unlimited -cp3_res_bin -gc-frac=0.1 -rew -no-randInp -minLBDMinimizingClause=3 -laHack -cp3_cce_steps=3000000 -cp3_res_eagerSub -cp3_bve_resolve_learnts=2 -cp3_rew_min=2 -hack=1 -cp3_bve_learnt_growth=0 -symm -no-subsimp -cp3_itechs=up -cp3_rew_minA=13 -ccmin-mode=0 -no-cp3_rew_1st -sym-min=4 -cce -bve_heap_updates=1 -sym-size -sym-cons=0 -probe -cp3_res_inpInc=2000 -cp3_inp_cons=80000 -cp3_bve_heap=1 -up -specialIncReduceDB=900 -no-longConflict -bve_red_lits=0 -R=2.0 -bve_strength -cp3_bve_inpInc=50000 -lhbr-sub -no-dense -cp3_rew_Addlimit=10000 -cla-decay=0.995 -cp3_bve_limit=25000000 -sym-propA -hlaTop=64 -cp3_viv_inpInc=100000 -no-bva -pr-viviP=60 -minLBDFrozenClause=30 -bve_cgrow=10 -no-vmtf -sym-iter=0 -incReduceDB=450 -no-bve_gates -init-pol=0 -szLBDQueue=70 -no-sym-propF -sls -phase-saving=1 -hlaLevel=3 -cp3_ptechs=u3svghpwcv -enabled_cp3 -lhbr-max=20000000 -cp3_rew_limit=120000 -cp3_res3_ncls=100000 -hlaevery=8 -cp3_cce_sizeP=80 -inc-inp -hte -tabu -pr-vivi -bve_totalG"
fi

#CSSC-CircuitFuzz-300s-2day:
if [ "$choice" -eq 10 ]
then
param="-rnd-seed=9207562  -no-3resolve -no-symm -no-bve_unlimited -hack=0 -cp3_bve_heap=1 -no-cp3_ee_it -alluiphack=2 -bve_heap_updates=2 -no-cp3_uhdNoShuffle -no-inprocess -no-sls -no-hte -init-act=0 -unhide -subsimp -up -minSizeMinimizingClause=30 -rnd-freq=0 -ee -cla-decay=0.995 -updLearnAct -init-pol=0 -cp3_uhdUHLE=1 -no-vmtf -no-cce -dense -bve_BCElim -no-fm -no-bve_gates -no-xor -ee_reset -incReduceDB=300 -enabled_cp3 -no-bve_strength -gc-frac=0.2 -no-bva -no-cp3_randomized -no-otfss -learnDecP=66 -cp3_str_limit=3000000 -rfirst=100 -cp3_bve_limit=25000000 -no-cp3_strength -no-rew -cp3_call_inc=100 -var-decay-e=0.99 -var-decay-b=0.99 -ccmin-mode=2 -phase-saving=2 -rtype=2 -cp3_ee_limit=1000000 -specialIncReduceDB=1000 -no-ee_sub  -cp3_uhdUHTE -firstReduceDB=4000 -cp3_uhdIters=1 -no-longConflict -cp3_ee_bIter=400000000 -no-cp3_uhdTrans -cp3_sub_limit=3000000 -all_strength_res=0 -bve_red_lits=1 -no-bve_totalG -no-probe -no-laHack -bve_cgrow=0 -lhbr=0 -minLBDMinimizingClause=9 -minLBDFrozenClause=30 -bve -rinc=3"
fi

#CSSC-GI-300s-2day:
if [ "$choice" -eq 11 ]
then
param="-rnd-seed=9207562  -learnDecP=75 -incReduceDB=200 -var-decay-e=0.85 -var-decay-b=0.85 -otfssL -lhbr-max=200000 -otfssMLDB=16 -minLBDMinimizingClause=9 -gc-frac=0.3 -vmtf -rfirst=1 -minLBDFrozenClause=15 -firstReduceDB=2000 -ccmin-mode=2 -no-longConflict -hack=0 -init-pol=5 -cla-decay=0.995 -specialIncReduceDB=900 -phase-saving=2 -no-lhbr-sub -rtype=1 -otfss -no-laHack -no-enabled_cp3 -updLearnAct -rnd-freq=0.5 -lhbr=3 -init-act=0 -alluiphack=2 -minSizeMinimizingClause=50 "
fi

#CSSC-BMC08-300s-2day:
if [ "$choice" -eq 12 ]
then
param="-rnd-seed=9207562  -rMax=10000 -szLBDQueue=70 -no-enabled_cp3 -lhbr-max=20000000 -init-act=3 -alluiphack=2 -szTrailQueue=4000 -phase-saving=2 -longConflict -lhbr=1 -hack=0 -no-vmtf -updLearnAct -otfssL -no-laHack -firstReduceDB=8000 -ccmin-mode=2 -minLBDFrozenClause=15 -learnDecP=100 -incReduceDB=450 -gc-frac=0.1 -rtype=0 -rMaxInc=2 -otfssMLDB=30 -init-pol=4 -var-decay-e=0.85 -var-decay-b=0.85 -minSizeMinimizingClause=30 -no-lhbr-sub -specialIncReduceDB=900 -rnd-freq=0 -otfss -minLBDMinimizingClause=6 -cla-decay=0.995 -R=1.6 -K=0.7"
fi

#CSSC-LABS-300s-2day:
if [ "$choice" -eq 13 ]
then
param="-rnd-seed=9207562  -rMax=10000 -no-lhbr-sub -gc-frac=0.3 -alluiphack=2 -no-enabled_cp3 -no-tabu -hlaMax=75 -var-decay-e=0.95 -var-decay-b=0.95 -R=1.6 -K=0.8 -rtype=0 -no-otfssL -minSizeMinimizingClause=50 -laHack -specialIncReduceDB=900 -rnd-freq=0.5 -hlabound=1024 -hack-cost -no-vmtf -szTrailQueue=4000 -otfss -minLBDFrozenClause=50 -no-longConflict -firstReduceDB=8000 -learnDecP=75 -init-act=5 -hlaevery=64 -ccmin-mode=2 -updLearnAct -otfssMLDB=30 -minLBDMinimizingClause=6 -lhbr=3 -init-pol=5 -incReduceDB=300 -hlaLevel=4 -dyn -rMaxInc=2 -phase-saving=0 -szLBDQueue=30 -lhbr-max=2000000 -hlaTop=64 -hack=1 -cla-decay=0.5"
fi

#CSSC-SWV-300s-2days:
if [ "$choice" -eq 14 ]
then
param="-rnd-seed=9207562  -no-otfss -learnDecP=66 -alluiphack=2 -var-decay-e=0.7 -var-decay-b=0.7 -rnd-freq=0 -incReduceDB=200 -gc-frac=0.2 -minLBDFrozenClause=15 -init-pol=2 -ccmin-mode=0 -minLBDMinimizingClause=6 -no-longConflict -hack=0 -no-vmtf -cla-decay=0.5 -rfirst=1000 -phase-saving=2 -firstReduceDB=8000 -no-enabled_cp3 -updLearnAct -rtype=2 -lhbr=0 -specialIncReduceDB=1100 -no-laHack -rinc=3 -minSizeMinimizingClause=30 -init-act=3 "
fi

./riss3g $1 /tmp/glucose-out-$$ $param #> /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
