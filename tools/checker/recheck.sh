#
# script to re-run all checks with the listed bugs in the directory and the current solver script
#
for f in red*.cnf bug*.cnf; 
do
	./toolCheck.sh ./solver.sh $f > /dev/null 2> /dev/null; 
	ec=$?
	
	if [ "$ec" -ne "10" -a "$ec" -ne "20" ]
	then
		echo "$f"; 
		grep "p cnf" $f; 
		echo $ec; 
		echo ""; 
	fi
done
