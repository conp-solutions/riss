#!/bin/sh

#
# shrink a circuit, so that the model still is wrong
#

circuit=$1
shift

tmp=/tmp/shrink_$$

cp $circuit $tmp


