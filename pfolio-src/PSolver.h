/***************************************************************************************[PSolver.h]
Copyright (c) 2014,      Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef Minisat_PSolver_h
#define Minisat_PSolver_h

#include "core/Solver.h"
#include "core/CoreConfig.h"
#include "coprocessor-src/CP3Config.h"

#include "core/Communication.h"

#include "pthread.h"

namespace Minisat {

  using namespace Coprocessor;
  
class PSolver {
  
  bool initialized;
  int threads;
  
  vec<Solver*> solvers;
  CoreConfig* configs; // the configuration for each solver
  CP3Config*  ppconfigs; // the configuration for each preprocessor
  
  CommunicationData* data;		// major data object that takes care of the sharing
  Communicator** communicators;		// interface between controller and SAT solvers
  pthread_t* threadIDs;			// pthread handles for the threads
  
public:
  
  PSolver(const int threadsToUse) ;
  
  ~PSolver();
  
  // Output for DRUP unsat proof
  FILE* drupProofFile;
  
  int verbosity, verbEveryConflicts; // how much information to be printed
  
  vec<lbool> model;             // If problem is satisfiable, this vector contains the model (if any).
  vec<Lit>   conflict;          // If problem is unsatisfiable (possibly under assumptions),  this vector represent the final conflict clause expressed in the assumptions.
  
  //
  // Control of parallel behavior
  //
  CoreConfig& getConfig( const int solverID );
  
  CP3Config& getPPConfig( const int solverID );
  
  //
  // solve the formula in parallel, including communication and all that
  //
  /** Search for a model that respects a given set of assumptions
   *  Note: when this method is called the first time, the solver incarnations are created
   */
  lbool solveLimited (const vec<Lit>& assumps); 
  
  //
  // executed only for the first solver (e.g. for parsing and simplification)
  //
  //
  /// The current number of original clauses of the 1st solver.
  int nClauses () const; 
  
  /// The current number of variables of the 1st solver.
  int nVars    () const; 
  
   /// The current number of total literals in the formula of the 1st solver.
  int nTotLits () const;
  
  /// reserve space for enough variables in the first solver
  void reserveVars( Var v );

  /// Removes already satisfied clauses in the first solver
  bool simplify(); 
  
  //
  // executed for all present solvers:
  //
  //
  /// Add a new variable with parameters specifying variable mode to all solvers
  Var  newVar (bool polarity = true, bool dvar = true, char type = 'o'); 
  
  bool addClause_(vec<Lit>& ps); // Add a clause to the solver without making superflous internal copy. Will change the passed vector 'ps'.
  
  void interrupt(); // Trigger a (potentially asynchronous) interruption of the solver.
  
protected:

  /** initialize all the threads 
   * @return false, if the initialization failed
   */
  bool initializeThreads();
    
  /** start solving all tasks with the given number of threads
   */
  void start();
  
  /** the master thread sleeps until some thread is done
   * @param waitState wait until the given condition is met
   *  note: will sleep, and sleep again, until the criterion is reached
   */
  void waitFor(const WaitState waitState);
  
  /** interrupt all solvers that are running at the moment once they reached level 0
   * @param forceRestart forces all solvers to perform a restart
   * note: calling thread will sleep until all threads are interrupted and waiting
   */
  void interrupt( const bool forceRestart );
  
  /** continue with the current work
   *  note: simply releases all waiting workers once
   */
  void continueWork();
  
  /** stops all parallel workers and kill their processes
   * note: afterwards, no other operations should be executed any more (for now)
   */
  void kill();
};

};

#endif