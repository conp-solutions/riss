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
    if( lit != 0 ) currentClause.push( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) );
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
    if( lit != 0 ) currentAssumptions.push( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) );
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
    const lbool ret = lit > 0 ? solver->model[ lit-1 ] : (solver->model[ -lit-1 ] ^ true); // second statement should invert the value
    const int iret = ret == l_False ? -1 : (ret == l_True ? 1 : 0); // if the value is undefined, map it to true! (important for the output state!)
    return iret;
  }
  
  void setMaxVar( int maxVar ) {
    while( solver->nVars() < maxVar ) solver->newVar();
  }
  
  CoreConfig& getConfig(){ return config; }
  
  /** give current model on stderr */
  void printModel () {
   fprintf(stderr,"s SATISFIABLE\nv ");
	      for (int i = 0; i < solver->nVars(); i++)
		if (solver->model[i] != l_Undef)
		  fprintf(stderr,"%s%s%d", (i==0)?"":" ", (solver->model[i]==l_True)?"":"-", i+1);
	fprintf(stderr," 0\n");	      
  }
  
  /** dump formula to stderr */
  void printFormula() {
    // top level unit clauses
    for( int i = 0 ; i < solver->trail.size(); ++ i ) {
      const Lit& l =  solver->trail[i];
      cerr << l << " " << 0 << endl;
    }
    for( int i = 0 ; i < solver->clauses.size(); ++ i ) {
      const Clause& c = solver->ca[solver->clauses[i]];
      if( c.can_be_deleted() ) continue; // do only process valid clauses!
      for( int j = 0; j < c.size(); ++ j ) {
	const Lit& l =  c[j];
	cerr << l << " ";
      }
      cerr << 0 << endl;
    }
  }
  
  // TODO add interface for simplification and all that!
  // remove everything from the solver!
  void clearSolver() {
    // set ok to true
    solver->ok = true;
    // clear all watch lists
    solver->watches.cleanAll();
    solver->watchesBin.cleanAll();
    // clear clauses
    solver->clauses.clear();
    solver->learnts.clear();
    // clear clause allocator
    solver->ca.clear();
    
    // clear all watches!
    for (int v = 0; v < solver->nVars(); v++)
      for (int s = 0; s < 2; s++)
	solver->watches[ mkLit(v, s) ].clear();
      
    // for glucose, also clean binary clauses!
    for (int v = 0; v < solver->nVars(); v++)
      for (int s = 0; s < 2; s++)
	solver->watchesBin[ mkLit(v, s) ].clear();

    solver->learnts_literals = 0;
    solver->clauses_literals = 0;
    
    // clear all internal structures
    solver->assigns  .clear();
    solver->model    .clear();
    solver->vardata  .clear();
    //activity .push(0);
    solver->activity .clear();
    solver->seen     .clear();
    solver->permDiff .clear();
    solver->polarity .clear();
    solver->decision .clear();
    // remove all assignments
    solver->trail.clear();
    solver->trail_lim.clear();
    solver->qhead = 0;
    solver->conflicts = 0;
    
    // specifically for glucose 22
    solver->curRestart = 1;
    solver->lbdQueue.fastclear();
    // TODO: any other (new) things?
  }
}; 
  
}

#endif