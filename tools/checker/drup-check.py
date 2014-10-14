#!/bin/python

#
# python script to verify proofs
#
# print "usage: python drup-check.py <formula> <proof> "
#


import sys

def remove_negated_literals(c, ass):
    return [l for l in c if -l not in ass]

def not_satisfied(c, ass):
    for l in c:
        if l in ass:
            return False
    return True

def reduct(formula, ass):
    return [remove_negated_literals(c, ass) for c in formula if not_satisfied(c, ass)]

def bcp(formula, ass):
    r = reduct(formula, ass)
    units = [c[0] for c in r if len(c) == 1]
    if len(units) == 0:
        return (r, ass)
    else:
        return bcp(r, ass + units)

def is_rup(formula, clause):
    (r, a) = bcp(formula, [-x for x in clause])
    units = list(set(a))
    for x in units:
        if -x in units:
            return True
    for c in r:
        if len(c)==0:
            return True

    print 'ASS ', units
    return False



#
# Main procedure
#
# Load CNF
#
formula = []

print "usage: python drup-check.py <formula> <proof> "
print "open formula " + sys.argv[1]
with open( sys.argv[1] , 'r') as cnf:
    for l in cnf.readlines():
        if l.startswith('c') or l.startswith('p'):
            continue
        lits = [int(l) for l in l.split(' ')[:-1]]
        lits = sorted( lits )
        formula.append(lits)

# have all clauses in the formula list
print "open proof " + sys.argv[2]
with open(sys.argv[2], 'r') as pfile:
    for l in pfile.readlines():
        if l.startswith('c') :
            continue
        if l.startswith('d') :
          l = l[2:]  # ignore the d
          lits = [int(l) for l in l.split(' ')[:-1]] # get the literals
          lits = sorted(lits) # sort lits
          # remove the clause from the current formula
          try:
            formula.remove( lits )
          except Exception: 
            print 'FAILED (active:', len(formula),') d ', lits
            raise Exception('no drup')
            pass
            
          print 'OK (active:', len(formula),') d ', lits
        else :
        
          lits = [int(l) for l in l.split(' ')[:-1]]          
          if is_rup(formula, lits):
              print 'OK (active:', len(formula),') ', lits
              formula.append( sorted(lits) )      
          else:
              print 'FAILED (active:', len(formula),') ', lits
              raise Exception('no rup')
    # final check:
    if is_rup(formula, []) :
        print "Verified"
	sys.exit(0)
    else:
        print "Empty Clause not verified"
	sys.exit(1)

