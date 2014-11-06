#
# receive a file of exit codes (10 = sat, 20 = unsat), calculate the unique solver contributions
#
# store result in $1
#

#
# calculate unique solver contributions
#
tmp=/tmp/sum_$$

echo "unique configuration contributions"
awk '{ 
			if(NR == 1) {namesNr = NF; for(i=2; i <= NF; i++) names[i]=$i;} 
			else { 
				hit=-1; 
				for( i=2; i <= NF; i++) {
					if( ($i == 10 || $i == 20)) {
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

	solved=`cat $f | awk '{ if( $2 == "ok" && ($3 == "10" || $3 == "20" ) && $4 < 7200 ) print $0;}' |  wc -l`;
	echo "$bn $solved" >> $tmp
done

# show sorted result
sort -k 2 -g $tmp

# delete temporary file
rm -f $tmp
	
		
