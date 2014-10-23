#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

#param=" -enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bve -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -2sat -2sat-units"
param=" -var-decay-e=0.85 -no-ics -minLBDFrozenClause=50 -firstReduceDB=4000 -alluiphack=2 -keepWorst=0.000000 -vsids-d=5000 -minLBDMinimizingClause=3 -agil-limit=0.22 -lhbr=0 -minSizeMinimizingClause=50 -no-otfssL -no-enabled_cp3 -rlevel=1 -var-decay-b=0.85 -otfssMLDB=16 -biAsserting -quickRed -lbdupd=2 -vsids-i=0.0001 -varActB=2 -no-rmf -rer-r=2 -rer -longConflict -no-updLearnAct -agil-decay=0.99 -laHack -learnDecP=-1 -sUhdProbe=0 -rer-new-act=2 -phase-saving=1 -rer-max-size=30 -no-lbdIgnL0 -sInterval=0 -no-dontTrust -rer-window=2 -var-decay-d=10000 -rer-freq=0.1 -cla-decay=0.5 -no-dyn -agil-r -no-rer-l -init-act=6 -init-pol=2 -actDec=2 -biAsFreq=8 -actStart=2048.0 -hlaevery=8"

`cat solver.txt` $1 -mem-lim=2048 /tmp/glucose-out-$$ `cat params.txt` > /dev/null 2> /dev/null
status=$?

cat /tmp/glucose-out-$$

rm -f /tmp/glucose-out-$$

exit $status
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
