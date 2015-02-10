/**********************************************************************************[Experimental.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef EXPERIMENTAL_HH
#define EXPERIMENTAL_HH

#include "core/Solver.h"
#include "coprocessor-src/Technique.h"
#include "coprocessor-src/CoprocessorTypes.h"

using namespace Riss;

namespace Coprocessor {

/** experimental techniques, test bed
 */
class ExperimentalTechniques : public Technique  {
    
  CoprocessorData& data;
  Solver& solver;
  
  double processTime;
  int subsumed;
  int removedClauses;
  int extraSubs;
  
public:
  ExperimentalTechniques( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver );

  void reset();
  
  /** run experimental technique
  * @return true, if something has been altered
  */
  bool process();
    
  void printStatistics(ostream& stream);

  void giveMoreSteps();
  
  void destroy();
  
  /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
  void reSetupSolver();
  
  /** remove all clauses from the watch lists inside the solver */
  void cleanSolver();
};

}

#endif