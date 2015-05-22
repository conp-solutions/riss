#
# reruns check on all CNF files in this directory
#
for f in bug*.cnf red*.cnf; 
do 
	# perform check
	./toolCheck.sh ./solver.sh $f > /dev/null 2> /dev/null; 
	ec=$?; 
	
	# print file if check failed
	if [ "$ec" -ne "20" -a "$ec" -ne "10" ]; 
	then 
		echo "$f"; 
		grep "p cnf" $f; 
		echo $ec; 
		echo ""; 
	fi; 
done
