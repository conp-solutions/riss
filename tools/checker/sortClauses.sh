#
# for each line of a CNF file, except the first line
#
# treats the 0 special, and adds a 0 at the end of the line again
#
tmp=/tmp/sort-$$

awk ' { if( NR > 1 ) { split( $0, a, " " ); asort( a ); for( i = 1; i <= length(a); i++ ) if( a[i] != 0 ) printf( "%s ", a[i] ); printf( "0\n" ); } else {print $0;} } ' $1 > $tmp
mv $tmp $1
