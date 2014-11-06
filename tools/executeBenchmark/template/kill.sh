#!/bin/bash

JN=`cat jobNumbers | sed 's/.*<\([0-9]\+\)>.*/\1/'`

for line in $JN
do
 bkill $line
done
