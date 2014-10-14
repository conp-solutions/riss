#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#


pram="-rnd-seed=9207562  -updLearnAct -szTrailQueue=6000 -rnd-freq=0.5 -no-cp3_randomized -no-cp3_res3_reAdd -cp3_res3_steps=100000 -hack-cost -bve_cgrow_t=10000 -no-dyn -no-bve_BCElim -cp3_cce_level=1 -sym-clLearn -pr-csize=4 -cp3_hte_steps=2147483 -init-act=2 -hlabound=-1 -no-ee -cp3_rew_ratio -no-unhide -sym-ratio=0.3 -sym-consT=1000 -firstReduceDB=2000 -K=0.7 -3resolve -var-decay-e=0.85 -var-decay-b=0.85 -sym-prop -rtype=0 -pr-viviL=5000000 -no-fm -bve -no-xor -sls-ksat-flips=2000000000 -sls-adopt-cls -no-otfss -minSizeMinimizingClause=30 -lhbr=4 -learnDecP=100 -cp3_cce_inpInc=60000 -cp3_hte_inpInc=60000 -sls-rnd-walk=4000 -rMax=-1 -no-pr-probe -inprocess -alluiphack=2 -cp3_rew_Vlimit=1000000 -no-bve_unlimited -cp3_res_bin -gc-frac=0.1 -rew -no-randInp -minLBDMinimizingClause=3 -laHack -cp3_cce_steps=3000000 -cp3_res_eagerSub -cp3_bve_resolve_learnts=2 -cp3_rew_min=2 -hack=1 -cp3_bve_learnt_growth=0 -symm -no-subsimp -cp3_itechs=up -cp3_rew_minA=13 -ccmin-mode=0 -no-cp3_rew_1st -sym-min=4 -cce -bve_heap_updates=1 -sym-size -sym-cons=0 -probe -cp3_res_inpInc=2000 -cp3_inp_cons=80000 -cp3_bve_heap=1 -up -specialIncReduceDB=900 -no-longConflict -bve_red_lits=0 -R=2.0 -bve_strength -cp3_bve_inpInc=50000 -lhbr-sub -no-dense -cp3_rew_Addlimit=10000 -cla-decay=0.995 -cp3_bve_limit=25000000 -sym-propA -hlaTop=64 -cp3_viv_inpInc=100000 -no-bva -pr-viviP=60 -minLBDFrozenClause=30 -bve_cgrow=10 -no-vmtf -sym-iter=0 -incReduceDB=450 -no-bve_gates -init-pol=0 -szLBDQueue=70 -no-sym-propF -sls -phase-saving=1 -hlaLevel=3 -cp3_ptechs=u3svghpwcv -enabled_cp3 -lhbr-max=20000000 -cp3_rew_limit=120000 -cp3_res3_ncls=100000 -hlaevery=8 -cp3_cce_sizeP=80 -inc-inp -hte -tabu -pr-vivi -bve_totalG"


./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
