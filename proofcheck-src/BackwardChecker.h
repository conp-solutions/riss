/*******************************************************************************[BackwardChecker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#ifndef RISS_BACKWARD_H
#define RISS_BACKWARD_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"

namespace Riss {
  
class SequentialBackwardWorker;
  
/** verify a given proof with respect to a given formula in a backward fashion
 */
class BackwardChecker 
{
public:
  
  /** cumulate all the data that is necessary for a clause in a proof */
  class ClauseData {         // 16 byte
    union { 
      CRef ref;              // reference to the clause in the allocator. abstraction in the clause stores the firstLiteral of the clause
      Lit lit;               // literal of a unit clause (used during propagation)
    } data;
    unsigned long id :48;         // id of the clause (the clause has been added to the formula/proof at this step)
    unsigned long validUntil :48; // indicate id until this clause is valid
  public:
    /** initially, the item is never valid (only at the very last point in the proof )*/
    ClauseData () : id( (~(0ull)) ), validUntil( (~(0ull)) ) { data.ref = CRef_Undef; }
    ClauseData (const CRef& outer_ref, const int64_t& outer_id) 
    : id(outer_id ), validUntil( (~(0ull)) ) {
      data.ref = outer_ref;
      assert( outer_id <= ( 281474976710655 ) && "stay in precision" );
    }
    ClauseData (const Lit& outer_lit, const int64_t& outer_id, const int64_t outer_validUntil) 
    : id(outer_id ), validUntil( outer_validUntil ) {
      data.lit = outer_lit;
      assert( outer_id <= ( 281474976710655 ) && "stay in precision" );
      assert( outer_validUntil <= ( 281474976710655 ) && "stay in precision" );
    }
    
    /** getters */
    Lit getLit() const { return data.lit; }
    CRef getRef() const { return data.ref; }
    int64_t getID() const { return id; }
    int64_t getValidUntil() const { return validUntil; }
    /** check whether the clause is valid to a given ponit in the proof */
    bool isValidAt( const int64_t& clauseID ) const { return clauseID >= getID() && clauseID < getValidUntil(); }  
    /** get type of the entry in the proof */
    bool isDelete() const { return validUntil == 281474976710654; } // 2 ^ 48 - 2
    bool isEmptyClause() const { return data.ref == CRef_Undef; } 
    
    /** setters */
    void setCRef( const CRef& cref )               { data.ref = cref; }
    void setLit ( const Lit& lit )                 { data.lit = lit; }
    void setID( const int64_t outer_id )           { id = outer_id; }
    void setInvalidation( const int64_t outer_id ) { validUntil = outer_id; }
    void setIsDelete()                             { validUntil = 281474976710654; }
    void setIsEmpty()                              { data.ref = CRef_Undef; }
  };

  /** indicator of the state of a clause in the proof / formula (marked, verified, ...)*/
  class ClauseLabel {
    uint8_t x;
  public:
    ClauseLabel() : x(0) {};
    void setMarked()        { x = x | 1; } /// set mark, not thread safe
    void setVerified()      { x = x | 2; } /// set mark, not thread safe
    bool isMarked()   const { return ( x & 1 ) != 0; }
    bool isVerified() const { return ( x & 2 ) != 0; }
  };
  
protected:
  
  /** necessary object for watch list */
  struct ClauseDataDeleted
  {
    bool operator()(const ClauseData& w) const { return false; }
  };
  
  // data structures
  int formulaClauses;        // number of clauses in the formula (these clause do not have to be verified)
  vec<ClauseLabel> label;    // stores the label per clause in the formula and proof
  vec<ClauseData> fullProof; // store the data of the proof (including all clauses of the formula)
  ClauseAllocator ca;        // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more

  SequentialBackwardWorker* sequentialChecker;  // pointer to sequential checking object

  vec<int> clauseCount;      // count number of occurrences of a clause that is present in the formula (to be able to merge duplicates)
  OccLists<Lit, vec<ClauseData>, ClauseDataDeleted> oneWatch; // one watch list
   
  // operation options
  bool drat;                   // verify drat
  bool fullRAT;                // do check RAT only for the first literal?
  int  threads;                // number of threads that should be used for the analysis
  int  checkDuplicateLits;     // how to handle duplicate literals in clauses (0 = ignore, 1 = warn, 2 = remove during parsing)
  int  checkDuplicateClauses;  // how to handle duplicate clauses in the proof (0 = ignore, 1 = warn, 2 = merge during parsing)
  int  verbose;                // how verbose the tool should be
  
  // operation data
  int variables;                   // number of seen variables
  bool hasBeenInterupted;          // indicate external interupt
  bool readsFormula;               // indicate that we are still parsing the formula
  vec<int> presentLits;            // to check input clauses for duplicates and to compare clauses
  int64_t currentID;               // id of the clause that is added next
  bool sawEmptyClause;             // memorize that an empty clause has been parsed
  bool formulaContainsEmptyClause; // memorize that we parsed an empty clause in the formula
  bool inputMode;                  // we expect more clauses to be added
  
  // statistics
  int64_t duplicateClauses, clausesWithDuplicateLiterals, mergedElement;
  
  
public:
  /** create backward checking object */  
  BackwardChecker ( bool opt_drat, int opt_threads, bool opt_fullRAT );
  
  /** allocate space for the given number of variables in the data structures (only during parsing)*/
  void reserveVars(int newVariables); 

  /** tell system that there is one more variable
   * @return highest variable that is in the system now (variables range from 0 - N-1)
   */
  int newVar();
  
  /** set an interupt flag, such that verifying the proof can be interupted from the outside */
  void interupt();
  
  /** add the given clause to the data structures for the proof 
   * @param ps clause to be added, Note: will be modified (sorted)
   * @param proofClause indicate that this clause belongs to the proof
   * @param isDelete the curent entry is deletes the clause from the proof
   * @return true, if adding the clause is fine (during addition, no checks are done in backward checking)
   */
  bool addProofClause(vec< Lit >& ps, bool proofClause, bool isDelete = false);
  
  /** check whether the given clause is entailed in the proof already 
   * @param clause clause to be checked
   * @param drupOnly perform only drup check, even if everything is prepared for DRAT
   * @param clearMarks resets all marks that have been made durnig the verification
   * @return true, if the clause can be added to the proof
   */
  bool checkClause(vec< Lit >& clause, bool drupOnly = false, bool clearMarks = false);
  
  /** tells the checker that there will not be any further clauses to be read
   *  will destroy the onewatch data structure, and set an according flag
   * @return false, if no empty clause was added to the proof
   */
  bool prepareVerification();
    
protected:


  
};

}

#endif