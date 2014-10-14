#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#


param="-rnd-seed=9207562  -no-otfss -learnDecP=66 -alluiphack=2 -var-decay-e=0.7 -var-decay-b=0.7 -rnd-freq=0 -incReduceDB=200 -gc-frac=0.2 -minLBDFrozenClause=15 -init-pol=2 -ccmin-mode=0 -minLBDMinimizingClause=6 -no-longConflict -hack=0 -cla-decay=0.5 -rfirst=1000 -phase-saving=2 -firstReduceDB=8000 -no-enabled_cp3 -updLearnAct -rtype=2 -lhbr=0 -specialIncReduceDB=1100 -no-laHack -rinc=3 -minSizeMinimizingClause=30 -init-act=3 "


./riss3g $1 /tmp/glucose-out-$$ $param > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
