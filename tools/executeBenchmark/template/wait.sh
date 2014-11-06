#!/bin/bash

JN=`cat jobNumbers | sed 's/.*<\([0-9]\+\)>.*/\1/'`

while true
do
	BN=`bjobs | tail -n +2 | cut -d" " -f1`
	[ -f jobNumbers ] || break;
	grep -qf <(echo "$JN") <(echo "$BN") || break;
	sleep 60
done

touch finishedExperiment
