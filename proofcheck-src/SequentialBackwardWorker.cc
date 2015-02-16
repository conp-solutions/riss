/**********************************************************************[SequentialBackwardWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck-src/SequentialBackwardWorker.h"

using namespace Riss;

SequentialBackwardWorker::SequentialBackwardWorker(bool opt_drat, ClauseAllocator& outer_ca, vec< BackwardChecker::ClauseData >& outer_fullProof, vec< BackwardChecker::ClauseLabel >& outer_label, int outer_formulaClauses, int outer_variables, bool keepOriginalClauses)
:
drat( opt_drat ),
parallelMode( none ),
verbose(0),   // all details for development
#warning ADD A VERBOSITY OPTION TO THE TOOL
formulaClauses( outer_formulaClauses ),
label( outer_label ),
fullProof( outer_fullProof ),
ca(0),                                 // have an empty allocator
originalClauseData( outer_ca ),        // have a reference to the original clauses
workOnCopy( !keepOriginalClauses ),

nonMarkedWatches       ( ClauseDataClauseDeleted(ca) ),
markedWatches ( ClauseDataClauseDeleted(ca) ),
variables( outer_variables ),

markedQhead(0),
markedUnitHead(0), 
nonMarkedQHead(0), 
nonMarkedUnithead(0),

clausesToBeChecked(0),
num_props(0),
verifiedClauses(0)
{
}


void SequentialBackwardWorker::initialize(int64_t proofPosition, bool duplicateFreeClauses)
{
  if(workOnCopy) originalClauseData.copyTo(ca);
  else originalClauseData.moveTo( ca );
  
  cerr << "c [S-BW-CHK] initialize (copy?" << workOnCopy << "), clause storage elements: " << ca.size() << "(original: " << originalClauseData.size() << ") variables: " << variables << endl;
  
  nonMarkedWatches  .init(mkLit(variables, false)); // get storage for watch lists
  nonMarkedWatches  .init(mkLit(variables, true ));
  
  markedWatches     .init(mkLit(variables, false)); // get storage for watch lists
  markedWatches     .init(mkLit(variables, true));
  
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
  
  movedToMarked.growTo( fullProof.size(), 0 );
  
  // add all clauses that are relevant to the watches (all clauses that are valid at the latest point in time, the size of the proof + 1 )
  const int currentID = proofPosition;
  for( int i = 0 ; i < fullProof.size(); ++ i ) {
    if( fullProof[i].isValidAt( currentID ) ) {
      if( fullProof[i].getRef() == CRef_Undef ) { // jump over the empty clause
	if( i + 1 < fullProof.size() && i > formulaClauses ) cerr << "c WARNING: found an empty clause in the middle of the proof" << endl;
	continue;
      }
      
      enableProofItem( fullProof[i] ); // add corresponding clause to structures
    }
  }
}

void SequentialBackwardWorker::enableProofItem( const BackwardChecker::ClauseData& proofItem ) {
  const Clause& c = ca[ proofItem.getRef() ];
  if( c.mark() != 0 ) return;  // this clause has just been removed due to being a isTautology
      
  assert( c.size() > 0 && "there should not be empty clauses in the proof" );
  if( c.size() == 1 ) {
    nonMarkedUnitClauses.push( BackwardChecker::ClauseData( c[0], proofItem.getID(), proofItem.getValidUntil()  ) );
  } else {
    attachClause( proofItem );
  }
}

void SequentialBackwardWorker::release() {
  cerr << "c [S-BW-CHK] release (clear?" << workOnCopy << ")" << endl; 
  if (workOnCopy) ca.clear( true );      // free internal data
  else originalClauseData.moveTo( ca );  // move copied data back to other object
}

void SequentialBackwardWorker::attachClause(const BackwardChecker::ClauseData& cData)
{
  const Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
  nonMarkedWatches[~c[0]].push( cData );
  nonMarkedWatches[~c[1]].push( cData );
}

void SequentialBackwardWorker::uncheckedEnqueue(Lit p)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] enqueue literal " << p << endl;
  assigns[var(p)] = lbool(!sign(p));
  // prefetch watch lists
  __builtin_prefetch( & nonMarkedWatches[p] );
  trail.push_(p);
}


lbool    SequentialBackwardWorker::value (Var x) const   { return assigns[x]; }

lbool    SequentialBackwardWorker::value (Lit p) const   { return assigns[var(p)] ^ sign(p); }

void SequentialBackwardWorker::restart()
{
  for (int c = trail.size()-1; c >= 0; c--) assigns[var(trail[c])] = l_Undef;
  markedQhead = 0; nonMarkedUnithead = 0; nonMarkedQHead = 0; markedUnitHead = 0;
  trail.clear();
  
  if( verbose > 3 ) cerr << "c [S-BW-CHK] restart" << endl;
}

void SequentialBackwardWorker::removeBehind( const int& position )
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

bool SequentialBackwardWorker::checkClause(vec< Lit >& clause, int64_t untilProofPosition)
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
      cerr << "c item [" << i << "] id: " << fullProof[i].getID() << " valid until " << fullProof[i].getValidUntil() << "  clause: ";
      if(fullProof[i].getRef() == CRef_Undef ) cerr << "empty"; else cerr << ca[ fullProof[i].getRef() ] ;
      cerr << endl;
    }
    cerr << "c END FULL PROOF" << endl;
  }
  
  clausesToBeChecked = 0;
  vec<Lit> dummy;
  if (!checkSingleClauseAT( (int64_t) fullProof.size(), clause, dummy ) ) {
    if( verbose > 2 ) cerr << "c [S-BW-CHK] AT check failed" << endl;
    restart(); // clean trail again, reset assignments
    return false;
  }
  restart(); // clean trail again, reset assignments
  
  if( verbose > 1 ) cerr << "c [S-BW-CHK] check clauses that have been marked (" << clausesToBeChecked << ")" << endl;
  
  // check the bound that should be used
  const int64_t maxCheckPosition = formulaClauses > untilProofPosition ? formulaClauses : untilProofPosition;

  if( verbose > 2 ) cerr << "c [S-BW-CHK] check from position " << fullProof.size() -1 << " to " << maxCheckPosition << endl;
  for( int i = fullProof.size() - 1; i >= maxCheckPosition; -- i ) {
    
    BackwardChecker::ClauseData& proofItem = fullProof[i];
    
    if( proofItem.isDelete() ) {
      assert( ! label[ proofItem.getID() ].isMarked() && "deletion information cannot be marked" );
      enableProofItem( proofItem ); // activate this item in the data structures (watch lists, or unit list)
    } else if ( label[ proofItem.getID() ].isMarked() ) {
      
      if( ! checkSingleClauseAT( proofItem.getID(), ca[ proofItem.getRef() ], dummy ) ) {
	cerr << "c WARNING: clause with id " << proofItem.getID() << " cannot be verified with DRUP: " << ca[ proofItem.getRef() ] << endl;
	restart(); // clean trail again, reset assignments
	return false;
      }
      restart(); // clean trail again, reset assignments
      
      assert( clausesToBeChecked > 0 && "there had to be work before" );
      label[ proofItem.getID()  ].setVerified();
      clausesToBeChecked --; // we checked another clause
      
    } else {
      // nothing to do with unlabeled clauses, they stay in the watch list/full occurrence list until some garbage collection is performed
    }
    
  }
  restart(); // clean trail again, reset assignments
  return true;
}

bool SequentialBackwardWorker::markToBeVerified(const int64_t& proofItemID)
{
  if( label[ proofItemID ].isMarked() ) return false;
  if( verbose > 3 ) cerr << "c [S-BW-CHK] mark clause with ID " << proofItemID << endl;
  label[ proofItemID ].setMarked(); // TODO: might be made more thread safe
  clausesToBeChecked ++;            // set by ourself
  return true;
}

Riss::CRef SequentialBackwardWorker::propagateMarked(const int64_t currentID, bool addUnits)
{
  CRef    confl     = CRef_Undef;
  int     num_props = 0;
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
	    assert( c.size() > 2 && "at this point, only larger clauses should be handled!" );
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

Riss::CRef SequentialBackwardWorker::propagateUntilFirstUnmarkedEnqueueEager(const int64_t currentID)
{
  CRef    confl     = CRef_Undef;
  int     num_props = 0;
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
        Lit            p   = trail[nonMarkedQHead++];     // 'p' is enqueued fact to propagate.
        vec<BackwardChecker::ClauseData>&  ws  = nonMarkedWatches[p];
        BackwardChecker::ClauseData        *i, *j, *end;
        num_props++;
#warning: IMPROVE DATA STRUCTURES AS IN RISS WITH AN 'ISBINARY' INDICATOR, AND A BLOCKING LITERAL
        // propagate longer clauses here!
        for (i = j = (BackwardChecker::ClauseData*)ws, end = i + ws.size();  i != end;)
	{
	    if( ! i->isValidAt( currentID ) ) { ++i; continue; }       // simply remove this element from the watch list after the loop ended
	    if( movedToMarked[ i->getID() ] == 1 ) { ++i; continue; }  // this clause has been moved to the watch lists for marked clauses previously (with the other watched literal)
	    
	    
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
	    nonMarkedWatches[~c[0]].push(w);
	    nonMarkedWatches[~c[1]].push(w);
	    markToBeVerified( w.getID() );          // mark the unit clause - TODO: handle for parallel version: returns true, if this worker set the mark
	    movedToMarked[ w.getID() ] = 1; // indicate that this clause has been moved
	    
            // Copy the remaining watches:
            while (i < end)
               *j++ = *i++;
	    
	    return confl;
	    
        NextClause:;
        }
        ws.shrink(i - j); // remove all duplciate clauses!
  }
  return confl;  
}


