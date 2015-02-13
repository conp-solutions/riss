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
  
  int      markedHead, qhead;   // Head of queue,as index into the trail, one for marked propagation, one for non-marked propagation
  vec<Lit> trail; // Assignment stack; stores all assigments made in the order they were made.
  vec<BackwardChecker::ClauseData> unitClauses;  // vector of top level units (not yet marked)
  vec<BackwardChecker::ClauseData> markedUnits;  // vector of top level units (marked)
  
  vec<lbool> assigns; // current assignment, one space for each variable
  
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
   *  @param duplicateFreeClauses indicate whether checking for these clauses is relevant 
   */
  void initialize(bool duplicateFreeClauses = false);
  
  /** check whether the given clause is in the proof 
   * @param clause clause to be verified
   * @return true, if the proof is a valid unsatisfiability proof
   */
  bool checkClause( vec<Lit>& clause );
  
  /** check whether the proof is valid for the empty clause 
   * @return true, if the proof is a valid unsatisfiability proof
   */
  bool checkClause(){ vec<Lit> dummy; return checkClause( dummy ); }
  
  /** correct changes, outer usage of clause allocation object becomes safe again */
  void release();
  
protected: 
  
};

}

#endif // SEQUENTIALBACKWARDWORKER_H
