/**********************************************************************************[ProofChecker.cc]
Copyright (c) 2015, All rights reserved, Norbert Manthey
***************************************************************************************************/

#include "proofcheck/ProofChecker.h"

#include "proofcheck/OnlineProofChecker.h"
#include "proofcheck/BackwardChecker.h"

#include <iostream>

using namespace Riss;
using namespace std;

static IntOption  opt_printEvery    ("PROOC-CHECK", "pc-print-every",  "number of clauses until next output is printed during input (0=off)", 0, IntRange(0, INT32_MAX));

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
parsedEmptyClause(false),
addedClauses(0),
lastAddedClauses(0),
lastCpuT(0)
{
  checkClock.start();
  cerr << "c create proof checker with " << threads << " threads, drat: " << checkDrat << endl;
  
  if( !opt_first ) {
    cerr << "c WARNING: full RAT checking not implemented yet, keep default setup" << endl;
    testRATall = false;
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

void ProofChecker::setDRUPproof() {
  checkDrat = false; 
  if( checkBackwards ) backwardChecker->setDRUPproof();
  else {} // nothing to be done for the forward checker after parsing the full proof
}

int ProofChecker::nVars() const
{
  return variables;
}

Var ProofChecker::newVar()
{
  int oldVariables = variables;
  variables ++;
  if( checkBackwards ) backwardChecker->newVar();
  return oldVariables;
}

void ProofChecker::reserveVars(int newVariables)
{
  variables = newVariables;
  if( checkBackwards ) backwardChecker->reserveVars( newVariables );
}

void ProofChecker::setReveiceFormula(bool nextIsFormula)
{
  receiveFormula = nextIsFormula;
}

bool ProofChecker::parsingOk() const {
  return ok;
}

bool ProofChecker::addClause_(vec< Lit >& ps, bool isDelete)
{
  if( ps.size() == 0 ) parsedEmptyClause = true; // memorize that the empty clause has been added 
  
  assert( (!isDelete || !receiveFormula) && "cannot delete clauses within the formula" );

  // we already know that something went wrong
  if( !ok ) return false;
  
  addedClauses++;
  
  if(opt_printEvery != 0 && addedClauses % opt_printEvery == 0 ) {
    double relTime = checkClock.getRunningCpuTime() - lastCpuT ;
    relTime = relTime == 0 ? 0.000001 : relTime;
    cerr << "c [PC] " << checkClock.getRunningCpuTime() << " , " << checkClock.getRunningWallTime() << ": clauses: " << addedClauses << endl 
                      <<" (per sec: global:" << addedClauses / checkClock.getRunningWallTime() << " relative: " << (addedClauses - lastAddedClauses) / relTime << " memory: " << memUsed() << endl;
    lastAddedClauses = addedClauses;
    lastCpuT = checkClock.getRunningCpuTime();
  }
  
  if( !checkBackwards ) {
    if( receiveFormula ) {
      assert( !isDelete && "there should not be delete information inside the proof" );
      forwardChecker->addParsedclause( ps );
    }else {
      if( isDelete ) ok = ok && forwardChecker->removeClause( ps );
      else ok = ok && forwardChecker->addClause( ps );
    }
  } else {
    if( receiveFormula ) ok = ok &&  backwardChecker->addProofClause( ps, false ); // might fail if DRAT is enabled, because than verifying RAT might become unsound (if clauses have been added)
    else ok = ok &&  backwardChecker->addProofClause( ps, true, isDelete );
  }
  return ok;
}

bool ProofChecker::checkClauseDRUP(vec< Lit >& clause, bool add)
{
  assert( !receiveFormula && "clauses of the input formula are not checked" );
  if( receiveFormula ) return true; // clause could obviously be added 
  if( !checkBackwards ) {
    return forwardChecker->addClause( clause, true );
  } else {
    // work on the formula, but do not touch the clauses
    bool ret = backwardChecker->checkClause( clause, true, true ); // only use DRUP, do not build too expensive data structures undo marks afterwards!
    backwardChecker->clearLabels();
    return ret;
  }
}

bool ProofChecker::emptyPresent()
{
  return parsedEmptyClause;
}

bool ProofChecker::verifyProof()
{
  double cupT = checkClock.getCpuTime(), wallT = checkClock.getWallClockTime();
  cerr << "c [PC] verify proof with " << addedClauses << " elements, after " << checkClock.getRunningCpuTime() << " , " << checkClock.getRunningWallTime() << " seconds, memory: " << memUsed() << endl;
  
  bool ret = false;
  if( !checkBackwards ) {
    ret = parsedEmptyClause;   // in forward checking each clause is checked, hence also the first empty clause
  } else {
    if( !parsedEmptyClause ) {  // a proof without an empty clause cannot be valid
      cerr << "c WARNING: should verify proof without parsing an empty clause" << endl;
      ret = false;
    } else {
      ret =  backwardChecker->verifyProof();
    }
  }
  
  cupT = checkClock.getCpuTime() - cupT;
  wallT = checkClock.getWallClockTime() - wallT;
  cerr << "c [PC] proof verification took " << checkClock.getRunningCpuTime() << " , " << checkClock.getRunningWallTime() << " seconds, memory: " << memUsed() << endl;
  return ret;
}

bool ProofChecker::receivedInterupt()
{
  return isInterrupted;
}

void ProofChecker::setVerbosity(int verbosity)
{
  if (forwardChecker != 0) forwardChecker->setVerbosity( verbosity );
}


