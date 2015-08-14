#!/bin/bash
#
# extract top value from a WCNF file, compare it against the sum of all soft weights
# (assume the file to be WCNF with top value)
#

file=$1

# extract top value
top=$(awk '/p wcnf/ {print $5}' $file);

# print top value
echo "top value: $top"

# sum all soft clauses (clauses with a weight greater equal top are ignored)
sum=$(awk -v top=$top '{ if ( $1 >= top ) {sum+=$1}; } END {print sum}' $file)

echo "sum value: $sum"

if [ "$sum" -ge "$top" ]
then
	echo "inconsistent WCNF file: $1"
        exit 1
else
	exit 0
fi
