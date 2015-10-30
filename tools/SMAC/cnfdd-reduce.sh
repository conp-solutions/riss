# cnfdd-reduce.sh, Norbert Manthey, 2015
#
# minimize bug with cnfdd, based on smac parameters
# 

file=$1
shift
outfile=$1
shift

params=`python ./mapCSSCparams.py $*`
echo "-checkModel -config= $params"
./cnfdd $file $outfile ./riss -checkModel -config= $params

