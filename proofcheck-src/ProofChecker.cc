/**********************************************************************************[ProofChecker.cc]
Copyright (c) 2015, All rights reserved, Norbert Manthey
***************************************************************************************************/

#include "proofcheck-src/ProofChecker.h"

#include "proofcheck-src/OnlineProofChecker.h"
#include "proofcheck-src/BackwardChecker.h"

#include <iostream>

using namespace Riss;
using namespace std;

ProofChecker::ProofChecker(bool opt_drat, bool opt_backward, int opt_threads, bool opt_first)
:
checkDrat( opt_drat ), 
checkBackwards( opt_backward ),
testRATall( !opt_first ), 
threads( opt_threads ),
isInterrupted( false ),
variables(0),
receiveFormula(true),
forwardChecker(0),
backwardChecker(0),
ok(true),
parsedEmptyClause(false)
{
  
  if( opt_backward || threads > 1 || !opt_first ) {
    cerr << "c WARNING: backward checking, parallel checking, or full RAT checking not implemented yet, keep default setup" << endl;

    checkBackwards = false;
    testRATall = false;
    threads = 1;
  }

  if( checkBackwards ) {
    backwardChecker = new BackwardChecker( checkDrat, threads, testRATall );
  } else {
    forwardChecker = new OnlineProofChecker( checkDrat ? dratProof : drupProof );
  }
}

ProofChecker::~ProofChecker()
{
  if( forwardChecker != 0 )  delete forwardChecker;
  if( backwardChecker != 0 ) delete backwardChecker;
}

void ProofChecker::interupt()
{
  isInterrupted = true;
  // no need to interupt the forward checker
  if( backwardChecker != 0 ) backwardChecker->interupt();
}

int ProofChecker::nVars() const
{
  return variables;
}

Var ProofChecker::newVar()
{
  int oldVariables = variables;
  variables ++;
  backwardChecker->newVar();
  // TODO: adapt data structures
  return oldVariables;
}

void ProofChecker::reserveVars(int newVariables)
{
  variables = newVariables;
  // TODO: adapt data structures in a nice way (resize instead of adding one by one)
  backwardChecker->reserveVars( newVariables );
}

void ProofChecker::setReveiceFormula(bool nextIsFormula)
{
  receiveFormula = nextIsFormula;
}

bool ProofChecker::addClause_(vec< Lit >& ps, bool isDelete)
{
  if( ps.size() == 0 ) parsedEmptyClause = true; // memorize that the empty clause has been added 
  
  assert( (!isDelete || !receiveFormula) && "cannot delete clauses within the formula" );
  
  if( !checkBackwards ) {
    if( receiveFormula ) forwardChecker->addParsedclause( ps );
    else return forwardChecker->addClause( ps );
  } else {
    if( receiveFormula ) return backwardChecker->addProofClause( ps, false ); // might fail if DRAT is enabled, because than verifying RAT might become unsound (if clauses have been added)
    return backwardChecker->addProofClause( ps, true, isDelete );
    return true;
  }
  return true;
}

bool ProofChecker::checkClause(vec< Lit >& clause, bool add)
{
  assert( !receiveFormula && "clauses of the input formula are not checked" );
  if( receiveFormula ) return true; // clause could obviously be added 
  if( !checkBackwards ) {
    return forwardChecker->addClause( clause, true );
  } else {
    return backwardChecker->checkClause( clause, true, true ); // only use DRUP, do not build too expensive data structures undo marks afterwards!
  }
}

bool ProofChecker::emptyPresent()
{
  return parsedEmptyClause;
}

bool ProofChecker::verifyProof()
{
  if( !checkBackwards ) {
    return parsedEmptyClause;   // in forward checking each clause is checked, hence also the first empty clause
  } else {
    if( !parsedEmptyClause ) {  // a proof without an empty clause cannot be valid
      cerr << "c WARNING: should verify proof without parsing an empty clause" << endl;
      return false;
    }
    if( backwardChecker->prepareVerification() ) { // tell the checker that there will not be any more clauses coming
      return false;
    }
    vec<Lit> dummy; // will not allocate memory, so it's ok to be used as a temporal object
    return backwardChecker->checkClause( dummy );
  }
}

bool ProofChecker::receivedInterupt()
{
  return isInterrupted;
}

void ProofChecker::setVerbosity(int verbosity)
{
  if (forwardChecker != 0) forwardChecker->setVerbosity( verbosity );
}


