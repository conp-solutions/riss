/**********************************************************************[SequentialBackwardWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck-src/SequentialBackwardWorker.h"

using namespace Riss;

SequentialBackwardWorker::SequentialBackwardWorker(bool opt_drat, ClauseAllocator& outer_ca, vec< BackwardChecker::ClauseData >& outer_fullProof, vec< BackwardChecker::ClauseLabel >& outer_label, int outer_formulaClauses, int outer_variables, bool keepOriginalClauses)
:
drat( opt_drat ),
formulaClauses( outer_formulaClauses ),
label( outer_label ),
fullProof( outer_fullProof ),
ca(0),                                 // have an empty allocator
originalClauseData( outer_ca ),        // have a reference to the original clauses
workOnCopy( !keepOriginalClauses )

watches ( ClauseDataClauseDeleted(ca) ),
variables( outer_variables )
{
}


void SequentialBackwardWorker::initialize(bool duplicateFreeClauses)
{
  vec<char> presentLiterals;

  if(workOnCopy) originalClauseData.copy(ca);
  else originalClauseData.moveTo( ca );
  
  watches  .init(mkLit(variables, false)); // get storage for watch lists
  watches  .init(mkLit(variables, true ));
  
  assigns.growTo( variables );
  trail.growTo( variables );  // initial storage allows to use push_ instead of push. will be faster, as there is no capacity check
  
  if( !duplicateFreeClauses ) {
    for( int i = fullProof.size(); i >= 0 ; -- i ) { // go backward in the proof
      Clause& c = ca[ fullProof[i].getRef() ];
      
      if( fullProof[i].isDelete() ) { // add the clause 
	
      }
    }
  
  }
}

void SequentialBackwardWorker::release() {
  if (workOnCopy) ca.clear( true );      // free internal data
  else originalClauseData.moveTo( ca );  // move copied data back to other object
}

bool SequentialBackwardWorker::checkClause(vec< Lit >& clause)
{

  return false;
}
