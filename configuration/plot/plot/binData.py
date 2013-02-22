

#
# this script takes file at parameter $1 and counts the number of occurrences of pairs in columns $2 and $3. It prints the cumulated
# Output: all the different pairs with their number of occurrence
# Optionally: A third parameter that defines the width of each bucket
#

import sys
import string

if( len(sys.argv) < 4 ):
	print "usage: python binData.py dataFile col1 col2 [width]"
	exit(1)

# extract commandline parameters
fn=str(sys.argv[1])
col1=int( sys.argv[2] ) - 1
col2=int( sys.argv[3] ) - 1
maxCol = col1
if( col2 > maxCol ):
	maxCol = col2

width=0
if( len(sys.argv) == 5 ):
	width=float( sys.argv[4] )

# summary
print "# f:",str(fn)," c1:",str(col1)," c2:",str(col2)," width:",str(width),"   maxCol:",str(maxCol)



d = dict()


f = open(fn)
lines = f.readlines()

count = 0
for line in lines:
	if( count == 0 ):
		count = 1;
		continue;
	sp = string.split(line," ")
	if( len(sp) < maxCol ):
		continue
	if( sp[col1] == "-" or sp[col2] == "-" ):
		continue
	n1 = float(sp[col1])
	n2 = float(sp[col2])
#	print str(n1),"-",str(n2)
	tup = (n1,n2)
	# insert current tuple according to width
	if( width == 0 ):
		if( tup in d ):
			d[tup] = d[tup]+1
		else:
			d[tup] = 1
	else:
		tup = ( int( tup[0] / width ),int( tup[1] / width ) )
		if( tup in d ):
			d[tup] = d[tup]+1
		else:
			d[tup] = 1

# output each tuple with its number
for key in d.iterkeys():
	if( width == 0 ):
	 print key[0], key[1], d[key]
	else:
	 print key[0]*width, key[1]*width, d[key]
	 
