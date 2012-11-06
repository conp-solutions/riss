/**********************************************************************************[Coprocessor.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Coprocessor.h"

#include <iostream>

static const char* _cat = "COPROCESSOR 3";

static IntOption opt_threads  (_cat, "cp3_threads",  "Number of extra threads that should be used for preprocessing", 2, IntRange(0, INT32_MAX));

using namespace std;
using namespace Coprocessor;

Preprocessor::Preprocessor( Solver* _solver, int32_t _threads)
: threads( _threads < 0 ? opt_threads : _threads)
, solver( _solver )
, ca( solver->ca )
, data( solver->ca, solver )
, controller( opt_threads )
// attributes and all that

// classes for preprocessing methods
, subsumption( solver->ca, controller )
, propagation( solver->ca, controller )
{
  controller.init();
}

Preprocessor::~Preprocessor()
{
  
}
  
lbool Preprocessor::preprocess()
{
  cerr << "c start preprocessing with coprocessor" << endl;
  
  // first, remove all satisfied clauses
  if( !solver->simplify() ) return l_False;
  
  lbool status = l_Undef;
  // delete clauses from solver
  cleanSolver ();
  // initialize techniques
  data.init( solver->nVars() );
  initializePreprocessor ();
  cerr << "c coprocessor finished initialization" << endl;
  // do preprocessing

  cerr << "c coprocessor propagate" << endl;
  if( status == l_Undef ) status = propagation.propagate(data, solver);
  
  // begin clauses have to be sorted here!!
  sortClauses();
  
  cerr << "c coprocessor subsume/strengthen" << endl;
  if( status == l_Undef ) subsumption.subsumeStrength(data);  // cannot change status, can generate new unit clauses
  
  
  // clear / update clauses and learnts vectores and statistical counters
  // attach all clauses to their watchers again, call the propagate method to get into a good state again
  cerr << "c coprocessor re-setup solver" << endl;
  reSetupSolver();

  // destroy preprocessor data
  cerr << "c coprocessor free data structures" << endl;
  data.destroy();
  
  return l_Undef;
}

lbool Preprocessor::inprocess()
{
  // TODO: do something before preprocessing? e.g. some extra things with learned / original clauses
  
  
  return preprocess();
}

lbool Preprocessor::preprocessScheduled()
{
  // TODO execute preprocessing techniques in specified order
  return l_Undef;
}

void Preprocessor::initializePreprocessor()
{
  uint32_t clausesSize = (*solver).clauses.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    const CRef cr = solver->clauses[i];
    data.addClause( cr );
    subsumption.initClause( cr );
    propagation.initClause( cr );
  }
  
  clausesSize = solver->learnts.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    const CRef cr = solver->learnts[i];
    data.addClause( cr );
    subsumption.initClause( cr );
    propagation.initClause( cr );
  }
}

void Preprocessor::destroyPreprocessor()
{
  subsumption.destroy();
  propagation.destroy();
}


void Preprocessor::cleanSolver()
{
  // clear all watches!
  for (int v = 0; v < solver->nVars(); v++)
    for (int s = 0; s < 2; s++)
      solver->watches[ mkLit(v, s) ].clear();

  solver->learnts_literals = 0; 
  solver->clauses_literals = 0;
}

void Preprocessor::reSetupSolver()
{
    int j = 0;

    for (int i = 0; i < solver->clauses.size(); ++i)
    {
        const CRef cr = solver->clauses[i];
        Clause & c = ca[cr];
        if (c.can_be_deleted())
            delete_clause(cr);
        else 
        {   
	  assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
            if (c.size() > 1)
            {
                solver->clauses[j++] = cr;    
                solver->attachClause(cr);
            }
            else if (solver->value(c[0]) == l_Undef)
                solver->uncheckedEnqueue(c[0]);
	    else if (solver->value(c[0]) == l_False )
	    {
	      assert( false && "This UNSAT case should be recognized before re-setup" );
	      solver->ok = false;
	    }
        }
    }
    int c_old = solver->clauses.size();
    solver->clauses.shrink(solver->clauses.size()-j);
    
    fprintf(stderr,"c Subs-STATs: removed clauses: %i of %i," ,c_old - j,c_old);

    int learntToClause = 0;
    j = 0;
    for (int i = 0; i < solver->learnts.size(); ++i)
    {
        const CRef cr = solver->learnts[i];
        Clause & c = ca[cr];
        assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
        if (c.can_be_deleted())
        {
            delete_clause(cr);
            continue;
        } else if (c.learnt()) {
            if (c.size() > 1)
            {
                solver->learnts[j++] = solver->learnts[i];
            }
        } else { // move subsuming clause from learnts to clauses
	    c.set_has_extra(true);
            c.calcAbstraction();
            learntToClause ++;
	    	if (c.size() > 1)
            {
                solver->clauses.push(cr);
            }
 	    }
        if (c.size() > 1)
            solver->attachClause(cr);
        else if (solver->value(c[0]) == l_Undef)
          solver->uncheckedEnqueue(c[0]);
	else if (solver->value(c[0]) == l_False )
	{
	  assert( false && "This UNSAT case should be recognized before re-setup" );
	  solver->ok = false;
	}
    }
    int l_old = solver->learnts.size();
    solver->learnts.shrink(solver->learnts.size()-j);
    printf(" moved %i and removed %i from %i learnts\n",learntToClause,(l_old - j) -learntToClause, l_old);
}

void Preprocessor::sortClauses()
{
  uint32_t clausesSize = (*solver).clauses.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    Clause& c = ca[solver->clauses[i]];
    const uint32_t s = c.size();
    for (uint32_t j = 1; j < s; ++j)
    {
        const Lit key = c[j];
        int32_t i = j - 1;
        while ( i >= 0 && toInt(c[i]) > toInt(key) )
        {
            c[i+1] = c[i];
            i--;
        }
        c[i+1] = key;
    }
  }
  
  clausesSize = solver->learnts.size();
  for (int i = 0; i < clausesSize; ++i)
  { 
    Clause& c = ca[solver->learnts[i]];
    const uint32_t s = c.size();
    for (uint32_t j = 1; j < s; ++j)
    {
        const Lit key = c[j];
        int32_t i = j - 1;
        while ( i >= 0 && toInt(c[i]) > toInt(key) )
        {
            c[i+1] = c[i];
            i--;
        }
        c[i+1] = key;
    }
  }
}




void Preprocessor::delete_clause(const Minisat::CRef cr)
{
  Clause & c = ca[cr];
  c.mark(1);
  ca.free(cr);
}
