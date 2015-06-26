/*******************************************************************************************[BCE.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_BCE_HH
#define RISS_BCE_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"
#include "Propagation.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for blocked clause elimination procedure
 */
class BlockedClauseElimination : public Technique<BlockedClauseElimination>
{

    CoprocessorData& data;
    Coprocessor::Propagation& propagation;

    /** compare two literals */
    struct LitOrderBCEHeapLt { // sort according to number of occurrences of complement!
        CoprocessorData& data;  // data to use for sorting
        bool useComplements; // sort according to occurrences of complement, or actual literal
        bool operator()(int& x, int& y) const
        {
            if (useComplements) { return data[ ~Riss::toLit(x)] < data[ ~Riss::toLit(y) ]; }
            else { return data[ Riss::toLit(x)] < data[ Riss::toLit(y) ]; }
        }
        LitOrderBCEHeapLt(CoprocessorData& _data, bool _useComplements) : data(_data), useComplements(_useComplements) {}
    };

    // attributes
    int bceSteps, testedLits;
    int cleCandidates; // number of clauses that have been checked for cle
    int remBCE, remCLE, cleUnits; // how many clauses / literals have been removed
    int bcm_cls, bcm_cls_cands, bcm_lits;        // number of clauses that have been affected by BCM, candidate clauses, literals that have been removed by BCM
    Clock bceTime, claTime; // clocks for the two methods

    int claTestedLits, claSteps, claExtendedClauses, claExtensions;
    int64_t possibleClaExtensions; // cla stats


  public:
    BlockedClauseElimination(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Coprocessor::Propagation& _propagation);

    void reset();

    /** applies blocked clause elimination algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

  protected:
    /** check whether resolving c and d on literal l results in a tautology
     * Note: method assumes c and d to be sorted
     * @return Lit_Undef, if the resolvent is no tautology, otherwise the (first) literal of c, which produces the tautologic resolvent
     */
    Riss::Lit tautologicResolvent(const Riss::Clause& c, const Riss::Clause& d, const Riss::Lit& l) const ;

    /** run blocked clause elimination, and covered literal elimination */
    void blockedClauseElimination();

    /** run a covered literal addition to increase the size of clauses */
    void coverdLiteralAddition();

};

}

#endif
