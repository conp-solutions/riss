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
, data( solver->ca )
// attributes and all that

// classes for preprocessing methods
, subsumption( solver->ca )
, propagation( solver->ca )
{
  
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
  
  cerr << "c coprocessor subsume/strengthen" << endl;
  if( status == l_Undef ) subsumption.subsumeStrength();  // cannot change status, can generate new unit clauses
  
  
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
    //TODO: port!
/*
    for (int i = 0; i < clauses.size(); ++i)
    {
        CRef cr = clauses[i];
        Clause & c = ca[cr];
        if (c.can_be_deleted())
            delete_clause(cr);
        else 
        {    
            if (c.size() > 1)
            {
                clauses[j++] = cr;    
                attachClause(cr);
            }
            else if (value(c[0]) == l_Undef)
                uncheckedEnqueue(c[0]);
        }
    }
    int c_old = clauses.size();
    clauses.shrink(clauses.size()-j);
    
    printf("c Subs-STATs: removed clauses: %i of %i," ,c_old - j,c_old);

    int learntToClause = 0;
    j = 0;
    for (int i = 0; i < learnts.size(); ++i)
    {
        CRef cr = learnts[i];
        Clause & c = ca[cr];
        if (c.can_be_deleted())
        {
            delete_clause(cr);
            continue;
        }
        else if (c.learnt())
        {
            if (c.size() > 1)
            {
                learnts[j++] = learnts[i];
            }
        }
        // move subsuming clause from learnts to clauses
        else
	    {
		    c.set_has_extra(true);
            c.calcAbstraction();
            learntToClause ++;
	    	if (c.size() > 1)
            {
                clauses.push(cr);
            }
 	    }
        if (c.size() > 1)
            attachClause(cr);
        else if (value(c[0]) == l_Undef)
            uncheckedEnqueue(c[0]);
    }
    int l_old = learnts.size();
    learnts.shrink(learnts.size()-j);
    printf(" moved %i and removed %i from %i learnts\n",learntToClause,(l_old - j) -learntToClause, l_old);
    */
}


