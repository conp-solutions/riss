/***************************************************************************************[Probing.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef PROBING_HH
#define PROBING_HH

#include "core/Solver.h"
#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Technique.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {
  
/** class that implements probing techniques */
class Probing : public Technique {
  
  CoprocessorData& data;
  Solver& solver;

  // necessary local variables
  deque<Var> variableHeap;
  unsigned probeLimit;
  vec<lbool> prPositive;
  
  vector<Lit> doubleLiterals;
  
public:
  Probing( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver);
  
  bool probe();
  
  /** This method should be used to print the statistics of the technique that inherits from this class
   */
  void printStatistics( ostream& stream );
  
  
protected:
  
  /** perform special propagation for probing (track ternary clauses, LHBR if enabled) */
  CRef prPropagate(); 
  
  /** perform conflict analysis and enqueue each unit clause that could be learned 
   * @return false, if formula is unsatisfiable
   */
  bool prAnalyze();
  
  /** perform double look ahead on literals that have been traced during level1 probing
   * @return true, if first decision is failed literal
   */
  bool prDoubleLook();
  
};
  
};

#endif