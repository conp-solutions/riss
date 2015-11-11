/**************************************************************************************[Unhiding.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_UNHIDING_HH
#define RISS_UNHIDING_HH

#include "riss/core/Solver.h"

#include "coprocessor/CoprocessorTypes.h"

#include "coprocessor/Technique.h"
#include "Propagation.h"
#include "Subsumption.h"
#include "EquivalenceElimination.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** This class implement hidden tautology elimination
 */
class Unhiding : public Technique<Unhiding>
{

    CoprocessorData& data;        // object to store all coprocessor data
    Propagation& propagation;     // object that takes care of unit propagation
    Subsumption& subsumption;     // object that takes care of subsumption and strengthening
    EquivalenceElimination& ee;   // object that takes care of equivalent literal elimination

    BIG big;

    bool uhdTransitive;       // transitive graph reduction?
    int unhideIter;           // mulitple iterations?
    int  doUHLE;              // run hidden literal elimination?
    bool doUHTE;              // run hidden tautology elimination?
    bool uhdNoShuffle;        // do not perform randomized depth first search in BIG
    bool uhdEE;               // use equivalent literal elimination

    unsigned removedClauses;  // number of removed clauses
    unsigned removedLiterals; // number of removed literals
    unsigned removedLits;     // number of literals that are removed by unhiding
    double unhideTime;        // seconds for unhiding

    uint64_t uhdProbeSteps;   // steps for probing during unhiding
    unsigned uhdProbeL1Units; // unit clauses found during weak uhd probe
    unsigned uhdProbeL2Units; // unit clauses found during weak uhd probe
    unsigned uhdProbeL3Units; // unit clauses found during weak uhd probe
    unsigned uhdProbeL4Units; // units that have been found by larger clause
    unsigned uhdProbeL5Units; // units that have been found by larger clause
    double unhideProbeTime;   // seconds for uhd probe

    unsigned uhdProbeEEChecks, uhdProbeEECandss, uhdProbeEE; // stats about probe EE

    /** structure that store all necessary stamp information of the paper for each literal */
    struct literalData {
        uint32_t fin;       // finished
        uint32_t dsc;       // discovered
        uint32_t obs;       // observed last
        Riss::Lit parent;   // parent literal (directly implied by)
        Riss::Lit root;     // root literal of the subtree that also implied this literal
        Riss::Lit lastSeen; //
        uint32_t index;     // index of the literal that has already been processed in the adjacence list of the literal
        literalData() : fin(0), dsc(0), obs(0), parent(Riss::lit_Undef), root(Riss::lit_Undef), index(0) {};
    };

    // stamp information (access via literalData[ literal.toIndex() ] ), is maintained by extendStructures-method
    std::vector<literalData> stampInfo;

    // queue of literals that have to be stamped in the current function call
    std::deque< Riss::Lit > stampQueue;
    // equivalent literals during stamping
    std::vector< Riss::Lit > stampEE;
    std::vector< Riss::Lit > stampClassEE;
    std::vector< char > unhideEEflag;

    std::vector< int > currentPosition; // fur full probing approximation
    std::vector< Riss::Lit > currentLits; // current literals for full probing approximation
    std::vector< int > currentLimits; // all combination limits for full probing

  public:

    Unhiding(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data,  Propagation& _propagation, Subsumption& _subsumption, EquivalenceElimination& _ee);

    /** perform unhiding algorithm */
    bool process();

    /** This method should be used to print the statistics of the technique that inherits from this class */
    void printStatistics(std::ostream& stream);

    void destroy();

    void giveMoreSteps();

  protected:

    /** sorts the given array with increasing discovery stamp
     * NOTE: uses insertion sort
     */
    void sortStampTime(Riss::Lit* literalArray, const uint32_t size);

    /** execute the advanced stamping algorithm
     * NOTE: there is a parameter that controls whether the advanced stamping is used
     *
     *  @param literal root literal for the subtree to stamp
     *  @param stamp current stamp index
     *  @param detectedEE mark whether equivalent literals have been found
     */
    uint32_t stampLiteral(const Riss::Lit& literal, uint32_t stamp, bool& detectedEE);

    /** linear version of the advanced stamping */
    uint32_t linStamp(const Riss::Lit& literal, uint32_t stamp, bool& detectedEE);

    /** simplify the formula based on the literal stamps */
    bool unhideSimplify(bool borderIteration, bool& foundEE);
};


};

#endif
