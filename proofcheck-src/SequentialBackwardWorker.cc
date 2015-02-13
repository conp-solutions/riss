/**********************************************************************[SequentialBackwardWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck-src/SequentialBackwardWorker.h"

using namespace Riss;

SequentialBackwardWorker::SequentialBackwardWorker(bool opt_drat, ClauseAllocator& outer_ca, vec< BackwardChecker::ClauseData >& outer_fullProof, vec< BackwardChecker::ClauseLabel >& outer_label, int outer_formulaClauses, int outer_variables, bool keepOriginalClauses)
:
drat( opt_drat ),
verbose(0), 
#warning ADD A VERBOSITY OPTION TO THE TOOL
formulaClauses( outer_formulaClauses ),
label( outer_label ),
fullProof( outer_fullProof ),
ca(0),                                 // have an empty allocator
originalClauseData( outer_ca ),        // have a reference to the original clauses
workOnCopy( !keepOriginalClauses ),

watches ( ClauseDataClauseDeleted(ca) ),
variables( outer_variables ),

markedHead(0), 
markedUnitHead(0), 
qhead(0),

clausesToBeChecked(0),
num_props(0),
verifiedClauses(0)
{
}


void SequentialBackwardWorker::initialize(int64_t proofPosition, bool duplicateFreeClauses)
{
  if(workOnCopy) originalClauseData.copyTo(ca);
  else originalClauseData.moveTo( ca );
  
  watches  .init(mkLit(variables, false)); // get storage for watch lists
  watches  .init(mkLit(variables, true ));
  
  assigns.growTo( variables );
  trail.growTo( variables );  // initial storage allows to use push_ instead of push. will be faster, as there is no capacity check
  
  if( !duplicateFreeClauses ) {
    vec<char> presentLiterals ( 2*variables );
    for( int i = 0; i < fullProof.size() ; ++ i ) { // check each clause for duplicates
      if( fullProof[i].isDelete() ) continue; // do not look at this type
      
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
  
  // add all clauses that are relevant to the watches (all clauses that are valid at the latest point in time, the size of the proof + 1 )
  const int currentID = proofPosition;
  for( int i = 0 ; i < fullProof.size(); ++ i ) {
    if( fullProof[i].isValidAt( currentID ) ) {
      if( fullProof[i].getRef() == CRef_Undef ) { // jump over the empty clause
	if( i + 1 < fullProof.size() ) {
	  cerr << "c WARNING: found an empty clause in the middle of the proof" << endl;
	}
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
    unitClauses.push( BackwardChecker::ClauseData( c[0], proofItem.getRef(), proofItem.getValidUntil()  ) );
  } else {
    attachClause( proofItem );
  }
}

void SequentialBackwardWorker::release() {
  if (workOnCopy) ca.clear( true );      // free internal data
  else originalClauseData.moveTo( ca );  // move copied data back to other object
}

void SequentialBackwardWorker::attachClause(const BackwardChecker::ClauseData& cData)
{
  const Clause& c = ca[ cData.getRef() ];
  assert(c.size() > 1 && "cannot watch unit clauses!");
  assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
  watches[~c[0]].push( cData );
  watches[~c[1]].push( cData );
}

void SequentialBackwardWorker::uncheckedEnqueue(Lit p)
{
  if( verbose > 3 ) cerr << "c [S-BW-CHK] enqueue literal " << p << endl;
  assigns[var(p)] = lbool(!sign(p));
  // prefetch watch lists
  __builtin_prefetch( & watches[p] );
  trail.push_(p);
}


lbool    SequentialBackwardWorker::value (Var x) const   { return assigns[x]; }

lbool    SequentialBackwardWorker::value (Lit p) const   { return assigns[var(p)] ^ sign(p); }

void SequentialBackwardWorker::restart()
{
  for (int c = trail.size()-1; c >= 0; c--) assigns[var(trail[c])] = l_Undef;
  qhead = 0; markedHead = 0; markedUnitHead = 0;
  trail.clear();
}

void SequentialBackwardWorker::removeBehind( const int& position )
{
  assert( position >= 0 && position < trail.size() && "can only undo to a position that has been seen already" );
  for (int c = trail.size()-1; c >= position; c--) assigns[var(trail[c])] = l_Undef;
  qhead = qhead < position ? qhead : position; 
  markedHead = 0; // there might have been more marked clauses
  markedUnitHead = 0;
  if( position < trail.size() ) trail.shrink( trail.size() - position );
}

bool SequentialBackwardWorker::checkClause(vec< Lit >& clause, int64_t untilProofPosition)
{
  clausesToBeChecked = 0;
  vec<Lit> dummy;
  if (!checkSingleClause( (int64_t) fullProof.size(), clause, dummy ) ) return false;
  
  // check the bound that should be used
  const int64_t maxCheckPosition = formulaClauses > untilProofPosition ? formulaClauses : untilProofPosition;

  for( int i = fullProof.size() - 1; i >= maxCheckPosition; -- i ) {
    
    BackwardChecker::ClauseData& proofItem = fullProof[i];
    
    if( proofItem.isDelete() ) {
      assert( ! label[ proofItem.getID() ].isMarked() && "deletion information cannot be marked" );
      enableProofItem( proofItem ); // activate this item in the data structures (watch lists, or unit list)
    } else if ( label[ proofItem.getID() ].isMarked() ) {
      
      if( ! checkSingleClause( proofItem.getID(), ca[ proofItem.getRef() ], dummy ) ) {
	cerr << "c WARNING: clause with id " << proofItem.getID() << " cannot be verified with DRUP: " << ca[ proofItem.getRef() ] << endl;
	return false;
      }
      
      assert( clausesToBeChecked > 0 && "there had to be work before" );
      label[ proofItem.getID()  ].setVerified();
      clausesToBeChecked --; // we checked another clause
      
    } else {
      // nothing to do with unlabeled clauses, they stay in the watch list/full occurrence list until some garbage collection is performed
    }
    
  }
  return true;
}

void SequentialBackwardWorker::markToBeVerified(const int64_t& proofItemID)
{
  if( label[ proofItemID ].isMarked() ) return;
  label[ proofItemID ].setMarked();
}


bool SequentialBackwardWorker::checkSingleClause(const int64_t currentID, const Clause& c, vec< Lit >& extraLits, bool reuseClause )
{
  assert( (reuseClause || trail.size() == 0) && "there cannot be assigned literals, other than the reused literals" );
  markedHead = 0; // there might have been more marked clauses in between, so start over
  
  if( !reuseClause ) {
    for( int i = 0 ; i < c.size(); ++ i ) { // enqueue all complements
      if( value( ~c[i] ) == l_False ) return true;                 // there is a conflict
      if( value( ~c[i] ) == l_Undef ) uncheckedEnqueue( ~c[i] );   // there is a conflict
    }
  }
  
  for( int i = 0 ; i < extraLits.size() ; ++ i ) {
    if( value( ~extraLits[i] ) == l_False ) return true;                 // there is a conflict
    if( value( ~extraLits[i] ) == l_Undef ) uncheckedEnqueue( ~extraLits[i] );   // there is a conflict
  }
  
  CRef confl = CRef_Undef;
  do {
    


     // end propagate on marked clauses
    
  } while ( markedHead < trail.size() );

}

Riss::CRef SequentialBackwardWorker::propagateMarked(bool addUnits = false)
{
  CRef    confl     = CRef_Undef;
  int     num_props = 0;
  watches.cleanAll(); 
  
  if( verbose > 3 ) cerr << "c [DRAT-OTFC] propagate ... " << endl;
#error check validation information, and modify data structures accordingly!
  // propagate units first!
  for( int i = 0 ; i < unitClauses.size() ; ++ i ) { // propagate all known units
    const Lit l = unitClauses[i];
    if( value(l) == l_True ) continue;
    else if (value(l) == l_False ) return true;
    else uncheckedEnqueue( l );
  }
  
  while( qhead < trail.size() ) {
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;
	    // First, Propagate binary clauses 
	const vec<Watcher>&  wbin  = watches[p];
	for(int k = 0;k<wbin.size();k++) {
	  if( !wbin[k].isBinary() ) continue;
	  const Lit& imp = wbin[k].blocker();
	  assert( ca[ wbin[k].cref() ].size() == 2 && "in this list there can only be binary clauses" );
	  if(value(imp) == l_False) {
	    return true;
	    break;
	  }
	  
	  if(value(imp) == l_Undef) {
	    uncheckedEnqueue(imp);
	  } 
	}

        // propagate longer clauses here!
        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;)
	{
	    if( i->isBinary() ) { *j++ = *i++; continue; } // skip binary clauses (have been propagated before already!}
	    assert( ca[ i->cref() ].size() > 2 && "in this list there can only be clauses with more than 2 literals" );
	    
            // Try to avoid inspecting the clause:
            const Lit blocker = i->blocker();
            if (value(blocker) == l_True){ // keep binary clauses, and clauses where the blocking literal is satisfied
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            const CRef cr = i->cref();
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
	    assert( c.size() > 2 && "at this point, only larger clauses should be handled!" );
            const Watcher& w     = Watcher(cr, first, 1); // updates the blocking literal
            if (first != blocker && value(first) == l_True) // satisfied clause
	    {
	      *j++ = w; continue; } // same as goto NextClause;

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!
		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // if( config.opt_printLhbr ) cerr << "c keep clause (" << cr << ")" << c << " in watch list while propagating " << p << endl;
            if ( value(first) == l_False ) {
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                qhead = trail.size();
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
}

Riss::CRef SequentialBackwardWorker::propagateUntilFirstUnmarkedEnqueue()
{
  // use the following
//   markedUnitHead 
//   markedHead
}


