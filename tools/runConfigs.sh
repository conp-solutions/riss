#
# run a CNF file [param1] with all configs that are stored in the file configs.txt
#

#
# run each command with the CSSC model check script
#
while read line
do
	echo "$line"
	configName=$(echo $line | awk '{print $1;}')
	params=$(echo $line | awk '{s = ""; for (i = 2 ; i <= NF; ++i ) s = (s " " $i) ; print s;}')
	echo "$configName --- $params"
	echo ""
	./riss $params $1 -quiet > $configName.out  2> $configName.err
done < configs.txt
