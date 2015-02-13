/******************************************************************************[BackwardChecker.cc]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck-src/BackwardChecker.h"

#include "proofcheck-src/SequentialBackwardWorker.h"

using namespace Riss;

BackwardChecker::BackwardChecker(bool opt_drat, int opt_threads, bool opt_fullRAT)
:
formulaClauses( 0 ),
oneWatch ( ClauseDataDeleted() ), // might fail in compilation depending on which features of the object are used
drat( opt_drat ),
fullRAT( opt_fullRAT ),
threads( opt_threads ),
checkDuplicateLits (1),
checkDuplicateClauses (1),
variables(0),
hasBeenInterupted( false ),
readsFormula( true ),
currentID(0),
sawEmptyClause( false ),
inputMode( true ),
duplicateClauses(0), 
clausesWithDuplicateLiterals(0),
mergedElement(0)
{
  ca.extra_clause_field = drat; // if we do drat verification, we want to remember the first literal in the extra field
}

void BackwardChecker::interupt()
{
  hasBeenInterupted = true;
#warning FORWARD INTERUPT TO VERIFICATION WORKER OBJECTS
}

int BackwardChecker::newVar()
{
  const int v = variables;  
  if( readsFormula ) {
    oneWatch.init(mkLit(variables, false)); // get space in onewatch structure, if we are still parsing
    oneWatch.init(mkLit(variables, true ));
    presentLits.growTo( 2 * variables ); // per literal
  }
  variables ++;
  return v;
}

void BackwardChecker::reserveVars(int newVariables)
{
  variables = newVariables;
  if( readsFormula ) {
    oneWatch.init(mkLit(newVariables, false));
    oneWatch.init(mkLit(newVariables, true ));
    presentLits.growTo( 2 * newVariables ); // per literal
  }
}

bool BackwardChecker::addProofClause(vec< Lit >& ps, bool proofClause, bool isDelete)
{
  // check the state
  if( !inputMode ) {
    cerr << "c WARNING: tried to add clauses after input mode has been deactivated already" << endl;
    return false;
  }
  
  if( !readsFormula && !proofClause ) return false; // if we added proof clauses already, no more formula clauses can be added
  if( readsFormula && proofClause ) formulaClauses = fullProof.size(); // memorize the number of clauses in the formula
  readsFormula = readsFormula && !proofClause;     // memorize that we saw a proof clause
  
  const int64_t id = currentID; // id of the current clause

  if( ps.size() == 0 ) {
    fullProof.push( ClauseData( CRef_Undef, id ) );
    currentID ++;
    sawEmptyClause = true;
    return true;
  }
  
  Lit minLit = ps[0];
  bool hasDuplicateLiterals = false; // stats to be counted in the object
  bool foundDuplicateClause = false; // stats to be counted in the object
  bool isTautology = false;          // stats to be counted in the object
  
  if( checkDuplicateLits == 0 && checkDuplicateClauses == 0 && !isDelete) { // fast routine if not additional checks are enabled
    for( int i = 0 ; i < ps.size(); ++ i ) {
      minLit = ps[i] < minLit ? ps[i] : minLit;
    }
  } else {
    int litsToKeep = 0;
    // make a list of how many literals are present of which type
    for( int i = 0 ; i < ps.size(); ++ i ) {
      minLit = ps[i] < minLit ? ps[i] : minLit;      // select minimum
      assert( presentLits[ toInt( ps[i] ) ] >= 0 && "number of occurrence cannot be negative, used for below comparison" );
      if( presentLits[ toInt( ~ps[i] ) ] != 0 ) isTautology = true;
      if( presentLits[ toInt( ps[i] ) ] != 0 ) {
	hasDuplicateLiterals = true;
	if( checkDuplicateLits != 0 ) {       // optimize most frequent execution path that occurs when duplicates are ignored
	  if( checkDuplicateLits == 1 ) {     // keep duplicates, but warn
	    presentLits[ toInt( ps[i] ) ] ++; // count the duplicate
	    ps[ litsToKeep++ ] = ps[i];       // keep the duplicate literal
	  }
	  // otherwise the literal is implicitely deleted, corresponds to an empty else branch
	}
      } else {
	presentLits[ toInt( ps[i] ) ] = 1;  // count first occurrence
	ps[ litsToKeep++ ] = ps[i];         // keep this literal
      }
    }
    ps.shrink_( ps.size() - litsToKeep );   // remove the literals that occured multiple times
    
    if( hasDuplicateLiterals ) {
      cerr << "c WARNING: clause " << ps << " has/had duplicate literals" << endl;
#warning add a verbosity option to the object, so that this information is only shown in higher verbosities
    }
  }
  
  // scan for duplicate clauses, or the first clause that could be deleted
  int clausePosition = -1;  // position of the clause in the occurrence list of minLit
  if( checkDuplicateClauses != 0 || isDelete ) {
    for( int i = 0 ; i < oneWatch[ minLit ].size(); ++ i ) {
      const Clause& clause = ca[ oneWatch[ minLit ][i].getRef() ];
      if( clause.size() != ps.size() ) continue; // not the same size means no the same clause, avoid more expensive checks
      bool matches = true;
      for( int j = 0; j < clause.size(); ++ j ) {
	const Lit& l = clause[j];
	if( presentLits[ toInt(l) ] == 0 ) { matches = false; break; } // there is a literal, that is not present in the current clause (often enough)
	assert( presentLits[ toInt(l) ] > 0 && "number of occurence cannot be negative" );
	presentLits[ toInt(l) ] --;
      }
      if( matches ) {  // found a clause we are looking for
	clausePosition = i; break;
	foundDuplicateClause = true;
      }
    }
    if( checkDuplicateClauses != 0 ) { // optimize most relevant execution path
      if( checkDuplicateClauses == 1 ) {
	cerr << "c WARNING: clause " << ps << " occurs multiple times in the proof, e.g. at position " << oneWatch[ minLit ][ clausePosition ].getID() << " of the full proof" << endl;
	#warning add a verbosity option to the object, so that this information is only shown in higher verbosities
      }
    }
  }
  
  // clean literal occurrence list again
  if( !checkDuplicateLits > 0 || checkDuplicateClauses > 0 || isDelete) { // TODO: be more precise here, if delete was succesful with duplicate merging, the vector is already cleared
    for( int i = 0 ; i < ps.size(); ++ i ) presentLits[ toInt( ps[i] ) ] = 0; // set all to 0!
  }
  
  // tautologies are ignored, if duplicate literals should be handled properly
  if( isTautology && checkDuplicateLits == 2 ) {
    return true; // tautology is always fine
  }
  
  // handle the data and add current clause to the proof
  if ( isDelete ) {
    // did we find the corresponding clause?
    if( clausePosition == -1 ) {
      cerr << "c WARNING: clause " << ps << " that should be deleted at position " << id << " did not find a maching partner" << endl;
      return false;
    }
    
    bool hasToDelete = true;  // indicate that the current clause has to be delete (might not be the case due to merging clauses)
    // find the corresponding clause, add the deletion information, delete this clause from the current occurrence list (as its not valid any more)
    if( checkDuplicateClauses == 2 ) {
      const int otherID = oneWatch[ minLit ][ clausePosition ].getID();
      clauseCount.growTo( otherID + 1 );
      if( clauseCount[ otherID ] > 0 ) {
	clauseCount[ otherID ] --;
	hasToDelete = false;
      }
    }
    
    if( hasToDelete ) { // either duplicates are not merged, or the last occuring clause has just been removed
      const int otherID = oneWatch[ minLit ][ clausePosition ].getID();
      ClauseData& cdata = fullProof[ otherID ];
      assert( cdata.getID() == otherID && "make sure we are working with the correct object (clause)" );
      assert( cdata.isValidAt( id )  && "until here this clause should be valid" );
      cdata.setInvalidation( id );      // tell the clause that it is no longer valid
      assert( !cdata.isValidAt( id ) && "now the clause should be invalid" );
      oneWatch[ minLit ][ clausePosition ] = oneWatch[ minLit ][ oneWatch[ minLit ].size() - 1 ];
      oneWatch[ minLit ].shrink_(1);  // fast version of shrink (do not call descructor)
      
      // add to proof an element that represents this deletion
      fullProof.push( ClauseData( cdata.getRef(), id ) );
    }
    
  } else {
    if( foundDuplicateClause ) duplicateClauses ++;             // count statistics
    if( hasDuplicateLiterals ) clausesWithDuplicateLiterals ++; // count statistics
    if( !foundDuplicateClause || checkDuplicateClauses != 2 ) { // only add the clause, if we do not merge duplicate clauses
      const CRef cref = ca.alloc( ps );                         // allocate clause
      if( drat ) ca[ cref ].setExtraLiteral( ps [0] );          // memorize first literal of the clause (if DRAT should be checked on the first literal only)
      oneWatch[minLit].push( ClauseData( cref, id ) );          // add clause to structure so that it can be checked
      
      fullProof.push( ClauseData( CRef_Undef, id ) );           // add clause data to the proof (formula)
    } else {
      clauseCount.growTo( oneWatch[ minLit ][ clausePosition ].getID() + 1 ); // make sure the storage for the other ID exists
      clauseCount[ oneWatch[ minLit ][ clausePosition ].getID() + 1 ] ++;     // memorize that this clause has been seen one time more now 
    }
  }
  
  // update ID, if something has been added to the proof
  assert( (currentID == fullProof.size() || currentID == fullProof.size() + 1) && "the change can be at most one element" );
  if( fullProof.size() > currentID ) {
    currentID ++;
    if( currentID == ((1ull << 32) -1 ) ) {
      cerr << "c WARNING: handling very large proof (" << currentID << ")" << endl;
    }
    if( currentID == ((1ull << 48) -2 ) ) {
      cerr << "c WARNING: handling very very large proof (" << currentID << ")" << endl;
    }
  } else {
    mergedElement ++; // count statistics
  }
  

  return true;
}


bool BackwardChecker::checkClause(vec< Lit >& clause, bool drupOnly, bool clearMarks)
{
// #error TO BE IMPLEMENTED
  if( threads > 1 ) {
    cerr << "c WARNING: currently only the sequential algorithm is supported" << endl;
  }
  
  label.growTo( fullProof.size() );
  
  // setup checker, verify, destroy object
#warning have an option that allows to keep the original clauses
  sequentialChecker = new SequentialBackwardWorker( drupOnly ? false : drat, ca, fullProof, label, formulaClauses, variables, false );
  bool ret = sequentialChecker->checkClause( clause );
  delete sequentialChecker;
  
  if( clearMarks ) label.clear( true ); // free resources
  
  return ret;
}



bool BackwardChecker::prepareVerification()
{
  if( !sawEmptyClause ) {
    cerr << "c WARNING: did not parse an empty clause, proof verification will fail" << endl;
    return false;
  }
  inputMode = false;     // we do not expect any more clauses
  oneWatch.clear( true );     // free used resources
  clauseCount.clear( true );  // free used resources
}
