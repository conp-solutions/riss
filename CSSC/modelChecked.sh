#!/bin/sh

#
#
# call riss with the file and all the parameters
# return exit code by riss, if not 10
# otherwise, return exit code of verify tool
#
# usage ./modelChecked.sh <file> <params>
#

# get file parameter
f=$1
shift

# call riss with remaining parameters
./riss $f /tmp/model_$$ $*
stat=$?

# check exit code
if [ $stat -ne 10 ]
then
	exit $stat
fi

# check model
./verify SAT /tmp/model_$$ $f
vstat=$?

# clean up
rm -f  /tmp/model_$$

# tell outside about exit code
exit $vstat
