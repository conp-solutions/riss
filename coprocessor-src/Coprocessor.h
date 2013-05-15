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
#include "coprocessor-src/Resolving.h"
#include "coprocessor-src/Rewriter.h"
#include "coprocessor-src/Dense.h"
#include "coprocessor-src/Symmetry.h"

#include "coprocessor-src/sls.h"
#include "coprocessor-src/TwoSAT.h"

#include <string>
#include <cstring>

using namespace Minisat;
using namespace std;

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
  double ppwTime;		// time to do preprocessing   (wall clock)
  double ipwTime;		// time to do inpreprocessing (wall clock)
  int thisClauses;		// number of original clauses before current run
  int thisLearnts;		// number of learnt clauses before current run
  
  int lastInpConflicts;		// number of conflicts when inprocessing has been called last time
  int formulaVariables;		// number of variables in the initial formula
  
public:

  Preprocessor(Solver* solver, int32_t _threads=-1 );
  ~Preprocessor();

  // major methods to start preprocessing
  lbool preprocess();
  lbool inprocess();
  lbool preprocessScheduled();
  lbool performSimplificationScheduled(string techniques);

  void extendModel( vec<lbool>& model );
  
  /* TODO:
   - extra classes for each extra techniques, which are friend of coprocessor class
   - extend model
   - ...
  */
  
  /** print formula (DIMACs), and dense, if another filename is given */
  void outputFormula(const char *file, const char *varMap = 0);

  // handle model processing
  
  const int getFormulaVariables() const { return formulaVariables; }
  
  /** parse model, if no file is specified, read from stdin 
   * @return false, if some error happened
   */
  int parseModel(const string& filename);

  /** parse model extend information 
   * @return false, if some error happened
   */
  bool parseUndoInfo(const string& filename);

  /** write model extend information to specified file 
   * @return false, if some error happened
   */
  bool writeUndoInfo(const string& filename);
  
  /** return info about formula to be writtern*/
  void getCNFinfo(int& vars, int& cls);

  /** write formula into file of file descriptor 
   * @param clausesOnly: will not print the cnf header (e.g. to print something before)
   */
  void printFormula(FILE* fd, bool clausesOnly = false);
  
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
  Resolving res;
  Rewriter rew;
  Dense dense;
  Symmetry symmetry;
  
  Sls sls;
  TwoSatSolver twoSAT;
  
  int shuffleVariable;  // number of variables that have been present when the formula has been shuffled
  
  // do the real work
  lbool performSimplification();
  void printStatistics(ostream& stream);
  
  // own methods:
  void cleanSolver();                // remove all clauses from structures inside the solver
  void reSetupSolver();              // add all clauses back into the solver, remove clauses that can be deleted
  void initializePreprocessor();     // add all clauses from the solver to the preprocessing structures
  void destroyTechniques();        // free resources of all preprocessing techniques

  void giveMoreSteps();
  
  void shuffle();			// shuffle the formula
  void unshuffle( vec< lbool >& model );	// unshuffle the formula
  
  // small helpers
  void sortClauses();                // sort the literals within all clauses
  void delete_clause(const CRef cr); // delete a clause from the solver (clause should not be attached within the solver)

  bool checkLists(const string& headline); // check each clause list for duplicate occurrences
  void fullCheck(const string& headline);  // check solver state before control is passed to solver
  void scanCheck(const string& headline);	// check clauses for duplicate literals
  
  // print formula
  inline void printClause(FILE * fd, CRef cr);
  inline void printLit(FILE * fd, int l);
  void printFormula( const string& headline );
  

  /** print current state of the solver */
  void printSolver(ostream& s, int verbose);
};

};

#endif
