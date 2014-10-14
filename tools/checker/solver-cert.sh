#
# use riss to produce a drup proof
#

#
# set parameters
#
param="-enabled_cp3 -up -unhide -bve -subsimp -ee -hte"

#
# temp files
#
tmpOut=/tmp/output_$$
tmpDrup=/tmp/proof_$$

#
# ru solver with parameters, output proof, add all comments from command line
#
echo "execute: ./riss3g $1 $param -drup=$tmpDrup " # ${@:2}"
./riss3g $1 $param -drup=$tmpDrup   > $tmpOut #    ${@:2}
exitCode=$?

#
# check whether proof is necessary
#
if [ "$exitCode" -eq "20" ]
then
  cat $tmpDrup >> $tmpOut
#  echo "proof: "
#  cat $tmpDrup
fi

#
# check result
#
# valgrind -v --track-origins=yes 
./certifiedSAT $1 $tmpOut ;
ec=$?

if [ "$ec" -eq "0" ]
then
  echo "passed check"
fi

#
# cleanup
#
echo "remove files" && rm -f $tmpDrup $tmpOut
#echo "used instances: drup: $tmpDrup output: $tmpOut"

#
# exit
#
exit $ec
