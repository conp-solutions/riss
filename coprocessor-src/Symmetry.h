/**************************************************************************************[Symmetry.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef SYMMETRY_HH
#define SYMMETRY_HH

#include "core/Solver.h"
#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Technique.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {
 
// forward declaration
  
/** class that implements local symmetry detection */
class Symmetry : public Technique {
  
  CoprocessorData& data;
  Solver& solver;
  
  // necessary local variables
  
public:
  Symmetry( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver);
  
  /** perform local symmetry detection
   * @return false, if formula is UNSAT
   */
  bool process();
  
  /** This method should be used to print the statistics of the technique that inherits from this class
   */
  void printStatistics( ostream& stream );
  
  void destroy();
  
  void giveMoreSteps();
  
protected:

};
  
};

#endif