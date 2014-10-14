#
#
# shrink the number of parameters on a remote machine
#
#

machine="141.76.34.76"
path="/tmp/shrink_$$"

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
strip $tool
scp shrinkParams.py  $tool $machine:$path
scp $bug $machine:$path/bug.cnf

#
# setup environment
#
ssh $machine "echo \"$*\" > $path/params.txt"
ssh $machine "echo \"$tool\" > $path/solver.txt"

#
# call fuzzer
#
echo "shrink ..."
ssh $machine "cd $path; python shrinkParams.py $tool $*"
#
# wil run until aborted
#

#
# copy files here
#
echo "get files ..."
mkdir -p shrink_$$
scp $machine:$path/{red.cnf,*.log,*.txt} shrink_$$
echo "$bug" > shrink_$$/buggy-file.txt
echo "store with pid $$"

#
# delete remote directory
#
echo "clean up ..."
ssh $machine "rm -rf $path"
