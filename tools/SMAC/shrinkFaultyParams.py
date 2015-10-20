# Norbert Manthey, 2015
# script to reduce the number of parameters to a program by preserving the exit code
#
# usage: python shringFaultyParams intervaloptionfile faulty-SMAC-ruby-call-with-options-and-parameters
#

import sys
import subprocess
import string
import random
import os;
import shutil;
from subprocess import *
import resource
import time


#
# function that tests the tool call "./tool <input> <parameter>" 
# and tries to reduce the number of parameters by preserving the output of the tool
# tool could also be a verifier script
#
def main( intervalfile, callparams ): 

	pid = os.getpgid(0)

	print "process id: " + str(pid)
	
	print "run " + str(callparams[1:])

	end = len( callparams )
	keepParams = list([])
	
	# remove parameters pair wise, keep parameters, if we do not find "Result for ParamILS: CRASHED" any more
	for n in range(end , 7, -2):
		print str(n) + " vs end: " + str(end);

		print "run   " + str(callparams[1:n] + list(keepParams))
		
#		if n < end:
#			sys.exit(0)
		
		timeS = time.time()
		os.chdir("solvers")
		process = Popen( callparams[1:n] + list(keepParams) , stdout=PIPE, stderr=PIPE )
		(output, err) = process.communicate()
		thisExit = process.wait()
		os.chdir("..")
		timeE = time.time()
		
		print "output:"
		print output
		
		
		# if the tool still crashes, keep the parameters
		if( -1 == output.find("Result for ParamILS: CRASHED") ):
			print "run did not crash"
			keepParams.append( callparams[n] );
			keepParams.append( callparams[n+1] );
			print "new full keep param: " + str( keepParams )
#			print "thisExit: " + str(thisExit) + " without " + callparams[n]
#			print "keep param: " + str( callparams[n-1] )
		else:
			print "full keep param: " + str( keepParams )
			
		print " +++ " # next iteration of for loop
	
#	print "final call:"
#	call = ""
#	for n in range(1 , 11):
#		call = call + " " + callparams[n]
	
	
	# print such that SMAC already sees it as invalid combination
	precall=""
	for s in callparams[1:n] :
		precall = precall + " " + s
	for s in keepParams :
		precall = precall + " " + s
	call = ""
	secondOfPair = False
	pairNumber = 0
	for s in keepParams :
		if( secondOfPair ):
			call = call + "=" + s.replace('\'','')
			pairNumber = pairNumber + 1
		else :
			if ( pairNumber > 0 ):
				call = call + ", " + s[1:]
			else :
				call = call + " " + s[1:]
		secondOfPair = not secondOfPair # switch in each iteration
	print call
	print "in middle of pair: " + str(secondOfPair)
	print "final faulty options: {" + str(call) + " } # reduced faulty configuration"
	print "final tool call: " + precall

  # excludable options
  
	# get options that should not appear in reduced call
	infile = open(intervalfile, "r")
	tmpoptions = infile.readlines()
	intervaloptions = []
	for op in tmpoptions:
		intervaloptions.append( op.rstrip('\r\n') )
	#print "found options that are invalid for being included: " +  str(len(intervaloptions) ) + " first: " + intervaloptions[0] + " last: " + intervaloptions[-1]
	option_set = set(intervaloptions)
	#print "found options (set) that are invalid for being included: " + str( len(intervaloptions) )
  
  # build call once more
	call = ""
	secondOfPair = False
	blockParam = False
	pairNumber = 0
	for s in keepParams :
		if( secondOfPair ):
			if not blockParam : 
				call = call + "=" + s.replace('\'','')
				pairNumber = pairNumber + 1
		else :
			if s[1:] not in option_set:
				#print "option " + s[1:] + " not present"
				blockParam = False
				if ( pairNumber > 0 ):
					call = call + ", " + s[1:]
				else :
					call = call + " " + s[1:]
			else :
				#print "option " + s + " present"
				blockParam = True
		secondOfPair = not secondOfPair # switch in each iteration
	print call
	print "final reduced-excludable options: {" + str(call) + " } # reduced excludable faulty configuration"
	

	
	return 
	
#	
# call to main method	
#
print "called with #param: " + str( len(sys.argv) )
main( sys.argv[1], sys.argv[1:] )
