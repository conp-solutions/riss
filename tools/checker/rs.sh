#
#
# wrapper script for riss to pass parameters to it
#
# argument: 1) a cnf file 2) [optional] a file for the drat proof
# return code: 10 or 20 (SAT/UNSAT)
# print to screen: solution for the cnf
#

#param=" -enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bve -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -2sat -2sat-units"
#param="-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -no-cp3_uhdUHLE -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu"
#param="-longConflict"
#param=" -enabled_cp3 -cp3_stats -up -2sat -2sat-units -2sat-cq"
#param="-lhbr=4"
#param="-enabled_cp3 -up -fm -fm-debug=0 -cp3_fm_grow=20 -cp3_fm_grow=20000  -cp3_fm_newAlo=1 -cp3_fm_newAlk=0 -cp3_fm_amt"
#param="-laHack -lhbr=4"
#param="-enabled_cp3 -up -xor -xorFindRes"
#param="-otfss -enabled_cp3 -up -ee"
#param="-learnDecP=50"
#param="-laHack -sUHLEsize=30 -sUHLElbd=6 -alluiphack=2 -sUhdProbe=3 -hack=2"
#param="-ecl -no-ecl-l -ecl-rn -ecl-maxLBD=8"
#param="-proof-oft-check=2 -enabled_cp3 -cp3_iters=2  -up -subsimp -bve -ee -cp3_ee_level=2 -cp3_stats -bce -bce-cla -bce-cle -rate -enabled_cp3 -cp3_stats -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -bva -cp3_bva_subOr"
#param="-proof-oft-check=1 -enabled_cp3 -cp3_iters=2  -up -subsimp -bve -cp3_stats -rate -enabled_cp3 -cp3_stats -lhbr=3 -otfss -hte -ee " # -ics "
#param="-enabled_cp3 -proof-oft-check=0 -ee_debug=0 -cp3_verbose=0  -bce -no-bce-bce -bce-cle"
#param="-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000 -rer -rer-rn -no-rer-l"
#param="-config=LABSDRAT -no-dense"

#param="-incReduceDB=200 -minLBDFrozenClause=50 -minSizeMinimizingClause=15 -var-decay-b=0.99 -var-decay-e=0.99 -cla-decay=0.995 -rnd-freq=0.005 -rnd-seed=9.20756e+06 -rtype=2 -rfirst=1000 -gc-frac=0.3 -alluiphack=2 -laHack -hlaevery=8 -hlabound=1024 -cp3_randomized -enabled_cp3 -subsimp -bve -bva -unhide -3resolve -dense -cp3_ptechs=u3sghpvwc -sls -cp3_bve_limit=2500000 -no-bve_strength -bve_red_lits=1 -no-bve_gates -cp3_bve_heap=1 -bve_cgrow=-1 -bve_cgrow_t=10000 -bve_totalG -no-bve_BCElim -cp3_bva_limit=12000000 -cp3_bva_incInp=20000 -no-cp3_bva_compl -cp3_bva_subOr -cp3_res3_steps=100000 -cp3_res_inpInc=2000 -sls-ksat-flips=-1 -sls-adopt-cls -all_strength_res=4 -cp3_sub_limit=400000000 -cp3_call_inc=50 -cp3_uhdIters=1 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdNoShuffle -no-dense"
param="-enabled_cp3 -cp3_stats -rate -no-rate-rate -rate-bcs -quiet"


#
# select between printing a proof and not printing a proof
#
if [ "x$2" == "x" ] 
then
	# run riss without printing a proof
	./riss $1 -mem-lim=2048 /tmp/glucose-out-$$ $param #> /dev/null 2> /dev/null
	status=$?
else
	# run riss with printing a DRUP/DRAT proof
	echo "generate proof"
	./riss $1 -mem-lim=2048 /tmp/glucose-out-$$ $param -drup=$2 -proofFormat="" -verb-proof=0 #> /dev/null 2> /dev/null
	status=$?
fi
cat /tmp/glucose-out-$$
rm -f /tmp/glucose-out-$$
exit $status
