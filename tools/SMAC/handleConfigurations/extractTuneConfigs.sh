#
#
# if executed in the directory "smac-output" the script creates a file that contains all the best configurations of the last SMAC run
#
#

if [ "x$1" == "x" ]
then
	echo "no output file specified"
	exit 1
fi

rm -f $1 $1.info

# for all runs use the trajectory file
for f in RunGroup-*/traj-run-*.txt
do 
	echo $f;
	# if there are not enough configurations, skip the file 
	ln=`cat $f | wc -l`; if [ "$ln" -lt "3" ]; 
	then
		continue;
	fi;
	
	echo "use with lines: $ln"
	
	# extract the parameters of the very last configuration and store it to the file in parameter 1
	tail -n 1 $f  | awk ' { for(i = 6; i < NF; ++ i ) printf " %s",$i; print "" }' >> $1
	
	# store scenarios for each configuration
	dn=`dirname $f`
	echo "`grep \"^instance_file\" $dn/state-run*/scenario.txt` `grep \"^tunerTimeout\" $dn/state-run*/scenario.txt` " >> $1.info
	
done
