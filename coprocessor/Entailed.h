/**************************************************************************************[Entailed.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_ENTAILED_HH
#define RISS_ENTAILED_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for checking whether clauses are entailed by the remaining formula cheaply
 *  (so far, check whether resolving two other clauses produces this clause)
 */
class EntailedRedundant : public Technique<EntailedRedundant>
{

    CoprocessorData& data;

    double processTime;
    int subsumed;
    int removedClauses;
    int extraSubs;

  public:
    EntailedRedundant(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data);

    void reset();

    /** check whether a clause is a resolvent of two other clauses in the formula, if yes - remove it
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();
};

}

#endif
