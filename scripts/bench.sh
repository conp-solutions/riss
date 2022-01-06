#!/bin/bash

SCRIPT_DIR="$(dirname $0)"

DIRECTORY=$1

FILES=$(find $DIRECTORY -type f -iname "*.cnf")

if test ! -d "benchout";
then
    mkdir benchout
fi

if test ! -d "benchlog";
then
    mkdir benchlog
fi

for FILE in $FILES
do
    echo "Processing $FILE..."
    OUTPUTFILE=$(basename $FILE)
    ./coprocessor-for-modelcounting.sh -o benchout/${OUTPUTFILE} -F -p -backbone $FILE > benchlog/${OUTPUTFILE}log 2>&1
done

echo DONE
