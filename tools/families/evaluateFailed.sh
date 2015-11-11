#
# sorts all files that (lines that contain " failed ") into family buckets. 
# Summarizes the number of failed formulas per family
#
# ./script.sh data.csv familyFile.txt

#
# algorithm
#
file=$1

tmpd=/tmp/tmp_$$
tmpf=/tmp/evaFail_$$
tmpf2=/tmp/evaFail_$$_2

mkdir $tmpd

# get failed files
grep " failed " $file | awk '{ print $1 }' > $tmpf
echo "found failed instances: `cat $tmpf | wc -l`"

# add family information
./addFamily.sh $tmpf > $tmpf2

# found families:
echo "found matching family information: `cat $tmpf2 | wc -l`"

# print all files per family
./printFamiliyFiles.sh $tmpf2 $tmpd > /dev/null

thisdir=`pwd`

echo "failed instances per family:"
# go to files and print caridnalities
cd $tmpd
{ for f in *.txt; do echo "`basename $f .txt` `cat $f | wc -l`"; done } | sort -k 2 -g
cd $thisdir

# cleanup
rm -rf $tmpd $tmpf $tmpf2
