/***************************************************************************************[Rewriter.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_REWRITER_HH
#define RISS_REWRITER_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

#include "Subsumption.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for bounded variable addition (replace patterns by introducion a fresh variable)
 */
class Rewriter : public Technique<Rewriter>
{

    CoprocessorData& data;

    // statistics
    double processTime;             // seconds of process time
    double rewAmoTime, rewImplTime; // time per procedure
    double amoTime;                 // seconds of process time
    double rewTime;                 // seconds of process time
    unsigned rewLimit;              // upper limit of steps
    unsigned steps;                 // current number of steps

    // TODO: initialize these ones!
    unsigned detectedDuplicates;     // how many clauses after rewriting detected as duplicate
    unsigned createdClauses;
    unsigned droppedClauses;
    unsigned enlargedClauses;
    unsigned sortCalls;
    unsigned reuses;
    unsigned processedAmos, processedChains;
    unsigned foundAmos;
    unsigned exoAMOs;
    unsigned maxAmo;
    unsigned addedVariables;
    unsigned removedVars;
    unsigned removedViaSubsubption;
    unsigned maxChain, minChain, foundChains;

    Propagation& propagation;    // UP object
    Subsumption& subsumption;    // object that takes care of subsumption and strengthening

    // work data
    /** compare two literals */
    struct LitOrderHeapLt {
        CoprocessorData& data;
        bool operator()(int& x, int& y) const
        {
            return data[ Riss::toLit(x)] < data[Riss::toLit(y)];
        }
        LitOrderHeapLt(CoprocessorData& _data) : data(_data) {}
    };
    Riss::Heap<LitOrderHeapLt> rewHeap; // heap that stores the variables according to their frequency

  public:

    Rewriter(Coprocessor::CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, Coprocessor::CoprocessorData& _data, Coprocessor::Propagation& _propagation, Coprocessor::Subsumption& _subsumption);

    void reset();

    /** applies bounded variable addition algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void destroy();

    void giveMoreSteps();

  protected:

    /** take care of creating a new variable */
    Riss::Var nextVariable(char type);

    /** method responsible for rewriting AMO constraints */
    bool rewriteAMO() ;
    /** method responsible for rewriting implication chains constraints */
    bool rewriteImpl() ;

    /** check whether the clause represented in the std::vector c has duplicates, and remove clauses that are subsumed by c */
    bool hasDuplicate(std::vector<Riss::CRef>& list, const Riss::vec<Riss::Lit>& c);
    bool hasDuplicate(std::vector<Riss::CRef>& list, const Riss::Clause& c);

    bool checkPush(Riss::vec<Riss::Lit>& ps, const Riss::Lit& l);
    bool ordered_subsumes(const Riss::Clause& c, const Riss::Clause& other) const;
    bool ordered_subsumes(const Riss::vec<Riss::Lit>& c, const Riss::Clause& other) const;
    bool ordered_subsumes(const Riss::Clause& c, const Riss::vec<Riss::Lit>& other) const;

  public:


};

};

#endif
