#
#
# script to fuzz on a remote machine
#
#

machine="141.76.34.67"
path="/tmp/deltadebug_$$"

tool=$1
shift
bug=$1
shift

echo "run with pid $$"
#
# create path
#
echo "setup ..."
ssh $machine "mkdir -p $path"

#
# copy relevant files to machine
#
echo "copy ..."
strip $tool riss
scp modelChecked.sh riss toolCheck.sh lgl verify cnfdd remoteRS.sh $tool $machine:$path
scp $bug $machine:$path/bug.cnf

#
# setup environment
#
ssh $machine "echo \"$*\" > $path/params.txt"
ssh $machine "echo \"$tool\" > $path/solver.txt"

#
# call fuzzer
#
echo "cnfdd ..."
ssh $machine "cd $path; ./cnfdd bug.cnf red.cnf ./toolCheck.sh ./remoteRS.sh"
#
# wil run until aborted
#

#
# copy files here
#
echo "get bugs ..."
mkdir -p cnfdd_$$
scp $machine:$path/{red.cnf,*.log,*.txt} cnfdd_$$
echo "$bug" > cnfdd_$$/buggy-file.txt
echo "store with pid $$"

#
# delete remote directory
#
echo "clean up ..."
ssh $machine "rm -rf $path"
