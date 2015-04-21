import sys
import string
import math
import array
import time

# sorts all lines except the first one according to their first column. keeps order of columns with the same name
# returns a file with "sort" prefix


def normalize_enumber( number ):
	#format of number: A,Be+C
	if( number.find(",") == -1 ):
		return number

	# number with "A,Be+C"
	if( number.find("e") != -1 ):
		epos = number.find("e")
		cpos = epos + 2
		kpos = number.find(",")
		A = number[0:kpos]
	#	#print "number: " + number + " with length" + str(len(number))
	#	#print "kpos: " + str(kpos)
	#	#print "epos: " + str(epos)

	#	#print "A: " + A
		B = number[kpos + 1 : epos]
	#	#print "B: " + B
		blen = len(B)
		while number[cpos] == '0':
			cpos = cpos + 1
	#	#print "cpos: " + str(cpos)
		cnumber = number[cpos:len(number)]
		#print "number " + number + " -> Cnum: " + cnumber
		C = int(cnumber) - blen
	#	#print "C: " + str(C)
		nr = A+B
		while C > 0:
			nr = nr+"0"
			C = C - 1
		return nr
	
	# number A,B
	else:
		kpos = number.find(",")
		A = number[0:kpos]
		return A


#program starts here!

#a = float( normalize_enumber( "122,90e+08" ) )
##print a
#sys.exit()


count = 0
for arg in sys.argv:
	count = count + 1

histogrambins = 1000
if (count > 1):
	histogrambins	= int( sys.argv[1] )

print "use " + str(histogrambins) + " bins for histogram"

#how details shoud the given information be presented?

maxsize = 0
maxlifetime = 0
maxtime = 0
memsum = 0
unfreed = 0;

freefile = open('free.dat', 'r')

# holds timestamp of current malloc item
current_time = 0;

item = [];

frees = freefile.readlines()

nr_frees = len( frees )

freedata = []
# convert free data from ascii into binary!
for f in frees:
	ffirstsem = f.find(" ")
	fsecondsem = f.find(" ", ffirstsem + 1 )
	ptr = int( f[ 0: ffirstsem ], 16 )
	freetime = int( f[ ffirstsem + 1: fsecondsem] )
	freedata.append([ptr,freetime])

frees = []
freefile.close()

allocdata = []
allocsfile = open('allocs.dat', 'r')
allocs = allocsfile.readlines()
for f in allocs:
	ffirstsem = f.find(" ")
	fsecondsem = f.find(" ", ffirstsem + 1 )
	fthirdsem = f.find(" ", fsecondsem + 1 )
	ptr = int( f[ 0: ffirstsem ], 16 )
	size = int( f[ ffirstsem + 1: fsecondsem] )
	alloctime = int( f[ fsecondsem + 1: fthirdsem] )	
	allocdata.append([ptr,size,alloctime])

allocs = []
allocsfile.close()

current_free = 0
permille = 1

elements = len( allocdata )

fdata = {}
#create dictionary!
for f in freedata:
	if f[0] in fdata:
		fdata[ f[0] ].append( f[1] )
	else:
		fdata[ f[0] ] = [ f[1] ]

count = 0
for i in freedata:
	fdata[count] = i[0]
	count = count + 1

count = 0
#fdata = array.array( freedata )
starttime = time.clock()
for line in allocdata:
	count = count + 1
	if (count * 100) / elements > permille:
		print str(permille) + "%(" + str( time.clock() - starttime ) + "s)",
		permille = permille + 1
	#find according pointer
	ptr = line[0]

	if line[1] > maxsize :
		maxsize = line[1]
	if maxtime < line[2]:
		maxtime = line[2]
	memsum = memsum + line[1]
		
	# find according free
	if ptr in fdata:
		flist = fdata[ptr]
		ftime = 0
		#find right time of free
		for fl in flist:
			if fl > line[2]:
				ftime = fl
				flist.remove(fl)
				if len(flist) == 0 :
					del fdata[ptr]
				break
		if ftime == 0:
			unfreed = unfreed + 1	
			print " invalid freentry found "
			item.append( [line[1], line[2], -1 ] )			
		else:
			if maxtime < ftime:
				maxtime = ftime
			if maxlifetime < ftime - line[2]:
				maxlifetime = ftime - line[2]
			if ftime - line[2] < 0:
				print "neg life " + str(ptr) + " "
			item.append( [line[1], line[2],ftime - line[2]] );	
	else:
		#nothing found -> lifetime = -1
		unfreed = unfreed + 1	
		item.append( [line[1], line[2], -1 ] )

print ""

#finished matching process
print "maxlifetime: " + str(maxlifetime)
print "sum of mem: " + str(memsum)

# print lifetime dat file
lifetime = []
# init lifetime container
for i in range( 0 , histogrambins + 1 ):
	lifetime.append( 0 )

#linear sorting!
for i in item:
	if i[2] == -1:
		continue
	index = int(math.ceil( ( float(i[2] * float(histogrambins) ) / float(maxlifetime)  )) )
	lifetime[ index ] = lifetime[index] + 1

wf = open("lifetime.dat", 'w')

wf.write( "#object lifetime, scala is linear and has " + str( histogrambins ) + " entries\n" )
wf.write( "# lifetime frequency\n" )


count = 0;

wf.write( "  " + "0" + " " + str( unfreed ) + "\n" )
for st in lifetime:
	count = count + 1
	wf.write( "  " + str( float(count)*float(maxlifetime)/float(histogrambins) ) + " " + str( st ) + "\n" )


# lifetime vs size
wf = open("lifetime_size.dat", 'w')

wf.write( "# lifetime and size, " + str( histogrambins ) + " entries\n" )
wf.write( "# lifetime size\n" )

print "number of items: " + str( len( item ) )

for i in item:
	if i[2] == -1:
		wf.write( "  " + "0" + " " + str(i[0]) + "\n" )
		continue
	wf.write( "  " + str(i[2]) + " " + str(i[0]) + "\n" )	

wf.close()

# lifetime vs size, 3D
msize = 100

print "maxsize " + str(maxsize)
print "maxlife " + str(maxlifetime)
print "mat size " + str(msize)

wf = open("lifetime_size_3d.dat", 'w')

wf.write( "# lifetime and size and occurence, " + str( histogrambins ) + " entries\n" )
wf.write( "# lifetime size occurence\n" )
#create matrix
mat = []
for i in range( 0 , msize + 1 ):
	r = []
	for j in range( 0 , msize + 1):
		r.append(0)
	mat.append( r );

#fill matrix
for i in item:
	t = i[2]
	s = i[0]
	if i[2] == -1:
		t = 0
#	print "old t=" + str(t) + " s=" + str(s)
	t = (t * msize) / maxlifetime
	s = (s * msize) / maxsize
#	print "new nt=" + str(t) + " ns=" + str(s)
	mat[t][s] = mat[t][s] + 1

print "conversion done"
#for i in range( 0 , msize + 1 ):
#	for j in range( 0 , msize + 1 ):
#		print " " + str( mat[i][j] ),
#	print ""


#write file
for i in range( 0 , msize + 1 ):
	for j in range( 0 , msize + 1 ):
#		print "old nt=" + str(i) + " ns=" + str(j)
#		print "new nt=" + str((i*maxlifetime) / msize) + " ns=" + str((j*maxsize) / msize )
		wf.write( " " + str( (i*maxlifetime) / msize) + " " + str( (j*maxsize) / msize ) + " " + str( mat[i][j] ) + "\n" )

wf.close()

# memory vs time
alloctimeline = []
freetimeline = []

for i in range( 0 , histogrambins + 1 ):
	alloctimeline.append( 0 )
	freetimeline.append( 0 )

for i in item:
	index = int( math.ceil( float(i[1]) / float(maxtime) * float(histogrambins) ) )
	alloctimeline[ index ] = alloctimeline[ index ] + i[0]
	
	index = int( math.ceil( float(i[1] + i[2]) / float(maxtime) * float(histogrambins) ) )
	if index > histogrambins :
		index = histogrambins
	freetimeline[ index ] = freetimeline[ index ] + i[0]
	
items = len( item )


wf = open("memory_time.dat", 'w')

wf.write( "# memory (allocs,frees,current) over time, " + str( histogrambins ) + " entries\n" )
wf.write( "# time alloc free current\n" )
wf.write( "  " + str(0) + " " + str( int(alloctimeline[0]) ) + " " + str( 0 ) + " " + str( 0 ) + "\n")

for i in range( 1, len(alloctimeline) ):
	t = float(i) * float(maxtime) / float(histogrambins)
	alloctimeline[i] = alloctimeline[i] + alloctimeline[i-1]
	freetimeline[i] = freetimeline[i] + freetimeline[i-1]
	wf.write( "  " + str(t) + " " + str(alloctimeline[i]) + " " + str(freetimeline[i]) + " " + str( alloctimeline[i] - freetimeline[i-1] ) + "\n" )

t = float(len(alloctimeline)) * float(maxtime) / float(histogrambins)
wf.write( "  " + str(t) + " " + str(0) + " " + str( 0 ) + " " + str( alloctimeline[ len(alloctimeline) -1 ] - freetimeline[ len(alloctimeline) - 1 ] ) + "\n" )

sys.exit(0)
