
#
#
# inverts the clause that is given on the commandline
# outputs the negated literals
#
#


import sys,signal,os,subprocess
import string

# PID of the child process
ppid = 0

# handler that takes care of delivering the signal to the child process
def handler(signum, frame):
    print 'Signal handler called with signal', signum, ' child pid: ',ppid
    # kill child
    os.kill(ppid, signum)
    exit(127)




count = 0;
#subprocess p;
strings = list()

# setup signal handler
signal.signal(signal.SIGTERM, handler)


# take the input and output all numbers negated
for arg in sys.argv:

		for s in string.split( arg, ' ' ):
			count = count + 1

			if ( count == 1 ):
				continue;
		
			strings.append( s )
		
#print " arguments: " + str(strings[0:len(strings)-2] )

outfile = strings[-2]
errfile = strings[-1]

#print "out " + outfile + " and err " + errfile

# call process
p = subprocess.Popen(strings, stdout=open(outfile, 'w'), stderr=open(errfile, 'w'))
ppid = p.pid

p.wait()
#print "exit python script normally"

exit( p.returncode )
