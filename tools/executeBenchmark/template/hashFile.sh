#
# print hash for given file $1
#


PARAMS=`cat params.txt`
if [ "$PARAMS" == "NOPARAM" ]
then
    PARAMS=""
fi

KEY=`./getHashKey.sh "$1" "$PARAMS"`

echo $KEY

