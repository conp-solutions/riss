#
#
# take list of SMAC trajectory file lines and convert their riss configurations into Riss tool calls
#
#

T=/tmp/convertConfigs.tmp.$$

# remove leading numbers
awk ' { for(i = 6; i < NF; ++ i ) printf " %s",$i; print "" }' $1 > $T

# count lines
l=`cat $T | wc -l`

# convert each line separately
for a in `seq 1 $l`; do python mapCSSCparams.py `head -n $a $T | tail -n 1`; done


