/*******************************************************************************[BackwardChecker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#ifndef RISS_BACKWARD_H
#define RISS_BACKWARD_H

#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/utils/System.h"

#include "riss/mtl/Map.h"

namespace Riss {
  
class ThreadController; 

#ifdef ONEWATCHLISTCHECKER
  class BackwardVerificationOneWatch;
#else
  class BackwardVerificationWorker;
#endif

  /** cumulate all the data that is necessary for a clause in a proof */
  class ProofClauseData {         // 16 byte
    union { 
      CRef ref;              // reference to the clause in the allocator. abstraction in the clause stores the firstLiteral of the clause
      Lit lit;               // literal of a unit clause (used during propagation)
    } data;
    unsigned long id :48;         // id of the clause (the clause has been added to the formula/proof at this step)
    unsigned long validUntil :48; // indicate id until this clause is valid
  public:
    /** initially, the item is never valid (only at the very last point in the proof )*/
    ProofClauseData () : id( (~(0ull)) ), validUntil( (~(0ull)) ) { data.ref = CRef_Undef; }
    ProofClauseData (const CRef& outer_ref, const int64_t& outer_id) 
    : id(outer_id ), validUntil( (~(0ull)) ) {
      data.ref = outer_ref;
      assert( outer_id <= ( 281474976710655 ) && "stay in precision" );
    }
    ProofClauseData (const Lit& outer_lit, const int64_t& outer_id, const int64_t outer_validUntil) 
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
  class ProofClauseLabel {
    uint8_t x;
  public:
    ProofClauseLabel() : x(0) {};
    void setMarked()        { x = x | 1; } /// set mark, not thread safe
    void setVerified()      { x = x | 2; } /// set mark, not thread safe
    bool isMarked()   const { return ( x & 1 ) != 0; }
    bool isVerified() const { return ( x & 2 ) != 0; }
  };

/** verify a given proof with respect to a given formula in a backward fashion
 */
class BackwardChecker 
{
public:
  
protected:
  
  struct Statistics {
    int64_t checks;
    int64_t prop_lits;
    int64_t max_marked;
    int64_t RATchecks;
    int64_t reusedOthersMark;
    int checkedByOtherWorker;  // elements that have been verified by another worker
    int resolutions;           // number of resolution steps that are done in this worker
    int removedLiterals;       // number of literals in a clause that have not been needed to verify that clause
    int redundantPropagations; // propagated literals that have not been useful for the actual verification of the clauses
    int maxSpace;              // maximal resolution-space that has been used during the verification with this worker
    /// simple constructor that sets all statistics to 0
    Statistics() : checks(0),prop_lits(0), max_marked(0), RATchecks(0), reusedOthersMark(0), checkedByOtherWorker(0), resolutions(0), removedLiterals(0),redundantPropagations(0), maxSpace(0) {}
  };
  
  vec<Statistics> statistics; // statistics per thread
  int proofWidth, proofLength, unsatisfiableCore; // statistics about verified proof
  Clock verificationClock;    // clock that measures the verification time
  
  /** data that is passed to the parallel job */
  struct WorkerData {
    
#ifdef ONEWATCHLISTCHECKER
  BackwardVerificationOneWatch* worker; // pointer to the actual worker intantiation
#else
  BackwardVerificationWorker* worker;   // pointer to the actual worker intantiation
#endif
    lbool result;                       // pointer to the result field of this worker
    char dummy [ 55 ];                  // makes sure that the element is alone on its cache line
  };
  
  /** struct to speed up clause look up */
  struct ClauseHash {
    ProofClauseData cd;
    uint64_t hash ;
    uint32_t size ;
    uint32_t dummy;
    ClauseHash ()  : cd( ProofClauseData() ), hash(0), size(0), dummy(0) {}
    ClauseHash (const ProofClauseData _cd, uint64_t _hash, uint32_t _size, Lit _dummy = lit_Undef ) : cd(_cd), hash(_hash), size(_size), dummy( toInt(_dummy) ) {}
  };

  /** dummy struct to be able to use minisat map */  
  struct EmptyData {
  };
  
  /** necessary object for watch list */
  struct ClauseDataDeleted
  {
    bool operator()(const ProofClauseData& w) const { return false; }
  };
  
  struct ClauseHashDeleted
  {
    bool operator()(const ClauseHash& w) const { return false; }
  };
  
  /** create a hash function somehow from the data */
  struct ClauseHashHashFunction { 
    uint32_t operator()(const ClauseHash& k) const { 
      uint32_t ret = k.size + k.dummy;
      ret = ret | k.hash & (~(1 << 31));
      ret = ret ^ (k.hash >> 32ull);
      return ret;
    } 
  };
  
  // data structures
  int formulaClauses;               // number of clauses in the formula (these clause do not have to be verified)
  vec<ProofClauseLabel> label;           // stores the label per clause in the formula and proof
  vec<ProofClauseData> fullProof;        // store the data of the proof (including all clauses of the formula)
  vec< vec<int64_t> > dependencies; // storage to store the tracecheck proof
  ClauseAllocator ca;               // storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more

#ifdef ONEWATCHLISTCHECKER
  BackwardVerificationOneWatch* sequentialChecker;   // pointer to sequential checking object
#else
  BackwardVerificationWorker* sequentialChecker;   // pointer to sequential checking object
#endif
  ThreadController* threadContoller;  // handle parallel work load
  
  
  vec<int> clauseCount;      // count number of occurrences of a clause that is present in the formula (to be able to merge duplicates)
//   OccLists<Lit, vec<ClauseHash>, ClauseHashDeleted> oneWatch; // one watch list
  Map<ClauseHash,EmptyData,ClauseHashHashFunction> oneWatchMap; // use hash map to find matching clauses
   
  // operation options
  bool drat;                   // verify drat
  bool fullRAT;                // do check RAT only for the first literal?
  int  threads;                // number of threads that should be used for the analysis
  int  checkDuplicateLits;     // how to handle duplicate literals in clauses (0 = ignore, 1 = warn, 2 = remove during parsing)
  int  checkDuplicateClauses;  // how to handle duplicate clauses in the proof (0 = ignore, 1 = warn, 2 = merge during parsing)
  int  verbose;                // how verbose the tool should be
  int  removedInvalidElements; // elements that have been removed from the oneWatch structure before finishing the input mode
  
  // operation data
  int variables;                   // number of seen variables
  bool hasBeenInterupted;          // indicate external interupt
  bool readsFormula;               // indicate that we are still parsing the formula
  int64_t currentID;               // id of the clause that is added next
  bool sawEmptyClause;             // memorize that an empty clause has been parsed
  bool formulaContainsEmptyClause; // memorize that we parsed an empty clause in the formula
  bool inputMode;                  // we expect more clauses to be added
  bool invalidProof;               // memorize that something went wrong during parsing the proof (e.g. deleted clause not found)
  
  // statistics
  int64_t duplicateClauses, clausesWithDuplicateLiterals, mergedElement;
  int loadUnbalanced;
  
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
  
  /** tell object that we are checking a DRUP proof (independently of the option of the binary) */
  void setDRUPproof();
  
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
   * @param workOnCopy do not touch the clauses
   * @return true, if the clause can be added to the proof
   */
  bool checkClause(vec< Lit >& clause, bool drupOnly = false, bool workOnCopy = false);
  
  /** verify the given proof. 
   *  @return true, if the proof can be verified
   */
  bool verifyProof ();
  
  /** tells the checker that there will not be any further clauses to be read
   *  will destroy the onewatch data structure, and set an according flag
   * @return false, if no empty clause was added to the proof
   */
  bool prepareVerification();
  
  /** clear/reset all labels 
   * @param freeResources release used memory
   */
  void clearLabels(bool freeResources = false);
    
  /** reset statistics */
  void resetStatistics() { statistics.clear(); }
  
  /** print statistics of verification to given stream */
  void printStatistics(std::ostream& s);
  
protected:

  /** add the statistics of the given worker to the specified statistics object 
   * @param stat statistics object
   * @param worker pointer to the worker whose statistics should be cumulated
   */
#ifdef ONEWATCHLISTCHECKER
  void addStatistics( Statistics& stat, BackwardVerificationOneWatch* worker );
#else
  void addStatistics( Statistics& stat, BackwardVerificationWorker* worker );
#endif
  
  /** method that points to the parallel verification */
  static void* runParallelVerfication(void* verificationData);
  
};

}

#endif
