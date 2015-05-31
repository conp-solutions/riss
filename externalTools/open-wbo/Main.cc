/*****************************************************************************************[Main.cc]
MiniSat  -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
            Copyright (c) 2007-2010, Niklas Sorensson
Open-WBO -- Copyright (c) 2013-2015, Ruben Martins, Vasco Manquinho, Ines Lynce

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
************************************************************************************************/

#include <errno.h>
#include <signal.h>
#include <zlib.h>
#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "Dimacs.h"
#include "MaxTypes.h"

// Algorithms
#include "algorithms/Alg_WBO.h"
#include "algorithms/Alg_incWBO.h"
#include "algorithms/Alg_LinearSU.h"
#include "algorithms/Alg_LinearUS.h"
#include "algorithms/Alg_MSU3.h"
#include "algorithms/Alg_WMSU3.h"

#define VER1_(x) #x
#define VER_(x) VER1_(x)
#define SATVER VER_(SOLVERNAME)
#define VER VER_(VERSION)

using namespace NSPACE;

//=================================================================================================

static MaxSAT *mxsolver;

static void SIGINT_exit(int signum)
{
  mxsolver->printAnswer(_UNKNOWN_);
  exit(_UNKNOWN_);
}

IntOption debug("Encodings", "dbg", "debug output verbosity.\n", 0, IntRange(0, 5));

#if NSPACE == Riss
void loadFromMprocessorToMaxsat( MaxSAT* S ) {
  
  Coprocessor::Mprocessor* mprocessor = S->getMprocessor();
  
  // calculate the header
  const int vars = mprocessor->S->nVars(); // might be smaller already due to dense
  int clss = mprocessor->S->trail.size() + mprocessor->S->clauses.size();
  int top = 0;
  for( Var v = 0 ; v < mprocessor->fullVariables; ++ v ) { // for each weighted literal, have an extra clause! use variables before preprocessing (densing)
    clss = (mprocessor->literalWeights[toInt(mkLit(v))] != 0 ? clss + 1 : clss);
    clss = (mprocessor->literalWeights[toInt(~mkLit(v))] != 0 ? clss + 1 : clss);
    top += mprocessor->literalWeights[toInt(mkLit(v))] + mprocessor->literalWeights[toInt(~mkLit(v))];
  }
  top ++;

  // if the option dense is used, these literals are already rewritten by "dense"
  // write header
  S->setProblemType( mprocessor->getProblemType() );
  while( S->nVars() < vars ) S->newVar();        // set variables in MS solver
  S->setHardWeight( top );                       // tell MS solver about top weight
  S->setPrintModelVars( mprocessor->specVars );  // tell MS solver about variables that originally occured in the formula only
  
  if( debug > 0 ) cerr << "c p wcnf " << S->nVars() << " " << clss << " " << top << endl;
  
  // write the hard clauses into the formula!
  // print assignments
  vec<Lit> cls;
  cls.push( lit_Undef );
  for (int i = 0; i < mprocessor->S->trail.size(); ++i)
  {
    cls[0] = mprocessor->S->trail[i];
    if( debug > 1 ) cerr << "c add hard: " << cls << " 0" << endl;
    S->addHardClause( cls );
  }
  // print clauses
  for (int i = 0; i < mprocessor->S->clauses.size(); ++i)
  {
    const Clause & c = mprocessor->S->ca[mprocessor->S->clauses[i]];
    if (c.mark()) continue;
    cls.growTo( c.size() );
    cls.clear();
    for (int i = 0; i < c.size(); ++i)
      cls.push_( c[i] );
    if( debug > 1 ) cerr << "c add hard: " << cls << " 0" << endl;
    S->addHardClause( cls );
  }  

  // if the option dense is used, these literals need to be adapted!
  // write all the soft clauses (write after hard clauses, because there might be units that can have positive effects on the next tool in the chain!)
  cls.clear();
  cls.push( lit_Undef );
  for( Var v = 0 ; v < mprocessor->fullVariables; ++ v ) { // for each weighted literal, have an extra clause!
    Lit l = mkLit(v);
    Lit nl = mprocessor->preprocessor->giveNewLit(l);
    // cerr << "c lit " << l << " is compress lit " << nl << endl;
    if( nl != lit_Undef && nl != lit_Error ) l = nl;
    else if (nl == lit_Undef && mprocessor->cpconfig.opt_dense ) {
      cerr << "c WARNING: soft literal " << l << " has been removed from the formula" << endl;
      continue;
    }
    if( mprocessor->literalWeights[toInt(mkLit(v))] != 0 ) { // setting literal l to true ha a cost, hence have unit!
      cls[0] = l;
      if( debug > 1 ) cerr << "c add soft: " << mprocessor->literalWeights[toInt(mkLit(v))] << " ; " << cls << " 0" << endl;
      S->setCurrentWeight( mprocessor->literalWeights[toInt(mkLit(v))] );
      S->updateSumWeights( mprocessor->literalWeights[toInt(mkLit(v))] );
      S->addSoftClause(mprocessor->literalWeights[toInt(mkLit(v))], cls);
    }
    if( mprocessor->literalWeights[toInt(~mkLit(v))] != 0 ) { // setting literal ~l to true ha a cost, hence have unit!
      cls[0] = ~l;
      if( debug > 1 ) cerr << "c add soft: " << mprocessor->literalWeights[toInt(~mkLit(v))] << " ; " << cls << " 0" << endl;
      S->setCurrentWeight( mprocessor->literalWeights[toInt(~mkLit(v))] );
      S->updateSumWeights( mprocessor->literalWeights[toInt(~mkLit(v))] );
      S->addSoftClause(mprocessor->literalWeights[toInt(~mkLit(v))], cls);
    }
  }
}
#endif


//=================================================================================================
// Main:

int main(int argc, char **argv)
{
  printf(
      "c\nc Open-WBO:\t a Modular MaxSAT Solver -- based on %s (%s version)\n",
      SATVER, VER);
  printf("c Version:\t 1.3.1 -- 18 February 2015\n");
  printf("c Authors:\t Ruben Martins, Vasco Manquinho, Ines Lynce\n");
  printf("c Contributors:\t Saurabh Joshi\n");
  printf("c Contact:\t open-wbo@sat.inesc-id.pt -- "
         "http://sat.inesc-id.pt/open-wbo/\nc\n");
  try
  {
    setUsageHelp("c USAGE: %s [options] <input-file>\n\n");

#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw);
    newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
    _FPU_SETCW(newcw);
    printf(
        "c WARNING: for repeatability, setting FPU to use double precision\n");
#endif

    IntOption cpu_lim("Open-WBO", "cpu-lim",
                      "Limit on CPU time allowed in seconds.\n", INT32_MAX,
                      IntRange(0, INT32_MAX));
    IntOption mem_lim("Open-WBO", "mem-lim",
                      "Limit on memory usage in megabytes.\n", INT32_MAX,
                      IntRange(0, INT32_MAX));
    IntOption verbosity("Open-WBO", "verbosity",
                        "Verbosity level (0=minimal, 1=more).\n", 1,
                        IntRange(0, 1));
    IntOption algorithm("Open-WBO", "algorithm",
                        "Search algorithm (0=wbo, 1=linear-su, 2=linear-us, "
                        "3=msu3, 4=wmsu3, 5=best).\n",
                        5, IntRange(0, 5));
    IntOption incremental("Open-WBO", "incremental",
                          "Incremental level (0=none, 1=blocking, 2=weakening, "
                          "3=iterative) (only for unsat-based algorithms).\n",
                          3, IntRange(0, 3));

    BoolOption bmo("Open-WBO", "bmo", "BMO search.\n", true);
    
    BoolOption incomplete("Open-WBO", "incomplete", "BMO search.\n", false);

    IntOption cardinality("Encodings", "cardinality",
                          "Cardinality encoding (0=cardinality networks, "
                          "1=totalizer, 2=modulo totalizer).\n",
                          1, IntRange(0, 2));

    IntOption amo("Encodings", "amo", "AMO encoding (0=Ladder).\n", 0,
                  IntRange(0, 0));

    IntOption pb("Encodings", "pb", "PB encoding (0=SWC).\n", 0,
                 IntRange(0, 0));

    IntOption weight(
        "WBO", "weight-strategy",
        "Weight strategy (0=none, 1=weight-based, 2=diversity-based).\n", 2,
        IntRange(0, 2));
    BoolOption symmetry("WBO", "symmetry", "Symmetry breaking.\n", true);
    IntOption symmetry_lim(
        "WBO", "symmetry-limit",
        "Limit on the number of symmetry breaking clauses.\n", 500000,
        IntRange(0, INT32_MAX));

 
#if NSPACE == Riss      
    StringOption    opt_pre_config  ("CONFIG", "preConfig",    "configuration for simplification",0);
    StringOption opt_solver_config  ("CONFIG", "solverConfig", "configuration for sat solver",    0);
    BoolOption   opt_simpOnlySimple ("SIMPLIFICATION", "sos",  "simplify only maxsat formulas with soft unit clauses", false);
#endif
    
    parseOptions(argc, argv, true);

    double initial_time = cpuTime();
    MaxSAT *S = NULL;

    switch ((int)algorithm)
    {
    case _ALGORITHM_WBO_:
      if (incremental == _INCREMENTAL_NONE_)
        S = new WBO(verbosity, weight, symmetry, symmetry_lim);
      else if (incremental == _INCREMENTAL_BLOCKING_)
        S = new incWBO(verbosity, weight, symmetry, symmetry_lim, amo);
      else
      {
        printf(
            "c Error: WBO algorithm only supports blocking incrementality.\n");
        printf("c UNKNOWN\n");
        exit(_ERROR_);
      }
      break;

    case _ALGORITHM_LINEAR_SU_:
      S = new LinearSU(verbosity, bmo, cardinality, pb);
      break;

    case _ALGORITHM_LINEAR_US_:
      S = new LinearUS(verbosity, incremental, cardinality);
      break;

    case _ALGORITHM_MSU3_:
      S = new MSU3(verbosity, incremental, cardinality);
      break;

    case _ALGORITHM_WMSU3_:
      S = new WMSU3(verbosity, incremental, cardinality, pb, bmo);
      break;

    case _ALGORITHM_BEST_:
      S = new MSU3(_VERBOSITY_SOME_, _INCREMENTAL_ITERATIVE_, _CARD_TOTALIZER_);
      break;

    default:
      printf("c Error: Invalid MaxSAT algorithm.\n");
      printf("c UNKNOWN\n");
      exit(_ERROR_);
    }

    mxsolver = S;
    mxsolver->setInitialTime(initial_time);

    printf("c set incomplete: %d\n", incomplete == true );
    mxsolver->setIncomplete( incomplete == true ); // tell solver to print each model
    
    signal(SIGXCPU, SIGINT_exit);
    signal(SIGTERM, SIGINT_exit);

    // Set limit on CPU-time:
    if (cpu_lim != INT32_MAX)
    {
      rlimit rl;
      getrlimit(RLIMIT_CPU, &rl);
      if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max)
      {
        rl.rlim_cur = cpu_lim;
        if (setrlimit(RLIMIT_CPU, &rl) == -1)
          printf("c WARNING! Could not set resource limit: CPU-time.\n");
      }
    }

    // Set limit on virtual memory:
    if (mem_lim != INT32_MAX)
    {
      rlim_t new_mem_lim = (rlim_t)mem_lim * 1024 * 1024;
      rlimit rl;
      getrlimit(RLIMIT_AS, &rl);
      if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max)
      {
        rl.rlim_cur = new_mem_lim;
        if (setrlimit(RLIMIT_AS, &rl) == -1)
          printf("c WARNING! Could not set resource limit: Virtual memory.\n");
      }
    }

    if (argc == 1)
      printf("c Reading from standard input... Use '--help' for help.\n");

    gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
    if (in == NULL)
      printf("c ERROR! Could not open file: %s\n",
             argc == 1 ? "<stdin>" : argv[1]),
          printf("c UNKNOWN\n"), exit(_ERROR_);

    // if we want to use a maxsat preprocessor, the preprocessor should be parsing
	  

#if NSPACE == Riss      
    if( (const char*)opt_solver_config != 0 ) {	  // set SAT solver configuration
      S->setSolverConfig( std::string(opt_solver_config) );
    }  
    
    if( (const char*)opt_pre_config == 0 ) {
      parse_DIMACS(in, S);
    } else {
      // setup maxsat preprocessor
      S->createMprocessor( (const char*)opt_pre_config  );
      S->getMprocessor()->setDebugLevel( debug ); // tell about level
      // parse with preprocessor      
      parse_DIMACS(in, S->getMprocessor() );
      
      if( opt_simpOnlySimple && S->getMprocessor()->hasNonUnitSoftClauses ) {
	printf("c Re-reading input due to non-unit soft clauses\n");
	if( argc == 1 ) {
	  printf("c Warning, cannot re-read from STDIN - abort\n");
	  exit(_ERROR_);
	}
	// destruct preprocessor
	S->deleteMprocessor();
	
	// re-open file and parse with usual solver again, as simplification will not be performed
	gzclose(in);
	in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
	if (in == NULL)
	  printf("c ERROR! Could not open file: %s\n",
		argc == 1 ? "<stdin>" : argv[1]),
	      printf("c UNKNOWN\n"), exit(_ERROR_);
	      
	// re-read formula into maxsat solver
	parse_DIMACS(in, S);
	
      } else {
	// simplify
	S->getMprocessor()->simplify();
	// load into actual solver
	loadFromMprocessorToMaxsat(S);
      }
    }
#else 
    parse_DIMACS(in, S);
#endif
      
    
//     opt_pre_config ("CONFIG", "preConfig",    "configuration for simplification",0);
//     opt_solver_config
    
    
    gzclose(in);

    printf("c |                                                                "
           "                                       |\n");
    printf("c ========================================[ Problem Statistics "
           "]===========================================\n");
    printf("c |                                                                "
           "                                       |\n");

    if (S->getProblemType() == _UNWEIGHTED_)
      printf("c |  Problem Type:  %19s                                         "
             "                          |\n",
             "Unweighted");
    else
      printf("c |  Problem Type:  %19s                                         "
             "                          |\n",
             "Weighted");

    printf("c |  Number of variables:  %12d                                    "
           "                               |\n",
           S->nVars());
    printf("c |  Number of hard clauses:    %7d                                "
           "                                   |\n",
           S->nHard());
    printf("c |  Number of soft clauses:    %7d                                "
           "                                   |\n",
           S->nSoft());

    double parsed_time = cpuTime();

    printf("c |  Parse time:           %12.2f s                                "
           "                                 |\n",
           parsed_time - initial_time);
    printf("c |                                                                "
           "                                       |\n");

    if (algorithm == _ALGORITHM_BEST_ && S->getProblemType() == _WEIGHTED_)
    {
      // Check if the formula is BMO without caching the bmo functions.
      bool bmo_strategy = S->isBMO(false);
      MaxSAT *solver;
      if (bmo_strategy)
        solver = new WMSU3(_VERBOSITY_SOME_, _INCREMENTAL_ITERATIVE_,
                           _CARD_TOTALIZER_, _PB_SWC_, true);
      else
        solver = new WBO(_VERBOSITY_SOME_, _WEIGHT_DIVERSIFY_, true, 500000);

      // HACK: Copy the contents of a solver to a fresh solver.
      // This could be avoided if the parsing was done to a DB that is
      // independent from the solver.
      S->copySolver(solver);
      delete S;
      mxsolver = solver;
      mxsolver->setInitialTime(initial_time);
      mxsolver->search();
    }
    else
      mxsolver->search();
  }
  catch (OutOfMemoryException &)
  {
    sleep(1);
    printf("c Error: Out of memory.\n");
    printf("s UNKNOWN\n");
    exit(_ERROR_);
  }
}
