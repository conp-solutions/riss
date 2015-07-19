#
# add family column to given data file, prints to stdout
#
# ./script.sh familyFile.txt result-dirname

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

# add family information
./addFamily.sh $tmpf > $tmpf2

# print all files per family
./printFamiliyFiles.sh $tmpf2 $tmpd

thisdir=`pwd`

# go to files and print caridnalities
cd $tmpd
{ for f in *.txt; do echo "`basename $f .txt` `cat $f | wc -l`"; done } | sort -k 2 -g
cd $thisdir

# cleanup
rm -rf $tmpd $tmpf $tmpf2
