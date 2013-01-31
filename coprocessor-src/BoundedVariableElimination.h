/********************************************************************[BoundedVariableElimination.h]
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
class BoundedVariableElimination : public Technique {
  const int heap_option;
  struct VarOrderBVEHeapLt {
        const int heapOption;
        CoprocessorData & data;
        bool operator () (Var x, Var y) const {/* assert (data != NULL && "Please assign a valid data object before heap usage" );*/ 
            switch (heapOption)
            {
                case 0: return data[x] < data[y]; 
                case 1: return data[x] > data[y]; 
                default: assert(false && "In case of random order no heap should be used");
            }
        }
        VarOrderBVEHeapLt(CoprocessorData & _data, int _heapOption) : data(_data), heapOption(_heapOption) { }
    };

  struct NeighborLt {
        bool operator () (Var x, Var y) const { return x < y; }
  }; 

  vector< Var > variable_queue;
  //VarOrderBVEHeapLt heap_comp;
  //Heap<VarOrderBVEHeapLt> variable_heap;
  Coprocessor::Propagation & propagation;
  Coprocessor::Subsumption & subsumption;
  MarkArray lastTouched; //MarkArray to track modifications of parallel BVE-Threads
  int removedClauses, removedLiterals, createdClauses, createdLiterals, removedLearnts, learntLits, newLearnts, 
      newLearntLits, testedVars, anticipations, eliminatedVars, removedBC, blockedLits, removedBlockedLearnt, learntBlockedLit, 
      skippedVars, unitsEnqueued;   
  double processTime, subsimpTime;

public:
  
  BoundedVariableElimination( ClauseAllocator& _ca, ThreadController& _controller , Coprocessor::Propagation & _propagation, Coprocessor::Subsumption & _subsumption);
  
  /** run BVE until completion */
  lbool runBVE(CoprocessorData& data, const bool doStatistics = true);

  void initClause(const CRef cr); // inherited from Technique

  void printStatistics(ostream& stream);
protected:
  
  bool hasToEliminate();       // return whether there is something in the BVE queue
  lbool fullBVE(Coprocessor::CoprocessorData& data);   // performs BVE until completion
  void bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, unsigned int start, unsigned int end, const bool force = false, const bool doStatistics = true);   
  
  void par_bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, Heap<NeighborLt> & neighbor_heap, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, const bool force = true, const bool doStatistics = true) ; 
  /** data for parallel execution */
  struct BVEWorkData {
    BoundedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    vector<SpinLock> * var_locks; // Spin Lock for Variables
    Heap<VarOrderBVEHeapLt> * heap; // Shared heap with variables for elimination check
    Heap<NeighborLt> * neighbor_heap; // heap for Neighbor calculation
  };

  /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  inline void removeClauses(CoprocessorData & data, const vector<CRef> & list, const Lit l, const bool doStatistics = true);
  inline void removeClausesThreadSafe(CoprocessorData & data, const vector<CRef> & list, const Lit l, SpinLock & data_lock);
  inline lbool resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const int p_limit, const int n_limit, const bool keepLearntResolvents = false, const bool force = false, const bool doStatistics = true);
  inline lbool resolveSetThreadSafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const bool keepLearntResolvents = false, const bool force = false);
  inline bool resolve(const Clause & c, const Clause & d, const int v, vec<Lit> & ps);
  inline int  tryResolve(const Clause & c, const Clause & d, const int v);
  inline bool checkPush(vec<Lit> & ps, const Lit l);
  inline char checkUpdatePrev(Lit & prev, const Lit l);
  inline lbool anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const int p_limit, const int n_limit, int32_t* pos_stats, int32_t* neg_stats, int & lit_clauses, int & lit_learnts, const bool doStatistics = true); 
  inline lbool anticipateEliminationThreadsafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, int32_t* pos_stats, int32_t* neg_stats, int & lit_clauses, int & lit_learnts, SpinLock & data_lock);
  inline bool findGates(CoprocessorData & data, const Var v, int & p_limit, int & n_limit, MarkArray * helper = NULL);
  inline void removeBlockedClauses(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l, const bool doStatistics = true );
  inline void removeBlockedClausesThreadSafe(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l, SpinLock & data_lock );
  inline void touchedVarsForSubsumption (CoprocessorData & data, const std::vector<Var> & touched_vars);
  inline void addClausesToSubsumption (CoprocessorData & data, const vector<CRef> & clauses);
public:

  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};

}
#endif 
