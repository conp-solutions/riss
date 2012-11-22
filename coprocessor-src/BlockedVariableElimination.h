/********************************************************************[BlockedVariableElimination.h]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/
#ifndef BVE_HH
#define BVE_HH

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
  Coprocessor::Propagation & propagation;

public:
  
  BlockedVariableElimination( ClauseAllocator& _ca, ThreadController& _controller , Coprocessor::Propagation & _propagation);
  
  /** run BVE until completion */
  void runBVE(CoprocessorData& data, Minisat::Solver * solver);

  void initClause(const CRef cr); // inherited from Technique

protected:
  
  bool hasToEliminate();       // return whether there is something in the BVE queue
  lbool fullBVE(Coprocessor::CoprocessorData& data);   // performs BVE until completion
  void bve_worker (CoprocessorData& data, Minisat::Solver * solver, unsigned int start, unsigned int end, bool force = false, bool doStatistics = true);   
  
  /** data for parallel execution */
  struct BVEWorkData {
    BlockedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    Solver * solver;              // solver for UP
    unsigned int     start;       // partition of the queue
    unsigned int     end;
  };

  /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  inline void removeClauses(CoprocessorData & data, vector<CRef> & list);
  inline void resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, bool force = false);
  inline bool resolve(Clause & c, Clause & d, int v, vec<Lit> & ps);
  inline int  tryResolve(Clause & c, Clause & d, int v);
  inline bool checkPush(vec<Lit> & ps, Lit l);
  inline bool checkUpdatePrev(Lit & prev, Lit l);
  inline lbool anticipateElimination(CoprocessorData & data, Minisat::Solver * solver, vector<CRef> & positive, vector<CRef> & negative, int v, char* pos_stats, char* neg_stats, int & lit_clauses, int & lit_learnts); 
  inline void removeBlockedClauses(vector< CRef> data, char stats[] );
public:

  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};

}
#endif 
