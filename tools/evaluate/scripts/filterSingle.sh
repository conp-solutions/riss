#
# reduces all / the given data file to the specified year
# by keeping all lines that contain "CNF/year" for all the years that are specified on the command line
#
# usage ./filter.sh [file] years
#
# Note: if a pattern occurs multiple times, the instances are duplicated as well!
#

file=$1

#
# Algorithm
#
tmp=/tmp/filter_$$
tmp2=/tmp/filter2_$$

shift

for f in $file; 
do 
	echo "work on $f"
	bn=`basename $f .csv` # get name of the file

	# keep the headline
  awk '{ if( NR == 1 ) print $0}' $f > $tmp2
  
  # select ll the required files
  rm -f $tmp
  for pattern in $*
  do
   grep "CNF/$pattern" $f >> $tmp
  done
  
  # to obtain a sorted file again

	cat $tmp2 > $f
	cat $tmp | sort -k 1b,1 >> $f
done

rm -f $tmp $tmp2
