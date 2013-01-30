/***********************************************************************************[Coprocessor.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSOR_HH
#define COPRECESSOR_HH


#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/CoprocessorThreads.h"

#include "coprocessor-src/Propagation.h"
#include "coprocessor-src/Subsumption.h"
#include "coprocessor-src/HiddenTautologyElimination.h"
#include "coprocessor-src/BoundedVariableElimination.h"
#include "coprocessor-src/ClauseElimination.h"
#include "coprocessor-src/EquivalenceElimination.h"
#include "coprocessor-src/Bva.h"
#include "coprocessor-src/Unhiding.h"
#include "coprocessor-src/Probing.h"

#include "coprocessor-src/sls.h"
#include "coprocessor-src/TwoSAT.h"

using namespace Minisat;

namespace Coprocessor {
/** Main class that connects all the functionality of the preprocessor Coprocessor
 */
class Preprocessor {

  // friends

  // attributes
  int32_t threads;             // number of threads that can be used by the preprocessor
  Solver* solver;              // handle to the solver object that stores the formula
  ClauseAllocator& ca;         // reference to clause allocator

  Logger log;                  // log output
  CoprocessorData  data;       // all the data that needs to be accessed by other classes (preprocessing methods)
  ThreadController controller; // controller for all threads

  bool isInprocessing;		// control whether current call of the preprocessor should be handled as inprocessing
  double ppTime;		// time to do preprocessing
  double ipTime;		// time to do inpreprocessing
  
public:

  Preprocessor(Solver* solver, int32_t _threads=-1 );
  ~Preprocessor();

  // major methods to start preprocessing
  lbool preprocess();
  lbool inprocess();
  lbool preprocessScheduled();

  void extendModel( vec<lbool>& model );
  
  /* TODO:
   - extra classes for each extra techniques, which are friend of coprocessor class
   - extend model
   - ...
  */
  // print formula (DIMACs)
  void outputFormula(const char *file);


protected:
  // techniques
  Subsumption subsumption;
  Propagation propagation;
  HiddenTautologyElimination hte;
  BoundedVariableElimination bve;
  BoundedVariableAddition bva;
  ClauseElimination cce;
  EquivalenceElimination ee;
  Unhiding unhiding;
  Probing probing;
  
  Sls sls;
  TwoSatSolver twoSAT;

  // do the real work
  lbool performSimplification();
  void printStatistics(ostream& stream);
  
  // own methods:
  void cleanSolver();                // remove all clauses from structures inside the solver
  void reSetupSolver();              // add all clauses back into the solver, remove clauses that can be deleted
  void initializePreprocessor();     // add all clauses from the solver to the preprocessing structures
  void destroyPreprocessor();        // free resources of all preprocessing techniques

  // small helpers
  void sortClauses();                // sort the literals within all clauses
  void delete_clause(const CRef cr); // delete a clause from the solver (clause should not be attached within the solver)

  // print formula
  void printFormula(FILE * fd);
  inline void printClause(FILE * fd, CRef cr);
  inline void printLit(FILE * fd, int l);
  void printFormula( const string& headline );

};

};

#endif
