/********************************************************************[HiddenTautologyElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef HIDDENTAUTOLOGYELIMINATION_HH
#define HIDDENTAUTOLOGYELIMINATION_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement hidden tautology elimination
 */
class HiddenTautologyElimination : public Technique {
  
  int steps;                   // how many steps is the worker allowed to do
  vector<Var> activeVariables; // which variables should be considered?
  vector<char> activeFlag;     // array that stores a flag per variable whether it is active
  

public:
  
  HiddenTautologyElimination( ClauseAllocator& _ca, ThreadController& _controller );
  
  /** run subsumption and strengthening until completion */
  void eliminate(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique

  /** fills the mark arrays for a certain variable */
  Lit fillHlaArrays(Var v, Coprocessor::BIG& big, Coprocessor::MarkArray& hlaPositive, Coprocessor::MarkArray& hlaNegative, Lit* litQueue, bool doLock = false);
  
  /** mark all literals that would appear in HLA(C) 
   * @return true, if clause can be removed by HTE
   */
  bool hlaMarkClause(const Minisat::CRef cr, Coprocessor::BIG& big, Coprocessor::MarkArray& markArray, Lit* litQueue );
  /// same as above, but can add literals to the vector, so that the vector represents the real HLA(C) clause
  bool hlaMarkClause(vec< Lit >& clause, Coprocessor::BIG& big, Coprocessor::MarkArray& markArray, Lit* litQueue, bool addLits = false);
  
  /** mark all literals that would appear in ALA(C) 
   * @return true, if clause can be removed by ATE
   */
  bool alaMarkClause(const Minisat::CRef cr, Coprocessor::CoprocessorData& data, Coprocessor::MarkArray& markArray, Coprocessor::MarkArray& helpArray);
  /// same as above, but can add literals to the vector, so that the vector represents the real ALA(C) clause
  bool alaMarkClause(vec< Lit >& clause, Coprocessor::CoprocessorData& data, Coprocessor::MarkArray& markArray, Coprocessor::MarkArray& helpArray, bool addLits = false);
  
protected:

  /** is there currently something to do? */
  bool hasToEliminate();
  
  /** apply hte for the elements in the queue in the intervall [stard,end[ (position end is not touched!) */
  void elimination_worker (CoprocessorData& data, uint32_t start, uint32_t end, BIG& big, bool doStatistics = true, bool doLock = false); // subsume certain set of elements of the processing queue, does not write to the queue
  
  /** run hte for the specified variable */
  bool hiddenTautologyElimination(Var v, Coprocessor::CoprocessorData& data, Coprocessor::BIG& big, Coprocessor::MarkArray& hlaPositive, Coprocessor::MarkArray& hlaNegative, bool statistic = true, bool doLock = false);
  
  /** data for parallel execution of HTE */
  struct EliminationData {
    HiddenTautologyElimination* hte; // class with code
    CoprocessorData* data;           // formula and maintain lists
    BIG* big;                        // handle to binary implication graph
    Var start;                       // partition of the queue
    Var end;
  };

  /** run parallel subsumption with all available threads */
  void parallelElimination(Coprocessor::CoprocessorData& data, Coprocessor::BIG& big);
  
public:

  /** converts arg into EliminationData*, runs hte of its part of the queue */
  static void* runParallelElimination(void* arg);

};

}

#endif