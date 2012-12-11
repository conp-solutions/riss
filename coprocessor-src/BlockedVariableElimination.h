/********************************************************************[BlockedVariableElimination.h]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/
#ifndef BVE_HH
#define BVE_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Subsumption.h"
#include "mtl/Heap.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement blocked variable elimination
 */
class BlockedVariableElimination : public Technique {
  struct VarOrderBVEHeapLt {
        CoprocessorData & data;
        bool operator () (Var x, Var y) const {/* assert (data != NULL && "Please assign a valid data object before heap usage" );*/ return data[x]  > data[y]; }
        VarOrderBVEHeapLt(CoprocessorData & _data) : data(_data) { }
    };
  vector<int> variable_queue;
  //VarOrderBVEHeapLt heap_comp;
  //Heap<VarOrderBVEHeapLt> variable_heap;
  Coprocessor::Propagation & propagation;
  Coprocessor::Subsumption & subsumption;
public:
  
  BlockedVariableElimination( ClauseAllocator& _ca, ThreadController& _controller , Coprocessor::Propagation & _propagation, Coprocessor::Subsumption & _subsumption);
  
  /** run BVE until completion */
  lbool runBVE(CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique

protected:
  
  bool hasToEliminate();       // return whether there is something in the BVE queue
  lbool fullBVE(Coprocessor::CoprocessorData& data);   // performs BVE until completion
  void bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, unsigned int start, unsigned int end, bool force = false, bool doStatistics = true);   
  
  /** data for parallel execution */
  struct BVEWorkData {
    BlockedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
  };

  /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  inline void removeClauses(CoprocessorData & data, vector<CRef> & list, Lit l);
  inline lbool resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, bool keepLearntResolvents = false, bool force = false);
  inline bool resolve(Clause & c, Clause & d, int v, vec<Lit> & ps);
  inline int  tryResolve(Clause & c, Clause & d, int v);
  inline bool checkPush(vec<Lit> & ps, Lit l);
  inline char checkUpdatePrev(Lit & prev, Lit l);
  inline lbool anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, int32_t* pos_stats, int32_t* neg_stats, int & lit_clauses, int & lit_learnts); 
  inline void removeBlockedClauses(CoprocessorData & data, vector< CRef> & list, int32_t stats[], Lit l );
  inline void touchedVarsForSubsumption (CoprocessorData & data, const std::vector<Var> & touched_vars);
  inline void addClausesToSubsumption (CoprocessorData & data, const vector<CRef> & clauses);
public:

  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};

}
#endif 
