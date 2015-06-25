/****************************************************************************************[TwoSAT.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_TWOSAT_HH
#define RISS_TWOSAT_HH

#include "riss/core/Solver.h"

#include "coprocessor/CoprocessorTypes.h"

#include "coprocessor/Technique.h"

#include <vector>
#include <deque>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** perform 2-SAT checking */
class TwoSatSolver : public Technique
{
    CoprocessorData& data;
    BIG big;

    std::vector<char> tempVal, permVal;
    std::deque<Riss::Lit> unitQueue, tmpUnitQueue;
    int lastSeenIndex;

    double solveTime;     // number of seconds for solving
    int touchedLiterals;      // number of literals that have been touched during search
    int permLiterals;     // number of permanently fixed literals
    int calls;            // number of calls to twosat solver

  public:
    TwoSatSolver(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data);
    ~TwoSatSolver();

    bool solve();

    char getPolarity(const Riss::Var v) const;

    bool isSat(const Riss::Lit& l) const;

    bool isPermanent(const Riss::Var v) const;

    /** This method should be used to print the statistics of the technique that inherits from this class
    */
    void printStatistics(std::ostream& stream);

    void destroy();

    void giveMoreSteps();

  protected:

    bool unitPropagate();

    bool tmpUnitPropagate();

    bool hasDecisionVariable();

    Riss::Lit getDecisionVariable();
};

};

#endif
