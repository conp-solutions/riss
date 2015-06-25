/**********************************************************************************[Experimental.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_EXPERIMENTAL_HH
#define RISS_EXPERIMENTAL_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

// using namespace Riss;

namespace Coprocessor
{

/** experimental techniques, test bed
 */
class ExperimentalTechniques : public Technique<ExperimentalTechniques>
{

    CoprocessorData& data;
    Riss::Solver& solver;

    double processTime;
    int subsumed;
    int removedClauses;
    int extraSubs;

  public:
    ExperimentalTechniques(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Riss::Solver& _solver);

    void reset();

    /** run experimental technique
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

    /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
    void reSetupSolver();

    /** remove all clauses from the watch lists inside the solver */
    void cleanSolver();
};

}

#endif
