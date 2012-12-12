/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
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
  
public:
  
  Subsumption( ClauseAllocator& _ca, ThreadController& _controller, Coprocessor::Propagation& _propagation );
  
  
  /** run subsumption and strengthening until completion */
  void subsumeStrength(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique
  
  /** add a clause to the queues, so that this clause will be checked by the next call to subsumeStrength */
  void addClause( const CRef cr );
  
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */

protected:
  
  bool hasToSubsume();       // return whether there is something in the subsume queue
  lbool fullSubsumption(CoprocessorData& data);   // performs subsumtion until completion
  void subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics = true); // subsume certain set of elements of the processing queue, does not write to the queue
  
  bool hasToStrengthen();    // return whether there is something in the strengthening queue
  lbool fullStrengthening(CoprocessorData& data); // performs strengthening until completion, puts clauses into subsumption queue
  void strengthening_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics = true);
  
  /** data for parallel execution */
  struct SubsumeWorkData {
    Subsumption*     subsumption; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
  };

  /** run parallel subsumption with all available threads */
  void parallelSubsumption(CoprocessorData& data);
  void parallelStrengthening(CoprocessorData& data);  
public:

  /** converts arg into SubsumeWorkData*, runs subsumption of its part of the queue */
  static void* runParallelSubsume(void* arg);
  static void* runParallelStrengthening(void * arg);
};

}

#endif
