#!/bin/bash

#
#
# extract as much information as possible from stderr of riss
#
# parameter: stderr-file timeoutseconds instance-filename
#
#

TLIMIT=$2


PARAMETERS_IN_FILES=23

#
# extract:
# solution,time, memory
# solve time, pp time
# clause reduction
# all pp techniques: reduction, reduction/call, reduction/ms
# all search values (relative)
# all simplification techniques: reduction, reduction/call, reduction/ms
#


# main script
#
#

#echo "work on file $1"

runlim=`cat $1 | grep "runlim"`

let "LINES_TO_GET = 28 + $PARAMETERS_IN_FILES"

details=`tail -n $LINES_TO_GET $1`

status="-"
exitCode="-"
rtime="-"
ctime="-"
memory="-"
dec="-"
con="-"
ratio="-"
upc="-"
dups="-"
dups1="-"
dups2="-"
dups3="-"
pps="-"

#echo ""
#echo ""
#echo "work on file $3"
#echo "$runlim"

ctime=`echo "$details" | awk '/] time:/ {print $3}'`
if [ "$ctime" == "" ]
then
 ctime="-"
fi

#echo "---"
#echo "ctime = $ctime"
#echo "---"

rtime=`echo "$details" | awk '/] real:/ {print $3}'`
if [ "$rtime" == "" ]
then
 rtime="-"
fi

#echo "---"
#echo "rtime = $rtime"
#echo "---"

status=`echo "$details" | awk '/] status:/ {print $3}'`

exitCode="-"
memory="-"

# evaluate status of solver
if [ "$status" == "ok" ]
then
	status="ok"

        exitCode=`echo "$details" | awk '/] result:/ {print $3}'`
        if [ "$exitCode" == "" ]
        then
          exitCode="-"
        fi

        memory=`echo "$details" | awk '/] space:/ {print $3}'`
        if [ "$memory" == "" ]
        then
          memory="-"
        fi
else
        ctime="-"
        rtime="-"

#	status="fail"
	status=`echo "$details" | awk '/] status:/ {print $5}'`
	if [ "$status" == "time" ]
	then
		status="timeout"
	else
		if [ "$status" == "memory" ]
		then
			status="memoryout"
		else
			status="fail"
		fi
	fi

        exitCode="-"
        memory="-"
fi

#exitCode=`echo "$details" | awk '/] result:/ {print $3}'`
#if [ "$exitCode" == "" ]
#then
# exitCode="-"
#fi

#memory=`echo "$details" | awk '/] space:/ {print $3}'`
#if [ "$memory" == "" ]
#then
# memory="-"
#fi

# c UP Clauses            : 1598043323
# c Duplicate Reasons     : 899639547 ( 0.562963 )

ratio="-"
dups="-"
upc="-"

if [ "$status" == "ok" ] 
then

ratio=`echo "$details" | awk '/c Duplicate Reasons / {print $7}'`
if [ "$ratio" == "" ]
then
 ratio="-"
fi

upc=`echo "$details" | awk '/c UP Clauses/ {print $5}'`
if [ "$upc" == "" ]
then
 upc="-"
fi

dups=`echo "$details" | awk '/c Duplicate Reasons / {print $5}'`
if [ "$dups" == "" ]
then
 dups="-"
fi

dups1=`echo "$details" | awk '/c Duplicate Reasons1/ {print $5}'`
if [ "$dups1" == "" ]
then
 dups1="-"
fi

dups2=`echo "$details" | awk '/c Duplicate Reasons2/ {print $5}'`
if [ "$dups2" == "" ]
then
 dups2="-"
fi

dups3=`echo "$details" | awk '/c Duplicate ReasonsMore/ {print $5}'`
if [ "$dups3" == "" ]
then
 dups3="-"
fi

con=`echo "$details" | awk '/c conflicts / {print $4}'`
if [ "$con" == "" ]
then
 con="-"
fi

dec=`echo "$details" | awk '/c decisions / {print $4}'`
if [ "$dec" == "" ]
then
 dec="-"
fi

#c propagations          : 1643271        (2738616 /sec)

#BEGIN {FS = "[\ ][\(]"}

pps=`echo "$details" | awk 'BEGIN {FS="[ ()]+"} /c propagations /  {print $5}'`
if [ "$pps" == "" ]
then
 pps="-"
fi



fi

#
# when adding more statistic extraction here, make sure you add it also to the below line, and to the headline in the file "evaluate.sh" !!
#


echo "$3 $status $exitCode $rtime $ctime $memory $dec $con $ratio $upc $dups $dups1 $dups2 $dups3 $pps"

exit 0

