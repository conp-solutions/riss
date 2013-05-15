/*************************************************************************************[Symmetry.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Symmetry.h"

#include "mtl/Sort.h"

using namespace Coprocessor;
using namespace Minisat;

static const char* _cat = "COPROCESSOR 3 - SYMMETRY";

//static BoolOption    opt_hhack             (_cat, "sym-hack",    "sym-ack", false);
static BoolOption    opt_hsize             (_cat, "sym-size",    "scale with the size of the clause", false);
static BoolOption    opt_hpol              (_cat, "sym-pol",     "consider the polarity of the occurrences", false);
static BoolOption    opt_hpushUnit         (_cat, "sym-unit",    "ignore unit clauses", false); // there should be a parameter delay-units already!
static IntOption     opt_hmin              (_cat, "sym-min",     "minimum symmtry to be exploited", 2, IntRange(1, INT32_MAX) );
static DoubleOption  opt_hratio            (_cat, "sym-ratio",   "only consider a variable if it appears close to the average of variable occurrences", 0.4, DoubleRange(0, true, HUGE_VAL, true));
static IntOption     opt_iter              (_cat, "sym-iter",    "number of symmetry approximation iterations", 3, IntRange(0, INT32_MAX) );
static BoolOption    opt_pairs             (_cat, "sym-show",    "show symmetry pairs", false);
static BoolOption    opt_print             (_cat, "sym-print",   "show the data for each variable", false);
static BoolOption    opt_exit              (_cat, "sym-exit",    "exit after analysis", false);
static BoolOption    opt_hprop             (_cat, "sym-prop",    "try to generate symmetry breaking clauses with propagation", false);
static BoolOption    opt_hpropF            (_cat, "sym-propF",    "generate full clauses", false);
static BoolOption    opt_hpropA            (_cat, "sym-propA",    "test all four casese instead of two", false);
static BoolOption    opt_cleanLearn        (_cat, "sym-clLearn",  "clean the learned clauses that have been created during symmetry search", false);
static IntOption     opt_conflicts         (_cat, "sym-cons",     "number of conflicts for looking for being implied", 0, IntRange(0, INT32_MAX) );
static IntOption     opt_total_conflicts   (_cat, "sym-consT",    "number of total conflicts for looking for being implied", 10000, IntRange(0, INT32_MAX) );
#if defined CP3VERSION  
static const int debug_out = 0;
#else
static IntOption debug_out        (_cat, "sym-debug", "debug output for probing",0, IntRange(0,4) );
#endif

Symmetry::Symmetry( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver)
: Technique(_ca, _controller)
, data(_data)
, solver(_solver)
, processTime(0)
, addPairs(0)
, maxEq(0)
, maxOcc(0)
, eqs(0)
, replaces(0)
, totalConflicts(0)
, notYetAnalyzed(true)
, symmAddClause(0)
{  
}

bool Symmetry::process() {
  MethodTimer mt( &processTime );
  if( false ) {
  printf("formula:\n");
  for( int i = 0 ; i < solver.clauses.size(); ++i ) {
      printf("c clause %3d / %3d  :",i,solver.clauses.size() );
      const Clause& c = ca[solver.clauses[i]];
      for( int j = 0 ; j < c.size(); ++ j ) {
	printf("%s%d ", sign(c[i]) ? "-" : "", var(c[i])+1);
      }
      printf("\n");	
  }
  }
  
  printf("c found units: %d, propagated: %d\n", solver.trail.size(), solver.qhead );
  
  vec<Lit> cls; cls.growTo(3,lit_Undef);
  Symm* varSymm = new Symm [ solver.nVars() ]; // ( solver.nVars() );
  Symm* oldSymm = new Symm [ solver.nVars() ]; // ( solver.nVars() );
  for( Var v = 0 ; v < solver.nVars(); ++ v ) {
    varSymm[v].v = v; oldSymm[v].v = v; 
  }
  int* freq = new int[ solver.nVars() ];
  memset( freq,0,sizeof(int) * solver.nVars());
  
  Symm* thisIter = varSymm;
  Symm* lastIter = oldSymm;
  
  const int iteration = opt_iter;
  
  for( int i = 0 ; i < solver.clauses.size(); ++i ) {
    //printf("c clause %d / %d\n",i,solver.clauses.size() );
      const Clause& c = ca[solver.clauses[i]];
      //printf("c clause size: %d\n",c.size() );
      for( int j = 0 ; j < c.size(); ++ j ) {
	Var v = var(c[j]);
	if( false &&  sign( c[j] ) && opt_hpol ) varSymm[v].sub( c.size() ); // take care of signs, is enabled
	else varSymm[v].add( c.size() );
	freq[v] ++;
      }
  }
  
  if( false ) {
    for( Var v = 0 ; v < solver.nVars(); ++ v ) {
      printf("c %4d[%4d] : %3d -- %3d %3lld %3lld %3lld %3lld %3lld %3lld \n", v+1,freq[v],varSymm[v].v+1,varSymm[v].c2,varSymm[v].c3,varSymm[v].c4,varSymm[v].c5,varSymm[v].c6,varSymm[v].c7,varSymm[v].cl);
    }
  }
  
  for( int iter = 0 ; iter < iteration; ++ iter ) {
    Symm* tmp = thisIter; thisIter = lastIter; lastIter = tmp; // swap pointers
    
    // cumulate neighbors to variables
    for( int i = 0 ; i < solver.clauses.size(); ++i ) {
      const Clause& c = ca[solver.clauses[i]];
      int factor = opt_hsize ? c.size() : 1;
      Symm thisClause;
      for( int j = 0 ; j < c.size(); ++ j ) {
	Var v = var(c[j]);
	// printf("c process variable %d\n", v + 1 );
	thisClause = ( sign(c[j]) && opt_hpol ? lastIter[v].inv():  lastIter[v] ) * factor + thisClause; // substract, if necessary! // be careful with order here, otherwise, variable will be overwritten!
      }
      for( int j = 0 ; j < c.size(); ++ j ) {
	Var v = var(c[j]);
	thisIter[v] = lastIter[v] + thisClause;
      }
    }
    
    if( false ) {
    printf("c iteration %d\n",iter);
    for( Var v = 0 ; v < solver.nVars(); ++ v ) {
      printf("c %4d[%4d] : %3d -- %3d %3d %3d %3d %3d %3d %3d \n", v+1,freq[v],varSymm[v].v+1,varSymm[v].c2,varSymm[v].c3,varSymm[v].c4,varSymm[v].c5,varSymm[v].c6,varSymm[v].c7,varSymm[v].cl);
    }
    }
    
  }
  
  // sort var structrure, to detect symmetry candidates
  sort<Symm>( thisIter, solver.nVars() );
  
  int avgFreq = (solver.clauses_literals + solver.nVars() - 1) / solver.nVars();
  printf("c avg freq: %d\n", avgFreq );
  // have new variable, if dependend variables are sufficiently frequent!
  // have at most the srqt of the size of the class pairs (first-last, seconds-second last, ...)
  
  if( false ) {
    for( Var v = 0 ; v < solver.nVars(); ++ v ) {
      printf("c %d : %3d -- %3d %3d %3d %3d %3d %3d %3d \n", v+1,thisIter[v].v+1,thisIter[v].c2,thisIter[v].c3,thisIter[v].c4,thisIter[v].c5,thisIter[v].c6,thisIter[v].c7,thisIter[v].cl);
    }
  }
  
  // TODO continue her!
  
  vec<ScorePair> scorePairs;
  vec<Lit> assumptions;
  
  for( int i = 0 ; i < solver.nVars(); ++ i ) {
    int j = i+1;
    if( j >= solver.nVars() ) break;
    while( j < solver.nVars() && thisIter[i] == thisIter[j]) ++j ;
    if( j > i + 1 ) {
       int thisR = j - i;
       eqs ++;
       maxEq = thisR > maxEq ? thisR : maxEq;
       
       if( opt_print ) {
       for( int k = 0; k < thisR; ++k ) {
	 if( freq[ thisIter[i+k].v ] > 0) printf("c %4d[%4d] : %3d -- %3ulld %3ulld %3ulld %3ulld %3ulld %3ulld %3ulld \n", thisIter[i+k].v+1,freq[thisIter[i+k].v],thisIter[i+k].v+1,thisIter[i+k].c2,thisIter[i+k].c3,thisIter[i+k].c4,thisIter[i+k].c5,thisIter[i+k].c6,thisIter[i+k].c7,thisIter[i+k].cl);
       } }
       
       
       int b = j - 1; // pointer to last element
       for( int k = 0; k < thisR; ++k ) {
	 if( freq[ thisIter[i+k].v ] <= opt_hmin || freq[ thisIter[i+k].v ] < ( avgFreq * opt_hratio) ) continue;
	 
	 if( opt_hprop ) { // try to add clauses by checking propagation
	    assert( solver.decisionLevel() == 0 && "can add those clauses only at level 0" );
	    
	    solver.propagate(); // in case of delayed units, get rid of them now!
	    
	    for( int m = thisR - 1 ; m > k ; -- m ) {
	      if ( freq[ thisIter[i+m].v ] <= opt_hmin || freq[ thisIter[i+m].v ] < ( avgFreq * opt_hratio) ) continue; // do not consider too small literals!
	      for( int o = 0 ; o < 4; ++ o ) { // check all 4 polarity combinations?
		if( !opt_hpropA  && ( o == 1 || o == 2 ) ) continue; // just check equivalence
		const Lit l1 = mkLit(thisIter[i+k].v, o == 0 || o == 1);
		const Lit l2 = mkLit(thisIter[i+m].v, o == 0 || o == 2);
	      
		int oldHead = solver.realHead;
		if( solver.value(l1) != l_Undef ) continue; // already top level unit?
		if( solver.value(l2) != l_Undef ) continue; // already top level unit?

		solver.newDecisionLevel();
		assert( solver.decisionLevel() == 1 && "can do this only on level 1" );
		solver.uncheckedEnqueue(l1);solver.uncheckedEnqueue(l2); // double lookahead
		bool entailed = false;
		if( CRef_Undef != solver.propagate() ) { // check if conflict, if yes, add new clause!
		  if( solver.realHead > oldHead + 1) entailed = true; // only add clause, if ont already present!
		}
		solver.cancelUntil(0);
		if( entailed ) {
		  data.lits.clear();
		  data.lits.push_back( ~l1 );
		  data.lits.push_back( ~l2 );
		  CRef cr = ca.alloc(data.lits, false); // no learnt clause!!
		  solver.clauses.push(cr);
		  solver.attachClause(cr); // no data initialization necessary!!
		  if( debug_out > 1 ) cerr << "c add clause [" << ~l1 << ", " << ~l2 << "]" << endl;
		  symmAddClause ++;
		} else if( opt_conflicts > 0 && totalConflicts < opt_total_conflicts ) {
		  assert( solver.assumptions.size() == 0 && "apply symmetry breaking only if no assumptions are used" );
		  bool oldUsePP = solver.useCoprocessor;
		  int oldVerb = solver.verbosity;
		  totalConflicts = solver.conflicts - totalConflicts;
		  solver.verbosity = 0;
		  solver.useCoprocessor = false;
		  solver.setConfBudget( opt_conflicts );
		  assumptions.clear();
		  assumptions.push( l1 );assumptions.push( l2 );
		  lbool ret = solver.solveLimited(assumptions) ;
		  solver.assumptions.clear();
		  solver.verbosity = oldVerb;
		  solver.useCoprocessor = oldUsePP;
		  totalConflicts = solver.conflicts - totalConflicts;
		  if( ret == l_False ) {
		    // entailed! 
		  } else if ( ret == l_True ) {
		    // found a model for the formula - handle it! 
		  }
		  // do nothing if no result has been found!
		}
	      }
	      if( !opt_hpropF ) break; // stop after first iteration!
	    }
	  }
	 
	 
	 while( b > i+k && ( freq[ thisIter[b].v ] <= opt_hmin || freq[ thisIter[b].v ] < ( avgFreq * opt_hratio) ) ) --b;
	 
	 if( b <= k + i ) break; // stop here!

	  // printf("c add %d -- %d\n",i+k,b);
	  // printf("c add pair for variables %3d and %3d\n", thisIter[i+k].v+1, thisIter[b].v+1 );	 
	 addPairs ++;
	 // score is occurrences multiplied by size of symmetry candidate
	 scorePairs.push( ScorePair(thisIter[i+k].v,thisIter[b].v, (freq[ thisIter[i+k].v ] + freq[ thisIter[b].v ]) * thisR  )  );

	 b--; // do not consider the same variable again!
       }
       
    }
    i = j-1; // move pointer forward
  }
  
  // clear all the learned clauses inside the solver!
  if( opt_cleanLearn ) {
    for( int i = 0 ; i < solver.learnts.size(); ++ i ) {
      solver.removeClause( solver.learnts[i] ); 
    }
  }
  
  if( addPairs > 0 ) solver.rebuildOrderHeap();
  
  sort(scorePairs);
  
  if( opt_pairs ) {
    for( int i = 0 ; i < scorePairs.size(); ++ i ) {
      printf( "c %d : %d == %d , sc= %f\n", i, scorePairs[i].v1 + 1 , scorePairs[i].v2, + 1, scorePairs[i].score ); 
    }
  }
 
  notYetAnalyzed = false;
  
  delete [] varSymm;
  delete [] oldSymm;
  delete [] freq;
  if( opt_exit ) {
    printStatistics(cerr);
    exit(0);
  }
}

void Coprocessor::Symmetry::printStatistics(ostream& stream)
{
  stream << "c [STAT] SYMM " << processTime << " s, " << eqs << " symm-cands, " << maxEq << " maxSize, " << addPairs  << " addedPairs " << symmAddClause << " entailedClauses, " << totalConflicts << " totCons, " << endl;
}
