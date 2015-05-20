#!/bin/bash 

params="-quiet -printDec=3 -rem-debug=3 -learn-debug -verb=0"

#
# run the  solvers
#
./riss-master $params $1 2> /tmp/err_$$  | grep -v "time" | grep -v "riss" | grep -v "Ratios" | grep -v "cpu" | grep -v "sec" > /tmp/out_$$

./riss  $params $1 2> /tmp/err2_$$ | grep -v "time" | grep -v "riss" | grep -v "Ratios" | grep -v "cpu" | grep -v "sec" > /tmp/out2_$$

# stderr
diff /tmp/err_$$ /tmp/err2_$$ > /dev/null
ec2=$?

echo ""
echo "STDERR1"
echo ""
#cat /tmp/err_$$

echo ""
echo "STDERR2"
echo ""
#cat /tmp/err2_$$

echo ""
echo ""
echo ""

# stdout
diff /tmp/out_$$ /tmp/out2_$$ > /dev/null
ec1=$?

# stderr missmatch
if [ "$ec2" == "1" ]
then
  # analyze manually
  	meld /tmp/err_$$ /tmp/err2_$$
	exit 2
fi

# stdout missmatch
if [ "$ec1" == "1" ]
then
	exit 1
fi


exit 0
