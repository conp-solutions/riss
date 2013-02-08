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
  
  Coprocessor::Propagation & propagation;
  Coprocessor::Subsumption & subsumption;

  // variable heap comperators
  const int heap_option;
  struct VarOrderBVEHeapLt {
        CoprocessorData & data;
        const int heapOption;
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
  // variable queue for random variable-order
  vector< Var > variable_queue;

  // sequential member variables
  vec< Lit > resolvent; // vector for sequential resolution

  // parallel member variables
  MarkArray lastTouched;                    //MarkArray to track modifications of parallel BVE-Threads
  MarkArray dirtyOccs;                      // tracks occs that contain CRef_Undef
  vector< Job > jobs;                     
  vector< SpinLock > variableLocks;         // 3 extra SpinLock for data, heap, ca
  vector< vector < CRef > > subsumeQueues;
  vector< vector < CRef > > strengthQueues;
  Heap<NeighborLt>  ** neighbor_heaps;
  vector< CRef > sharedSubsumeQueue; 
  vector< CRef > sharedStrengthQeue;
  NeighborLt neighborComperator;
  ReadersWriterLock allocatorRWLock;
  
  // stats variables
  int removedClauses, removedLiterals, createdClauses, createdLiterals, removedLearnts, learntLits, newLearnts, 
      newLearntLits, testedVars, anticipations, eliminatedVars, removedBC, blockedLits, removedBlockedLearnt, learntBlockedLit, 
      skippedVars, unitsEnqueued, foundGates, usedGates;   
  double processTime, subsimpTime, gateTime;

public:
  
  BoundedVariableElimination( ClauseAllocator& _ca, ThreadController& _controller , Coprocessor::Propagation & _propagation, Coprocessor::Subsumption & _subsumption);
  
  /** run BVE until completion */
  lbool runBVE(CoprocessorData& data, const bool doStatistics = true);

  void initClause(const CRef cr); // inherited from Technique

  void printStatistics(ostream& stream);

protected:
  
  bool hasToEliminate();                               // return whether there is something in the BVE queue

  // sequential functions:
  void bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, const bool force = false, const bool doStatistics = true);   
  inline void removeClauses(CoprocessorData & data, const vector<CRef> & list, const Lit l, const bool doStatistics = true);
  inline lbool resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative
          , const int v, const int p_limit, const int n_limit
          , const bool keepLearntResolvents = false, const bool force = false, const bool doStatistics = true);
  inline lbool anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative
          , const int v, const int p_limit, const int n_limit, int32_t* pos_stats, int32_t* neg_stats
          , int & lit_clauses, int & lit_learnts, const bool doStatistics = true); 
  inline void addClausesToSubsumption (const vector<CRef> & clauses);
  inline void touchedVarsForSubsumption (CoprocessorData & data, const std::vector<Var> & touched_vars);

  
  /** data for parallel execution */
  struct BVEWorkData {
    BoundedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    vector<SpinLock> * var_locks; // Spin Lock for Variables
    ReadersWriterLock * rw_lock;  // rw-lock for CA
    Heap<VarOrderBVEHeapLt> * heap; // Shared heap with variables for elimination check
    Heap<NeighborLt> * neighbor_heap; // heap for Neighbor calculation
    MarkArray * dirtyOccs;
    vector<CRef> * subsumeQueue;
    vector<CRef> * sharedSubsumeQueue;
    vector<CRef> * strengthQueue;
    vector<CRef> * sharedStrengthQeue;
  };


  // parallel functions:
  void par_bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, Heap<NeighborLt> & neighbor_heap
          , vector <CRef> & subsumeQueue, vector <CRef> & sharedSubsumeQueue, vector < CRef > & strengthQueue
          , vector < CRef > & sharedStrengthQeue, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock
          , const bool force = true, const bool doStatistics = true) ; 

  inline void removeBlockedClauses(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l, const bool doStatistics = true );
  
    /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  inline void removeClausesThreadSafe(CoprocessorData & data, const vector<CRef> & list, const Lit l, SpinLock & data_lock);
  inline lbool resolveSetThreadSafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, vec < Lit > & ps, AllocatorReservation & memoryReservation, const bool keepLearntResolvents = false, const bool force = false);
  inline lbool anticipateEliminationThreadsafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, vec<Lit> & resolvent, int32_t* pos_stats, int32_t* neg_stats, int & lit_clauses, int & lit_learnts, int & new_clauses, int & new_learnts, SpinLock & data_lock);
  inline void removeBlockedClausesThreadSafe(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l, SpinLock & data_lock );

  // Special subsimp implementations for par bve:
  void par_bve_strengthening_worker(CoprocessorData& data, vector< SpinLock > & var_lock, deque <CRef> & subsumeQueue, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, MarkArray & dirtyOccs, const bool strength_resolvents, const bool doStatistics);


inline void strength_check_pos(CoprocessorData & data, vector < CRef > & list, deque<CRef> & subsumeQueue, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Var fst, vector < SpinLock > & var_lock, MarkArray & dirtyOccs, const bool strength_resolvents, const bool doStatistics = true); 

inline void strength_check_neg(CoprocessorData & data, vector < CRef > & list, deque<CRef> & subsumeQueue, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Lit min, Var fst, vector < SpinLock > & var_lock, MarkArray & dirtyOccs, const bool strength_resolvents, const bool doStatistics = true); 
  // Helpers for both par and seq
  inline bool resolve(const Clause & c, const Clause & d, const int v, vec<Lit> & ps);
  inline int  tryResolve(const Clause & c, const Clause & d, const int v);
  inline bool checkPush(vec<Lit> & ps, const Lit l);
  inline char checkUpdatePrev(Lit & prev, const Lit l);
  inline bool findGates(CoprocessorData & data, const Var v, int & p_limit, int & n_limit, MarkArray * helper = NULL);


public:
  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};


  /**
   * expects c to contain v positive and d to contain v negative
   * @return true, if resolvent is satisfied
   *         else, otherwise
   */
  inline bool BoundedVariableElimination::resolve(const Clause & c, const Clause & d, const int v, vec<Lit> & resolvent)
  {
    unsigned i = 0, j = 0;
    while (i < c.size() && j < d.size())
    {   
        if (c[i] == mkLit(v,false))      ++i;   
        else if (d[j] == mkLit(v,true))  ++j;
        else if (c[i] < d[j])
        {
           if (checkPush(resolvent, c[i]))
                return true;
           else ++i;
        }
        else 
        {
           if (checkPush(resolvent, d[j]))
              return true;
           else
                ++j; 
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))    ++i;   
        else if (checkPush(resolvent, c[i]))
            return true;
        else 
            ++i;
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))     ++j;
        else if (checkPush(resolvent, d[j]))
            return true;
        else                           ++j;

    }
    return false;
  } 
  /**
   * expects c to contain v positive and d to contain v negative
   * @return -1, if resolvent is tautology
   *         number of resolvents literals, otherwise
   */
  inline int BoundedVariableElimination::tryResolve(const Clause & c, const Clause & d, const int v)
  {
    unsigned i = 0, j = 0, r = 0;
    Lit prev = lit_Undef;
    while (i < c.size() && j < d.size())
    {   
        if (c[i] == mkLit(v,false))           ++i;   
        else if (d[j] == mkLit(v,true))       ++j;
        else if (c[i] < d[j])
        {
           char status = checkUpdatePrev(prev, c[i]);
           if (status == -1)
             return -1;
           else 
           {     
               ++i;
               r+=status;;
           }
        }
        else 
        {
           char status = checkUpdatePrev(prev, d[j]);
           if (status == -1)
              return -1;
           else
           {     
               ++j; 
               r+=status;
           }
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
            ++i;   
        else
        {   
            char status = checkUpdatePrev(prev, c[i]);
            if (status == -1)
                return -1;
            else 
            {
                ++i;
                r+=status;
            }
        }
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))              ++j;
        else 
        {
            char status = checkUpdatePrev(prev, d[j]);
            if (status == -1)
                return -1;
            else 
            {
                ++j;
                r+=status;
            }
        }
    }
    return r;
  } 
 
inline bool BoundedVariableElimination::checkPush(vec<Lit> & ps, const Lit l)
  {
    if (ps.size() > 0)
    {
        if (ps.last() == l)
         return false;
        if (ps.last() == ~l)
         return true;
    }
    ps.push(l);
    return false;
  }

inline char BoundedVariableElimination::checkUpdatePrev(Lit & prev, const Lit l)
  {
    if (prev != lit_Undef)
    {
        if (prev == l)
            return 0;
        if (prev == ~l)
            return -1;
    }
    prev = l;
    return 1;
  }
}
#endif 
