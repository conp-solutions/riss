/**********************************************************************************[Coprocessor.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Coprocessor.h"
#include "../coprocessor-src/VarGraphUtils.h"

#include <iostream>

static const char* _cat = "COPROCESSOR 3";
static const char* _cat2 = "CP3 TECHNIQUES";

// options
static IntOption  opt_threads    (_cat, "cp3_threads",    "Number of extra threads that should be used for preprocessing", 0, IntRange(0, INT32_MAX));
static BoolOption opt_unlimited  (_cat, "cp3_unlimited",  "No limits for preprocessing techniques", true);
static BoolOption opt_randomized (_cat, "cp3_randomized", "Steps withing preprocessing techniques are executed in random order", false);
static IntOption  opt_verbose    (_cat, "cp3_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3));
static BoolOption opt_printStats (_cat, "cp3_stats",      "Print Technique Statistics", false);
// techniques
static BoolOption opt_up        (_cat2, "up",            "Use Unit Propagation during preprocessing", false);
static BoolOption opt_subsimp   (_cat2, "subsimp",       "Use Subsumption during preprocessing", false);
static BoolOption opt_hte       (_cat2, "hte",           "Use Hidden Tautology Elimination during preprocessing", false);
static BoolOption opt_cce       (_cat2, "cce",           "Use (covered) Clause Elimination during preprocessing", false);
static BoolOption opt_ee        (_cat2, "ee",            "Use Equivalence Elimination during preprocessing", false);
static BoolOption opt_enabled   (_cat2, "enabled_cp3",   "Use CP3", false);
static BoolOption opt_inprocess (_cat2, "inprocess",     "Use CP3 for inprocessing", false);
static BoolOption opt_bve       (_cat2, "bve",           "Use Bounded Variable Elimination during preprocessing", false);
static BoolOption opt_bva       (_cat2, "bva",           "Use Bounded Variable Addition during preprocessing", false);
static BoolOption opt_unhide    (_cat2, "unhide",        "Use Bounded Variable Addition during preprocessing", false);
static BoolOption opt_probe     (_cat2, "probe",         "Use Bounded Variable Addition during preprocessing", false);
static BoolOption opt_sls       (_cat2, "sls",           "Use Simple Walksat algorithm to test whether formula is satisfiable quickly", false);
static BoolOption opt_twosat    (_cat2, "2sat",          "2SAT algorithm to check satisfiability of binary clauses", false);
static BoolOption opt_ts_phase  (_cat2, "2sat-phase",    "use 2SAT model as initial phase for SAT solver", false);

static BoolOption opt_debug     (_cat2, "cp3-debug",     "do more debugging", false);
static IntOption  opt_log       (_cat, "log",            "Output log messages until given level", 0, IntRange(0, 3));

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
, isInprocessing( false )
, ppTime( 0 )
, ipTime( 0 )
// classes for preprocessing methods
, propagation( solver->ca, controller )
, subsumption( solver->ca, controller, data, propagation )
, hte( solver->ca, controller, propagation )
, bve( solver->ca, controller, propagation, subsumption )
, bva( solver->ca, controller, data )
, cce( solver->ca, controller )
, ee ( solver->ca, controller, propagation, subsumption )
, unhiding ( solver->ca, controller, data, propagation, subsumption, ee )
, probing  ( solver->ca, controller, data, propagation, *solver )
, sls ( data, solver->ca, controller )
, twoSAT( solver->ca, controller, data)
{
  controller.init();
}

Preprocessor::~Preprocessor()
{

}

lbool Preprocessor::performSimplification()
{
  if( ! opt_enabled ) return l_Undef;
  if( opt_verbose > 2 ) cerr << "c start preprocessing with coprocessor" << endl;

  if( isInprocessing ) ipTime = cpuTime() - ipTime;
  else ppTime = cpuTime() - ppTime;
  
  // first, remove all satisfied clauses
  if( !solver->simplify() ) return l_False;

  lbool status = l_Undef;
  // delete clauses from solver
  cleanSolver ();
  // initialize techniques
  data.init( solver->nVars() );
  initializePreprocessor ();
  if( opt_verbose > 2 )cerr << "c coprocessor finished initialization" << endl;
  
  const bool printBVE = false, printBVA = false, printCCE = false, printEE = false, printHTE = false, printSusi = false, printUP = false;  
  
  // do preprocessing
  if( opt_up ) {
    if( opt_verbose > 2 )cerr << "c coprocessor propagate" << endl;
    if( status == l_Undef ) status = propagation.propagate(data);
  }
  
  // begin clauses have to be sorted here!!
  sortClauses();
  
  if( false  || printUP  ) {
   printFormula("after Sorting");
  }
  
  // clear subsimp stats
  if ( true ) 
      subsumption.resetStatistics();

  if( opt_subsimp ) {
    if( opt_verbose > 2 )cerr << "c coprocessor subsume/strengthen" << endl;
    if( status == l_Undef ) subsumption.subsumeStrength();  // cannot change status, can generate new unit clauses
    if (! solver->okay())
        status = l_False;
  }

  if( false  || printSusi ) {
   printFormula("after Susi");
  }
  
  if( opt_debug ) checkLists("after SUSI");
  
  if( opt_ee ) { // before this technique nothing should be run that alters the structure of the formula (e.g. BVE;BVA)
    if( opt_verbose > 2 )cerr << "c coprocessor equivalence elimination" << endl;
    if( status == l_Undef ) ee.eliminate(data);  // cannot change status, can generate new unit clauses
    if (! data.ok() )
        status = l_False;
  }
  
  if( opt_debug ) checkLists("after EE");
  
  if( false ) {
   for( Var v = 0 ; v < data.nVars() ; ++v ) {
    for( int s = 0 ; s < 2; ++s ) {
      const Lit l = mkLit(v,s!=0);
      cerr << "c clauses with " << l << endl;
      for( int i = 0 ; i < data.list(l).size(); ++ i )
	if( !ca[data.list(l)[i]].can_be_deleted()  ) cerr << ca[data.list(l)[i]] << endl;
    }
   }
  }

  if( false  || printEE ) {
   printFormula("after EE");
  }
  
  if ( opt_unhide ) {
    if( opt_verbose > 2 )cerr << "c coprocessor unhiding" << endl;
    if( status == l_Undef ) unhiding.unhide(); 
    if( !data.ok() ) status = l_False;
  }
  
  if( opt_debug ) checkLists("after UNHIDING");
  
  if( opt_hte ) {
    if( opt_verbose > 2 )cerr << "c coprocessor hidden tautology elimination" << endl;
    if( status == l_Undef ) hte.eliminate(data);  // cannot change status, can generate new unit clauses
  }

  if( opt_debug ) checkLists("after HTE");
  if( false  || printHTE ) {
   printFormula("after HTE");
  }
  
  if ( opt_probe ) {
    if( opt_verbose > 2 )cerr << "c coprocessor probing" << endl;
    if( status == l_Undef ) probing.probe(); 
    if( !data.ok() ) status = l_False;
  }
  if( opt_debug ) checkLists("after PROBE");
  
  if ( opt_bve ) {
    if( opt_verbose > 2 )cerr << "c coprocessor blocked variable elimination" << endl;
    if( status == l_Undef ) status = bve.runBVE(data);  // can change status, can generate new unit clauses
  }
  
  if( opt_debug ) checkLists("after BVE");
  if( false || printBVE  ) {
   printFormula("after BVE");
  }
  
  if ( opt_bva ) {
    if( opt_verbose > 2 )cerr << "c coprocessor blocked variable addition" << endl;
    if( status == l_Undef ) bva.variableAddtion(true); 
    if( !data.ok() ) status = l_False;
  }
  
  if( opt_debug ) checkLists("after BVA");
  if( false || printBVA  ) {
   printFormula("after BVA");
  }
  
  if( opt_cce ) {
    if( opt_verbose > 2 )cerr << "c coprocessor (covered) clause elimination" << endl;
    if( status == l_Undef ) cce.eliminate(data);  // cannot change status, can generate new unit clauses
  }
  
  if( false || printCCE ) {
   printFormula("after CCE");
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
  VarGraphUtils utils;

  if( opt_sls ) {
    if( opt_verbose > 2 )cerr << "c coprocessor sls" << endl;
    if( status == l_Undef ) {
      bool solvedBySls = sls.solve( data.getClauses(), 200000 );  // cannot change status, can generate new unit clauses
      if( solvedBySls ) {
	cerr << "c formula was solved with SLS!" << endl;
	cerr // << endl 
	 << "c ================================" << endl 
	 << "c  use the result of SLS as model " << endl
	 << "c ================================" << endl;
      }
    }
    if (! solver->okay())
        status = l_False;
  }
  
  if( opt_twosat ) {
    if( opt_verbose > 2 )cerr << "c coprocessor 2SAT" << endl;
    if( status == l_Undef ) {
      bool solvedBy2SAT = twoSAT.solve();  // cannot change status, can generate new unit clauses
      if( solvedBy2SAT ) {
	// cerr << "binary clauses have been solved with 2SAT" << endl;
	// check satisfiability of whole formula!
	bool isNotSat = false;
	for( int i = 0 ; i < data.getClauses().size(); ++ i ) {
	  const Clause& cl = ca[ data.getClauses()[i] ];
	  int j = 0;
	  for(  ; j < cl.size(); ++ j ) {
	    if( twoSAT.isSat(cl[j]) ) break;
	  }
	  if( j == cl.size() ) { isNotSat = true; break; }
	}
	if( isNotSat ) {
	  // only set the phase before search!
	  if( opt_twosat && !isInprocessing) {
	    for( Var v = 0; v < data.nVars(); ++ v ) solver->polarity[v] = ( 1 == twoSAT.getPolarity(v) );
	  }
	  cerr // << endl 
	  << "c ================================" << endl 
	  << "c  2SAT model is not a real model " << endl
	  << "c ================================" << endl;
	} else {
	  cerr // << endl 
	  << "c =================================" << endl 
	  << "c  use the result of 2SAT as model " << endl 
	  << "c =================================" << endl;
	}
      } else {
	cerr // << endl 
	 << "================================" << endl 
	 << " unsatisfiability shown by 2SAT " << endl
	 << "================================" << endl;
	data.setFailed();
      }
    }
    if (! solver->okay())
        status = l_False;
  }  
  
  // clear / update clauses and learnts vectores and statistical counters
  // attach all clauses to their watchers again, call the propagate method to get into a good state again
  if( opt_verbose > 2 ) cerr << "c coprocessor re-setup solver" << endl;
  if ( data.ok() ) {
    propagation.propagate(data);
    if ( data.ok() ) reSetupSolver();
  }

  if( isInprocessing ) ipTime = cpuTime() - ipTime;
  else ppTime = cpuTime() - ppTime;
  
  if( opt_printStats ) {
    printStatistics(cerr);
    propagation.printStatistics(cerr);
    subsumption.printStatistics(cerr);
    ee.printStatistics(cerr);
    hte.printStatistics(cerr);
    bve.printStatistics(cerr);
    bva.printStatistics(cerr);
    probing.printStatistics(cerr);
    unhiding.printStatistics(cerr);
    cce.printStatistics(cerr);
    sls.printStatistics(cerr);
    twoSAT.printStatistics(cerr);
  }
  
  // destroy preprocessor data
  if( opt_verbose > 2 ) cerr << "c coprocessor free data structures" << endl;
  data.destroy();

  if( !data.ok() ) status = l_False; // to fall back, if a technique forgets to do this
  return status;
}


lbool Preprocessor::preprocess()
{
  isInprocessing = false;
  return performSimplification();
}

lbool Preprocessor::inprocess()
{
  // if no inprocesing enabled, do not do it!
  if( !opt_inprocess ) return l_Undef;
  // TODO: do something before preprocessing? e.g. some extra things with learned / original clauses
  if (opt_inprocess) {
    isInprocessing = true;
    return performSimplification();
  }
  else 
    return l_Undef; 
}

lbool Preprocessor::preprocessScheduled()
{
  // TODO execute preprocessing techniques in specified order
  return l_Undef;
}

void Preprocessor::printStatistics(ostream& stream)
{
stream << "c [STAT] CP3 "
<< ppTime << " s-ppTime, " 
<< ipTime << " s-ipTime, "
<< data.getClauses().size() << " cls, " 
<< data.getLEarnts().size() << " learnts "
<< endl;
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
  bve.destroy();
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
		// do not watch literals that are false!
		int j = 2;
		for ( int k = 0 ; k < 2; ++ k ) { // ensure that the first two literals are undefined!
		  if( solver->value( c[k] ) == l_False ) {
		    for( ; j < c.size() ; ++j )
		      if( solver->value( c[j] ) != l_False ) 
		        { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
		  }
		}
		if( (solver->value( c[0] ) == l_False && solver->value( c[1] ) == l_False) ) {
		  cerr << "c found unsatisfiable clause " << c << endl;
		  data.setFailed();
		  break;
		} else if( solver->value( c[0] ) == l_Undef && solver->value( c[1] ) == l_False) {
		  cerr << "c found unit clause " << c << endl;
		  solver->uncheckedEnqueue( c[0] );
		}
		
		assert( (solver->value( c[0] ) != l_False || solver->value( c[1] ) != l_False) && "Cannot watch falsified literals" );
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
        if (c.size() > 1) {
	  // do not watch literals that are false!
	  int j = 2;
	  for ( int k = 0 ; k < 2; ++ k ) { // ensure that the first two literals are undefined!
	    if( solver->value( c[k] ) == l_False ) {
	      for( ; j < c.size() ; ++j )
		if( solver->value( c[j] ) != l_False ) 
		  { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
	    }
	  }
	  assert( (solver->value( c[0] ) != l_False || solver->value( c[1] ) != l_False) && "Cannot watch falsified literals" );
	  solver->attachClause(cr);
	} else if (solver->value(c[0]) == l_Undef)
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

    if( false ) {
      cerr << "c trail after cp3: ";
      for( int i = 0 ; i< solver->trail.size(); ++i ) 
      {
	cerr << solver->trail[i] << " " ;
      }
      cerr << endl;
      
      if( false ) {
      cerr << "formula: " << endl;
      for( int i = 0 ; i < data.getClauses().size(); ++ i )
	if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
      for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
      }
      
      cerr << "c watch lists: " << endl;
      for (int v = 0; v < solver->nVars(); v++)
	  for (int s = 0; s < 2; s++) {
	    const Lit l = mkLit(v, s == 0 ? false : true );
	    cerr << "c watch for " << l << endl;
	    for( int i = 0; i < solver->watches[ l ].size(); ++ i ) {
	      cerr << ca[solver->watches[l][i].cref] << endl;
	    }
	  }
    }

}

void Preprocessor::sortClauses()
{
  uint32_t clausesSize = (*solver).clauses.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    Clause& c = ca[solver->clauses[i]];
    if( c.can_be_deleted() ) continue;
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
    if( c.can_be_deleted() ) continue;
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

void Preprocessor::printFormula(const string& headline)
{
   cerr << "=== Formula " << headline << ": " << endl;
   for( int i = 0 ; i < data.getSolver()->trail.size(); ++i )
       cerr << "[" << data.getSolver()->trail[i] << "]" << endl;   
   for( int i = 0 ; i < data.getClauses().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
   for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
   cerr << "==================== " << endl;
}

bool Preprocessor::checkLists(const string& headline)
{
  bool ret = false;
  cerr << "c check data structures: " << headline << " ... " << endl;
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    for( int p = 0 ; p < 2; ++ p ) {
      const Lit l = p == 0 ? mkLit(v,false) : mkLit(v,true);
      for( int i = 0 ; i < data.list(l).size(); ++ i ) {
	for( int j = i+1 ; j < data.list(l).size(); ++ j ) {
	  if( data.list(l)[i] == data.list(l)[j] ) {
	    ret = true;
	    cerr << "c duplicate " << data.list(l)[j] << " for lit " << l << " at " << i << " and " << j << " out of " << data.list(l).size() << endl;
	  }
	}
      }
    }
  }
  return ret;
}
