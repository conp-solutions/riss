

tmpAIG=/tmp/tmp_$$.aig
tmpOut=/tmp/tmp_$$.out
tmpErr=/tmp/tmp_$$.err

i=0
t=0
while true
do
  let "t=$t+1"
  echo "test $t"
	./aigfuzz -2 -S -b -z -m > $tmpAIG;
	ands=`head -n 1 $tmpAIG | awk '{print $2}'`;
	latches=`head -n 1 $tmpAIG | awk '{print $4}'`;
	outputs=`head -n 1 $tmpAIG | awk '{print $5}'`;
  bads=`head -n 1 $tmpAIG | awk '{print $7}'`;

  echo "gen: $ands $latches $outputs $bads"

  # generate another circuit, if this one does not match size expectations
	if [ $ands -gt "1000" -o $latches -lt 2 -o $latches -gt 30 -o $outputs -gt 10 ]
	then
	  continue;
	fi
	
	# solve circuit, check properties
	./aigbmc /tmp/tmp.aig 50 -bmc_m -bmc_v -bmc_p -bmc_s -bmc_l > $tmpOut 2> $tmpErr
	
	# get number of iterations
	iter=`cat $tmpErr | grep "\[aigbmc\] encoded " | tail -n 1 | awk '{print $3}'`
	echo "iter: $iter"
	if [ "$iter" -lt "2" ]
	then
	  continue;
	fi
	
	# check whether circuit has a bad state and has been solved
	solved=`tail -n 1 $tmpOut`
	if [ "x$solved" != "x." ]
	then
	  continue
	fi
	
	#
	# found a circuit that has a good size and is solvable in a small amount of iteratinos (low bound)
	#
	echo $tmpErr
	echo $tmpOut
	
	cp $tmpAIG circuit-$i.aig
	let "i=$i+1"
done
