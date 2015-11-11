#
# loop over instances and check whether a family is known for the given instance, prints the matching results to stdout
#
# ./addFamily.sh data.csv 

#
# algorithm
#
tmp=/tmp/joinTimes_$$
tmp2=/tmp/joinTimes2_$$

#
# join files, keep header, sort body
#
for f in `cat $1 | awk '{print $1}'`
do
	grep "^$f" families.txt
done

