
# temporary files
tmpOut=/tmp/tmp_$$.out
tmpErr=/tmp/tmp_$$.err

#./aigfuzz -2 -S -b -z -m > $tmpAIG;
ands=`head -n 1 $1 | awk '{print $2}'`;
latches=`head -n 1 $1 | awk '{print $4}'`;
outputs=`head -n 1 $1 | awk '{print $5}'`;

echo "$1: $ands $latches $outputs $bads"

# generate another circuit, if this one does not match size expectations
#if [ $ands -gt "35000" -o $latches -lt 2 -o $latches -gt 30 -o $outputs -gt 10 ]
if [ $latches -lt 4 -o $outputs -gt 10 ]
then
  echo "reject size"
  exit 0
fi

# solve circuit with limited resources, check properties
timeout -k 61 60 ./aigbmc -v -v -m $1 20 > $tmpOut 2> $tmpErr

# get number of iterations
iter=`cat $tmpErr | grep "\[aigbmc\] encoded " | tail -n 1 | awk '{print $3}'`
echo "iter: $iter"
if [ "$iter" -lt "3" ]
then
  echo "reject iters"
  rm -f $tmpErr $tmpOut
  exit 0
fi

# check whether circuit has a bad state and has been solved
solved=`tail -n 1 $tmpOut`
echo "solved: $solved"
if [ "$solved" != "." ]
then
  echo "reject solve-status"
  rm -f $tmpErr $tmpOut
  exit 0
fi

#
# found a circuit that has a good size and is solvable in a small amount of iteratinos (low bound)
#
rm -f $tmpErr $tmpOut
exit 1
