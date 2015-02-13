/**********************************************************************[SequentialBackwardWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#ifndef SEQUENTIALBACKWARDWORKER_H
#define SEQUENTIALBACKWARDWORKER_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"

#include "proofcheck-src/BackwardChecker.h"

namespace Riss {

/** given an internal representation of a DRAT/DRUP proof, this class performs backward checking */
class SequentialBackwardWorker
{
  bool drat;
  int verbose;
  
  // data structures that are re-used from the outside
  int formulaClauses;                          // number of clauses in the formula (these clause do not have to be verified)
  vec<BackwardChecker::ClauseLabel>& label;    // stores the label per clause in the formula and proof
  vec<BackwardChecker::ClauseData>& fullProof; // store the data of the proof (including all clauses of the formula)
  ClauseAllocator  ca;                         // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  ClauseAllocator& originalClauseData;         // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  bool workOnCopy;                             // indicate whether we are really working on the storage, or on a copy
  
  // data structures for propagation
  
  /** necessary object for watch list */
  
  struct ClauseDataClauseDeleted
  {
    const ClauseAllocator& ca;
    ClauseDataClauseDeleted(const ClauseAllocator& _ca) : ca(_ca) {}
    bool operator()(const BackwardChecker::ClauseData& w) const { return ca[ w.getRef() ].mark() == 1; }
  };
  
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> watches; // watch list
  int variables;                // number of variables in the proof (highest variable)
  
  int      markedHead, markedUnitHead, qhead;   // Head of queue,as index into the trail, one for marked propagation, one for non-marked propagation, also for non-marked units
  vec<Lit> trail; // Assignment stack; stores all assigments made in the order they were made.
  vec<BackwardChecker::ClauseData> unitClauses;  // vector of top level units (not yet marked)
  vec<BackwardChecker::ClauseData> markedUnits;  // vector of top level units (marked)
  
  vec<lbool> assigns; // current assignment, one space for each variable
  
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
   * @param untilProofPosition perform partial check until the specified position
   * @return true, if the proof is a valid unsatisfiability proof
   */
  bool checkClause( vec<Lit>& clause, int64_t untilProofPosition = 1);
  
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

  /** modify all data structures so that its known that this clause has to be marked */
  void markToBeVerified( const int64_t& proofItemID );
  
  /** propagate only on valid already marked clauses and unit clauses
   * @param addUnits do add Units only when the this method is called the first time for checking a clause (otherwise, units are scanned again and again)
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateMarked(bool addUnits = false);

  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueue();
  
  /** check the given clause with the partial proof until currentID
   * @param currentID the position of that clause in the proof
   * @param c the clause to be checked
   * @param extraLits literals that belong to the clause (e.g. after resolution)
   * @param reuseClause do not re-enqueue the literals of c again, as it is ensured that those literals have already been tested
   * @return true, if the verification of this clause is ok
   */
#error use a template for the clause c, so that also vectors can be used
  bool checkSingleClause( const int64_t currentID, const Clause& c, vec<Lit>& extraLits, bool reuseClause = false );
  
};

}

#endif // SEQUENTIALBACKWARDWORKER_H
