/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/

#ifndef SUBSUMPTION_HH
#define SUBSUMPTION_HH

#include "core/Solver.h"

#include "coprocessor-src/Propagation.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement subsumption and strengthening, and related techniques
 */
class Subsumption : public Technique {
  
  Coprocessor::CoprocessorData& data;
  Coprocessor::Propagation& propagation;
  
  int subsumedClauses;  // statistic counter
  int subsumedLiterals; // sum up the literals of the clauses that have been subsumed
  int removedLiterals;  // statistic counter
  int subsumeSteps;     // number of clause comparisons in subsumption 
  int strengthSteps;    // number of clause comparisons in strengthening
  double processTime;   // statistic counter
  double strengthTime;  // statistic counter


  vec<Lit> ps;  // Resolution vector for keepAllResolvent
  vector<CRef> toDelete; // Delete vector for keepAllResolvent
  vector<CRef> newClauses; //Collect new Strengthening Clauses to avoid endless loops
  
  // Structure to track updates of occurrence lists
  struct OccUpdate {
      CRef cr;
      Lit  l;
      OccUpdate(const CRef _cr, const Lit _l) : cr(_cr), l(_l) {} 
  };

  // local stats for parallel execution
  struct SubsumeStatsData {
    int subsumedClauses;  // statistic counter
    int subsumedLiterals; // sum up the literals of the clauses that have been subsumed
    int removedLiterals;  // statistic counter
    int subsumeSteps;     // number of clause comparisons in subsumption 
    int strengthSteps;    // number of clause comparisons in strengthening
    double processTime;   // statistic counter
    double strengthTime;  // statistic counter
    double lockTime;      // statistic counter
    };

  // Member var seq subsumption
  vector < CRef > subs_occ_updates;
  // Member var seq strength
  vector < OccUpdate > strength_occ_updates;
  // Member vars parallel Subsumption
  vector< vector < CRef > > toDeletes;
  vector< vector < CRef > > nonLearnts;
  vector< struct SubsumeStatsData > localStats;
  
  // Member vars parallel Strengthening
  vector< SpinLock > var_locks; // 1 extra SpinLock for data
  vector< vector < OccUpdate > > occ_updates;
            
public:
  
  Subsumption( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Coprocessor::Propagation& _propagation );
  
  
  /** run subsumption and strengthening until completion */
  void process(Heap<VarOrderBVEHeapLt> * heap = NULL, const bool doStatistics = true);

  void initClause(const CRef cr); // inherited from Technique
  
  /** indicate whether clauses could be reduced */
  bool hasWork() const ;
  
  void printStatistics(ostream& stream);
  
  void resetStatistics();
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */

  void destroy();
  
protected:

  inline void updateOccurrences(const vector< Coprocessor::Subsumption::OccUpdate >& updates, Heap<VarOrderBVEHeapLt> * heap);

  bool hasToSubsume() const ;       // return whether there is something in the subsume queue
  lbool fullSubsumption(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics);   // performs subsumtion until completion
  void subsumption_worker (unsigned int start, unsigned int end, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true); // subsume certain set of elements of the processing queue, does not write to the queue
  void par_subsumption_worker ( unsigned int start, unsigned int end, vector<CRef> & to_delete, vector< CRef > & set_non_learnt, struct SubsumeStatsData & stats, const bool doStatistics = true);
  
  bool hasToStrengthen() const ;    // return whether there is something in the strengthening queue
  
  lbool fullStrengthening( Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true); // performs strengthening until completion, puts clauses into subsumption queue
  lbool strengthening_worker ( unsigned int start, unsigned int end, Heap<VarOrderBVEHeapLt> * heap, bool doStatistics = true);
lbool createResolvent( const CRef cr, CRef & resolvent, const int negated_lit_pos, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics);
  void par_strengthening_worker( unsigned int start, unsigned int stop, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, vector<OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true); 
  void par_nn_strengthening_worker( unsigned int start, unsigned int end, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, vector<OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true);
  inline lbool par_nn_strength_check(CoprocessorData & data, vector < CRef > & list, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Var fst, vector < SpinLock > & var_lock, struct SubsumeStatsData & stats, vector<OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true) ; 
  inline lbool par_nn_negated_strength_check(CoprocessorData & data, vector < CRef > & list, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Lit min, Var fst, vector < SpinLock > & var_lock, struct SubsumeStatsData & stats, vector<OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true);
  
  /** data for parallel execution */
  struct SubsumeWorkData {
    Subsumption*     subsumption; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
    vector<SpinLock> * var_locks;
    vector<CRef>*    to_delete;
    vector<CRef>*    set_non_learnt;
    vector<OccUpdate> * occ_updates;
    struct SubsumeStatsData* stats;
    Heap<VarOrderBVEHeapLt> * heap;
    };
  
  /** run parallel subsumption with all available threads */
  void parallelSubsumption( const bool doStatistics = true);
  void parallelStrengthening(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics = true);  
public:

  /** converts arg into SubsumeWorkData*, runs subsumption of its part of the queue */
  static void* runParallelSubsume(void* arg);
  static void* runParallelStrengthening(void * arg);
};

}

#endif
