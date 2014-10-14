#!/bin/python
import sys

proof = {}

INITIAL_COUNT = 1

print "c usage: python drat-remove-duplicates.py <formula> <proof> "
if( len(sys.argv) < 3 ):
	sys.exit()


print "c open formula " + sys.argv[1]
with open( sys.argv[1] , 'r') as cnf:
    for l in cnf.readlines():
        if l.startswith('c') or l.startswith('p'):
            continue
        lits = [int(l) for l in l.split(' ')[:-1]]
        lits = tuple(sorted(lits))
        proof[lits] = INITIAL_COUNT

print "c open proof " + sys.argv[2]
with open(sys.argv[2], 'r') as pfile:
    for line in pfile.readlines():
        if line.startswith('c'):
            continue

        if line.startswith('d'):
          lits = [int(l) for l in line[2:].split(' ')[:-1]] # get the literals
          lits = tuple(sorted(lits))

          if lits not in proof.keys():
              raise Exception('The clause was never seen before: ' + str(lits))

          proof[lits] = proof[lits] - 1


          if proof[lits] == 0:
              print line,
          elif proof[lits] < 0:
              raise Exception('Too many deletions: ' + str(lits))

          continue

        lits = [int(l) for l in line.split(' ')[:-1]] 
        lits = tuple(sorted(lits))

        if lits not in proof.keys():
            proof[lits] = 1
            print line,
        else:
            proof[lits] = proof[lits] + 1

        

