#
# takes a feature file and prepares a better CSV file that does not contain time information any more
#

file=$1
prefix=$2
tmp=/tmp/process_$$
tmp2=/tmp/process2_$$

# analyze file
echo "columns in $file: "
awk '{ print NF }' $file | sort -n | uniq
# print single element lines
awk '{ if( NF==1) print $0 }' $file

# replace all "'" by ""
cat $1 | sed -e "s:'::g" > $tmp
# replace all "," by " "
cat $tmp |sed -e "s:,: :g" > $tmp2
# replace prefix
cat $tmp2 |sed -e "s:^$prefix::g" > $tmp
mv $tmp $tmp2

# remove the second column (mentions instance once more, first has to be kept for problematic formulas!)
awk '{for (i=1; i<NF; i++) if(i!=2) printf $i " "; print $NF}' $tmp2 > $tmp
cat $tmp | wc -l

# remove all colums that contain timing information

# get columns with time information
TIMECOLS=$(awk '{ if( NR == 1 ) { for(i=1; i<=NF; i++) {if (index($i, "time") != 0) {print i} } } }' $tmp | sort -n -r)
echo "columns with time information:"
echo $TIMECOLS

# cut each col with time information
for t in $TIMECOLS
do
  echo "cut col $t"
	awk -v cutcol=$t '{for (i=1; i<=NF; i++) if( i != cutcol ) printf $i " "; print ""}' $tmp > $tmp2
	mv $tmp2 $tmp
	cat $tmp | wc -l
done

# test for time once more
TIMECOLS=$(awk '{ if( NR == 1 ) { for(i=1; i<=NF; i++) {if (index($i, "time") != 0) {print i} } } }' $tmp | sort -g -r)
echo "columns with time information:"
echo $TIMECOLS

# print problematic formulas
echo "problematic formulas:"
awk '{ if( NF < 10 ) print $0 }' $tmp 
awk '{ if( NF < 10 ) print $0 }' $tmp | wc -l

# copy all lines that are ok (more than 10 elements, or not "std::bad_alloc")
awk '{ if( NF >= 10 || $2 != "std::bad_alloc" ) print $0 }' $tmp > $tmp2
cat $tmp2 | wc -l

# replace std::bad_alloc with basename of the formulas in the first field
for f in `awk '{ if( NF < 10 && $2 == "std::bad_alloc" ) print $1 }' $tmp`
do
  echo "$f `basename $f`" >> $tmp2
done

cat $tmp2 | wc -l

# overwrite input file
mv $tmp2 $file
