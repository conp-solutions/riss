/********************************************************************[HiddenTautologyElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_HIDDENTAUTOLOGYELIMINATION_HH
#define RISS_HIDDENTAUTOLOGYELIMINATION_HH

#include "riss/core/Solver.h"

#include "coprocessor/Technique.h"

#include "coprocessor/CoprocessorTypes.h"

#include "Propagation.h"

#include <vector>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** This class implement hidden tautology elimination
 */
class HiddenTautologyElimination : public Technique<HiddenTautologyElimination>
{

    Propagation& propagation;    // object that takes care of unit propagation

    int steps;                   // how many steps is the worker allowed to do
    double processTime;          // how many seconds have been used
    int removedClauses;          // how many clauses could be removed
    int removedLits;             // how many literals have been removed

    std::vector<Riss::Var> activeVariables; // which variables should be considered?
    std::vector<char>      activeFlag;      // array that stores a flag per variable whether it is active



  public:

    HiddenTautologyElimination(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, Propagation& _propagation);

    /** run subsumption and strengthening until completion */
    void process(Coprocessor::CoprocessorData& data);

    void initClause(const Riss::CRef& cr);  // inherited from Technique

    void printStatistics(std::ostream& stream);

    /** fills the mark arrays for a certain variable */
    Riss::Lit fillHlaArrays(Riss::Var v, Coprocessor::BIG& big, Riss::MarkArray& hlaPositive, Riss::MarkArray& hlaNegative, Riss::Lit* litQueue, bool doLock = false);

    /** mark all literals that would appear in HLA(C)
     * @return true, if clause can be removed by HTE
     */
    bool hlaMarkClause(const Riss::CRef& cr, Coprocessor::BIG& big, Riss::MarkArray& markArray, Riss::Lit* litQueue);

    /** same as above, but can add literals to the std::vector, so that the std::vector represents the real HLA(C) clause */
    bool hlaMarkClause(Riss::vec< Riss::Lit >& clause, Coprocessor::BIG& big, Riss::MarkArray& markArray, Riss::Lit* litQueue, bool addLits = false);

    /** mark all literals that would appear in ALA(C)
     * @return true, if clause can be removed by ATE
     */
    bool alaMarkClause(const Riss::CRef& cr, Coprocessor::CoprocessorData& data, Riss::MarkArray& markArray, Riss::MarkArray& helpArray);

    /** same as above, but can add literals to the std::vector, so that the std::vector represents the real ALA(C) clause */
    bool alaMarkClause(Riss::vec< Riss::Lit >& clause, Coprocessor::CoprocessorData& data, Riss::MarkArray& markArray, Riss::MarkArray& helpArray, bool addLits = false);

    void destroy();

    void giveMoreSteps();

  protected:

    /** is there currently something to do? */
    bool hasToEliminate();

    /** apply hte for the elements in the queue in the intervall [stard,end[ (position end is not touched!) */
    void elimination_worker(CoprocessorData& data, uint32_t start, uint32_t end, BIG& big, bool doStatistics = true, bool doLock = false);  // subsume certain set of elements of the processing queue, does not write to the queue

    /** run hte for the specified variable */
    bool hiddenTautologyElimination(Riss::Var v, Coprocessor::CoprocessorData& data, Coprocessor::BIG& big, Riss::MarkArray& hlaPositive, Riss::MarkArray& hlaNegative, bool statistic = true, bool doLock = false);

    /** data for parallel execution of HTE */
    struct EliminationData {
        HiddenTautologyElimination* hte; // class with code
        CoprocessorData* data;           // formula and maintain lists
        BIG* big;                        // handle to binary implication graph
        Riss::Var start;                       // partition of the queue
        Riss::Var end;
    };

    /** run parallel subsumption with all available threads */
    void parallelElimination(Coprocessor::CoprocessorData& data, Coprocessor::BIG& big);

  public:

    /** converts arg into EliminationData*, runs hte of its part of the queue */
    static void* runParallelElimination(void* arg);

};

}

#endif
