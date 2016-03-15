/***************************************************************************************[ModPrep.h]
Copyright (c) 2016, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_MODPREP_HH
#define RISS_MODPREP_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"
#include "riss/utils/SimpleGraph.h"

// using namespace Riss;

namespace Coprocessor
{

/** experimental techniques, test bed
 */
class ModPrep : public Technique<ModPrep>
{

    CoprocessorData& data;
    Riss::Solver& solver;

    double processTime;

  public:
    ModPrep(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Riss::Solver& _solver);

    void reset();

    /** run experimental technique
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

    /** remove all clauses from the watch lists inside the solver */
    void cleanSolver();
    
    protected:
      
    bool getRandomCommunities(int randomCommunities, std::vector< int >& communityPerVariable, SimpleGraph*& communities, SimpleGraph*& communityNeighbors);
    
    bool getCommunities(vector< int >& communityPerVariable, SimpleGraph*& communities, SimpleGraph*& communityNeighbors);
    
    SimpleGraph* getVIG(int& step, int steplimit);
};

}

#endif
