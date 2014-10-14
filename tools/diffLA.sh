S1=$( ./riss $* -quiet -laHack | grep "c LAs:" | awk '{ print $8}')
S2=$( ./riss-oldLA $* -quiet -laHack | grep "c LAs:"  | awk '{print $8}' )

sleep 0.2

if [ "$S1" == "$S2" ]
then
	exit 0
else
	echo "S1: >$S1<"
	echo ""
	echo "S2: >$S2<"
	exit 1
fi

exit 2
