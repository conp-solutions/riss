/********************************************************************[BackwardVerificationWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck/BackwardVerificationWorker.h"

#include "riss/utils/Options.h"

using namespace Riss;
using namespace std;

static IntOption  opt_verbose      ("BACKWARD-CHECK", "bvw-verbose", "verbosity level of the verification worker", 0, IntRange(0, 9));
static BoolOption opt_lazy         ("BACKWARD-CHECK", "bvw-lazy",    "lazy clause removal (faster,more space)", true);
static BoolOption opt_analyze      ("BACKWARD-CHECK", "bvw-analyze", "use conflict analysis to mark relevant clauses", true);
static BoolOption opt_dependencies ("BACKWARD-CHECK", "bvw-trace",   "trace and store clause dependencies (memory expensive)", false);
static BoolOption opt_preUnit      ("BACKWARD-CHECK", "bvw-unit",    "re-use conseqeunces of active unit clauses", true);
static BoolOption opt_oldestReason ("BACKWARD-CHECK", "bvw-oreason", "try to mark clauses that are at the beginning of the proof", false);
static BoolOption opt_preferFormula("BACKWARD-CHECK", "bvw-pformula", "try to mark clauses that are at the beginning of the proof", false);

BackwardVerificationWorker::BackwardVerificationWorker(bool opt_drat, ClauseAllocator& outer_ca, vec<ProofClauseData>& outer_fullProof,
                                                       vec<ProofClauseLabel>& outer_label, vec<vec<int64_t>>& outer_dependencies,
                                                       int outer_formulaClauses, int outer_variables, bool keepOriginalClauses)
    :
    drat(opt_drat)
    , fullDrat(false)
    , parallelMode(none)
    , lazyRemoval(opt_lazy)
    , analyzeMark(opt_analyze)
    , sequentialLimit(0)     // initially, there is no limit and the sequential worker
    , verbose(opt_verbose)   // all details for development
    , formulaClauses(outer_formulaClauses)
    , label(outer_label)
    , fullProof(outer_fullProof)
    , dependencies(outer_dependencies)
    , ca(0)                               // have an empty allocator
    , originalClauseData(outer_ca)        // have a reference to the original clauses
    , workOnCopy(keepOriginalClauses)

    , markedWatches    (ClauseDataClauseDeleted(ca) )
    , fullWatch        (ClauseDataClauseDeleted(ca) )
    , variables(outer_variables)

    , lastPosition(0)
    , minimalMarkedProofItem( -1 )
    , markedQhead(0)
    , markedUnitHead(0)
    , nonMarkedQHead(0)
    , nonMarkedUnithead(0)

    , unitTrailSize(0)
    , activeUnits(0)
    , rerunPrePropagation(false)
    , clearUnitTrail(false)
    , preUnitConflict(-1)

    , lastUnmarkedLiteral(lit_Undef)
    , lastUnmarkedPosition(0)

    , interuptLock(0) // usually, there is no interrupt

    , clausesToBeChecked(0)
    , maxClausesToBeChecked(0)
    , num_props(0)
    , verifiedClauses(0)
    , ratChecks(0)
    , usedOthersMark(0)
    , checkedByOtherWorker(0)
    , resolutions(0)
    , removedLiterals(0)
    , redundantPropagations(0)
    , maxSpace(0)
{
}


void BackwardVerificationWorker::initialize(int64_t proofPosition, bool duplicateFreeClauses)
{
  if(workOnCopy) originalClauseData.copyTo(ca);
  else originalClauseData.moveTo( ca );
  
  if( verbose > 0 ) cerr << "c [S-BW-CHK] initialize (copy?" << workOnCopy << "), clause storage elements: " << ca.size() << "(original: " << originalClauseData.size() << ") variables: " << variables << endl;
  
  
  markedWatches     .init(mkLit(variables, false)); // get storage for watch lists
  markedWatches     .init(mkLit(variables, true));
  
  reasonID.growTo( variables, -1 ); // initially all variables have no reason
  
  if( analyzeMark ) {
    seen.growTo( variables, 0 );      // initially, no variables have been used during resolution
  }
  
  if( drat ) {
   fullWatch.init(mkLit(variables, false)); // get storage for watch lists
   fullWatch.init(mkLit(variables, true));  // get storage for watch lists
  }
  
  assigns.growTo( variables, l_Undef );
  trailPos.growTo( variables, -1 );
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
  if( parallelMode == BackwardVerificationWorker::shared ) watchedXORLiterals.growTo( fullProof.size() );
  const int currentID = proofPosition;
  for( int i = 0 ; i < fullProof.size(); ++ i ) {
    if( fullProof[i].isValidAt( currentID ) && !fullProof[i].isDelete() ) {
      if( fullProof[i].isEmptyClause() ) { // jump over the empty clause
	if( i + 1 < fullProof.size() && i > formulaClauses ) cerr << "c WARNING: found an empty clause in the middle of the proof [" << i << " / " << fullProof.size() << "]" << endl;
	continue;
      }
      
      enableProofItem( fullProof[i] ); // add corresponding clause to structures
      if( parallelMode == BackwardVerificationWorker::shared ) watchedXORLiterals[i].init( ca[ fullProof[i].getRef() ][0], ca[ fullProof[i].getRef() ][1] );
    }
  }
  
  if( opt_preUnit ) {
    // get everything clean
    rerunPrePropagation = true;
    clearUnitTrail=true;
    restart(true);
  }
}

int64_t BackwardVerificationWorker::prePropagateUnits( const int64_t currentID, bool performRestart)
{
  if( performRestart ) {
    if( verbose > 4 ) cerr << "c perform full prePropagation restart" << endl;
    assert( trail.size() == unitTrailSize && "should not contain more literals already" );
    unitTrailSize = 0; // remove all current units
    restart();
    assert( trail.size() == 0 && markedQhead == 0 && nonMarkedQHead == 0 && "nothing propagated yet" );
    clearUnitTrail = false;   // cleared trail right now
    preUnitConflict = -1;     // there cannot be a conflict
  }
  
  nonMarkedQHead = 0; markedUnitHead = 0; // take care of unit heads
  
  if( preUnitConflict != -1 ) return preUnitConflict; // this conflict must still be present, as we did not change anything in the active unts and related reason clauses
  
  int64_t confl = -1; // initially, there is no conflict
  do {
    // propagate on all marked clauses we already collected, includes unit clauses
    confl = propagateMarked( currentID, analyzeMark );
    assert( (confl != -1 || markedQhead == trail.size()) && "visitted all literals during propagation" );
    assert( (confl != -1 || markedUnitHead >= markedUnitClauses.size()) && "visitted all marked unit clauses" );
    // if we found a conflict, stop
    if( confl != -1 ) break;
    // end propagate on marked clauses
    
    // if there is not yet a conflict, enqueue the first non-marked unit clause
    // will move clauses from non-marked to marked, but this is ok, as we will backtrack after this call has finished
    confl = propagateUntilFirstUnmarkedEnqueueEager( currentID, analyzeMark );
    // if we found a conflict, stop
    if( confl != -1 ) break;
    
  } while ( nonMarkedQHead < trail.size() || nonMarkedUnithead < nonMarkedUnitClauses.size() || markedQhead < trail.size() ); // saw all clauses that might be interesting 
  
  // mark units and memorize size of implied trail
  unitTrailSize = trail.size(); // store the size of the trail after propagating unit clauses
  
  rerunPrePropagation = false; // run prepropagation right now
  
  preUnitConflict = confl;
  return confl;
}


void BackwardVerificationWorker::enableProofItem( const ProofClauseData& proofItem ) {
  const Clause& c = ca[ proofItem.getRef() ];
  if( c.mark() != 0 ) return;  // this clause has just been removed due to being a isTautology
      
  assert( c.size() > 0 && "there should not be empty clauses in the proof" );
  if( c.size() == 1 ) {
    if( label[ proofItem.getID() ].isMarked() ) {
      if( ! proofItemProperties[ proofItem.getID() ].isMoved() ) proofItemProperties[ proofItem.getID() ].moved(); // make sure the clause is marked as moved
      markedUnitClauses.push( ProofClauseData( c[0], proofItem.getID(), proofItem.getValidUntil()  ) );
    }
    else nonMarkedUnitClauses.push( ProofClauseData( c[0], proofItem.getID(), proofItem.getValidUntil()  ) );
    
    // handle unit clauses specially
    if( opt_preUnit ) {
      activeUnits ++;              // count active units
      if( verbose > 4 ) cerr << "c activate rerunning prepropagation due to removing unit clause" << endl;
      rerunPrePropagation = true;  // memorize that we have to update the unit conseqeunces and related information (run prePropagation )
    }
    
  } else {
    if( label[ proofItem.getID() ].isMarked() 
      || (opt_preferFormula && proofItem.getID() < formulaClauses) // the clause is in the formula, and hence should be used directly 
    ) {
      if( ! proofItemProperties[ proofItem.getID() ].isMoved() ) proofItemProperties[ proofItem.getID() ].moved(); // make sure the clause is marked as moved
      attachClauseMarked( proofItem );
    }
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

void BackwardVerificationWorker::disableProofItem(ProofClauseData& proofItem)
{
  if( verbose > 2 ) cerr << "c [S-BW-CHK] disable proof item: " << proofItem.getID() << endl; 
  
  if( opt_preUnit && !proofItem.isEmptyClause()  ){ // if a unit has been removed, or a corresponding reason, run prepropagation again (but lazily, to avoid rerunning when removing multiple units)
    const Clause& c = ca[ proofItem.getRef() ];
    if( c.size() == 1 || 
       proofItem.getID() == reasonID[ var(c[0]) ] ||
       proofItem.getID() == reasonID[ var(c[1]) ] ) {
      assert( trail.size() == unitTrailSize && "for the above check there should not be more literals" );
      if( verbose > 4 ) cerr << "c enable running prepropagation due to removing clause implied by active units" << endl;
      rerunPrePropagation = true; // update prepropagation information
      clearUnitTrail = true;      // start from "scratch" with propagation of present unit clauses
      activeUnits = c.size() == 1 ? activeUnits - 1 : activeUnits;   
    }
  }
  
      if( ! lazyRemoval && !proofItem.isEmptyClause()  ) {
	if( parallelMode != shared ) { // eager removal does not work together with shared data structures
	  const Clause& c = ca[ proofItem.getRef() ];
	   if( verbose > 6 ) cerr << "c [S-BW-CHK] remove clause " << c << " eagerly from data structures" << endl;
	  if( c.size() == 1 ) { // handle unit clauses specially
	    for( int e = 0 ; e < nonMarkedUnitClauses.size() ; ++ e ) {
	      if ( nonMarkedUnitClauses[e].getID() == proofItem.getID() ) {
		nonMarkedUnitClauses[e] = nonMarkedUnitClauses[ nonMarkedUnitClauses.size() - 1 ];
		nonMarkedUnitClauses.shrink_(1);
		break;
	      }
	    }
	    if( label[ proofItem.getID() ].isMarked() ) {
	      for( int e = 0 ; e < markedUnitClauses.size() ; ++ e ) {
		if ( markedUnitClauses[e].getID() == proofItem.getID() ) {
		  markedUnitClauses[e] = markedUnitClauses[ markedUnitClauses.size() - 1 ];
		  markedUnitClauses.shrink_(1);
		  break;
		}
	      }
	    }
	  } else { // remove larger clause from the watch lists
	    int found = 0;
		OccLists<Lit, vec<ProofClauseData>, ClauseDataClauseDeleted>& watchList = markedWatches;
		for( int p = 0; p < 2; ++ p ) {
		  vec<ProofClauseData> & wlist = watchList[ ~c[p] ];
		  for( int e = 0 ; e < wlist.size(); ++ e ) {
		    if( wlist[e].getID() == proofItem.getID() ) {
		      wlist[e] = wlist[ wlist.size() -1 ];
		      wlist.shrink_(1);
		      found ++;
		      if( verbose > 8 ) cerr << "c [S-BW-CHK] found " << c << " in list for " << ~c[p] << " ( p: " << p << ")" << endl;
		      break;
		    }
		  }
		  for( int a = 0; a < wlist.size(); ++ a ) for( int b = a+1; b < wlist.size(); ++ b ) assert( wlist[a].getID() != wlist[b].getID() && "do not have the same element" );
		}

	  }
	  
	}
	
	if( drat ) { // remove clause from full watch lists
	  const Clause& c = ca[ proofItem.getRef() ];
	  for( int p = 0 ; p < c.size(); ++ p ) {
	    bool found = false;
	    for( int e = 0 ; e < fullWatch[ c[p] ].size(); ++ e ) { // found the element, replace it with the last element of the list
	      if( fullWatch[ c[p] ][e].getID() == proofItem.getID() ) {
		fullWatch[ c[p] ][e] = fullWatch[ c[p] ][ fullWatch[ c[p] ].size() - 1 ];
		fullWatch[ c[p] ].shrink_(1);
		found = true;
		break;
	      }
	    }
	    assert( found && "clause had to be in the list before" );
	  }
	}
      }
}

void BackwardVerificationWorker::release() {
  if( verbose > 2 ) cerr << "c [S-BW-CHK] release (clear?" << workOnCopy << ") before storage elements:" << ca.size() << " original: " << originalClauseData.size() << endl; 
  if (workOnCopy) ca.clear( true );      // free internal data
  else ca.moveTo( originalClauseData );  // move copied data back to other object
  if( verbose > 3 ) cerr << "c [S-BW-CHK] after release storage elements:" << ca.size() << " original: " << originalClauseData.size() << endl; 
}

void BackwardVerificationWorker::attachClauseMarked(const ProofClauseData& cData)
{
  Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
  
  if( opt_preUnit ) prepareClauseForAttach( c );
  
  if( verbose > 7 ) cerr << "c [S-BW-CHK] add clause with id: " << cData.getID() << " to marked watch lists" << endl;
  markedWatches[~c[0]].push( cData );
  markedWatches[~c[1]].push( cData );
  assert( proofItemProperties[ cData.getID() ].isMoved() && "has to be set as moved" );
  assert( ( label[ cData.getID() ].isMarked() || cData.getID() < formulaClauses ) && "has to be verified or member of formula" );
}

void BackwardVerificationWorker::attachClauseNonMarked(const ProofClauseData& cData)
{
  Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
  
  if( opt_preUnit ) prepareClauseForAttach( c );
  
  if( verbose > 7 ) cerr << "c [S-BW-CHK] add clause with id: " << cData.getID() << " to non-marked watch lists" << endl;
  assert( !proofItemProperties[ cData.getID() ].isMoved() && "has to be set as non-moved" );
  markedWatches[~c[0]].push( cData );
  markedWatches[~c[1]].push( cData );
}

void BackwardVerificationWorker::prepareClauseForAttach( Clause& c ){
  assert( c.size() > 1 && "no empty or unit clauses ");
  
  // analyze the current clause
  int satLits = 0, unsatLits = 0, undefLits = 0;
  
  // check whether there are sufficiently many sat/undef literals
  for( int i = 0 ; i < c.size(); ++ i ) {
    const lbool truthvalue = value( c[i] );
    if( truthvalue != l_False ) {
      Lit tmp = c[undefLits+satLits]; c[undefLits+satLits] = c[i]; c[i] = tmp;
      undefLits = (truthvalue == l_Undef) ? undefLits + 1 : undefLits;
      satLits = (truthvalue == l_True) ? satLits + 1 : satLits;
      if( satLits + undefLits > 1 ) return ; // stop if we found enough literals such that the clause can be watched arbitrarily
    } else 
      unsatLits ++;
  }
  
  assert( unsatLits > 0 && "there has to be at least one falsified literal" );
  assert( unitTrailSize == trail.size() && "check makes sense only for pre units" ); 
  
  if( verbose > 4 ) cerr << "c enable running prepropagation due to adding clause whose reduct is smaller than binary" << endl;
  
  if( opt_preUnit ) {
    rerunPrePropagation = true;  // memorize that we have to update the unit conseqeunces and related information (run prePropagation )
    clearUnitTrail = true;       // use this clause nicely in the watch lists
  }
  
  return;
}

void BackwardVerificationWorker::uncheckedEnqueue(Lit p, int useReasonID)
{
  if( verbose > 3 ) {
    cerr << "c [S-BW-CHK] enqueue literal " << p << " with reason " << useReasonID << endl;
    if( useReasonID != -1 ) cerr << "c used reason clause (" << ca[ fullProof[useReasonID].getRef() ] << " )" << endl;
  }
  assigns[var(p)] = lbool(!sign(p));
  // prefetch watch lists
  __builtin_prefetch( &    markedWatches[p] );
  trailPos[var(p)] = trail.size(); // set position in trail
  trail.push_(p);
  if( useReasonID != -1 ) {
//     assert( analyzeMark && "should only be used if we use reasons" );
    reasonID[ var(p) ] = useReasonID;
  }
}


lbool    BackwardVerificationWorker::value (Var x) const   { return assigns[x]; }

lbool    BackwardVerificationWorker::value (Lit p) const   { return assigns[var(p)] ^ sign(p); }

void BackwardVerificationWorker::restart(bool reset)
{
  int keepSize = unitTrailSize;
  if( reset ) {
    keepSize = 0;          // reset initial unit trail?
    if( verbose > 4 ) cerr << "c enable running prepropagation due to restart with reset" << endl;
    if( opt_preUnit ) {
      rerunPrePropagation = true;
      clearUnitTrail = true;
  #warning this data structure is not needed
      unitTrailSize = 0;
    }
    nonMarkedUnithead = 0; markedUnitHead = 0;  // take care of unit heads
  }
  
  for (int c = trail.size()-1; c >= keepSize; c--) {
    assigns[var(trail[c])] = l_Undef;
    if( analyzeMark ) reasonID[ var(trail[c]) ] = -1;   // reset reasons
  }
  
  // if reset, then all variable assignments have to be undone
  if( reset ) for( int i = 0; i < variables; ++ i ) assert( value( i ) == l_Undef && "everything has to be reset" );
  
  trail.shrink_( trail.size() - keepSize );                  // remove literals from trail
  markedQhead = trail.size(); nonMarkedQHead = trail.size(); // set heads accordingly

  if( verbose > 3 ) cerr << "c [S-BW-CHK] restart" << endl;
  if( verbose > 4 ) cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
}

void BackwardVerificationWorker::removeBehind( const int& position )
{
  assert( position >= 0 && position < trail.size() && "can only undo to a position that has been seen already" );
  for (int c = trail.size()-1; c >= position; c--) {
    assigns[var(trail[c])] = l_Undef;
    if( analyzeMark ) reasonID[ var(trail[c]) ] = -1;   // reset reasons
  }
  
  markedQhead = markedQhead < position ? markedQhead : position; 
  nonMarkedUnithead = 0; // to be on the safe side, replay units
  nonMarkedQHead = 0; // there might have been more marked clauses
  markedUnitHead = 0;
  if( position < trail.size() ) trail.shrink_( trail.size() - position );
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] reduce trail to position " << position << " new trail: " << trail << endl;
}


void BackwardVerificationWorker::setParallelMode(ParallelMode mode) { 
  if( parallelMode == BackwardVerificationWorker::copy && mode == BackwardVerificationWorker::shared ) {
    // use extra XOR-watchers in shared mode
    watchedXORLiterals.growTo( fullProof.size() );
    for( int i = 0 ; i < fullProof.size(); ++ i ) {
      if( fullProof[i].isValidAt( lastPosition ) && !fullProof[i].isDelete() ) {
	if( fullProof[i].isEmptyClause() ) { // jump over the empty clause
	  if( i + 1 < fullProof.size() && i > formulaClauses ) cerr << "c WARNING: found an empty clause in the middle of the proof [" << i << " / " << fullProof.size() << "]" << endl;
	  continue;
	}
	watchedXORLiterals[i].init( ca[ fullProof[i].getRef() ][0], ca[ fullProof[i].getRef() ][1] );
      }
    }
    
  }
  // set mode
  parallelMode = mode;
}

BackwardVerificationWorker* BackwardVerificationWorker::splitWork()
{
  assert( parallelMode != none && "beginning with this method we have to use a parallel mode" );
  assert( trail.size() == unitTrailSize && "split work only after verification of a clause finished" );
  
  // make sure that everything is reset in this worker, to split work nicely
  restart( true );
  assert( trail.size() == 0 && "state of the worker should be cleared" );
  assert( unitTrailSize == 0 && "pre propagation has to be run from scratch again" );
  
  BackwardVerificationWorker* worker = 0;
  
  if( parallelMode != none ) {
    // create a worker that works on a copy of the clauses - copy data from already modified clause storage
    worker = new BackwardVerificationWorker( drat, ca, fullProof, label, dependencies, formulaClauses, variables, true );
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
	  worker->proofItemProperties[i].moved(); // indicate that this clause is already moved into the marked watch lists
	  worker->clausesToBeChecked ++;
	  lastMovedItem = i;
	  proofItemProperties[i].resetMarked();
	  clausesToBeChecked --;
	  if( verbose > 4 ) cerr << "c [S-BW-CHK] moved proof item " << i << endl;
	}
	counter ++;
	
      }
    }
    worker->minimalMarkedProofItem = lastMovedItem;
    worker->restart(true);
  } else {
    assert( false && "none-parallel mode is not yet implemented" );
  }
  worker->parallelMode = parallelMode; // set the parallel mode
  
  worker->activeUnits = activeUnits;
  
  return worker;
}

lbool BackwardVerificationWorker::continueCheck(int64_t untilProofPosition) {
  
  if( verbose > 1 ) cerr << "c [S-BW-CHK] check clauses that have been marked (" << clausesToBeChecked << ") start with position " << lastPosition << endl;
  
  if( verbose > 7 ) {
    cerr << "c [S-BW-CHK] full state: " << endl;
    cerr << "c non-marked-units ("<< nonMarkedUnitClauses.size() << "): "; for( int i = 0 ; i < nonMarkedUnitClauses.size(); ++ i ) { cerr << " " << nonMarkedUnitClauses[i].getLit() ; } cerr << endl;
    cerr << "c marked-units (" << markedUnitClauses.size()  << "): "; for( int i = 0 ; i < markedUnitClauses.size(); ++ i ) { cerr << " " << markedUnitClauses[i].getLit() ; } cerr << endl; 
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

  restart(true);
  
  vec<Lit> dummy;
  if( verbose > 2 ) cerr << "c [S-BW-CHK] check from position " << fullProof.size() -1 << " to " << maxCheckPosition << endl;
  for( ; lastPosition >= maxCheckPosition; -- lastPosition ) {
    
    if( verbose > 4 ) cerr << "c [S-BW-CHK] look at proof position " << lastPosition << " / " << fullProof.size() << " limit: " << maxCheckPosition << endl;
    
    ProofClauseData& proofItem = fullProof[lastPosition];
    maxClausesToBeChecked = maxClausesToBeChecked >= clausesToBeChecked ? maxClausesToBeChecked : clausesToBeChecked; // get maximum
    if( proofItem.isDelete() ) {
      assert( ! label[ lastPosition ].isMarked() && "deletion information cannot be marked" ); // delete items point to the ID of the clause that should be added instead
      if( verbose > 3 ) cerr << "c [S-BW-CHK] enable clause from proof item " << proofItem.getID() << " with clause " << ca[ proofItem.getRef() ] << endl;
      enableProofItem( fullProof[ proofItem.getID() ] );  // activate this item in the data structures (watch lists, or unit list)
    } else {
      
      // eager removal, remove clause from its watch lists, remove clause from full occurrence lists
      disableProofItem( proofItem );
      
      if ( proofItemProperties[proofItem.getID()].isMarkedByMe() && label[ proofItem.getID() ].isMarked() && !label[ proofItem.getID()  ].isVerified() ) { // verify only items that have been marked by me
	
	assert( lastPosition == proofItem.getID() && "item to be verified have to have the same ID as their index" );
	
	if( verbose > 3 ) cerr << "c [S-BW-CHK] check clause " << ca[ proofItem.getRef() ] << " at position " << lastPosition << " with id " << proofItem.getID() << endl;
	
	// all items that are behind the current item in the proof have to be verified, if they are marked by this thread
	for( int j = lastPosition + 1; j < fullProof.size(); ++ j ) {
	  assert( ( ! proofItemProperties[j].isMarkedByMe() || label[j].isVerified() ) && "each marked item has to be verified until now in sequential mode" );
	}
	
	// check the item (without the item being in the data structures)
	if( ! checkSingleClauseAT( proofItem.getID(), &(ca[ proofItem.getRef() ]), dummy ) ) {
	  bool ret = false;
	  if( verbose > 2 ) cerr << "c [S-BW-CHK] AT check failed" << endl;
	  ratChecks ++;
	  if( drat ) ret = checkSingleClauseRAT( (int64_t) proofItem.getID(), dummy, &(ca[ proofItem.getRef() ]) ); // check RAT, if drat is enabled and AT failed
	  if( ! ret ) {
	    if( drat && verbose > 2 ) cerr << "c [S-BW-CHK] DRAT check failed" << endl;
	    restart(); // clean trail again, reset assignments
	    return l_False;
	  }
	} else {
	  if( verbose > 2 ) cerr << "c [S-BW-CHK] AT check succeed for clause " << ca[ proofItem.getRef() ] << endl;
	}
	restart(); // clean trail again, reset assignments
	verifiedClauses ++;
	
	if( verbose > 7 && ca[ proofItem.getRef() ].size() == 1 ) {
	  cerr << "c begin current formula: " << endl;
	  for( int i = 0 ; i < proofItem.getID(); ++ i ) {
	    if( proofItem.isValidAt( proofItem.getID() ) && label[i].isMarked() ) cerr << ca[ fullProof[i].getRef() ] << endl;
	  }
	  cerr << "c end current formula: " << endl;
	}
	
// 	assert( clausesToBeChecked > 0 && "there had to be work before" );
	label[ proofItem.getID()  ].setVerified();
	clausesToBeChecked --; // we checked another clause
	
	if( sequentialLimit != 0 && clausesToBeChecked > sequentialLimit ) {
	  return l_Undef;
	}
	if( interuptLock != 0 ) { // interrupted by master thread (which currently waits on the corresponding lock)
	  interuptLock->awake();
	  return l_Undef;
	}
      } else {
	if( proofItemProperties[ proofItem.getID() ].isMarkedByMe() && label[ proofItem.getID() ].isVerified() ) {
	  checkedByOtherWorker ++;
	  clausesToBeChecked --; // somebody else checked this clause already
	}
	// nothing to do with unlabeled clauses, they stay in the watch list/full occurrence list until some garbage collection is performed
	if( verbose > 4 ) cerr << "c [S-BW-CHK] jump over non-marked proof item " << proofItem.getID() << "( marked: " << label[ proofItem.getID()  ].isMarked() << " verified: " << label[ proofItem.getID()  ].isVerified() << endl;
      }

    }
    
  }
  restart(); // clean trail again, reset assignments

  if( verbose > 0 ) {
    cerr << "c still to check: " << clausesToBeChecked 
         << " (max: " << maxClausesToBeChecked 
         << ") propagatedLits: " <<  num_props 
         << " verifiedClauses: " << verifiedClauses 
         << " rat: " << ratChecks << endl;
  }

  if( verbose > 8 ) {
    cerr << "c [S-BW-CHK] full state: " << endl;
    cerr << "c non-marked-units ("<< nonMarkedUnitClauses.size() << "): "; for( int i = 0 ; i < nonMarkedUnitClauses.size(); ++ i ) { cerr << " " << nonMarkedUnitClauses[i].getLit() ; } cerr << endl;
    cerr << "c marked-units (" << markedUnitClauses.size()  << "): "; for( int i = 0 ; i < markedUnitClauses.size(); ++ i ) { cerr << " " << markedUnitClauses[i].getLit() ; } cerr << endl; 
    cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
    cerr << "c trail: " << trail << endl;
    cerr << "c BEGIN FULL PROOF" << endl;
    for( int i = 0 ; i < fullProof.size(); ++ i ) {
      cerr << "c item [" << i << "] id: " << fullProof[i].getID() << " valid until " << fullProof[i].getValidUntil() << " byMe: " << proofItemProperties[i].isMarkedByMe() << " marked: " << label[i].isMarked() 
      << " verified: " << label[i].isVerified() << " deleted: "  << fullProof[i].isDelete() << "  clause: ";
      if(fullProof[i].getRef() == CRef_Undef ) cerr << "empty"; else cerr << ca[ fullProof[i].getRef() ] ;
      cerr << endl;
    }
    cerr << "c END FULL PROOF" << endl;
  }
  
#warning this assertion is not working in full parallel mode (maybe due to items that are verified by another worker already s.t. the counter is missleaded)
//   assert( clausesToBeChecked == 0 && "has to process all elements of the proof" );
  
  return l_True;
}

lbool BackwardVerificationWorker::checkClause(vec< Lit >& clause, int64_t untilProofPosition, bool returnAfterInitialCheck)
{
  assert( parallelMode != shared && "not yet implemented propagating on shared structures without write access (initialization is implemented)" );
  
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
      restart(true); // clean trail again, reset assignments
      return l_False; 
    }
  }
  verifiedClauses ++;
  restart(true); // clean trail again, reset assignments
  if( returnAfterInitialCheck ) return l_True;
  
  lastPosition = fullProof.size() - 1;
  
  return continueCheck(untilProofPosition);
  
}

bool BackwardVerificationWorker::markToBeVerified(const int64_t& proofItemID)
{
  assert( ( !opt_dependencies || parallelMode == none ) && "dependencies require a single writer, hence, a lock becomes necessary" );
  if( label[ proofItemID ].isMarked() ) {
    if( ! proofItemProperties[ proofItemID ].isMarkedByMe() ) usedOthersMark ++; // count how often elements are used that have been marked by other threads
    return false; // somebody else might have marked that clause already
  }
  label[ proofItemID ].setMarked();                   // TODO: might be made more thread safe
  if( verbose > 3 ) cerr << "c [S-BW-CHK] mark clause with ID " << proofItemID << endl;
  clausesToBeChecked = proofItemID >= formulaClauses ? clausesToBeChecked + 1 : clausesToBeChecked; // set by ourself and to be checked in the proof
  proofItemProperties[ proofItemID ].markedByMe();  // this item has been marked by me
  minimalMarkedProofItem = ( proofItemID >= formulaClauses && minimalMarkedProofItem > proofItemID ) ? proofItemID : minimalMarkedProofItem; // store minimum (for split work)
  return true;
}

bool BackwardVerificationWorker::markedItem( const int itemID ) {
  assert( itemID > 0 && itemID < proofItemProperties.size() && "item has to stay in limits" );
  proofItemProperties[ itemID ].isMarkedByMe();
}

inline
int64_t BackwardVerificationWorker::propagateMarkedUnits(const int64_t currentID, bool markLazy)
{
  // propagate units first!
  int keptUnits = markedUnitHead;
  for( ; markedUnitHead < markedUnitClauses.size() ; ++ markedUnitHead ) { // propagate all known units
    num_props ++;
    const ProofClauseData& unit = markedUnitClauses[ markedUnitHead ];
    if( unit.isValidAt( currentID - 1 ) ) { // we check this position, so the same clause cannot be used for verification here!
      markedUnitClauses[ keptUnits ++ ] = markedUnitClauses[ markedUnitHead ]; // keep current element
      const Lit& l = unit.getLit();            // as its a unit clause, we can use the literal
      if( value(l) == l_True ) continue;       // check whether its SAT already
      else if (value(l) == l_False ) {
	if( verbose > 8 ) cerr << "c [S-BW-CHK] conflict with unit " << unit.getLit() << " (id: " << unit.getID() << ")" << endl;
	int64_t unitID = unit.getID(); // reference might become invalid below
	for( int i = markedUnitHead + 1; i < markedUnitClauses.size(); ++ i )  // kept the current literal already
	  markedUnitClauses[ keptUnits ++ ] = markedUnitClauses[ i ];          // keep all other elements element
	markedUnitClauses.shrink_( markedUnitClauses.size() - keptUnits );      // remove all removed elements
	return unitID; // we do not know the exact clause, be we see a conflict
      }
      else {
	if( verbose > 8 ) cerr << "c [S-BW-CHK] enqueue literal with unit " << unit.getLit() << " (id: " << unit.getID() << ")" << endl;
	uncheckedEnqueue( l, unit.getID() );              // if all is good, enqueue the literal
      }
    } else { // element not valid any more, remove it
      if( verbose > 8 ) cerr << "c [S-BW-CHK] remove unit " << unit.getLit() << " (id: " << unit.getID() << ") from proof" << endl;
      assert( currentID - 1 < unit.getID() && "a present element can only become invalid if its been removed from the proof (going backwards beyond that element)" );
    }
  }
  markedUnitClauses.shrink_( markedUnitClauses.size() - keptUnits ); // remove all removable elements (less elements in the next call)
  
  return -1; // no conflict
}

int64_t BackwardVerificationWorker::propagateMarked(const int64_t currentID, bool markLazy)
{ 
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate marked ... " << endl;

  // we found some conflict when propagating unit clauses
  int64_t unitConflict = propagateMarkedUnits( currentID ) ;
  if( unitConflict != -1 ) return unitConflict;
  
  // otherwise perform usual propagation
  int64_t confl     = -1;
  markedWatches.cleanAll(); 
  while( markedQhead < trail.size() ) {
        Lit            p   = trail[markedQhead++];     // 'p' is enqueued fact to propagate.
        vec<ProofClauseData>&  ws  = markedWatches[p];
	for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
        ProofClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (ProofClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID - 1) ) { 
	      if( verbose > 8 ) cerr << "c [S-BW-CHK] drop element with item " << i->getID() << " from list of literal " << p  << endl;
	      ++i; continue; 
	    } // simply remove this element from the watch list after the loop ended
	    
	    // do not process non-moved elements here!
	    if( ! proofItemProperties[ i->getID() ].isMoved() ) {
	      *j ++ = *i ++; // keep element in watch list
	      continue;
	    }

            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");

	    // If 0th watch is true, then clause is already satisfied.
            const Lit& first = c[0];
            const ProofClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;
            int maxPos = trailPos[ var(c[1]) ];
	    
	    if( !markLazy && opt_oldestReason ) {
	      if ( // first != blocker &&  // enable later again
		value(first) == l_True) // satisfied clause
	      {
		*j++ = w; continue; } // same as goto NextClause;
	    }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++) {
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
		    for( int a = 0 ; a < markedWatches[~c[1]].size(); ++ a ) assert( markedWatches[~c[1]][a].getID() != w.getID() && "element should not be present already" );
                    markedWatches[~c[1]].push(w);
                    goto NextClause; 
		}
		maxPos = maxPos > trailPos[ var( c[k] ) ] ? maxPos : trailPos[ var( c[k] ) ]; // get maximum position 
	    }
		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // if( config.opt_printLhbr ) cerr << "c keep clause (" << cr << ")" << c << " in watch list while propagating " << p << endl;
            if ( value(first) == l_False ) {
                confl = w.getID(); // use ID of the conflict
                markedQhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
		
		
		// handle formula clauses, if we put them into the marked list without marking them
		if( opt_preferFormula && ! label[ w.getID() ].isMarked() ) {
		  assert( w.getID() < formulaClauses && "applies only for clauses of the formual" );
		  markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
		}
		
            } else {
	      if(value(first) == l_True  ) {  // the newly watched literal is satisfied
		if( opt_oldestReason // look for the oldest reason
		    && trailPos[ var( first ) ] > maxPos                  // there is not other literal in the clause (so far) that has been assigned later
		    && (reasonID[ var( first ) ] > w.getID()              // the current clause is "older" then the current reason
		       || label[ reasonID[ var( first ) ] ].isMarked() )  // OR  the current reason is marked (even better case, replace non-marked with marked)
		    ){ 

		    // check conditions of being reason in one block, update reason information
		    assert( value(c[1]) == l_False && "the other watched literal has to be false" );
		    assert( value(c[0]) == l_True && "forced literal" );
		    for( int k = 1; k < c.size(); ++ k ) assert( value(c[k]) == l_False && trailPos[ var(c[k] ) ] < trailPos[ var(first) ] && "c is reason" );
		    reasonID[ var( c[1] ) ] = w.getID();
		}
	      } else {
		// handle usual case, enqueue newly found unit with according reason clause
	        assert( value(first) == l_Undef && "invariant of above methods" );
                uncheckedEnqueue(first, w.getID() );
	      }
	    }
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!
	for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
  }
  return confl;
}

int64_t BackwardVerificationWorker::propagateMarkedShared(const int64_t currentID)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate marked (shared) ... " << endl;

  // we found some conflict when propagating unit clauses
  int64_t unitConflict = propagateMarkedUnits( currentID ) ;
  if( unitConflict != -1 ) return unitConflict;
  
  // otherwise perform usual propagation
  int64_t confl     = -1;
  markedWatches.cleanAll(); 
  while( markedQhead < trail.size() ) {
        Lit            p   = trail[markedQhead++];     // 'p' is enqueued fact to propagate.
        vec<ProofClauseData>&  ws  = markedWatches[p];
        ProofClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (ProofClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID - 1 ) ) { ++i; continue; } // simply remove this element from the watch list after the loop ended

	    // do not process non-moved elements here!
	    if( ! proofItemProperties[ i->getID() ].isMoved() ) {
	      *j ++ = *i ++; // keep element in watch list
	      continue;
	    }
	    
            WatchedLiterals& watch = watchedXORLiterals[ i->getID() ]; // get watch data structure that belongs to this clause
            
            const Lit false_lit = ~p;
            const Lit otherWatchedLit = watch.getOther( ~p );
            
            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
	    const ProofClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;                                       // next iteration consider next literal
            
            if (  // first != blocker &&  // enable later again
	      value(otherWatchedLit) == l_True) // satisfied clause
	    {
	      *j++ = w; continue; } // same as goto NextClause;
	    
            const Clause&  c = ca[cr];

            // Look for new watch:
            for (int k = 0; k < c.size(); k++){
	     if( c[k] == otherWatchedLit || c[k] == false_lit ) continue; // do not look at the watched literals 
              if (value(c[k]) != l_False)
	      {
		for( int a = 0 ; a < markedWatches[~c[k]].size(); ++ a ) assert( markedWatches[~c[k]][a].getID() != w.getID() && "element should not be present already" );
                    markedWatches[~c[k]].push(w); // watch with literal c[k] instead of literal ~p (put into list ~c[k])
		    watch.update(~p, c[k] );      // update watch structure to now watch literal c[k], instead of ~p
                    goto NextClause; 
	      }
	    }
		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // if( config.opt_printLhbr ) cerr << "c keep clause (" << cr << ")" << c << " in watch list while propagating " << p << endl;
            if ( value(otherWatchedLit) == l_False ) {
                confl = w.getID();
                markedQhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            } else {
                uncheckedEnqueue(otherWatchedLit, w.getID() );
	    }
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!
	for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
  }
  return confl;
}

inline
int64_t BackwardVerificationWorker::propagateUnmarkedUnits(const int64_t currentID, bool markLazy)
{
  // propagate units first!
  for( ; nonMarkedUnithead < nonMarkedUnitClauses.size() ; ++ nonMarkedUnithead ) { // propagate all known units
    num_props ++;
    const ProofClauseData& unit = nonMarkedUnitClauses[ nonMarkedUnithead ];
    if( unit.isValidAt( currentID - 1) && ! proofItemProperties[unit.getID()].isMoved() ) {
      const Lit& l = unit.getLit();            // as its a unit clause, we can use the literal
      if( value(l) == l_True ) {  // check whether its SAT already, keeps unit in the list
	//TODO: handle for parallel version: check whether its already marked and handle it accordingly
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit satisfied already [" << nonMarkedUnithead << "/" << nonMarkedUnitClauses.size() << "]: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	continue;                                                                         
      } else if (value(l) == l_False ) {
	// add this unit to the list of marked units -- it will be marked with and without conflict analysis
	markedUnitClauses.push( unit );
	markToBeVerified( unit.getID() );         // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
	// memorize where to continue checking next time this method is called 	
	nonMarkedUnithead ++; // checked this unit already, next time, check the next element (if there would have been no conflict)
	// use this unit, as it leads to a conflict	
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit resulted in conflict: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	return unit.getID(); // we do not know the exact clause, but we saw a conflict
      }
      else {
	if( !markLazy ) { // mark this clause?
	  // add this unit to the list of marked units
	  markedUnitClauses.push( unit );
	  markToBeVerified( unit.getID() );         // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
	}
	// add literal to trail
	if( verbose > 4 ) cerr << "c [S-BW-CHK] unit can be enqueued: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
	uncheckedEnqueue( l, unit.getID() );            // if all is good, enqueue the literal
	// memorize where to continue checking next time this method is called 	
	nonMarkedUnithead ++;
	// use this unit, as it leads to a conflict	
	return -1; // we just enqueued another literal, so everything's fine
      }
    } else { // element not valid any more, or has been marked in another way (by the analyze method) remove it
      if( verbose > 4 ) cerr << "c [S-BW-CHK] removed invalid/marked unit: " << unit.getLit() << " with id " << unit.getID() << " valid until " << unit.getValidUntil() << endl;
      nonMarkedUnitClauses[ nonMarkedUnithead ] = nonMarkedUnitClauses[ nonMarkedUnitClauses.size() - 1 ]; // move element forward
      nonMarkedUnitClauses.shrink_(1); // remove one element 
      nonMarkedUnithead --;            // process element that has been moved forward next
    }
  }
  // did not find a unit clause
  return -1;
}

int64_t BackwardVerificationWorker::propagateUntilFirstUnmarkedEnqueueEager(const int64_t currentID, bool markLazy)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate non-marked with ID " << currentID << endl;

  int64_t conflictID = propagateUnmarkedUnits( currentID );
  if( conflictID != -1 ){
    return conflictID; // return ID of the faulty unit clause
  }
  
  int64_t    confl     = -1;
  markedWatches.cleanAll(); 
    
  while( nonMarkedQHead < trail.size() ) {
        const Lit            p   = trail[nonMarkedQHead++];     // 'p' is enqueued fact to propagate.
        if( verbose > 5 ) cerr << "c [S-BW-CHK] propagate literal" << p << " on non-marked " << endl;
        vec<ProofClauseData>&  ws  = markedWatches[p];
	
	if( verbose > 6 ) {
	  cerr << "c [S-BW-CHK] non-marked watch list for literal " << p << endl;
	  for( int i = 0 ; i < markedWatches[p].size(); ++ i ) {
	    if( markedWatches[p][i].isValidAt( currentID - 1 ) )
	    cerr << "c [" << i << "] ref: " << markedWatches[p][i].getRef() << " clause: " << ca[ markedWatches[p][i].getRef() ] << endl;
	  }
	}
	
	// set correct pointer
        ProofClauseData  *i = (lastUnmarkedLiteral == p)
	     ? (ProofClauseData*)ws + lastUnmarkedPosition // continue at last position
	     : (ProofClauseData*)ws;                           // start from the beginning
	lastUnmarkedPosition = (lastUnmarkedLiteral == p)
	     ? lastUnmarkedPosition
	     : 0;
	lastUnmarkedLiteral = p;
	
        ProofClauseData  *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (end = (ProofClauseData*)ws + ws.size();  i != end;  )
	{
	    if( ! i->isValidAt( currentID - 1 ) ) {  // if the element became invalid, or has been marked
	      end --;        // move end pointer forward
	      *i = *end;      // copy last element to current position
	      ws.shrink_(1);  // remove last element from vector
	      continue;
	    }

	    // do not process moved elements again!
	    if( proofItemProperties[ i->getID() ].isMoved() ) {
	      ++ i;   // keep the element in the list
	      continue;
	    }
	    
            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
	    const ProofClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;lastUnmarkedPosition++;    // move pointer forward explicitely
	    
	    // TODO: blocking literal
	    
            Clause&  c = ca[cr];
	    if( verbose > 5 ) cerr << "c [S-BW-CHK] propagate on clause " << c << endl;
	    
	    
	    // markLazy && opt_oldestReason --> check all clauses independently of their values!
	    if( !markLazy && opt_oldestReason ) {
	      if ( value(c[0]) == l_True || value(c[1]) == l_True) // satisfied clause
	      {
		continue; } // keep the element, continue is same as goto NextClause;
	    }
		markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");

	    // If 0th watch is true, then clause is already satisfied.
            const Lit& first = c[0];
	    int maxPos = trailPos[ var(c[1]) ];
            
            // Look for new watch:
            for (int k = 2; k < c.size(); k++) {
	        
                if (value(c[k]) != l_False) // found a literal that is true, or that is undef
		{
                    c[1] = c[k]; c[k] = false_lit;
                    markedWatches[~c[1]].push(w);
		    i--;lastUnmarkedPosition--;    // process the element once more (is overwritten in the mean time)
		    end --;                        // move end pointer forward
		    *i = *end;                     // copy last element to current position
		    ws.shrink_(1);                 // remove last element from vector
		    
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!
		
		maxPos = maxPos > trailPos[ var( c[k] ) ] ? maxPos : trailPos[ var( c[k] ) ]; // get maximum position 
	    }
		
            // Did not find watch -- clause is unit under assignment:
            // hence, the clause has to be marked, do not add the clause back to this list!
	    
            if ( value(first) == l_False ) { // we found a conflict
                confl = w.getID(); // independent of opt_long_conflict -> overwrite confl!
                nonMarkedQHead = trail.size();
		
		// automatically mark conflict clause
		// in either case we found something, so we can abort this method
		// add this clause to the watch list of marked clauses
// 		markedWatches[~c[0]].push(w);
// 		markedWatches[~c[1]].push(w);
		proofItemProperties[ w.getID() ].moved(); // indicate that this clause has been moved
		if( verbose > 5 ) cerr << "c [S-BW-CHK] move clause to marked watch lists id: " << w.getID() << " ref: " << w.getRef() << " clause: " << ca[ w.getRef() ] << endl;
		markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
// 		i--;lastUnmarkedPosition--;    // process the element once more (is overwritten in the mean time)
// 		end --;                        // move end pointer forward
// 		*i = *end;                     // copy last element to current position
// 		ws.shrink_(1);                 // remove last element from vector
		
            } else {  // we found a unit clause (or another reason for a given implied literal)
	      
		// check whether this clause can be a reason for the new literal c[1]
		if(  value(first) == l_True ) { // look for the oldest reason
		  if( opt_oldestReason // the newly watched literal is satisfied
		      && trailPos[ var( first ) ] > maxPos                // there is not other literal in the clause (so far) that has been assigned later
		      && reasonID[ var( first ) ] > i->getID()         // the current clause is "older" then the current reason
		      && !label[ reasonID[ var( first ) ] ].isMarked() // the current reason is also not marked
		      && trailPos[var(first)] > trailPos[var(c[1])]){  // the first literal is assigned before the first
			
		      // check conditions of being reason in one block, update reason information
		      assert( value(c[1]) == l_False && "the other watched literal has to be false" );
		      assert( value(c[0]) == l_True && "forced literal" );
		      for( int k = 1; k < c.size(); ++ k ) assert( value(c[k]) == l_False && trailPos[ var(c[k] ) ] < trailPos[ var(first) ] && "c is reason" );
		      reasonID[ var( c[1] ) ] = i->getID();
		  }
			
		} else {
		  
		  assert( value(first) == l_Undef && "invariant in propagation implementation" );
	      
		  uncheckedEnqueue(first, w.getID() );
		  nonMarkedQHead --; // in the next call we have to check this literal again // FIXME might result in checking the same list multiple times without any positive effect as the elements in the front are checked again and again
		  
		  if( !markLazy ) {
		    // automatically mark conflict clause
		    // in either case we found something, so we can abort this method
		    // add this clause to the watch list of marked clauses
  // 		  markedWatches[~c[0]].push(w);
  // 		   markedWatches[~c[1]].push(w);
		    proofItemProperties[ w.getID() ].moved(); // indicate that this clause has been moved
		    if( verbose > 5 ) cerr << "c [S-BW-CHK] move clause to marked watch lists id: " << w.getID() << " ref: " << w.getRef() << " clause: " << ca[ w.getRef() ] << endl;
		    markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark 
  // 		  i--;lastUnmarkedPosition--;    // process the element once more (is overwritten in the mean time)
  // 		  end --;                        // move end pointer forward
  // 		  *i = *end;                     // copy last element to current position
  // 		  ws.shrink_(1);                 // remove last element from vector
		  } else {      // otherwise keep the clause for now
				// keep the element (nothing to be done)
		  }
		}
	    }
	    for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
	    return confl;
	    
        NextClause:;
        }
	for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
  }
  return confl;  
}

int64_t BackwardVerificationWorker::propagateUntilFirstUnmarkedEnqueueEagerShared(const int64_t currentID, bool markLazy)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] propagate non-marked with ID " << currentID << endl;

  int64_t conflictID = propagateUnmarkedUnits( currentID );
  if( conflictID != -1 ){
    return conflictID; // return ID of the faulty unit clause
  }
  
  int64_t    confl     = -1;
  markedWatches.cleanAll(); 
  
  while( nonMarkedQHead < trail.size() ) {
        const Lit            p   = trail[nonMarkedQHead++];     // 'p' is enqueued fact to propagate.
        if( verbose > 5 ) cerr << "c [S-BW-CHK] propagate literal " << p << endl;
        vec<ProofClauseData>&  ws  = markedWatches[p];
	
	if( verbose > 6 ) {
	  cerr << "c [S-BW-CHK] watch list for literal " << p << endl;
	  for( int i = 0 ; i < markedWatches[p].size(); ++ i ) {
	    cerr << "c [" << i << "] ref: " << markedWatches[p][i].getRef() << " clause: " << ca[ markedWatches[p][i].getRef() ] << endl;
	  }
	}
	
        ProofClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (ProofClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID - 1 ) ) { ++i; continue; }       // simply remove this element from the watch list after the loop ended
	    if( proofItemProperties[ i->getID() ].isMoved() ) { *j ++ = *i ++;; continue; }  // this clause has been moved to the watch lists for marked clauses previously, keep it in list

            WatchedLiterals& watch = watchedXORLiterals[ i->getID() ]; // get watch data structure that belongs to this clause
            
            const Lit false_lit = ~p;
            const Lit otherWatchedLit = watch.getOther( ~p );
            
            // Make sure the false literal is data[1]:
            const CRef cr = i->getRef();
	    const ProofClauseData& w = *i; // enable later again Watcher(cr, first, 1); // updates the blocking literal
	    i++;                                       // next iteration consider next literal
            
            if (  // first != blocker &&  // enable later again
	      value(otherWatchedLit) == l_True) // satisfied clause
	    {
	      *j++ = w; continue; } // same as goto NextClause;

            const Clause&  c = ca[cr];

            // Look for new watch:
            for (int k = 0; k < c.size(); k++){
	     if( c[k] == otherWatchedLit || c[k] == false_lit ) continue; // do not look at the watched literals 
              if (value(c[k]) != l_False)
	      {
                    markedWatches[~c[k]].push(w); // watch with literal c[k] instead of literal ~p (put into list ~c[k])
		    watch.update(~p, c[k] );      // update watch structure to now watch literal c[k], instead of ~p
                    goto NextClause; 
	      }
	    }
		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // hence, the clause has to be marked, do not add the clause back to this list!
	    
            if ( value(otherWatchedLit) == l_False ) { // we found a conflict
                confl = w.getID(); // independent of opt_long_conflict -> overwrite confl!
                nonMarkedQHead = trail.size();
		
		// automatically mark conflict clause
		// in either case we found something, so we can abort this method
		// add this clause to the watch list of marked clauses
// 		markedWatches[~c[0]].push(w);
// 		markedWatches[~c[1]].push(w);
		proofItemProperties[ w.getID() ].moved(); // indicate that this clause has been moved
		if( verbose > 5 ) cerr << "c [S-BW-CHK] move clause to marked watch lists id: " << w.getID() << " ref: " << w.getRef() << " clause: " << ca[ w.getRef() ] << endl;
		markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
		
            } else {
                uncheckedEnqueue(otherWatchedLit, w.getID() );
		nonMarkedQHead --; // in the next call we have to check this literal again // FIXME might result in checking the same list multiple times without any positive effect as the elements in the front are checked again and again
		
		if( !markLazy ) {
		  // automatically mark conflict clause
		  // in either case we found something, so we can abort this method
		  // add this clause to the watch list of marked clauses
		  markedWatches[~c[0]].push(w);
		  markedWatches[~c[1]].push(w);
		  proofItemProperties[ w.getID() ].moved(); // indicate that this clause has been moved
		  if( verbose > 5 ) cerr << "c [S-BW-CHK] move clause to marked watch lists id: " << w.getID() << " ref: " << w.getRef() << " clause: " << ca[ w.getRef() ] << endl;
		  markToBeVerified( w.getID() );            // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark 
		} else {      // otherwise keep the clause for now
// 		  *j++ = w;  // keep the element (done above already)
		}
	    }
	    
            // Copy the remaining watches:
            while (i < end)
               *j++ = *i++;
	    ws.shrink_(i - j); // remove all duplciate clauses!
	    
	    for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
	    return confl;
	    
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!
	for( int a = 0 ; a < ws.size(); ++ a ) { for ( int b = a + 1; b < ws.size(); ++ b ) assert( ws[a].getID() != ws[b].getID() && "not the same elements in a watch list" ); }
  }
  return confl;  
}


void BackwardVerificationWorker::analyzeAndMark( int64_t confl, int64_t checkID) {

  int index = trail.size() -1;     // position of the literals in the trail that is analyzed for being resolved next
  Lit resolveLiteral = lit_Undef ; // literal that is used for resolution
  int propagationLits = 1;         // number of literals that have to be resolved away (1 pseudo literal for the initialization of the first clause)
  int64_t resolveItemId = confl;   // proof item to resolve with
  int space = 0;                   // number of resolvents used to verify the current clause
  resolutions --; 
  
  if ( verbose > 2 ) cerr << "c analyze conflict with id: " << confl << " during checking id: " << checkID << endl; 
  if ( verbose > 4 ) cerr << "c trail: " << trail << endl;
  
  for( int i = 0 ; i < variables; ++ i ) assert( seen[i] == 0 && "cannot have seen any variable" );
  
  do {
    resolveItemId = resolveLiteral == lit_Undef ? confl : resolveItemId; // set current id based on current literal
    
    // consider this item of the proof next
    ProofClauseData& proofItem = fullProof[ resolveItemId ];
    assert( !proofItem.isEmptyClause() && ! proofItem.isDelete() && "conflict has to be a usual clause" );
    
    const Clause& conflict = ca[ proofItem.getRef() ];
    if( verbose > 4 ) cerr << "c resolve with clause " << conflict << " on literal " << resolveLiteral << "( propLits: " << propagationLits << ")" << endl;
    resolutions ++; // count number of resolutions
    space ++;

    // memorize that proof item with id checkID has been verified by using the current item
    if( opt_dependencies ) dependencies[ checkID ].push( proofItem.getID() );
    
    // the clause is not marked yet (by this thread)
    markToBeVerified( proofItem.getID() ); // memorize that the clause has to be verified
    if( ! proofItemProperties[ proofItem.getID() ].isMoved() ) {
      	if( verbose > 5 ) cerr << "c [S-BW-CHK] analyze moves clause to marked watch lists id: " << proofItem.getID() << " ref: " << proofItem.getRef() << " clause: " << ca[ proofItem.getRef() ] << endl;
	if( conflict.size() == 1 ) {
	  ProofClauseData unitItem = proofItem;
	  unitItem.setLit( conflict[0] );
	  markedUnitClauses.push( unitItem );
	} else {
	  if( parallelMode == shared ) { // create private watchers
	    watchedXORLiterals[ proofItem.getID() ].init( conflict[0], conflict[1] );
	  } 
// 	  for( int a = 0 ; a < markedWatches[~conflict[0]].size(); ++ a ) {  assert( markedWatches[~conflict[0]][a].getID() != proofItem.getID() && "not the same elements in a watch list" ); }
// 	  for( int a = 0 ; a < markedWatches[~conflict[1]].size(); ++ a ) {  assert( markedWatches[~conflict[1]][a].getID() != proofItem.getID() && "not the same elements in a watch list" ); }
// 	  markedWatches[~conflict[0]].push(proofItem); // add clause to watch lists
// 	  markedWatches[~conflict[1]].push(proofItem); // add clause to watch lists
	}
	if( verbose > 7 ) cerr << "c [S-BW-CHK] moved element with id: " << proofItem.getID() << endl;
	proofItemProperties[ proofItem.getID() ].moved(); // indicate that this clause has been moved
    }
    
    // set that all variables that would be added to the resolvent
    for( int i = 0 ; i < conflict.size(); ++ i ) {
      if( verbose > 7 ) cerr << "c [S-BW-CHK] look at literal " << conflict[i] << " with seen: " << (int)seen [ var(conflict[i])] << " and reason: " << reasonID[ var(conflict[i]) ] << endl;
      if( seen[ var( conflict[i] ) ] == 0 ) {
	seen[ var( conflict[i] ) ] = 1 ; // memorize that the variable has to be resolved away
	if( reasonID[ var( conflict[i] )  ] != -1 ) { 
	  if( verbose > 7 ) cerr << "c [S-BW-CHK] memorize propagation literal " << conflict[i] << endl;
	  propagationLits ++; // memorize that there is one more literal that was used during propagation
	} else {
	  if( verbose > 7 ) cerr << "c [S-BW-CHK] ignore non-propagation literal " << conflict[i] << endl;
	}
      } else {
	if( verbose > 7 ) cerr << "c [S-BW-CHK] jump over already seen literal " << conflict[i] << endl;
      }
    }
    
    // search for the next literal
    propagationLits --; // we resolved the current literal away
    
    if( resolveLiteral != lit_Undef ) seen[ var(resolveLiteral) ] = 0; // reset seen flag
    
    if( propagationLits == 0 ) {
      if( verbose > 6 ) cerr << "c [S-BW-CHK] stop resolution due to propagation lits = 0, index: " << index << " trail: " << trail  << endl;
      break;
    }
    
    do {
      if( resolveLiteral != lit_Undef ) seen[ var(resolveLiteral) ] = 0; // reset seen flag (also for intermediate literals)
      while( seen[ var( trail [index--] ) ] == 0 ) {}
      assert( index >= -1 && "do not run below lower bound of array" );
      resolveLiteral = trail[index+1];
      resolveItemId = reasonID[ var( resolveLiteral ) ];
    } while ( resolveItemId == -1 ); // jump over "decisions", to reach preUnits
    
    if( verbose > 7 ) cerr << "c [S-BW-CHK] next resolve literal: " << resolveLiteral << " (index: " << index << ")" << endl;
    
  } while ( true ); // as long as there are literals to be resolved
  
  assert( trail.size() + 1 >= space && "cannot have used more literals" );
  
  maxSpace = maxSpace >= space ? maxSpace : space; // store maximum space that has been used
  
  // clear seen marks
  for( ; index >= 0; -- index ) {
    seen[ var( trail[index] ) ] = 0;
    removedLiterals += (int)( reasonID[ var(trail[index]) ] != -1 );
  }
  redundantPropagations += trail.size() - space;
  
}
