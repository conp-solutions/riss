# Norbert Manthey, 2015
# apply data from a file with the mapping to the full benchmark
#
# python applyMapping.py data.csv file-reverse-mapping.txt
#
# Input:
#  data.csv:                 1st line is headline (ignored). 1st column is name of the formula, all other columns store some data
#  file-reverse-mapping.txt: contains the alias for each formulas (the formula that is used in the uniq list instead of the current formula)
#
# Output:
#  data of data.csv scaled up to all files in the file file-reverse-mapping.txt
#


import sys
import string
import os;
    
#
# start of the main function
#

if len( sys.argv ) < 2:
  print "error: not enough input files given - abort"
  sys.exit(1)

datafile = open(sys.argv[1], "r")
datalines = datafile.readlines()

mapfile = open(sys.argv[2], "r")
maplines = mapfile.readlines()

# print the header, and remove it
print datalines[0].replace('\n', '').replace('\r', '')
datalines = datalines[1:]

# sort elements in files
datalines.sort()
maplines.sort()

# feed data into a dictionary to be able to search faster
dataset = {}
for line in datalines:
	tokenline = line.split( )
	# print "add " + tokenline[0] + " with " + line
	dataset[ tokenline[0] ] = line

# go over all files and print the data	
for line in maplines:
	tokenline = line.split( )
	# get the data (first column is still wrong)
	actualline = dataset[ tokenline[1] ]
	datatokens = actualline.split()
	# print "process line " + str(tokenline) + " with key " + tokenline[1] + " and actualline " + actualline
	
	# construct string to be printed
	path = tokenline[0]	
	for item in datatokens[1:]:
		path = path + " " + item
	
	# print the data line with the other formula name
	print path
	#print ""

