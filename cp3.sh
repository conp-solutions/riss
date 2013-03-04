
#
# solve instance $1 by simplifying first with coprocessor, then run a SAT solver, and finally, reconstruct the model
#

# some files in /tmp
undo=/tmp/cp3_undo_$$
tmpCNF=/tmp/cp3_tmpCNF_$$
model=/tmp/cp3_model_$$

#
# run coprocessor with most wanted parameters
#
./coprocessor  $1  -enabled_cp3  -cp3_stats -up -ee -cp3_undo=$undo -dimacs=$tmpCNF

#
# exit code == 0 -> could not solve the instance
# dimacs file will be printed always
# exit code could be 10 or 20, depending on whether coprocessor could solve the instance already
#

# run your favorite solver (output is expected to look like in the competition!)
checker/minisat $tmpCNF $model

#
# undo the model
# coprocessor can also handle "s UNSATISFIABLE"
#
./coprocessor -cp3_post -cp3_undo=$undo -cp3_model=$model > model.cnf

#
# verify final output?
#
#./verify SAT model.cnf $1

# clean p
rm -f $undo $tmpCNF $model
