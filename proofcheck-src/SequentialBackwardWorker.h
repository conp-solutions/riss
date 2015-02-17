/**********************************************************************[SequentialBackwardWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#ifndef SEQUENTIALBACKWARDWORKER_H
#define SEQUENTIALBACKWARDWORKER_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"

#include "proofcheck-src/BackwardChecker.h"

#include <deque> // to have a queue for LIFO and FIFO

namespace Riss {

/** given an internal representation of a DRAT/DRUP proof, this class performs backward checking */
class SequentialBackwardWorker
{
public:
  
  enum ParallelMode {
    none = 0,    // sequential execution
    shared = 1,  // the clause data base (and the full occurrence information) is shared for read only access
    copy = 2,    // each worker has his (private) copy of the data structures
  };
  
protected:
  
  bool drat;                    // check drat
  ParallelMode  parallelMode;   // how to operate on the data
  int verbose;                  // verbosity of the tool
  
  // data structures that are re-used from the outside
  int formulaClauses;                          // number of clauses in the formula (these clause do not have to be verified)
  vec<BackwardChecker::ClauseLabel>& label;    // stores the label per clause in the formula and proof
  vec<BackwardChecker::ClauseData>& fullProof; // store the data of the proof (including all clauses of the formula)
  ClauseAllocator  ca;                         // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  ClauseAllocator& originalClauseData;         // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  bool workOnCopy;                             // indicate whether we are really working on the storage, or on a copy
  std::deque< int64_t > idsToVerify;           // queue of elements that have to be verified by this worker
  
  // data structures for propagation
  
  /** necessary object for watch list */
  
  struct ClauseDataClauseDeleted
  {
    const ClauseAllocator& ca;
    ClauseDataClauseDeleted(const ClauseAllocator& _ca) : ca(_ca) {}
    bool operator()(const BackwardChecker::ClauseData& w) const { return ca[ w.getRef() ].mark() == 1; }
  };
  
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> nonMarkedWatches;  /// watch list for all non-marked clauses
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> markedWatches;     /// watch list for all marked clauses
  int variables;                                                                              /// number of variables in the proof (highest variable)

  int      markedQhead, markedUnitHead;       // Head of queue,as index into the trail, also for units
  int      nonMarkedQHead, nonMarkedUnithead; // one for marked propagation, one for non-marked propagation

  vec<Lit> trail; // Assignment stack; stores all assigments made in the order they were made.
  vec<BackwardChecker::ClauseData> nonMarkedUnitClauses;  // vector of top level units (not yet marked)
  vec<BackwardChecker::ClauseData> markedUnitClauses;     // vector of top level units (marked)
  
  vec<lbool> assigns; // current assignment, one space for each variable
  
  vec<char> movedToMarked; // indicate that this thread marked a certain clause during propagation on a watch alredy
  
  // data and statistics
  int clausesToBeChecked;  // number of marked clauses in the proof
  int num_props;           // number of propagated literals
  int verifiedClauses;     // number of verified clauses
  
public:
  
  /** setup checker, use data structures that have been set up before from the outside 
   *  @param keepOriginalClauses the internal representation removes duplicate literals, hence the checker would copy the whole clause data base
   */
  SequentialBackwardWorker( bool opt_drat, 
			    ClauseAllocator& outer_ca, 
			    vec<BackwardChecker::ClauseData>& outer_fullProof, 
			    vec<BackwardChecker::ClauseLabel>& outer_label, 
			    int outer_formulaClauses, 
			    int outer_variables, 
			    bool keepOriginalClauses );
    
  /** setup the internal data structures before performing verification 
   *  Note: might work on the outer clause allocator (that has been passed to the constructor before)
   *  @param proofPosition position in the proof from which on the current object should make its checks
   *  @param duplicateFreeClauses indicate whether checking for these clauses is relevant 
   */
  void initialize(int64_t proofPosition, bool duplicateFreeClauses = false);
  
  /** check whether the given clause is in the proof 
   * @param clause clause to be verified
   * @param untilProofPosition perform partial check until the specified position (when not using chronological, this might leave unverified clauses behind)
   * @param returnAfterInitialCheck check only the given clause - if its check succeeds, return true
   * @return true, if the proof is a valid unsatisfiability proof (or the given claus can be added to the current proof)
   */
  bool checkClause( vec<Lit>& clause, int64_t untilProofPosition = 0, bool returnAfterInitialCheck = false);
  
  /** check whether the proof is valid for the empty clause 
   * @param untilProofPosition perform partial check until the specified position
   * @return true, if the proof is a valid unsatisfiability proof
   */
  bool checkClause(int64_t untilProofPosition = -1){ vec<Lit> dummy; return checkClause( dummy, untilProofPosition ); }
  
  /** correct changes, outer usage of clause allocation object becomes safe again */
  void release();
  
protected: 

  /** add the clause to the watch lists of the checker
   * @param cData proof item data of the current clause (including time steps of being valid)
   */
  void attachClause( const BackwardChecker::ClauseData& cData);
  
  /** add clause related to given proofItem to the active data  structures
   */
  void enableProofItem( const BackwardChecker::ClauseData& proofItem );
  
  /** add a literal to the trail and the assignment structure 
   * Note: does not check for a conflict
   */
  void uncheckedEnqueue(Lit p);
  
  /** The current value of a variable. */
  lbool value (Var x) const;     
  
  /** The current value of a literal. */
  lbool value (Lit p) const;    
  
  /** clear the full assignment information */
  void restart();
  
  /** remove all literals behind a given position */
#warning USE FOR DRAT and checking multiple resolvents
  void removeBehind( const int& position );

  /** modify all data structures so that its known that this clause has to be marked 
   * @return true, if this worker marked the clause, false, if already marked before
   */
  bool markToBeVerified( const int64_t& proofItemID );
  
  /** propagate only on valid already marked clauses and unit clauses
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @param addUnits do add Units only when the this method is called the first time for checking a clause (otherwise, units are scanned again and again)
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateMarked(const int64_t currentID, bool addUnits = false);

  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued -- first will be used
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueueEager(const int64_t currentID);

  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued -- all are collected and a decision is made afterwards
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueueManaged(const int64_t currentID);
  
  /** check AT of the given clause with respect to the partial proof until element currentID
   * @param currentID the position of that clause in the proof
   * @param c the clause to be checked
   * @param extraLits literals that belong to the clause (e.g. after resolution)
   * @param reuseClause do not re-enqueue the literals of c again, as it is ensured that those literals have already been tested
   * @return true, if the verification of this clause is ok
   */
  template<class ClauseType>
  bool checkSingleClauseAT( const int64_t currentID, const ClauseType& c, vec<Lit>& extraLits, bool reuseClause = false );
  
};

template<class ClauseType>
bool SequentialBackwardWorker::checkSingleClauseAT(const int64_t currentID, const ClauseType& c, vec< Lit >& extraLits, bool reuseClause )
{
  assert( (reuseClause || trail.size() == 0) && "there cannot be assigned literals, other than the reused literals" );
  nonMarkedQHead = 0; // there might have been more marked clauses in between, so start over
  markedUnitHead = 0;
  
  if( verbose > 2 ) cerr << endl << endl << "c [S-BW-CHK] check AT for clause " << c << " " << extraLits << " at proof position " << currentID << " (reuse: " << reuseClause << ")" << endl;
  
  if( !reuseClause ) {
    for( int i = 0 ; i < c.size(); ++ i ) { // enqueue all complements
      if( value( ~c[i] ) == l_False ) return true;                 // there is a conflict
      else if( value( ~c[i] ) == l_Undef ) uncheckedEnqueue( ~c[i] );   
    }
  }
  
  for( int i = 0 ; i < extraLits.size() ; ++ i ) {
    if( value( ~extraLits[i] ) == l_False ) return true;                 // there is a conflict
    else if( value( ~extraLits[i] ) == l_Undef ) uncheckedEnqueue( ~extraLits[i] );   
  }
  
  CRef confl = CRef_Undef;
  bool firstCall = true;
  do {
    
    if( verbose > 4 ) {
      cerr << "c trail: " << trail.size() << " markedUnits: " << markedUnitClauses.size() << " non-markedUnits: " << nonMarkedUnitClauses.size() << endl;
      cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
      cerr << "c trail literals: " << trail << endl;
    }
    
    // propagate on all marked clauses we already collected
    confl = propagateMarked( currentID, firstCall );
    assert( (confl != CRef_Undef || markedQhead == trail.size()) && "visitted all literals during propagation" );
    assert( (confl != CRef_Undef || markedUnitHead >= markedUnitClauses.size()) && "visitted all marked unit clauses" );
    firstCall = false;
    // if we found a conflict, stop
    if( confl != CRef_Undef ) break;
    // end propagate on marked clauses
    
    if( verbose > 4 ) {
      cerr << "c trail: " << trail.size() << " markedUnits: " << markedUnitClauses.size() << " non-markedUnits: " << nonMarkedUnitClauses.size() << endl;
      cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
      cerr << "c trail literals: " << trail << endl;
    }
    
    // if there is not yet a conflict, enqueue the first non-marked unit clause
    // will move clauses from non-marked to marked, but this is ok, as we will backtrack after this call has finished
    confl = propagateUntilFirstUnmarkedEnqueueEager( currentID );
    // if we found a conflict, stop
    if( confl != CRef_Undef ) break;
    
  } while ( nonMarkedQHead < trail.size() || nonMarkedUnithead < nonMarkedUnitClauses.size() ); // saw all clauses that might be interesting

  if( verbose > 2 ) cerr << "c [S-BW-CHK] returned conflict reference: " << confl << endl;
  
  // return true, if there was a conflict (then the clause has AT)
  return confl != CRef_Undef;
}


}

#endif // SEQUENTIALBACKWARDWORKER_H
