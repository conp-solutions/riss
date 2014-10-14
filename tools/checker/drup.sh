f=~/cnf/unsat-cmu-bmc-barrel6.cnf;
tmpProof=/tmp/proof_$$
tmp2=/tmp/tmp_$$

#run solver
./riss3g $1 -drup=$tmpProof -alluiphack=2 -laHack -mem-lim=400 -no-pre -no-proofFormat  ; 
e=$?

# not unsat, nothing to proof
if [ "$e" -eq "10" ]
then
	rm -rf $tmpProof $tmp2
  return 0
fi

./drup-check $1 $tmpProof | tail -n 2 > $tmp2

cat $tmp2 | grep "s VERIFIED"
e=$?

if [ "$e" -eq "0" ]
then
	rm -rf $tmpProof $tmp2
  return 0
fi

cat $tmp2 | grep "s TRIVIAL"
e=$?
rm -rf $tmpProof $tmp2

exit $e
