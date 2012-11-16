/**********************************************************************************[Coprocessor.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Coprocessor.h"

#include <iostream>

static const char* _cat = "COPROCESSOR 3";
static const char* _cat2 = "CP3 TECHNIQUES";

// options
static IntOption  opt_threads    (_cat, "cp3_threads",    "Number of extra threads that should be used for preprocessing", 0, IntRange(0, INT32_MAX));
static BoolOption opt_unlimited  (_cat, "cp3_unlimited",  "No limits for preprocessing techniques", true);
static BoolOption opt_randomized (_cat, "cp3_randomized", "Steps withing preprocessing techniques are executed in random order", false);
static IntOption  opt_verbose    (_cat, "cp3_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3));
// techniques
static BoolOption opt_up      (_cat, "up",          "Use Unit Propagation during preprocessing", false);
static BoolOption opt_subsimp (_cat, "subsimp",     "Use Subsumption during preprocessing", false);
static BoolOption opt_hte     (_cat, "hte",         "Use Hidden Tautology Elimination during preprocessing", false);
static BoolOption opt_cce     (_cat, "cce",         "Use (covered) Clause Elimination during preprocessing", false);
static BoolOption opt_enabled (_cat, "enabled_cp3", "Use CP3", false);

static IntOption  opt_log     (_cat, "log",         "Output log messages until given level", 0, IntRange(0, 3));

using namespace std;
using namespace Coprocessor;

Preprocessor::Preprocessor( Solver* _solver, int32_t _threads)
: threads( _threads < 0 ? opt_threads : _threads)
, solver( _solver )
, ca( solver->ca )
, log( opt_log )
, data( solver->ca, solver, log, opt_unlimited, opt_randomized )
, controller( opt_threads )
// attributes and all that

// classes for preprocessing methods
, subsumption( solver->ca, controller )
, propagation( solver->ca, controller )
, hte( solver->ca, controller )
, cce( solver->ca, controller )
{
  controller.init();
}

Preprocessor::~Preprocessor()
{

}

lbool Preprocessor::preprocess()
{
  if( ! opt_enabled ) return l_Undef;
  if( opt_verbose > 2 ) cerr << "c start preprocessing with coprocessor" << endl;

  // first, remove all satisfied clauses
  if( !solver->simplify() ) return l_False;

  lbool status = l_Undef;
  // delete clauses from solver
  cleanSolver ();
  // initialize techniques
  data.init( solver->nVars() );
  initializePreprocessor ();
  if( opt_verbose > 2 )cerr << "c coprocessor finished initialization" << endl;
  // do preprocessing

  if( opt_up ) {
    if( opt_verbose > 2 )cerr << "c coprocessor propagate" << endl;
    if( status == l_Undef ) status = propagation.propagate(data, solver);
  }

  // begin clauses have to be sorted here!!
  sortClauses();

  if( opt_subsimp ) {
    if( opt_verbose > 2 )cerr << "c coprocessor subsume/strengthen" << endl;
    if( status == l_Undef ) subsumption.subsumeStrength(data);  // cannot change status, can generate new unit clauses
  }

  if( opt_hte ) {
    if( opt_verbose > 2 )cerr << "c coprocessor hidden tautology elimination" << endl;
    if( status == l_Undef ) hte.eliminate(data);  // cannot change status, can generate new unit clauses
  }

  if( opt_cce ) {
    if( opt_verbose > 2 )cerr << "c coprocessor (covered) clause elimination" << endl;
    if( status == l_Undef ) cce.eliminate(data);  // cannot change status, can generate new unit clauses
  }
  
  // tobias
//   vec<Var> vars;
//   MarkArray array;
//     array.create( solver->nVars() );
//     array.nextStep();
//   for( Var v = 0 ; v < solver->nVars(); ++v ) 
//   {
//     if(!array.isCurrentStep(v) ) {
//       vars.push(v); 
//       data.mark1(v);
//     }
//   }
  // vars = cluster variablen
  
  
  // clear / update clauses and learnts vectores and statistical counters
  // attach all clauses to their watchers again, call the propagate method to get into a good state again
  if( opt_verbose > 2 )cerr << "c coprocessor re-setup solver" << endl;
  reSetupSolver();

  // destroy preprocessor data
  if( opt_verbose > 2 )cerr << "c coprocessor free data structures" << endl;
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

void Preprocessor::extendModel(vec< lbool >& model)
{
  data.extendModel(model);
}


void Preprocessor::initializePreprocessor()
{
  uint32_t clausesSize = (*solver).clauses.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    const CRef cr = solver->clauses[i];
    assert( ca[cr].mark() == 0 && "mark of a clause has to be 0 before being put into preprocessor" );
    // if( ca[cr].mark() != 0  ) continue; // do not use any specially marked clauses!
    data.addClause( cr );
    // TODO: decide for which techniques initClause in not necessary!
    subsumption.initClause( cr );
    propagation.initClause( cr );
    hte.initClause( cr );
    cce.initClause( cr );
  }

  clausesSize = solver->learnts.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    const CRef cr = solver->learnts[i];
    assert( ca[cr].mark() == 0 && "mark of a clause has to be 0 before being put into preprocessor" );
    // if( ca[cr].mark() != 0  ) continue; // do not use any specially marked clauses!
    data.addClause( cr );
    // TODO: decide for which techniques initClause in not necessary! (especially learnt clauses)
    subsumption.initClause( cr );
    propagation.initClause( cr );
    hte. initClause( cr );
    cce.initClause( cr );
  }
}

void Preprocessor::destroyPreprocessor()
{
  cce.destroy();
  hte.destroy();
  propagation.destroy();
  subsumption.destroy();
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

    // check whether reasons of top level literals are marked as deleted. in this case, set reason to CRef_Undef!
    if( solver->trail_lim.size() > 0 )
      for( int i = 0 ; i < solver->trail_lim[0]; ++ i )
        if( solver->reason( var(solver->trail[i]) ) != CRef_Undef )
          if( ca[ solver->reason( var(solver->trail[i]) ) ].can_be_deleted() )
            solver->vardata[ var(solver->trail[i]) ].reason = CRef_Undef;

    // give back into solver
    for (int i = 0; i < solver->clauses.size(); ++i)
    {
        const CRef cr = solver->clauses[i];
        Clause & c = ca[cr];
	assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
        if (c.can_be_deleted())
            delete_clause(cr);
        else
        {
            assert( c.mark() == 0 && "only clauses without a mark should be passed back to the solver!" );
            if (c.size() > 1)
            {
                solver->clauses[j++] = cr;
                solver->attachClause(cr);
            }
            else if (solver->value(c[0]) == l_Undef)
                solver->uncheckedEnqueue(c[0]);
	    else if (solver->value(c[0]) == l_False )
	    {
	      // assert( false && "This UNSAT case should be recognized before re-setup" );
	      solver->ok = false;
	    }
        }
    }
    int c_old = solver->clauses.size();
    solver->clauses.shrink(solver->clauses.size()-j);

    if( opt_verbose > 0 ) fprintf(stderr,"c Subs-STATs: removed clauses: %i of %i,%s" ,c_old - j,c_old, (opt_verbose == 1 ? "\n" : ""));

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
        } else {
	  assert( c.mark() == 0 && "only clauses without a mark should be passed back to the solver!" );
	  if (c.learnt()) {
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
	}
        if (c.size() > 1)
            solver->attachClause(cr);
        else if (solver->value(c[0]) == l_Undef)
          solver->uncheckedEnqueue(c[0]);
	else if (solver->value(c[0]) == l_False )
	{
	  // assert( false && "This UNSAT case should be recognized before re-setup" );
	  solver->ok = false;
	}
    }
    int l_old = solver->learnts.size();
    solver->learnts.shrink(solver->learnts.size()-j);
    if( opt_verbose > 1 ) fprintf(stderr, " moved %i and removed %i from %i learnts\n",learntToClause,(l_old - j) -learntToClause, l_old);

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
