/********************************************************************[BackwardVerificationWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck-src/BackwardVerificationWorker.h"

#include "utils/Options.h"

using namespace Riss;

static IntOption opt_verbose ("BACKWARD-CHECK", "bvw-verbose", "verbosity level of the verification worker", 0, IntRange(0, 8));

BackwardVerificationWorker::BackwardVerificationWorker(bool opt_drat, ClauseAllocator& outer_ca, vec< BackwardChecker::ClauseData >& outer_fullProof, vec< BackwardChecker::ClauseLabel >& outer_label, int outer_formulaClauses, int outer_variables, bool keepOriginalClauses)
:
drat( opt_drat ),
fullDrat( false ),
parallelMode( none ),
sequentialLimit( 0 ),   // initially, there is no limit and the sequential worker
verbose(opt_verbose),   // all details for development
#warning ADD A VERBOSITY OPTION TO THE TOOL
formulaClauses( outer_formulaClauses ),
label( outer_label ),
fullProof( outer_fullProof ),
ca(0),                                 // have an empty allocator
originalClauseData( outer_ca ),        // have a reference to the original clauses
workOnCopy( keepOriginalClauses ),

nonMarkedWatches ( ClauseDataClauseDeleted(ca) ),
markedWatches    ( ClauseDataClauseDeleted(ca) ),
fullWatch        ( ClauseDataClauseDeleted(ca) ),
variables( outer_variables ),

lastPosition(0),
minimalMarkedProofItem( -1 ),
markedQhead(0),
markedUnitHead(0), 
nonMarkedQHead(0), 
nonMarkedUnithead(0),

clausesToBeChecked(0),
maxClausesToBeChecked(0),
num_props(0),
verifiedClauses(0),
ratChecks(0)
{
}


void BackwardVerificationWorker::initialize(int64_t proofPosition, bool duplicateFreeClauses)
{
  if(workOnCopy) originalClauseData.copyTo(ca);
  else originalClauseData.moveTo( ca );
  
  cerr << "c [S-BW-CHK] initialize (copy?" << workOnCopy << "), clause storage elements: " << ca.size() << "(original: " << originalClauseData.size() << ") variables: " << variables << endl;
  
  nonMarkedWatches  .init(mkLit(variables, false)); // get storage for watch lists
  nonMarkedWatches  .init(mkLit(variables, true ));
  
  markedWatches     .init(mkLit(variables, false)); // get storage for watch lists
  markedWatches     .init(mkLit(variables, true));
  
  if( drat ) {
   fullWatch.init(mkLit(variables, false)); // get storage for watch lists
   fullWatch.init(mkLit(variables, true));  // get storage for watch lists
  }
  
  assigns.growTo( variables, l_Undef );
  trail.growTo( variables );  // initial storage allows to use push_ instead of push. will be faster, as there is no capacity check
  trail.clear();              // remove all elements from the trail
  
  if( !duplicateFreeClauses ) {
    vec<char> presentLiterals ( 2*variables );
    for( int i = 0; i < fullProof.size() ; ++ i ) { // check each clause for duplicates
      if( fullProof[i].isDelete() ) continue; // do not look at this type
      if( fullProof[i].getRef() == CRef_Undef ) continue; // jump over the empty clause
      
      Clause& c = ca[ fullProof[i].getRef() ];
      int keptLiterals = 0;
      bool isTautology = false;
      for( int j = 0 ; j < c.size(); ++ j ) {
	if( presentLiterals[ toInt( ~c[j] ) ] != 0 ) {
	  isTautology = true; 
	  for( ; j < c.size(); ++ j ) c[ keptLiterals ++ ] = c[j];
	  break;
	}
	else if ( presentLiterals[ toInt( ~c[j] ) ] == 0 ) {
	  c[ keptLiterals ++ ] = c[j];
	  presentLiterals[ toInt( c[j] ) ] = 1;
	}
      }
      c.shrink( c.size() - keptLiterals );
      for( int i = 0; i < c.size(); ++ i ) {
	presentLiterals[ toInt( c[i] ) ] = 0;
      }
      
      if( isTautology ) c.mark(1); // indicate inside the clause that it has not to be handled
    }
  
  }
  
  proofItemProperties.growTo( fullProof.size() );
  minimalMarkedProofItem = fullProof.size(); // so far no element has been marked
  
  // add all clauses that are relevant to the watches (all clauses that are valid at the latest point in time, the size of the proof + 1 )
  const int currentID = proofPosition;
  for( int i = 0 ; i < fullProof.size(); ++ i ) {
    if( fullProof[i].isValidAt( currentID ) && !fullProof[i].isDelete() ) {
      if( fullProof[i].isEmptyClause() ) { // jump over the empty clause
	if( i + 1 < fullProof.size() && i > formulaClauses ) cerr << "c WARNING: found an empty clause in the middle of the proof [" << i << " / " << fullProof.size() << "]" << endl;
	continue;
      }
      
      enableProofItem( fullProof[i] ); // add corresponding clause to structures
    }
  }
}

void BackwardVerificationWorker::enableProofItem( const BackwardChecker::ClauseData& proofItem ) {
  const Clause& c = ca[ proofItem.getRef() ];
  if( c.mark() != 0 ) return;  // this clause has just been removed due to being a isTautology
      
  assert( c.size() > 0 && "there should not be empty clauses in the proof" );
  if( c.size() == 1 ) {
    if( label[ proofItem.getID() ].isMarked() ) markedUnitClauses.push( BackwardChecker::ClauseData( c[0], proofItem.getID(), proofItem.getValidUntil()  ) );
    else nonMarkedUnitClauses.push( BackwardChecker::ClauseData( c[0], proofItem.getID(), proofItem.getValidUntil()  ) );
  } else {
    if( label[ proofItem.getID() ].isMarked() ) attachClauseMarked( proofItem );
    else attachClauseNonMarked( proofItem );
  }
  if( drat ) { // add the proof item also to the full occurrence list
    assert( !proofItem.isDelete() && "cannot add delete clause to full occurence lists" );
    assert( !proofItem.isEmptyClause() && "cannot add empty clause to full occurence lists" );
    for( int i = 0 ; i < c.size(); ++ i ) {
      fullWatch[ c[i] ].push( proofItem );
    }
  }
}

void BackwardVerificationWorker::release() {
  if( verbose > 2 ) cerr << "c [S-BW-CHK] release (clear?" << workOnCopy << ") before storage elements:" << ca.size() << " original: " << originalClauseData.size() << endl; 
  if (workOnCopy) ca.clear( true );      // free internal data
  else ca.moveTo( originalClauseData );  // move copied data back to other object
  if( verbose > 3 ) cerr << "c [S-BW-CHK] after release storage elements:" << ca.size() << " original: " << originalClauseData.size() << endl; 
}

void BackwardVerificationWorker::attachClauseMarked(const BackwardChecker::ClauseData& cData)
{
  const Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
  markedWatches[~c[0]].push( cData );
  markedWatches[~c[1]].push( cData );
}

void BackwardVerificationWorker::attachClauseNonMarked(const BackwardChecker::ClauseData& cData)
{
  const Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
  nonMarkedWatches[~c[0]].push( cData );
  nonMarkedWatches[~c[1]].push( cData );
}

void BackwardVerificationWorker::uncheckedEnqueue(Lit p)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] enqueue literal " << p << endl;
  assigns[var(p)] = lbool(!sign(p));
  // prefetch watch lists
  __builtin_prefetch( & nonMarkedWatches[p] );
  trail.push_(p);
}


lbool    BackwardVerificationWorker::value (Var x) const   { return assigns[x]; }

lbool    BackwardVerificationWorker::value (Lit p) const   { return assigns[var(p)] ^ sign(p); }

void BackwardVerificationWorker::restart()
{
  for (int c = trail.size()-1; c >= 0; c--) assigns[var(trail[c])] = l_Undef;
  markedQhead = 0; nonMarkedUnithead = 0; nonMarkedQHead = 0; markedUnitHead = 0;
  trail.clear();
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] restart" << endl;
  if( verbose > 4 ) cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
}

void BackwardVerificationWorker::removeBehind( const int& position )
{
  assert( position >= 0 && position < trail.size() && "can only undo to a position that has been seen already" );
  for (int c = trail.size()-1; c >= position; c--) assigns[var(trail[c])] = l_Undef;
  markedQhead = markedQhead < position ? markedQhead : position; 
  nonMarkedUnithead = 0; // to be on the safe side, replay units
  nonMarkedQHead = 0; // there might have been more marked clauses
  markedUnitHead = 0;
  if( position < trail.size() ) trail.shrink( trail.size() - position );
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] reduce trail to position " << position << " new trail: " << trail << endl;
}

BackwardVerificationWorker* BackwardVerificationWorker::splitWork()
{
  assert( parallelMode != none && "beginning with this method we have to use a parallel mode" );
  assert( trail.size() == 0 && "split work only after verification of a clause finished" );
  
  BackwardVerificationWorker* worker = 0;
  
  if( parallelMode == copy ) {
    // create a worker that works on a copy of the clauses - copy data from already modified clause storage
    worker = new BackwardVerificationWorker( drat, ca, fullProof, label, formulaClauses, variables, true );
    // initialize worker, duplicates have already been removed
    worker->initialize(lastPosition, true);
    
    worker->lastPosition = lastPosition;           // continue at the same position
    worker->minimalMarkedProofItem = lastPosition; // so far we did not mark any item
    // share work
    int counter = 0;
    int lastMovedItem = lastPosition;
    assert( minimalMarkedProofItem != -1 && "has to be initialized before" );
    for( int i = lastPosition; i >= minimalMarkedProofItem; -- i ) {
      if( proofItemProperties[i].isMarkedByMe() ) {
	assert( label[i].isMarked() && "item has to be marked globally" );
	if( counter & 1 != 0 ) { // move every second element
	  worker->proofItemProperties[i].markedByMe(); 
	  worker->clausesToBeChecked ++;
	  lastMovedItem = i;
	  proofItemProperties[i].resetMarked();
	  clausesToBeChecked --;
	}
	counter ++;
	
      }
    }
    worker->minimalMarkedProofItem = lastMovedItem;
    
  } else {
    assert( false && "shared parallel mode is not yet implemented" );
  }
  worker->parallelMode = parallelMode; // set the parallel mode
  
  return worker;
}

lbool BackwardVerificationWorker::continueCheck(int64_t untilProofPosition) {
  
  if( verbose > 1 ) cerr << "c [S-BW-CHK] check clauses that have been marked (" << clausesToBeChecked << ") start with position " << lastPosition << endl;
  
  if( verbose > 7 ) {
    cerr << "c [S-BW-CHK] full state: " << endl;
    cerr << "c non-marked-units: "; for( int i = 0 ; i < nonMarkedUnitClauses.size(); ++ i ) { cerr << " " << nonMarkedUnitClauses[i].getLit() ; } cerr << endl;
    cerr << "c marked-units: "; for( int i = 0 ; i < markedUnitClauses.size(); ++ i ) { cerr << " " << markedUnitClauses[i].getLit() ; } cerr << endl; 
    cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
    cerr << "c trail: " << trail << endl;
    cerr << "c BEGIN FULL PROOF" << endl;
    for( int i = 0 ; i < fullProof.size(); ++ i ) {
      cerr << "c item [" << i << "] id: " << fullProof[i].getID() << " valid until " << fullProof[i].getValidUntil() << " byMe: " << proofItemProperties[i].isMarkedByMe() << " marked: " << label[i].isMarked() << " delete: " << fullProof[i].isDelete() << "  clause: ";
      if(fullProof[i].getRef() == CRef_Undef ) cerr << "empty"; else cerr << ca[ fullProof[i].getRef() ] ;
      cerr << endl;
    }
    cerr << "c END FULL PROOF" << endl;
  }
  
  // check the bound that should be used
  const int64_t maxCheckPosition = formulaClauses > untilProofPosition ? formulaClauses : untilProofPosition;

  vec<Lit> dummy;
  if( verbose > 2 ) cerr << "c [S-BW-CHK] check from position " << fullProof.size() -1 << " to " << maxCheckPosition << endl;
  for( ; lastPosition >= maxCheckPosition; -- lastPosition ) {
    
    if( verbose > 4 ) cerr << "c [S-BW-CHK] look at proof position " << lastPosition << " / " << fullProof.size() << " limit: " << maxCheckPosition << endl;
    
    BackwardChecker::ClauseData& proofItem = fullProof[lastPosition];
    maxClausesToBeChecked = maxClausesToBeChecked >= clausesToBeChecked ? maxClausesToBeChecked : clausesToBeChecked; // get maximum
    if( proofItem.isDelete() ) {
      assert( ! label[ lastPosition ].isMarked() && "deletion information cannot be marked" ); // delete items point to the ID of the clause that should be added instead
      if( verbose > 3 ) cerr << "c [S-BW-CHK] enable clause from proof item " << proofItem.getID() << " with clause " << ca[ proofItem.getRef() ] << endl;
      enableProofItem( fullProof[ proofItem.getID() ] );  // activate this item in the data structures (watch lists, or unit list)
    } else if ( proofItemProperties[proofItem.getID()].isMarkedByMe() && label[ proofItem.getID() ].isMarked() && !label[ proofItem.getID()  ].isVerified() ) { // verify only items that have been marked by me
      
      assert( lastPosition == proofItem.getID() && "item to be verified have to have the same ID as their index" );
      
      if( verbose > 3 ) cerr << "c [S-BW-CHK] check clause " << ca[ proofItem.getRef() ] << " at position " << lastPosition << " with id " << proofItem.getID() << endl;
      
      if( ! checkSingleClauseAT( proofItem.getID(), &(ca[ proofItem.getRef() ]), dummy ) ) {
	bool ret = false;
	if( verbose > 2 ) cerr << "c [S-BW-CHK] AT check failed" << endl;
	ratChecks ++;
	if( drat ) ret = checkSingleClauseRAT( (int64_t) fullProof.size(), dummy, &(ca[ proofItem.getRef() ]) ); // check RAT, if drat is enabled and AT failed
	if( ! ret ) {
	  if( drat && verbose > 2 ) cerr << "c [S-BW-CHK] DRAT check failed" << endl;
	  restart(); // clean trail again, reset assignments
	  return l_False;
	}
      }
      restart(); // clean trail again, reset assignments
      verifiedClauses ++;
      assert( clausesToBeChecked > 0 && "there had to be work before" );
      label[ proofItem.getID()  ].setVerified();
      clausesToBeChecked --; // we checked another clause
      
      if( sequentialLimit != 0 && clausesToBeChecked > sequentialLimit ) {
	return l_Undef;
      }
      
      
    } else {
      // nothing to do with unlabeled clauses, they stay in the watch list/full occurrence list until some garbage collection is performed
      if( verbose > 4 ) cerr << "c [S-BW-CHK] jump over non-marked proof item " << proofItem.getID() << "( marked: " << label[ proofItem.getID()  ].isMarked() << " verified: " << label[ proofItem.getID()  ].isVerified() << endl;
    }
    
  }
  restart(); // clean trail again, reset assignments
  
  assert( clausesToBeChecked == 0 && "has to process all elements of the proof" );
  if( verbose > 0 ) {
    cerr << "c still to check: " << clausesToBeChecked 
         << " (max: " << maxClausesToBeChecked 
         << ") propagatedLits: " <<  num_props 
         << " verifiedClauses: " << verifiedClauses 
         << " rat: " << ratChecks << endl;
  }
  
  return l_True;
}

lbool BackwardVerificationWorker::checkClause(vec< Lit >& clause, int64_t untilProofPosition, bool returnAfterInitialCheck)
{
  if( verbose > 1 ) cerr << "c [S-BW-CHK] check clause " << clause << endl;
  
  if( verbose > 4 ) {
    cerr << "c [S-BW-CHK] full state: " << endl;
    cerr << "c non-marked-units: "; for( int i = 0 ; i < nonMarkedUnitClauses.size(); ++ i ) { cerr << " " << nonMarkedUnitClauses[i].getLit() ; } cerr << endl;
    cerr << "c marked-units: "; for( int i = 0 ; i < markedUnitClauses.size(); ++ i ) { cerr << " " << markedUnitClauses[i].getLit() ; } cerr << endl; 
    cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
    cerr << "c trail: " << trail << endl;
    cerr << "c BEGIN FULL PROOF" << endl;
    for( int i = 0 ; i < fullProof.size(); ++ i ) {
      cerr << "c item [" << i << "] id: " << fullProof[i].getID() << " valid until " << fullProof[i].getValidUntil() << " byMe: " << proofItemProperties[i].isMarkedByMe() << " marked: " << label[i].isMarked() << " delete: " << fullProof[i].isDelete() << "  clause: ";
      if(fullProof[i].getRef() == CRef_Undef ) cerr << "empty"; else cerr << ca[ fullProof[i].getRef() ] ;
      cerr << endl;
    }
    cerr << "c END FULL PROOF" << endl;
  }
  
  clausesToBeChecked = 0;
  num_props = 0;
  vec<Lit> dummy;
  if (!checkSingleClauseAT( (int64_t) fullProof.size(), (Clause*)0x0, clause ) ) {
    if( verbose > 2 ) cerr << "c [S-BW-CHK] AT check failed" << endl;
    ratChecks ++;
    bool ret = false;
    if( drat ) ret = checkSingleClauseRAT( (int64_t) fullProof.size(), clause, (Clause*)0x0); // check RAT, if drat is enabled and AT failed
    if( ! ret ) {
      if( drat && verbose > 2 ) cerr << "c [S-BW-CHK] DRAT check failed" << endl;
      restart(); // clean trail again, reset assignments
      return l_False; 
    }
  }
  verifiedClauses ++;
  restart(); // clean trail again, reset assignments
  if( returnAfterInitialCheck ) return l_True;
  
  lastPosition = fullProof.size() - 1;
  
  return continueCheck(untilProofPosition);
  
}

bool BackwardVerificationWorker::markToBeVerified(const int64_t& proofItemID)
{
  if( label[ proofItemID ].isMarked() ) return false; // somebody else might have marked that clause already
  if( verbose > 3 ) cerr << "c [S-BW-CHK] mark clause with ID " << proofItemID << endl;
  label[ proofItemID ].setMarked();                   // TODO: might be made more thread safe
  clausesToBeChecked = proofItemID >= formulaClauses ? clausesToBeChecked + 1 : clausesToBeChecked; // set by ourself and to be checked in the proof
  proofItemProperties[ proofItemID ].markedByMe();  // this item has been marked by me
  minimalMarkedProofItem = ( proofItemID >= formulaClauses && minimalMarkedProofItem > proofItemID ) ? proofItemID : minimalMarkedProofItem; // store minimum (for split work)
  return true;
}

bool BackwardVerificationWorker::markedItem( const int itemID ) {
  assert( itemID > 0 && itemID < proofItemProperties.size() && "item has to stay in limits" );
  proofItemProperties[ itemID ].isMarkedByMe();
}

CRef BackwardVerificationWorker::propagateMarked(const int64_t currentID, bool addUnits)
{
  CRef    confl     = CRef_Undef;
  markedWatches.cleanAll(); 
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate marked ... " << endl;

  // propagate units first!
  int keptUnits = markedUnitHead;
  for( ; markedUnitHead < markedUnitClauses.size() ; ++ markedUnitHead ) { // propagate all known units
    num_props ++;
    const BackwardChecker::ClauseData& unit = markedUnitClauses[ markedUnitHead ];
    if( unit.isValidAt( currentID ) ) {
      markedUnitClauses[ keptUnits ++ ] = markedUnitClauses[ markedUnitHead ]; // keep current element
      const Lit& l = unit.getLit();            // as its a unit clause, we can use the literal
      if( value(l) == l_True ) continue;       // check whether its SAT already
      else if (value(l) == l_False ) {
	for( int i = markedUnitHead + 1; i < markedUnitClauses.size(); ++ i )  // kept the current literal already
	  markedUnitClauses[ keptUnits ++ ] = markedUnitClauses[ i ];          // keep all other elements element
	markedUnitClauses.shrink( markedUnitClauses.size() - keptUnits );      // remove all removed elements
	return 0; // we do not know the exact clause, be we see a conflict
      }
      else uncheckedEnqueue( l );              // if all is good, enqueue the literal
    } else { // element not valid any more, remove it
      assert( currentID < unit.getID() && "a present element can only become invalid if its been removed from the proof (going backwards beyond that element)" );
    }
  }
  markedUnitClauses.shrink( markedUnitClauses.size() - keptUnits ); // remove all removable elements (less elements in the next call)
  
  while( markedQhead < trail.size() ) {
        Lit            p   = trail[markedQhead++];     // 'p' is enqueued fact to propagate.
        vec<BackwardChecker::ClauseData>&  ws  = markedWatches[p];
        BackwardChecker::ClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (BackwardChecker::ClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID ) ) { ++i; continue; } // simply remove this element from the watch list after the loop ended

// re-enable later again!
//             // Try to avoid inspecting the clause:
//             const Lit blocker = i->blocker();
//             if (value(blocker) == l_True){ // keep binary clauses, and clauses where the blocking literal is satisfied
//                 *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");

	    // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            const BackwardChecker::ClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;
            
            if (  // first != blocker &&  // enable later again
	      value(first) == l_True) // satisfied clause
	    {
	      *j++ = w; continue; } // same as goto NextClause;

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
                    markedWatches[~c[1]].push(w);
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!
		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // if( config.opt_printLhbr ) cerr << "c keep clause (" << cr << ")" << c << " in watch list while propagating " << p << endl;
            if ( value(first) == l_False ) {
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                markedQhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            } else {
                uncheckedEnqueue(first);
	    }
        NextClause:;
        }
        ws.shrink(i - j); // remove all duplciate clauses!
  }
  return confl;
}

CRef BackwardVerificationWorker::propagateUntilFirstUnmarkedEnqueueEager(const int64_t currentID)
{
  CRef    confl     = CRef_Undef;
  nonMarkedWatches.cleanAll(); 
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate non-marked with ID " << currentID << endl;

  // propagate units first!
  int keptUnits = nonMarkedUnithead;
  for( ; nonMarkedUnithead < nonMarkedUnitClauses.size() ; ++ nonMarkedUnithead ) { // propagate all known units
    num_props ++;
    const BackwardChecker::ClauseData& unit = nonMarkedUnitClauses[ nonMarkedUnithead ];
    if( unit.isValidAt( currentID ) ) {
      const Lit& l = unit.getLit();            // as its a unit clause, we can use the literal
      if( value(l) == l_True ) {  // check whether its SAT already
	//TODO: handle for parallel version: check whether its already marked and handle it accordingly
	nonMarkedUnitClauses[ keptUnits ++ ] = nonMarkedUnitClauses[ nonMarkedUnithead ];    // keep current element for the future checks
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit satisfied already: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	continue;                                                                         
      } else if (value(l) == l_False ) {
	// add this unit to the list of marked units
	markedUnitClauses.push( unit );
	markToBeVerified( unit.getID() );         // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
// FIXME: could also be done after a clause check finished - here its an quadratic operation
#warning HANDLE THE ABOVE FIXME
	// next time, check the element of the position of the element that has been removed right now
	int continueToCheck = nonMarkedUnithead + 1;
	// memorize where to continue checking next time this method is called 	
	nonMarkedUnithead = keptUnits;
	// repair data structure before leaving the routine
	for(; continueToCheck  < nonMarkedUnitClauses.size(); ++ continueToCheck  )          // do not keep this unit that failed
	  nonMarkedUnitClauses[ keptUnits ++ ] = nonMarkedUnitClauses[ continueToCheck  ];   // keep all other elements element
	nonMarkedUnitClauses.shrink( nonMarkedUnitClauses.size() - keptUnits );              // remove all removed elements
	// use this unit, as it leads to a conflict	
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit resulted in conflict: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	return 0; // we do not know the exact clause, but we saw a conflict
      }
      else {
	// add this unit to the list of marked units
	markedUnitClauses.push( unit );
	markToBeVerified( unit.getID() );         // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
	// add literal to trail
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit can be enqueued: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	uncheckedEnqueue( l );            // if all is good, enqueue the literal
	// do not keep this element, because this clause is not marked
// FIXME: could also be done after a clause check finished - here its an quadratic operation
#warning HANDLE THE ABOVE FIXME
	// next time, check the element of the position of the element that has been removed right now
	int continueToCheck = nonMarkedUnithead + 1;
	// memorize where to continue checking next time this method is called 	
	nonMarkedUnithead = keptUnits;
	// repair data structure before leaving the routine
	for(; continueToCheck  < nonMarkedUnitClauses.size(); ++ continueToCheck  )          // do not keep this unit that failed
	  nonMarkedUnitClauses[ keptUnits ++ ] = nonMarkedUnitClauses[ continueToCheck  ];   // keep all other elements element
	nonMarkedUnitClauses.shrink( nonMarkedUnitClauses.size() - keptUnits );              // remove all removed elements
	// use this unit, as it leads to a conflict	
	return CRef_Undef; // we just enqueued another literal, so everything's fine
      }
    } else { // element not valid any more, remove it
      assert( currentID < unit.getID() && "a present element can only become invalid if its been removed from the proof (going backwards beyond that element)" );
      if( verbose > 4 ) cerr << "c [S-BW-CHK] removed invalid unit: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
    }
  }
  nonMarkedUnitClauses.shrink( nonMarkedUnitClauses.size() - keptUnits ); // remove all removable elements (less elements in the next call)
  
  while( nonMarkedQHead < trail.size() ) {
        const Lit            p   = trail[nonMarkedQHead++];     // 'p' is enqueued fact to propagate.
        if( verbose > 5 ) cerr << "c [S-BW-CHK] propagate literal " << p << endl;
        vec<BackwardChecker::ClauseData>&  ws  = nonMarkedWatches[p];
	
	if( verbose > 6 ) {
	  cerr << "c [S-BW-CHK] watch list for literal " << p << endl;
	  for( int i = 0 ; i < nonMarkedWatches[p].size(); ++ i ) {
	    cerr << "c [" << i << "] ref: " << nonMarkedWatches[p][i].getRef() << " clause: " << ca[ nonMarkedWatches[p][i].getRef() ] << endl;
	  }
	}
	
        BackwardChecker::ClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (BackwardChecker::ClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID ) ) { ++i; continue; }       // simply remove this element from the watch list after the loop ended
	    if( proofItemProperties[ i->getID() ].isMoved() ) { ++i; continue; }  // this clause has been moved to the watch lists for marked clauses previously (with the other watched literal)
	    
	    
// re-enable later again!
//             // Try to avoid inspecting the clause:
//             const Lit blocker = i->blocker();
//             if (value(blocker) == l_True){ // keep binary clauses, and clauses where the blocking literal is satisfied
//                 *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
            Clause&  c = ca[cr];
	    if( verbose > 5 ) cerr << "c [S-BW-CHK] propagate on clause " << c << endl;
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");

	    // If 0th watch is true, then clause is already satisfied.
            const Lit first = c[0];
            const BackwardChecker::ClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;
            
            if (  // first != blocker &&  // enable later again
	      value(first) == l_True) // satisfied clause
	    {
	      *j++ = w; continue; } // same as goto NextClause;

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
                    nonMarkedWatches[~c[1]].push(w);
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!
		
            // Did not find watch -- clause is unit under assignment:
            // *j++ = w; 
            // hence, the clause has to be marked, do not add the clause back to this list!
	    
            if ( value(first) == l_False ) {
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                nonMarkedQHead = trail.size();
            } else {
                uncheckedEnqueue(first);
		nonMarkedQHead --; // in the next call we have to check this literal again // FIXME might result in checking the same list multiple times without any positive effect as the elements in the front are checked again and again
	    }
	    // in either case we found something, so we can abort this method
	    // add this clause to the watch list of marked clauses
	    markedWatches[~c[0]].push(w);
	    markedWatches[~c[1]].push(w);
	    if( verbose > 5 ) cerr << "c [S-BW-CHK] move clause to marked watch lists id: " << w.getID() << " ref: " << w.getRef() << " clause: " << ca[ w.getRef() ] << endl;
	    markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
	    proofItemProperties[ w.getID() ].moved(); // indicate that this clause has been moved
	    
            // Copy the remaining watches:
            while (i < end)
               *j++ = *i++;
	    ws.shrink(i - j); // remove all duplciate clauses!
	    return confl;
	    
        NextClause:;
        }
        ws.shrink(i - j); // remove all duplciate clauses!
  }
  return confl;  
}
