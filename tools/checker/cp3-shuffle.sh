#
#
# wrapper script for minisat to pass parameters to it
#
# argument: a cnf file
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

#param=" -enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bve -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -2sat -2sat-units"
#param="-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu"
#param="-longConflict"
#param=" -enabled_cp3 -cp3_stats -up -2sat -2sat-units -2sat-cq"

./coprocessor $1 -cp3_undo=undo.cnf -shuffle -enabled_cp3 -cp3_stats -shuffle-order -shuffle-debug=0 -shuffle-seed=132 -bva -bve -dense -dimacs=shuffled.cnf; 
./riss3g shuffled.cnf model.cnf; 
./coprocessor -cp3_undo=undo.cnf -cp3_post -cp3_model=model.cnf -shuffle -enabled_cp3 -cp3_stats -shuffle-order -shuffle-debug=0 -shuffle-seed=132 
exit $?
#./riss3g  -enabled_cp3 -cp3_stats -up -2sat -2sat-units checker/bug2sat.cnf -quiet -cp3_verbose=1 -2sat-debug=0 -no-2sat-cq
