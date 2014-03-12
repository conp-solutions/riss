
echo "run fuzzcheck-aig with PID $$"

tmpAIG=/tmp/tmp_$$.aig
tmpOut=/tmp/tmp_$$.out
tmpErr=/tmp/tmp_$$.err

i=0
t=0
used=0
solved=0
while true
do
  let "t=$t+1"
  # echo "test $t"
	./aigfuzz -2 -S -a -b -z -m > $tmpAIG;
	ands=`head -n 1 $tmpAIG | awk '{print $2}'`;
	latches=`head -n 1 $tmpAIG | awk '{print $4}'`;
	outputs=`head -n 1 $tmpAIG | awk '{print $5}'`;
  bads=`head -n 1 $tmpAIG | awk '{print $7}'`;

#  print "gen: $ands    --    $latches    --    $outputs    --    $bads         "

	printf "gen: %d / %d / %d / %d  :      %d  --  %d     --     %6d  --  %6d               \r" "$i" "$solved" "$used" "$t" "$ands" "$latches" "$outputs" "$bads"

	sleep 0.015
  # generate another circuit, if this one does not match size expectations
	if [ $ands -gt "5000" -o $latches -lt 2 -o $latches -gt 60 -o $outputs -gt 1 -o $outputs -eq 0 ]
	then
	  continue;
	fi
	
	let "used=$used+1"
	
	sleep 0.02

	# solve circuit in at most 90 seconds, check properties
	# echo "solve ..."
	timeout 90 $* $tmpAIG 250 > $tmpOut 2> $tmpErr
	ecode=$?
	
	if [ "$ecode" -eq "1" ] 
	then
		let "solved=$solved+1"
		# verify model
		cat $tmpOut | ./aigsim -m -w  $tmpAIG > /dev/null 2> /dev/null
		verycode=$?
		if [ "$verycode" -ne "0" ]
		then
			echo "model is not a witness"
			let "i=$i+1"
			mv $tmpAIG bug-$i.aig
		fi
	else
		if [ "$ecode" -ne "0" ]
		then
			let "i=$i+1"
			mv $tmpAIG bug-$i.aig
		fi
		echo ""
		echo "exitCode: $ecode"
		tail -n 10 $tmpErr
		echo ""
		echo ""
	fi

	
done

rm -f $tmpAIG $tmpOut $tmpErr

