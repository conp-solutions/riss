#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

# set this to the right values!
#param="-enabled_cp3 -probe -cp3_unlimited"
#param="-enabled_cp3 -subsimp -naive_strength -bve -inprocess "
#param="-enabled_cp3 -bva -bve -ee -up -subsimp -unhide -probe -hte -cp3_bva_subOr -mem-lim=600 -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -cp3_par_gates -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -cp3_par_gates -cp3_bve_resolve_learnts=2 -cp3_bve_learnt_growth=5 -inprocess -bve_heap_updates"
#param="-enabled_cp3 -bva -bve -ee -up -subsimp -unhide -cp3_bva_subOr -mem-lim=500 -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2"
param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve=2 -cp3_threads=2 -par_bve_min_updates"
#param="-enabled_cp3 -bve -up -subsimp -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -bve_heap_updates -inprocess -all_strength_res"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -cp3_par_gates -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -cp3_par_gates -cp3_bve_resolve_learnts=2 -cp3_bve_learnt_growth=5 -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -bve_gates -cp3_bve_heap=0 -cp3_par_gates -cp3_bve_resolve_learnts=2 -cp3_bve_learnt_growth=5 -inprocess"
#param="-enabled_cp3 -bve -up -subsimp -cp3_par_bve -cp3_threads=2 -cp3_bve_heap=2 -bve_unlimited"
#param="-enabled_cp3 -bva -bve -ee -up -subsimp -cp3_stats -no-inprocess -cp3_bva_subOr"
#param="-enabled_cp3 -subsimp -cp3_threads=2 -inprocess -cp3_par_strength -cp3_par_subs -bve"
#param="-enabled_cp3 -ee -log=0 -no-cp3_extBlocked -cp3_extNgtInput -no-inprocess -cp3_eagerGates"
#param="-enabled_cp3 -subsimp -inprocess -bve -cp3_bve_heap=2 -cp3_threads=2 -cp3_par_strength"
#
# run the program
#
./minisat $1 /tmp/minisat-out $param > /dev/null 2> /dev/null

status=$?

cat /tmp/minisat-out

exit $status
