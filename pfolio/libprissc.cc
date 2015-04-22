/*************************************************************************************[libprissc.cc]
Copyright (c) 2014, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "pfolio/PSolver.h"
using namespace Riss;

/** struct that stores the necessary data for a preprocessor */
struct libpriss {
  Riss::vec<Riss::Lit> currentClause;
  Riss::vec<Riss::Lit> assumptions;
  Riss::PSolver* solver;
  Riss::lbool lastResult;
  libpriss () : lastResult(l_Undef) {} // default constructor to ensure everything is set to 0
};

// #pragma GCC visibility push(hidden)
// #pragma GCC visibility push(default)
// #pragma GCC visibility pop // now we should have default!

//#pragma GCC visibility push(default)

extern "C" {
  

/** initialize a solver instance, and return a pointer to the maintain structure 
* @param threads number of threads that should be used (1 <= threads <= 64), will be adjusted if not in these bounds!
*/
void* 
priss_init(int& threads, const char* presetConfig)
{
    if( threads < 1 ) threads = 1; // make sure that the number of available threads makes sense ...
    else if ( threads > 64 ) threads = 64;
    libpriss* priss = new libpriss;
    priss->solver = new Riss::PSolver (threads, presetConfig);
    return priss;
}
  
/** free the resources of the solver, set the pointer to 0 afterwards */
void 
priss_destroy(void*& priss)
{
  libpriss* solver = (libpriss*) priss;
  delete solver->solver;
  solver->currentClause.clear(true);
  solver->assumptions.clear(true);
  delete solver;
  priss = 0;
}
  
/** add a literal to the solver, if lit == 0, end the clause and actually add it */
int priss_add (void* priss, const int& lit) 
{
  libpriss* solver = (libpriss*) priss;
  bool ret = false;
  if( lit != 0 ) solver->currentClause.push( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) );
  else { // add the current clause, and clear the vector
    // reserve variables in the solver
    for( int i = 0 ; i < solver->currentClause.size(); ++i ) {
      const Lit l2 = solver->currentClause[i];
      const Var v = var(l2);
      while ( solver->solver->nVars() <= v ) solver->solver->newVar();
    }
    ret = solver->solver->addClause_( solver->currentClause );
    solver->currentClause.clear();
  }
  return ret ? 1 : 0;
}

/** add the given literal to the assumptions for the next solver call */
void 
priss_assume (void* priss, const int& lit)
{
  libpriss* solver = (libpriss*) priss;
  solver->assumptions.push( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true )  );
}
  
/** solve the formula that is currently present (priss_add) under the specified assumptions since the last call
 * Note: clears the assumptions after the solver run finished
 * @param nOfConflicts number of conflicts that are allowed for this SAT solver run (-1 = infinite)
 * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
 */
int 
priss_sat (void* priss, const int& nOfConflicts)
{
  libpriss* solver = (libpriss*) priss;
  if( nOfConflicts == -1 ) solver->solver->budgetOff();
  else solver->solver->setConfBudget( nOfConflicts );
  
  // make sure the assumptions fit into the memory of the solver!
  for( int i = 0 ; i < solver->assumptions.size(); ++ i ) {
    while( var(solver->assumptions[i]) >= solver->solver->nVars() ) solver->solver->newVar();
  }
  
  lbool ret = solver->solver->solveLimited( solver->assumptions );
  solver->assumptions.clear();		// clear assumptions after the solver call finished
  solver->lastResult = ret;		// store last solver result
  return ret == l_False ? 20 : ( ret == l_Undef ? 0 : 10); // return UNSAT, UNKNOWN or SAT, depending on solver result
}

/** return the polarity of a variable in the model of the last solver run (if the result was sat) */
int 
priss_deref (const void* priss, const int& lit) 
{
  const Var v = lit > 0 ? lit - 1 : -lit - 1;
  libpriss* solver = (libpriss*) priss;
  assert( solver->lastResult == l_True && "can deref literals only after SAT result" );
  lbool vValue = l_Undef;
  if( v < solver->solver->model.size() ) vValue = solver->solver->model[v];
  return ( lit < 0 ) ? (vValue == l_False ? 1 : (vValue == l_True ? -1 : 0) ) : (vValue == l_False ? -1 : (vValue == l_True ? 1 : 0) );
}

  
}

// #pragma GCC visibility pop

//#pragma GCC visibility pop