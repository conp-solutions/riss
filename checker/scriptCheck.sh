verFile=/tmp/verify.cnf

rm -f $verFile

#

FILE=$2
SCRIPT=$1

#
# 
#

$SCRIPT $FILE > $verFile
status=$?

echo "finish with state= $status" 

# cat /tmp/verify.cnf

if [ "$status" -eq "10" ]
then
        echo "ms sat"
        ./verify SAT $verFile $FILE 
        vstat=$?
        echo "exited with $vstat" 
        exit $vstat
else

        ./lgl $FILE 2> /dev/null > /dev/null
        lstat=$?

        if [ $lstat = 10 ] 
        then
           echo "lgl sat"
           exit 2
        fi
        if [ $lstat = 0 ]
        then
            echo "lgl fail"
            exit 3
        fi

        echo "exited with $status" 
        exit $status
fi

echo "reached end of script without exit" 

