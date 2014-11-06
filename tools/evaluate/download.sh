#
# download data from cluster and store it in a new directory
#
# /fastfs/nmanthey/$2$1/$confs/data.csv
#
# $1 ... the actual name of the directory to wirk with
# $2 ... another path prefix to work with on the remote side to get to the actual path!
#
# ignores directory names "src" , "template" and "old"
#

dirn=$1

if [ "x$dirn" == "x" ]
then
	echo "no directory parameter specified, abort"
	exit 1
fi

#
#
#
echo "work on remote directory $dirn with full path /fastfs/nmanthey/$2$dirn/"
dirs=`ssh atlas "ls /fastfs/nmanthey/$2$dirn"`

echo "found dirs: $dirs"


mkdir -p $dirn

for d in $dirs ; 
do 
	# do not copy default directories!
	if [ "$d" == "src" -o "$d" == "template" -o "$d" == "old" ]
	then
		continue
	fi
	# copy actual data
	scp atlas:/fastfs/nmanthey/$2$dirn/$d/data.csv $dirn/$d.csv; 
	scp atlas:/fastfs/nmanthey/$2$dirn/$d/drup.csv $dirn/$d.drup.txt; 
	scp atlas:/fastfs/nmanthey/$2$dirn/$d/params.txt $dirn/$d-params.txt; 
done; 



ls
