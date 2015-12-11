# Norbert Manthey, 2014
# script to reduce the number of parameters to a program by preserving the exit code
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
def main( callparams ): 

	pid = os.getpgid(0)

	print "process id: " + str(pid)
	
	print "run " + str(callparams[1:])

	end = len( callparams )

	timeS = time.time()
	process = Popen( callparams[1:end] , stdout=PIPE )
	(output, err) = process.communicate()
	fullExit = process.wait()
	timeE = time.time()
	initialTime = timeE - timeS
	print "finished with (in " + str(initialTime) + " s): " + str( fullExit )
	fastParams = callparams[1:end];
	
	keepParams = list([])
	
	for n in range(end-1 , 2, -1):
		print n;

		print "run" + str(callparams[1:n] + list(keepParams))
		timeS = time.time()
		process = Popen( callparams[1:n] + list(keepParams) , stdout=PIPE, stderr=PIPE )
		(output, err) = process.communicate()
		thisExit = process.wait()
		timeE = time.time()
		if( thisExit != fullExit ):
			print "thisExit: " + str(thisExit) + " without " + callparams[n]
#			print "keep param: " + str( callparams[n-1] )
			keepParams.append( callparams[n] );
#			print "full keep param: " + str( keepParams )
		elif( timeE - timeS < initialTime ):
			fastParams = callparams[1:n+1] + keepParams
			initialTime = timeE - timeS
			print "thisExit: " + str(thisExit) + " ... currently fastest call ( " + str(initialTime) + " s)"

	
	print "final call:"
	call = ""
	for n in range(1 , 3):
		call = call + " " + callparams[n]
	
	call = call + " "
	for s in keepParams :
		call = call + " " + s
	print call


	print "fastestcall:"
	fastCall = ""
	for s in fastParams :
		fastCall = fastCall + " " + s
	print fastCall	

	return 
	
#	
# call to main method	
#
print "called with #param: " + str( len(sys.argv) )

main( sys.argv );
