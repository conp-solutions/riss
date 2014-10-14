#!/bin/sh	

#
#
# remove a clause from a given WCNF file ($1) as long as the command in parameter 3 returns non-zero
# output to file in parameter 2
#

file=$1
outfile=$2
command=$3




tmpfile=/tmp/min_$$.wcnf
tmpfile2=/tmp/min_$$_2.wcnf

ln=`cat $file | wc -l`
ln=$(($ln / 2 ))
cp $file $tmpfile

c=1
while [ 1 ]
do
	 echo "line $c"
   c=$(($c + 1 ))
   
   if [ "$c" -ge "$ln" ]
   then
   	break
   fi
   
   rm -f $tmpfile2
   head -n $c $tmpfile > $tmpfile2 # first n lines
   
   l=`cat $tmpfile | wc -l`
   
   n=$(( $l - $c -1 ))
   
   if [ "$n" -lt "1" ]
   then
   	continue
   fi
   
   echo "ln:$ln c:$c n:$n"
   tail -n $n $tmpfile >> $tmpfile2 # remaining lines except middle 
   
   #cat $tmpfile2   
   
   $command $tmpfile2
   ec=$?
   
   echo "ec: $ec"
   sleep 0.2
   if [ "$ec" -ne "0" ]
   then
   	mv $tmpfile2 $tmpfile
   	echo "new lines: `cat $tmpfile | wc -l `"
   fi
   
done

cp $tmpfile $outfile
