timeout 60 ./blimc $1 40
e1=$?


timeout 60 ./ShiftBMC.sh $1 40
e2=$?

if [ "$e1" -ne "$e2" ]
then

	if [ "$e1" -eq "124" -o "$e2" -eq "124" ]
	then
		echo "TO"
	else
		echo "mismatch blimc $e1 shiftbmc $e2"
	fi
	
else
	echo "match"
fi
