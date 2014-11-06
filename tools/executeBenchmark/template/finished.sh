#!/bin/bash

JN=`cat jobNumbers | sed 's/.*<\([0-9]\+\)>.*/\1/'`

bj=`bjobs`

for line in $JN
do
 echo $bj | grep "$line" > /dev/null
 status=$?
 if [ "$status" -eq "0" ]
 then
  echo "not done, wait for $line"
  exit 1
 fi
done
echo "done"
exit 0
