# Norbert Manthey, 2014
# turn parameter representation of SpyBug into call string for Riss
#

import sys
import string
import os;

#
# start of the main function
#

# prepare output
finalString=""

#combine each pair of parameters
combinedParams = []
first = True
param = ""
for s in sys.argv[1:]:
	if( first ):
		param = s
	else:
		param = param + "=" + s
		combinedParams.append( param )
	first = not first
  
# run over pairs as in other file
for s in combinedParams:
	# get the parameter without the ',', and split the parameter at the equality-sign (usually, there is a comma, except after the last parameter)
	combo=s[ 0 : len(s)-1 ].split('=')
	if s[-1] != ',':
		combo=s[ 0 : len(s) ].split('=')
	# if its a boolean parameter, show it as enabled or disabled (minisat style)
	if combo[1]=="'yes'" or combo[1]=="'true'" or combo[1]=="yes" or combo[1]=="true":
#		print "-" + combo[0]
		finalString = str(finalString + " " + combo[0])
	elif combo[1]=="'no'" or combo[1]=="'false'" or combo[1]=="no" or combo[1]=="false":
#		print "-no-" + combo[0]
		finalString = str(finalString + " -no" + combo[0] )
	else:
#		print "-" + combo[0] + " " + combo[1]
		finalString = str( finalString + " " + combo[0] + "=" + combo[1] )
		

	# output the result
print finalString.replace('\'','')
