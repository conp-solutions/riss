/********************************************************************[BoundedVariableElimination.h]
Copyright (c) 2012, Kilian Gebhardt, Norbert Manthey, All rights reserved.
**************************************************************************************************/
#ifndef BVE_HH
#define BVE_HH

#include "riss/core/Solver.h"

#include "coprocessor/Technique.h"

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Subsumption.h"
#include "riss/mtl/Heap.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor {

/** This class implement blocked variable elimination
 */
class BoundedVariableElimination : public Technique {
  
  Coprocessor::Propagation & propagation;
  Coprocessor::Subsumption & subsumption;

  // variable heap comperators
  const int heap_option;
  
  struct PostponeReason {
      Riss::Var var, reason;
      PostponeReason ( Riss::Var _var, Riss::Var _reason) : var(_var), reason(_reason) {}
  };
  // Vector for restarting bve (seq and par)
  std::vector<Riss::Var> touched_variables;
  // variable queue for random variable-order
  std::vector< Riss::Var > variable_queue;

  // sequential member variables
  Riss::vec< Riss::Lit > resolvent; // std::vector for sequential resolution
  Riss::vec< int32_t > pos_stats;
  Riss::vec< int32_t > neg_stats; 
  // parallel member variables
  Riss::MarkArray lastTouched;                    //Riss::MarkArray to track modifications of parallel BVE-Threads
  Riss::MarkArray dirtyOccs;                      // tracks occs that contain CRef_Undef
  std::vector< Riss::Job > jobs;                     
  std::vector< SpinLock > variableLocks;         // 3 extra SpinLock for data, heap, ca
  std::vector< std::deque < Riss::CRef > > subsumeQueues;
  std::vector< std::deque < Riss::CRef > > strengthQueues;
  std::vector< std::deque < PostponeReason > > postpones;
  std::vector< Riss::MarkArray > gateMarkArrays;
  std::deque< Riss::CRef > sharedStrengthQueue;
  ReadersWriterLock allocatorRWLock;
  // stats variables
  struct ParBVEStats {
      int removedClauses, removedLiterals, createdClauses, createdLiterals, removedLearnts, learntLits, newLearnts, 
      newLearntLits, testedVars, anticipations, eliminatedVars, removedBC, blockedLits, removedBlockedLearnt, learntBlockedLit, 
      skippedVars, unitsEnqueued, foundGates, usedGates, subsumedClauses, subsumedLiterals, subsumedLearnts, subsumedLearntLiterals,
      subsimpSteps, strengthtLits, strengthtLearntLits;   
      int64_t parBveChecks;
      double processTime, subsimpTime, gateTime, upTime, lockNeighborTime, mereLockingTime;
      char _pad[60];
      ParBVEStats() :   removedClauses(0), removedLiterals(0), createdClauses(0), createdLiterals(0), removedLearnts(0)
                      , learntLits(0), newLearnts(0), newLearntLits(0), testedVars(0), anticipations(0), eliminatedVars(0)
                      , removedBC(0), blockedLits(0), removedBlockedLearnt(0), learntBlockedLit(0), skippedVars(0)
                      , unitsEnqueued(0), foundGates(0), usedGates(0), subsumedClauses(0), subsumedLiterals(0)
                      , subsumedLearnts(0), subsumedLearntLiterals(0), subsimpSteps(0)
                      , strengthtLits(0), strengthtLearntLits(0)
		      , parBveChecks(0)
                      , processTime(0), subsimpTime(0), gateTime(0), upTime(0), lockNeighborTime(0) , mereLockingTime(0){}
  };
  std::vector<struct ParBVEStats> parStats;

  // stats variables
  int removedClauses, removedLiterals, createdClauses, createdLiterals, removedLearnts, learntLits, newLearnts, 
      newLearntLits, testedVars, anticipations, eliminatedVars, removedBC, blockedLits, removedBlockedLearnt, learntBlockedLit, 
      skippedVars, unitsEnqueued, foundGates, usedGates, 
      initialClauses, initialLits, clauseCount, litCount, unitCount, elimCount, restarts;   
  int64_t seqBveSteps, bveLimit;
  int64_t nClsIncreases,nClsDecreases,nClsKeep,totallyAddedClauses; // number of clauses that have been added by bve
  double processTime, subsimpTime, gateTime;

public:
  
  BoundedVariableElimination( CP3Config &_config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller , Coprocessor::Propagation & _propagation, Coprocessor::Subsumption & _subsumption);
 
  Riss::lbool process(CoprocessorData & data, const bool doStatistics = true) { modifiedFormula = false; return runBVE(data, doStatistics); }

  /** run BVE until completion */
  Riss::lbool runBVE(CoprocessorData& data, const bool doStatistics = true);

  void initClause(const Riss::CRef cr); // inherited from Technique
  void destroy();
  void giveMoreSteps();
  void printStatistics(std::ostream& stream);

protected:
  
  void progressStats(CoprocessorData & data, const bool cputime = false);     // prints statistics before/after each BVE-Run
  bool hasToEliminate();                               // return whether there is something in the BVE queue

  // sequential functions:
  void sequentiellBVE(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const bool force = false, const bool doStatistics = true);
  void bve_worker (Coprocessor::CoprocessorData& data, Riss::Heap< Coprocessor::VarOrderBVEHeapLt >& heap, int64_t& bveChecks, const bool force = false, const bool doStatistics = true);   
  
  /** remove clauses from data structures and add to extension lists
   *  @param l literal that has been used to remove the clauses during elimination (if l == Riss::lit_Undef, clauses are not added to extension stack)
   */
  inline void removeClauses(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const std::vector<Riss::CRef> & list, const Riss::Lit l, const int limit, const bool doStatistics = true);
  
  
  /** ths method applies unit propagation during resolution, if possible! */
  inline Riss::lbool resolveSet(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, std::vector<Riss::CRef> & positive, std::vector<Riss::CRef> & negative
          , const int v, const int p_limit, const int n_limit, int64_t& bveChecks
          , const bool keepLearntResolvents = false, const bool force = false, const bool doStatistics = true);
  inline Riss::lbool anticipateElimination(CoprocessorData & data, std::vector<Riss::CRef> & positive, std::vector<Riss::CRef> & negative
          , const int v, const int p_limit, const int n_limit, Riss::vec<int32_t> & pos_stats, Riss::vec<int32_t> & neg_stats
          , int & lit_clauses, int & lit_learnts, int& resolvents, int64_t& bveChecks, const bool doStatistics = true); 
  inline void addClausesToSubsumption (const std::vector<Riss::CRef> & clauses);
  void touchedVarsForSubsumption (CoprocessorData & data, const std::vector<Riss::Var> & touched_vars);

  
  /** data for parallel execution */
  struct BVEWorkData {
    BoundedVariableElimination*  bve; // class with code
    CoprocessorData* data;        // formula and maintain lists
    std::vector<SpinLock> * var_locks; // Spin Lock for Variables
    ReadersWriterLock * rw_lock;  // rw-lock for CA
    Riss::Heap<VarOrderBVEHeapLt> * heap; // Shared heap with variables for elimination check
    Riss::MarkArray * dirtyOccs;
    std::deque<Riss::CRef> * strengthQueue;
    std::deque<Riss::CRef> * sharedStrengthQueue;
    std::deque< PostponeReason > * postponed;
    ParBVEStats * bveStats;
    Riss::MarkArray * gateMarkArray;
    int rwlock_count;
    BVEWorkData () : rwlock_count(0), garbageCounter(0) {}
    int garbageCounter;
  };


  // parallel functions:
  void par_bve_worker 
          ( CoprocessorData& data, Riss::Heap<VarOrderBVEHeapLt> & heap
          , std::deque < Riss::CRef > & strengthQueue , std::deque < Riss::CRef > & sharedStrengthQueue, std::deque < PostponeReason > & postponed 
          , std::vector< SpinLock > & var_lock, ReadersWriterLock & rwlock
          , ParBVEStats & stats , Riss::MarkArray * gateMarkArray, int & rwlock_count
          , int & garbageCounter
	  , int64_t&parBVEchecks
          , const bool force = false, const bool doStatistics = true) ; 

  inline void removeBlockedClauses(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const std::vector< Riss::CRef> & list, const int32_t stats[], const Riss::Lit l, const int limit, const bool doStatistics = true );
  
    /** run parallel bve with all available threads */
  void parallelBVE(CoprocessorData& data);
  
  inline void removeClausesThreadSafe(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const std::vector<Riss::CRef> & list, const Riss::Lit l, const int limit, SpinLock & data_lock, SpinLock & heap_lock, ParBVEStats & stats, int & garbageCounter, const bool doStatistics);
  inline Riss::lbool resolveSetThreadSafe(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, std::vector<Riss::CRef> & positive, std::vector<Riss::CRef> & negative, const int v, const int p_limit, const int n_limit, Riss::vec < Riss::Lit > & ps, Riss::AllocatorReservation & memoryReservation, std::deque<Riss::CRef> & strengthQueue, ParBVEStats & stats, SpinLock & data_lock, SpinLock & heap_lock, int expectedResolvents, int64_t& bveChecks, const bool doStatistics, const bool keepLearntResolvents = false);
  inline Riss::lbool anticipateEliminationThreadsafe(CoprocessorData & data, std::vector<Riss::CRef> & positive, std::vector<Riss::CRef> & negative, const int v, const int p_limit, const int n_limit, Riss::vec<Riss::Lit> & resolvent, Riss::vec < int32_t > & pos_stats, Riss::vec < int32_t > & neg_stats, int & lit_clauses, int & lit_learnts, int & new_clauses, int & new_learnts, SpinLock & data_lock, ParBVEStats & stats, int64_t& bveChecks, const bool doStatistics);
  inline void removeBlockedClausesThreadSafe(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const std::vector< Riss::CRef> & list, const int32_t _stats[], const Riss::Lit l, const int limit, SpinLock & data_lock, SpinLock & heap_lock, ParBVEStats & stats, int & garbageCounter, const bool doStatistics );

  // Special subsimp implementations for par bve:
  void par_bve_strengthening_worker(CoprocessorData& data, Riss::Heap<VarOrderBVEHeapLt> & heap, const Riss::Var ignore, std::vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, std::deque<Riss::CRef> & sharedStrengthQueue, std::deque<Riss::CRef> & localQueue, Riss::MarkArray & dirtyOccs, ParBVEStats & stats, int & rwlock_count, int & garbageCounter, const bool strength_resolvents, const bool doStatistics);

  // Special propagation for par bve
  Riss::lbool par_bve_propagate(CoprocessorData& data, Riss::Heap<VarOrderBVEHeapLt> & heap, const Riss::Var ignore, std::vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, Riss::MarkArray & dirtyOccs, std::deque <Riss::CRef> & sharedSubsimpQueue, ParBVEStats & stats, int & rwlock_count, int & garbageCounter, const bool doStatistics);

inline Riss::lbool strength_check_pos(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const Riss::Var ignore, std::vector < Riss::CRef > & list, std::deque<Riss::CRef> & sharedStrengthQueue, std::deque<Riss::CRef> & localQueue, Riss::Clause & strengthener, Riss::CRef cr, Riss::Var fst, std::vector < SpinLock > & var_lock, Riss::MarkArray & dirtyOccs, ParBVEStats & stats, int & garbageCounter, const bool strength_resolvents, const bool doStatistics); 

inline Riss::lbool strength_check_neg(CoprocessorData & data, Riss::Heap<VarOrderBVEHeapLt> & heap, const Riss::Var ignore, std::vector < Riss::CRef > & list, std::deque<Riss::CRef> & sharedStrengthQueue, std::deque<Riss::CRef> & localQueue, Riss::Clause & strengthener, Riss::CRef cr, Riss::Lit min, Riss::Var fst, std::vector < SpinLock > & var_lock, Riss::MarkArray & dirtyOccs, ParBVEStats & stats, int & garbageCounter, const bool strength_resolvents, const bool doStatistics); 
  // Helpers for both par and seq
  inline bool resolve(const Riss::Clause & c, const Riss::Clause & d, const int v, Riss::vec<Riss::Lit> & ps);
  inline int  tryResolve(const Riss::Clause & c, const Riss::Clause & d, const int v);
  inline bool checkPush(Riss::vec<Riss::Lit> & ps, const Riss::Lit l);
  inline char checkUpdatePrev(Riss::Lit & prev, const Riss::Lit l);
  bool findGates(CoprocessorData & data, const Riss::Var v, int & p_limit, int & n_limit, double & _gateTim, Riss::MarkArray * helper = NULL);


public:
  /** converts arg into BVEWorkData*, runs bve of its part of the queue */
  static void* runParallelBVE(void* arg);

};


  /**
   * expects c to contain v positive and d to contain v negative
   * @return true, if resolvent is satisfied
   *         else, otherwise
   */
  inline bool BoundedVariableElimination::resolve(const Riss::Clause & c, const Riss::Clause & d, const int v, Riss::vec<Riss::Lit> & resolvent)
  {
    unsigned i = 0, j = 0;
    while (i < c.size() && j < d.size())
    {   
        if (c[i] == Riss::mkLit(v,false))      ++i;   
        else if (d[j] == Riss::mkLit(v,true))  ++j;
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
        if (c[i] == Riss::mkLit(v,false))    ++i;   
        else if (checkPush(resolvent, c[i]))
            return true;
        else 
            ++i;
    }
    while (j < d.size())
    {
        if (d[j] == Riss::mkLit(v,true))     ++j;
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
  inline int BoundedVariableElimination::tryResolve(const Riss::Clause & c, const Riss::Clause & d, const int v)
  {
    unsigned i = 0, j = 0, r = 0;
    Riss::Lit prev = Riss::lit_Undef;
    while (i < c.size() && j < d.size())
    {   
        if (c[i] == Riss::mkLit(v,false))           ++i;   
        else if (d[j] == Riss::mkLit(v,true))       ++j;
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
        if (c[i] == Riss::mkLit(v,false))
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
        if (d[j] == Riss::mkLit(v,true))              ++j;
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
 
inline bool BoundedVariableElimination::checkPush(Riss::vec<Riss::Lit> & ps, const Riss::Lit l)
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

inline char BoundedVariableElimination::checkUpdatePrev(Riss::Lit & prev, const Riss::Lit l)
  {
    if (prev != Riss::lit_Undef)
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
