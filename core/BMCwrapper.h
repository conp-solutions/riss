/************************************************************************************[BMCwrapper.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

/**
 *  This file provides the interface between the Minisat based SAT solver für BMC
 */

#ifndef BMCwrapper_h
#define BMCwrapper_h

#include "core/Solver.h"

namespace Minisat {
  
/**
 *  variable representation for interface:  1 - n
 */
class IncSolver {
  CoreConfig config; // configuration for the solver
  Solver* solver;    // the solver itself

  vec<Lit> currentAssumptions; // store the assumptions for the next call to sat()
  vec<Lit> currentClause; // store the literals for the current clause
  
public:
  IncSolver (int& argc, char **& argv) 
  {
    config.parseOptions(argc,argv);
    solver = new Solver(config);
  }

  IncSolver (Solver* s, CoreConfig& c) 
  : config(c), solver (s)
  {}
  
  void destroy() { 
    if( solver != 0 ) { delete solver; solver = 0 ; } 
  }
  
  /** add a literal to the solver, if lit == 0, end the clause and actually add it */
  void add (int lit) {
    if( lit != 0 ) currentClause.push( lit > 0 ? mkLit( lit, false ) : mkLit( -lit, true ) );
    else { // add the current clause, and clear the vector
      // reserve variables in the solver
      for( int i = 0 ; i < currentClause.size(); ++i ) {
	const Lit l2 = currentClause[i];
	const Var v = var(l2);
	while ( solver->nVars() <= v ) solver->newVar();
      }
      solver->addClause( currentClause );
      currentClause.clear();
    }
  }

  /** add another literal to the assumptions vector for the next sat() call 
   *  Note: rejects lit==0
   */
  void assume (int lit) { 
    if( lit != 0 ) currentAssumptions.push( lit > 0 ? mkLit( lit, false ) : mkLit( -lit, true ) );
  }

  /** solve the current formula under the current set of assignments 
   * Note: will clear the assumption vector after the call
   */
  int sat () {
    lbool ret = solver->solveLimited( currentAssumptions );
    currentAssumptions.clear();
    assert ( ret != l_Undef && "current sat call did not succeed" );
    return ret == l_True ? 10 : 20;
  }

  /** returns the truth-value of the specified literal in the current model 
   *  1 = true, 0 = false
   */
  int deref (int lit) {
    assert (lit != 0 && "the method cannot hande lit == 0" );
    const Lit l = lit > 0 ? mkLit( lit, false ) : mkLit( -lit, true );
    return solver->value( l ) == l_True ? 1 : -1;
  }
  
  CoreConfig& getConfig(){ return config; }
  
  // TODO add interface for simplification and all that!
  
}; 
  
}

#endif