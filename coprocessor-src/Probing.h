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
 
// forward declaration
  
/** class that implements probing techniques */
class Probing : public Technique {
  
  CoprocessorData& data;
  Solver& solver;

  // necessary local variables
  deque<Var> variableHeap;
  vec<lbool> prPositive;
  
  vector<Lit> learntUnits;
  vector<Lit> doubleLiterals;
  
public:
  Probing( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver);
  
  /** perform probing 
   * @return false, if formula is UNSAT
   */
  bool probe();
  
  /** This method should be used to print the statistics of the technique that inherits from this class
   */
  void printStatistics( ostream& stream );
  
  
protected:
  
  /** perform special propagation for probing (track ternary clauses, LHBR if enabled) */
  CRef prPropagate(bool doDouble = true); 
  
  /** perform conflict analysis and enqueue each unit clause that could be learned 
   * @return false, if formula is unsatisfiable
   */
  bool prAnalyze(CRef confl);
  
  /** perform double look ahead on literals that have been traced during level1 probing
   * @return true, if first decision is failed literal
   */
  bool prDoubleLook();
  
  /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
  void reSetupSolver();
  
  /** remove all clauses from the watch lists inside the solver */
  void cleanSolver();
  
  // staistics
  unsigned probeLimit;		// step limit for probing
  double processTime;		// seconds for probing
  unsigned l1implied;		// number of found l1 implied literals
  unsigned l1failed;		// number of found l1 failed literals
  unsigned l1learntUnit;	// learnt unit clauses
  unsigned l1ee;		// number of found l1 equivalences
  unsigned l2failed;		// number of found l2 failed literals
  unsigned l2ee;		// number of found l2 equivalences
  unsigned totalL2cand;		// number of l2 probe candidates
  unsigned probes;		// number of probes
  unsigned l2probes;		// number of l2 probes
};
  
};

#endif