/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/

#ifndef RISS_SUBSUMPTION_HH
#define RISS_SUBSUMPTION_HH

#include "riss/core/Solver.h"

#include "Propagation.h"

#include "coprocessor/Technique.h"

#include "coprocessor/CoprocessorTypes.h"

#include <vector>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** This class implement subsumption and strengthening, and related techniques
 */
class Subsumption : public Technique<Subsumption>
{

    Coprocessor::CoprocessorData& data;
    Coprocessor::Propagation& propagation;

    int subsumedClauses;  // statistic counter
    int subsumedLiterals; // sum up the literals of the clauses that have been subsumed
    int removedLiterals;  // statistic counter
    // int64_t subsumeSteps;     // number of clause comparisons in subsumption
    // int64_t strengthSteps;    // number of clause comparisons in strengthening
    double processTime;   // statistic counter
    double strengthTime;  // statistic counter

    Stepper subsumptionStepper;
    Stepper strengtheningStepper;

    // int64_t subLimit; // step limit for subsumption
    // int64_t strLimit; // step limit for strengthening
    int64_t callIncrease; // step limit increase to be able to perform at least this number of checks
    int limitIncreases;   // number of times the limits have been relaxed
    int chunk_size;

    Riss::vec<Riss::Lit> ps;  // Resolution std::vector for keepAllResolvent
    std::vector<Riss::CRef> toDelete; // Delete std::vector for keepAllResolvent
    std::vector<Riss::CRef> newClauses; //Collect new Strengthening Clauses to avoid std::endless loops

    // Structure to track updates of occurrence lists
    struct OccUpdate {
        Riss::CRef cr;
        Riss::Lit  l;
        OccUpdate(const Riss::CRef&   _cr, const Riss::Lit& _l) : cr(_cr), l(_l) {}
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
    std::vector < Riss::CRef > subs_occ_updates;
    // Member var seq strength
    std::vector < OccUpdate > strength_occ_updates;
    // Member vars parallel Subsumption
    SpinLock balancerLock;
    std::vector< std::vector < Riss::CRef > > toDeletes;
    std::vector< std::vector < Riss::CRef > > nonLearnts;
    std::vector< struct SubsumeStatsData > localStats;

    // Member vars parallel Strengthening
    std::vector< SpinLock > var_locks; // 1 extra SpinLock for data
    std::vector< std::vector < OccUpdate > > occ_updates;

  public:

    Subsumption(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Coprocessor::Propagation& _propagation);


    /** run subsumption and strengthening until completion
     * @param doStrengthen use strengthening in this call?
     */
    bool process(bool doStrengthen = true, Riss::Heap<VarOrderBVEHeapLt> * heap = nullptr, const Riss::Var ignore = var_Undef, const bool doStatistics = true);

    void initClause(const Riss::CRef& cr, const bool& addToStrengthen = true);   // inherited from Technique

    /** indicate whether clauses could be reduced */
    bool hasWork() const ;

    void printStatistics(std::ostream& stream);

    void resetStatistics();
    /* TODO:
     *  - init
     *  - add to queue
     * stuff for parallel subsumption
     *  - struct for all the parameters that have to be passed to a thread that should do subsumption
     *  - static method that performs subsumption on the given part of the queue without stat updates
     */

    void destroy();

    void giveMoreSteps();

  protected:

    inline void updateOccurrences(std::vector< Coprocessor::Subsumption::OccUpdate >& updates, Riss::Heap< Coprocessor::VarOrderBVEHeapLt >* heap, const Riss::Var ignore = (-1));

    bool hasToSubsume() const ;       // return whether there is something in the subsume queue
    Riss::lbool fullSubsumption(Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);   // performs subsumtion until completion
    void subsumption_worker(unsigned int start, unsigned int end, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);  // subsume certain set of elements of the processing queue, does not write to the queue
    void par_subsumption_worker(unsigned int& next_start, unsigned int global_end, SpinLock& balancerLock, std::vector<Riss::CRef>& to_delete, std::vector< Riss::CRef >& set_non_learnt, struct SubsumeStatsData& stats, const bool doStatistics = true);

    bool hasToStrengthen() const ;    // return whether there is something in the strengthening queue

    Riss::lbool fullStrengthening(Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);  // performs strengthening until completion, puts clauses into subsumption queue
    Riss::lbool strengthening_worker(unsigned int start, unsigned int end, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, bool doStatistics = true);
    Riss::lbool createResolvent(const Riss::CRef& cr, Riss::CRef& resolvent, const int negated_lit_pos, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);
    void par_strengthening_worker(unsigned int& next_start, unsigned int global_stop, SpinLock& balancerLock, std::vector< SpinLock >& var_lock, struct SubsumeStatsData& stats, std::vector<OccUpdate>& occ_updates, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);
    void par_nn_strengthening_worker(unsigned int& next_start, unsigned int global_end, SpinLock& balancerLock, std::vector< SpinLock >& var_lock, struct SubsumeStatsData& stats, std::vector<OccUpdate>& occ_updates, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);
    inline Riss::lbool par_nn_strength_check(CoprocessorData& data, std::vector < Riss::CRef >& list, std::deque<Riss::CRef>& localQueue, Riss::Clause& strengthener, Riss::CRef cr, Riss::Var fst, std::vector < SpinLock >& var_lock, struct SubsumeStatsData& stats, std::vector<OccUpdate>& occ_updates, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true) ;
    inline Riss::lbool par_nn_negated_strength_check(CoprocessorData& data, std::vector < Riss::CRef >& list, std::deque<Riss::CRef>& localQueue, Riss::Clause& strengthener, Riss::CRef cr, Riss::Lit min, Riss::Var fst, std::vector < SpinLock >& var_lock, struct SubsumeStatsData& stats, std::vector<OccUpdate>& occ_updates, Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);

    /** data for parallel execution */
    struct SubsumeWorkData {
        CP3Config*       config;      // configuration of CP3 instantiation
        Subsumption*     subsumption; // class with code
        CoprocessorData* data;        // formula and maintain lists
        unsigned int *   start;       // partition of the queue
        unsigned int     end;
        SpinLock *       balancerLock;
        std::vector<SpinLock> * var_locks;
        std::vector<Riss::CRef>*    to_delete;
        std::vector<Riss::CRef>*    set_non_learnt;
        std::vector<OccUpdate> * occ_updates;
        struct SubsumeStatsData* stats;
        Riss::Heap<VarOrderBVEHeapLt> * heap;
        Riss::Var              ignore;
    };

    /** run parallel subsumption with all available threads */
    void parallelSubsumption(const bool doStatistics = true);
    void parallelStrengthening(Riss::Heap<VarOrderBVEHeapLt> * heap, const Riss::Var ignore = var_Undef, const bool doStatistics = true);
  public:

    /** converts arg into SubsumeWorkData*, runs subsumption of its part of the queue */
    static void* runParallelSubsume(void* arg);
    static void* runParallelStrengthening(void * arg);
};

}

#endif
