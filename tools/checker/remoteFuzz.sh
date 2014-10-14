#
#
# script to fuzz on a remote machine
#
#

machine="141.76.34.67"
path="/tmp/fuzz_$$"

tool=$1
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
strip $tool
scp fuzzcheck.sh toolCheck.sh lgl verify cnfdd cnfuzz remoteRS.sh $tool $machine:$path

#
# setup environment
#
ssh $machine "echo \"$*\" > $path/params.txt"
ssh $machine "echo \"$tool\" > $path/solver.txt"

#
# call fuzzer
#
echo "fuzz ..."
ssh $machine "cd $path; ./fuzzcheck.sh ./remoteRS.sh"
#
# wil run until aborted
#

#
# copy files here
#
echo "get bugs ..."
mkdir -p bugs_$$
scp $machine:$path/{*.cnf,*.log,*.txt} bugs_$$

echo "store with pid $$"
#
# delete remote directory
#
echo "clean up ..."
# ssh $machine "rm -rf $path"
