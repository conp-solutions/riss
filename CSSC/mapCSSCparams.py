# Norbert Manthey, 2014
# turn parameter representation of SMAC into call string for Riss
#

import sys
import string
import os;

#
# start of the main function
#

# prepare output
finalString=""

for s in sys.argv[1:]:
	# get the parameter without the ',', and split the parameter at the equality-sign
	combo=s[ 0 : len(s)-1 ].split('=')
	# if its a boolean parameter, show it as enabled or disabled (minisat style)
	if combo[1]=="'yes'" :
#		print "-" + combo[0]
		finalString = str(finalString + " -" + combo[0])
	elif combo[1]=="'no'":
#		print "-no-" + combo[0]
		finalString = str(finalString + " -no-" + combo[0] )
	else:
#		print "-" + combo[0] + " " + combo[1]
		finalString = str( finalString + " -" + combo[0] + "=" + combo[1] )
		
	# output the result
print finalString.replace('\'','')
