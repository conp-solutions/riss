#
#
# receive a file of run times codes ('-' is unsolved), calculate the unique solver contributions
# second parameter gives the time out to consider
# also give number of solved instances with the timeout for all .csv files in the directory
#

#
# calculate unique solver contributions
#
tmp=/tmp/sum_$$

echo "unique configuration contributions with timeout $2"
awk -v to=$2 '{ 
			if(NR == 1) {namesNr = NF; for(i=2; i <= NF; i++) names[i]=$i;} 
			else { 
				hit=-1; 
				for( i=2; i <= NF; i++) {
					if( ($i != "-" && $i <= to )) {
						if( hit == -1 ) hit = i; 
						else hit = -2;
					} 
				}
				if( hit > 0 ) usc[hit] ++; 
			}
		} 
		END { for(i=2; i <= namesNr; i++) print names[i], usc[i];  }
		' $1 > /tmp/sum_$$

# show sorted result
sort -k 2 -g $tmp

echo ""
echo ""		
echo "solved instances: "
rm -f $tmp

for f in *.csv ; 
do 
	bn=`basename $f .csv `
	
#	echo "$f with $bn"

	solved=`cat $f | awk -v to=$2 '{ if( $2 == "ok" && ($3 == "10" || $3 == "20" ) && $4 < to ) print $0;}' |  wc -l`;
	echo "$bn $solved" >> $tmp
done

# show sorted result
sort -k 2 -g $tmp

# delete temporary file
rm -f $tmp
	
		
