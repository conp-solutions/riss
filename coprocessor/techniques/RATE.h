/******************************************************************************************[rate.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_RATE_HH
#define RISS_RATE_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"
#include "Propagation.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for blocked clause elimination procedure
 */
class RATElimination : public Technique<RATElimination>
{

    CoprocessorData& data;
    Riss::Solver& solver;
    Coprocessor::Propagation& propagation;

    /** compare two literals */
    struct LitOrderRATEHeapLt { // sort according to number of occurrences of complement!
        CoprocessorData& data;  // data to use for sorting
        bool useComplements; // sort according to occurrences of complement, or actual literal
        bool operator()(int& x, int& y) const
        {
            if (useComplements) { return data[ ~Riss::toLit(x)] < data[ ~Riss::toLit(y) ]; }
            else { return data[ Riss::toLit(x)] < data[ Riss::toLit(y) ]; }
        }
        LitOrderRATEHeapLt(CoprocessorData& _data, bool _useComplements) : data(_data), useComplements(_useComplements) {}
    };

    // attributes
    int64_t rateSteps, ratmSteps, approxRatm;
    float approxFacAB, approxFacA;
    int rateCandidates; // number of clauses that have been checked for cle
    int remRAT, remAT, remHRAT, remBCE, remBRAT, blockCheckOnSameClause; // how many clauses
    int minRATM, minATM , unitRATM, conflRATM, conflATM, roundsRATM; // how may literals removed / propagated Units / conflicts
    Clock ratmTime, rateTime, bcaTime, bratTime; // clocks for the two methods


    // BCA
    int bcaCandidates, bcaResolutionChecks, bcaSubstitue, bcaSubstitueLits, bcaFullMatch, bcaATs, bcaStrenghening;

  public:
    RATElimination(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data,  Riss::Solver& _solver, Coprocessor::Propagation& _propagation);

    void reset();

    /** applies (hidden) resolution asymmetric tautology elimination algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

  protected:

    /** resolve clause D on literal l, ma stores the literals of the other clause
     *  resolve clause D, ~l \in D, with existing literals in resolvent on literal l
     * @param resolvent initially contains the literals of the other clause (except literal l), afterwards, stores the resolvent, if the resolvent is not a  tautology
     * @return true, if the resolvent is a tautology
     */
    bool resolveUnsortedStamped(const Riss::Lit& l, const Riss::Clause& d, Riss::MarkArray& ma, std::vector<Riss::Lit>& resolvent);

    /** run RAT elmination
     @return true, if modifications have been applied
     */
    bool eliminateRAT();

    /** run RAT minimization
    @return true, if modifications have been applied
    */
    bool minimizeRAT();

    bool shortATM(const Riss::CRef& clause, const Riss::Lit& left, int& trailPosition, std::vector<Riss::Lit>& atlits);

    bool propagateUnit(const Riss::Lit& unit, int& trailPosition);

    void checkedAttach(const Riss::CRef& clause, const int& detachTrailSize);

    bool minimizeAT();

    /** add new redundant clauses which turn another clause in the formula into an AT
     @return true, if modifications have been applied
     */
    bool blockedSubstitution();

    /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
    void reSetupSolver();

    /** remove all clauses from the watch lists inside the solver */
    void cleanSolver();
};

}

#endif
