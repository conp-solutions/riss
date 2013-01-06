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
  
  vector<CRef> clause_processing_queue;
  vector<CRef> strengthening_queue;         // vector of clausereferences, which can strengthen
  Coprocessor::Propagation& propagation;
  
  int subsumedClauses;  // statistic counter
  int subsumedLiterals; // sum up the literals of the clauses that have been subsumed
  int removedLiterals;  // statistic counter
  int subsumeSteps;     // number of clause comparisons in subsumption 
  int strengthSteps;    // number of clause comparisons in strengthening
  double processTime;   // statistic counter
  double strengthTime;  // statistic counter
  
public:
  
  Subsumption( ClauseAllocator& _ca, ThreadController& _controller, Coprocessor::Propagation& _propagation );
  
  
  /** run subsumption and strengthening until completion */
  void subsumeStrength(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique
  
  /** indicate whether clauses could be reduced */
  bool hasWork() const ;
  
  /** add a clause to the queues, so that this clause will be checked by the next call to subsumeStrength */
  void addClause( const CRef cr );
  
  void printStatistics(ostream& stream);
  
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */

protected:
  // local stats for parallel execution
  struct SubsumeStatsData {
    int subsumedClauses;  // statistic counter
    int subsumedLiterals; // sum up the literals of the clauses that have been subsumed
    int removedLiterals;  // statistic counter
    int subsumeSteps;     // number of clause comparisons in subsumption 
    int strengthSteps;    // number of clause comparisons in strengthening
    double processTime;   // statistic counter
    double strengthTime;  // statistic counter
    };

  bool hasToSubsume() const ;       // return whether there is something in the subsume queue
  lbool fullSubsumption(CoprocessorData& data);   // performs subsumtion until completion
  void subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, const bool doStatistics = true); // subsume certain set of elements of the processing queue, does not write to the queue
  void par_subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, vector<CRef> & to_delete, vector< CRef > & set_non_learnt, struct SubsumeStatsData & stats, const bool doStatistics = true);
  
  bool hasToStrengthen() const ;    // return whether there is something in the strengthening queue
  lbool fullStrengthening(CoprocessorData& data); // performs strengthening until completion, puts clauses into subsumption queue
  void strengthening_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics = true);
  void par_strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int stop, vector< SpinLock > & var_lock); 
  void par_nn_strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int end, vector< SpinLock > & var_lock);
  /** data for parallel execution */
  struct SubsumeWorkData {
    Subsumption*     subsumption; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
    vector<SpinLock> * var_locks;
    vector<CRef>*    to_delete;
    vector<CRef>*    set_non_learnt;
    struct SubsumeStatsData* stats;
    };
  
  /** run parallel subsumption with all available threads */
  void parallelSubsumption(CoprocessorData& data, const bool doStatistics = true);
  void parallelStrengthening(CoprocessorData& data);  
public:

  /** converts arg into SubsumeWorkData*, runs subsumption of its part of the queue */
  static void* runParallelSubsume(void* arg);
  static void* runParallelStrengthening(void * arg);
};

}

#endif
