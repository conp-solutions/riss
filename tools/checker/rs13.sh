#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#


param="-rnd-seed=9207562  -rMax=10000 -no-lhbr-sub -gc-frac=0.3 -alluiphack=2 -no-enabled_cp3 -no-tabu -hlaMax=75 -var-decay-e=0.95 -var-decay-b=0.95 -R=1.6 -K=0.8 -rtype=0 -no-otfssL -minSizeMinimizingClause=50 -laHack -specialIncReduceDB=900 -rnd-freq=0.5 -hlabound=1024 -hack-cost -szTrailQueue=4000 -otfss -minLBDFrozenClause=50 -no-longConflict -firstReduceDB=8000 -learnDecP=75 -init-act=5 -hlaevery=64 -ccmin-mode=2 -updLearnAct -otfssMLDB=30 -minLBDMinimizingClause=6 -lhbr=3 -init-pol=5 -incReduceDB=300 -hlaLevel=4 -dyn -rMaxInc=2 -phase-saving=0 -szLBDQueue=30 -lhbr-max=2000000 -hlaTop=64 -hack=1 -cla-decay=0.5"


./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
