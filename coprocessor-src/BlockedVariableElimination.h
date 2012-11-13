/********************************************************************[BlockedVariableElimination.h]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement blocked variable elimination
 */
class BlockedVariableElimination : public Technique {
  vector<int> variable_queue;
  
public:
  
  BlockedVariableElimination( ClauseAllocator& _ca, ThreadController& _controller );
  
  /** run BVE until completion */
  void runBVE(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique

protected:
  
  bool hasToEliminate();       // return whether there is something in the BVE queue
  lbool fullBVE(CoprocessorData& data);   // performs BVE until completion
  void bve_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics = true);   
  
  /** data for parallel execution */
  struct BVEWorkData {
    BlockedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
  };

  /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  bool resolve(Clause & c, Clause & d, int v, vector<Lit> & resolvent);
public:

  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};

}
