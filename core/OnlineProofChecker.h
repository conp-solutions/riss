/****************************************************************************[OnlineProofChecker.h]
Copyright (c) 2014, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef OnlineProofChecker_h
#define OnlineProofChecker_h

#include "mtl/Vec.h"
#include "mtl/Heap.h"
#include "mtl/Alg.h"
#include "utils/Options.h"
#include "utils/System.h"
#include "core/SolverTypes.h"
#include "core/Constants.h"

using namespace Minisat;


namespace Minisat {

/** class that can check DRUP/DRAT proof on the fly during executing the SAT solver 
 * Note: for DRAT clauses only the very first literal will be used for checking RAT!
 * (useful for debugging)
 */
class OnlineProofChecker {
public:
  
  /// to determine which proof style is used
  enum ProofStyle {
    drup = 0,
    drat = 1,
  };
  
protected:
  
  bool ok;		// indicate whether an empty clause has been in the input!
  ProofStyle proof; // current format
  ClauseAllocator ca;
  vec<CRef> clauses;
  vec<Lit> unitClauses; // store how many unit clauses have been seen already for a given literal (simply count per literal, use for propagation initialization!)
  vector< vector<CRef> > occ; // use for propagation!
  
  OccLists<Lit, vec<Solver::Watcher>, Solver::WatcherDeleted> watches; 
  
  MarkArray ma;	// array to mark literals (to find a clause, or for resolution)
  
  // two watched literal
  void attachClause (CRef cr); // Attach a clause to watcher lists.
  void detachClause (CRef cr); // Detach a clause to watcher lists.
  Var newVar (); // data structures for more variables are necessary
  int nVars() const; // number of current variables
  
  
  // unit propagation
  int      qhead; // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
  vec<Lit> trail; // Assignment stack; stores all assigments made in the order they were made.
  vec<Lit> lits;  // vector of literals for resolution
  vec<lbool> assigns; // current assignment
  
  lbool value (Var x) const;     // The current value of a variable.
  lbool value (Lit p) const;     // The current value of a literal.
  bool propagate ();             // Perform unit propagation. Returns possibly conflicting clause.
  void uncheckedEnqueue (Lit p); // Enqueue a literal. Assumes value of literal is undefined.
  void cancelUntil ();  // Backtrack until a certain level.
  
public:
  
  /** setup a proof checker with the given format */
  OnlineProofChecker( ProofStyle proofStyle );
  
  /** add a clause of the initial formula */
  void addParsedclause( vec<Lit>& cls ) ;
  
  /** add a clause during search 
   * Note: for DRAT clauses only the very first literal will be used for checking RAT!
   * @return true, if the addition of this clause is valid wrt. the current proof format
   */
  bool addClause( vec<Lit>& cls ) ;
  
  /** remove a clause during search */
  void removeClause( vec<Lit>& cls ) ;
  
};

inline
OnlineProofChecker::OnlineProofChecker(OnlineProofChecker::ProofStyle proofStyle) 
: ok(true), proof(proofStyle), watches (Solver::WatcherDeleted(ca)), qhead(0)
{}

inline 
void OnlineProofChecker::attachClause(Minisat::CRef cr)
{
    const Clause& c = ca[cr];
    assert(c.size() > 1 && "cannot watch unit clauses!");
    assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
    if(c.size()==2) {
      watches[~c[0]].push(Solver::Watcher(cr, c[1], 0)); // add watch element for binary clause
      watches[~c[1]].push(Solver::Watcher(cr, c[0], 0)); // add watch element for binary clause
    } else {
      watches[~c[0]].push(Solver::Watcher(cr, c[1], 1));
      watches[~c[1]].push(Solver::Watcher(cr, c[0], 1));
    }
}

inline
void OnlineProofChecker::detachClause(Minisat::CRef cr)
{
  const Clause& c = ca[cr];
  const int watchType = c.size()==2 ? 0 : 1; // have the same code only for different watch types!
  removeUnSort(watches[~c[0]], Solver::Watcher(cr, c[1],watchType)); 
  removeUnSort(watches[~c[1]], Solver::Watcher(cr, c[0],watchType)); 
}

inline 
int OnlineProofChecker::nVars() const
{
  return assigns.size();
}

inline 
Var OnlineProofChecker::newVar()
{
  int v = nVars();
  assigns.push( l_Undef );
  occ.push_back( vector<CRef> () ); // there are no new clauses here yet
  occ.push_back( vector<CRef> () );
  ma.resize( ma.size() + 2 ); // for each literal have a cell
  return v;
}

inline
void OnlineProofChecker::uncheckedEnqueue(Lit p)
{
  assigns[var(p)] = lbool(!sign(p));
  // prefetch watch lists
  __builtin_prefetch( & watches[p] );
  trail.push_(p);
}

inline
void OnlineProofChecker::cancelUntil()
{
  for (int c = trail.size()-1; c >= 0; c--) assigns[var(trail[c])] = l_Undef;
  qhead = 0;
  trail.clear();
}

inline 
lbool    OnlineProofChecker::value (Var x) const   { return assigns[x]; }
inline 
lbool    OnlineProofChecker::value (Lit p) const   { return assigns[var(p)] ^ sign(p); }

inline
bool OnlineProofChecker::propagate()
{
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll(); 
    // propagate units first!
    for( int i = 0 ; i < unitClauses.size() ; ++ i ) { // propagate all known units
      const Lit l = unitClauses[i];
      if( value(l) == l_True ) continue;
      else if (value(l) == l_False ) return true;
      else uncheckedEnqueue( l );
    }
    
    
    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Solver::Watcher>&  ws  = watches[p];
        Solver::Watcher        *i, *j, *end;
        num_props++;
	    // First, Propagate binary clauses 
	const vec<Solver::Watcher>&  wbin  = watches[p];
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
        for (i = j = (Solver::Watcher*)ws, end = i + ws.size();  i != end;)
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
            const Solver::Watcher& w     = Solver::Watcher(cr, first, 1); // updates the blocking literal
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
    
    return (confl != CRef_Undef); // return true, if something was found!
}

inline
void OnlineProofChecker::removeClause(vec< Lit >& cls)
{
  if( cls.size() == 0 || !ok ) return; // do not handle empty clauses here!
  if( cls.size() == 1 ) {
    
    return;
  } 
  // find correct CRef ...
  ma.nextStep();
  int smallestIndex = 0;
  ma.setCurrentStep( toInt( cls[0] ) );
  for( int i = 1; i < cls.size(); ++ i ) {
    ma.setCurrentStep( toInt( cls[i] ) );
    if( occ[ toInt( cls[i] ) ].size() < occ[smallestIndex].size() ) smallestIndex = i;
  }
  
  const Lit smallest = cls[smallestIndex];
  
  CRef ref = CRef_Undef;
  int i = 0 ; 
  for( ; i < occ[ toInt(smallest) ].size(); ++ i ) {
    const Clause& c = ca[ occ[toInt(smallest)][i] ];
    if( c.size() != cls.size() ) continue; // do not check this clause!
    
    int hit = 0;
    for( ; hit < c.size(); ++ hit ) {
      if( ! ma.isCurrentStep( toInt(c[hit]) ) ) break; // a literal is not in both clauses, not the correct clause
    }
    if( hit < c.size() ) continue; // not the right clause -> check next!

    ref = occ[toInt(smallest)][i];
    // remove from this occ-list
    occ[toInt(smallest)][i] = occ[toInt(smallest)][ occ[toInt(smallest)].size() - 1 ];
    occ[toInt(smallest)].pop_back();
    break;
  }
  if( i == occ[ toInt(smallest) ].size() ) {
    cerr << "c could not remove clause " << cls << " from list of literal " << smallest << endl;
  }
  
  // remove from the other occ-lists!
  for( int i = 0 ; i < cls.size(); ++ i ) {
    if( i == smallestIndex ) continue;
    vector<CRef>& list = occ[ toInt(cls[i]) ];
    int j = 0;
    for(  ; j < list.size(); ++ j ) {
      if( list[j] == ref ) { list[j] = list[ list.size() -1 ]; list.pop_back(); }
      break;
    }
    if( j == list.size() ) cerr << "c could not remove clause " << cls << " from list of literal " << cls[i] << endl;
  }
    
  // remove the clause from the two watched literal structures!
  detachClause( ref );
  
  // tell the garbage-collection-system that the clause can be removed
  ca[ref].mark(1); 
  ca.free(ref);
  
  // check garbage collection once in a while!
  // TODO
}

inline
void OnlineProofChecker::addParsedclause(vec< Lit >& cls)
{
  if( cls.size() == 0 ) { 
    ok = false;
    return;
  }
  
  // have enough variables present
  for( int i = 0 ; i < cls.size(); ++ i ) { 
    while( nVars() < var(cls[i]) ) newVar();
  }

  if( cls.size() > 1 ) {
    CRef ref = ca.alloc( cls, false );
    for( int i = 0 ; i < cls.size(); ++ i ) { 
      occ[ toInt( cls[i] ) ].push_back( ref );
    }
    attachClause( ref );
    clauses.push( ref );
  } else unitClauses.push( cls[0] );
  
  // here, do not check whether the clause is entailed, because its still input!
}

inline 
bool OnlineProofChecker::addClause(vec< Lit >& cls)
{
  if( !ok ) return true; // trivially true, we reached the empty clause already!
  bool conflict = false;
  
  const int initialVars = nVars();
  
  // have enough variables present
  for( int i = 0 ; i < cls.size(); ++ i ) { 
    while( nVars() < var(cls[i]) ) newVar();
  }
  
  // enqueue all complementary literals!
  cancelUntil();
  for( int i = 0 ; i < cls.size() ; ++ i ) {
    if( value( cls[i] ) == l_Undef ) { 
      uncheckedEnqueue( cls[i] );
    } else if( value(cls[i]) == l_False ) { conflict = true; break; }
  }
  
  if( propagate() ) conflict = true; // DRUP!
  else if( proof == drat ) { // are we checking DRAT?
    if( initialVars >= var(cls[0]) ) { // DRAT on the first variable, because this variable is not present before!
      // build all resolents on the first literal!
      ma.nextStep();
      lits.clear();
      for( int i = 1 ; i < cls.size() ; ++ i ) { 
	ma.setCurrentStep( toInt( cls[i] )); // mark all except the first literal!
	lits.push( cls[i] );
      }
      const int initialSize = lits.size(); // these literals are added by resolving with  the current clause on its first literal
      const Lit resolveLit = cls[0];
      conflict = true;
      const vector<CRef>& list = occ[ toInt(resolveLit) ];
      bool resovleConflict = false;
      for( int i = 0 ; i < list.size(); ++ i ) {
	// build resolvent
	lits.shrink( lits.size() - initialSize ); // remove literals from previous addClause call
	const Clause& d = ca[ list[i] ];
	int j = 0;
	for(  ; j < d.size(); ++ j ) {
	  if( d[j] == ~resolveLit ) continue; // this literal is used for resolution
	  if( ma.isCurrentStep( toInt( ~d[j] ) ) ) break; // resolvent is a tautology
	  if( !ma.isCurrentStep( toInt(d[j]) ) ) {
	    ma.setCurrentStep( toInt(d[j]) );	// this step might be redundant -- however, here we can ensure that also clauses with duplicate literals can be handled, as well as tautologic clauses!
	    lits.push(d[j]);
	  }
	}
	if( j != d.size() ) continue; // this resolvent would be tautological!
	// lits contains all literals of the resolvent, propagate and check for the conflict!
	
	// enqueue all complementary literals!
	resovleConflict = false;
	cancelUntil();
	for( int k = 0 ; k < lits.size() ; ++ k ) {
	  if( value( lits[k] ) == l_Undef ) { 
	    uncheckedEnqueue( lits[k] );
	  } else if( value(lits[k]) == l_False ) { resovleConflict = true; break; } // the clause itself is tautological ...
	}
	if( !propagate() ) {
	  conflict = false; // not DRAT, the current resolvent does not lead to a conflict!
	  cerr << "c the clause " << cls << " is not a DRAT clause -- resolution on " << resolveLit << " with " << d << " failed! (does not result in a conflict with UP)" << endl;
	  break;
	}
      }
    } else conflict = true;
  } else {
    cerr << "c the clause " << cls << " is not a DRUP clause" << endl;
  }
  
  // add the clause ... 
  if( cls.size() == 0 ) { 
    ok = false;
    return true;
  }

  if( cls.size() > 1 ) {
    CRef ref = ca.alloc( cls, false );
    for( int i = 0 ; i < cls.size(); ++ i ) { 
      occ[ toInt( cls[i] ) ].push_back( ref );
    }
    attachClause( ref );
    clauses.push( ref );
  } else unitClauses.push( cls[0] );
  return true;
}

};

#endif