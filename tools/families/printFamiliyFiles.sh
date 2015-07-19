#
# add family column to given data file, prints to stdout
#
# ./script.sh familyFile.txt result-dirname

#
# algorithm
#
tmp=/tmp/families_$$
tmp2=/tmp/joinTimes2_$$

mkdir -p $2
		
# iterate over all files, put into correct family file
awk '{ if( NR > 1 ) print $0}' $1 | while read f
do
	echo "line: $f"
	fam=`echo $f | awk '{ printf $2}'`
	file=`echo $f | awk '{ printf $1}'`
	echo "fam: $fam"
	echo $file >> $2/$fam.txt
done

		
rm -f $tmp $tmp2		
