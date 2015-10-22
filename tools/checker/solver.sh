#
#
# wrapper script for riss to pass parameters to it
#
# argument: 1) a cnf file 
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#
#param="-threads=2 -model -work-conflicts=-1 -work-timeout=-1 -split-mode=2 -split-timeout=1024 -presel-fac=0.1 -presel-min=64-presel-max=1024 -fail-lit=2 -nec-assign=2 -num-iterat=3 -con-resolv=1 -bin-const -la-heur=4 -presel-heur=2-clause-learn=2 -dir-prior=3 -child-count=7 -shrk-clause -var-eq=3-split-method=1 -split-depth=0 -dseq -h-acc=3 -h-maxcl=7 -h-cl-wg=5-h-upper=10900 -h-lower=0.1 -shpool-size=15000-shclause-size=2 -stop-children -adp-preselS=7 -sort-split -verbosity=0 -verb=0 -print-tree -g-priss-config=-ppconfig=Riss427"
#param="-enabled_cp3 -cp3_stats -bce -bce-bce -bce-bcm -bve -bva -xor -xorEncSize=4 -config=RERRW -revMin"
#param="-act-based -cir-bump=100 -rlevel=2 -pq-order -prob-step-width=1024"
#param="-enabled_cp3 -cp3_stats -quiet -hbr -config="
param="-config= "

#
# select between printing a proof and not printing a proof
#
# run riss without printing a proof
#gdb --args ./riss $1 -mem-lim=2048 /tmp/riss-out-$$ $param -proof=/tmp/proof-riss-$? 
./riss $1 -mem-lim=2048 /tmp/riss-out-$$ $param -proof=/tmp/proof-riss-$? #> /dev/null 2> /dev/null
#./pcasso $1 -mem-lim=2048 $param #> /dev/null 2> /dev/null
status=$?
cat /tmp/riss-out-$$
#
# if unsat, the solver furthermore emits a proof
#
if [ "$status" -eq "20" ]
then
	cat /tmp/proof-riss-$?
fi
rm -f /tmp/riss-out-$$ /tmp/proof-riss-$?
exit $status
