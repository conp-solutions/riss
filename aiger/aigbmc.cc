/***************************************************************************
Copyright (c) 2013, Norbert Manthey, All Rights reserved


Copyright (c) 2011, Armin Biere, Johannes Kepler University, Austria.


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software _and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, _and/or
sell copies of the Software, _and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice _and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
***************************************************************************/

#include "aiger/aiger.h"

// enable this to disable all the output that might tell about the content of the implementation
// #define TEST_BINARY

extern "C" { // we are compiling with G++, however, picosat is C code
  #include "picosat.h"
}

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 *  For g++ extension
 */
using namespace std;

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <iostream>

/**
 *  For using riss, CP3, ...
 */
#include "riss/core/BMCwrapper.h"
#include "coprocessor/Coprocessor.h"

/**
 *  For using abc ...
 */
#ifdef USE_ABC
extern "C" { // abc has been compiled with gcc
  // procedures to start and stop the ABC framework
  // (should be called before and after the ABC procedures are called)
  extern void   Abc_Start();
  extern void   Abc_Stop();
  // procedures to get the ABC framework and execute commands in it
  extern void * Abc_FrameGetGlobalFrame();
  extern int    Cmd_CommandExecute( void * pAbc, char * sCommand );
}
#endif 

/**
 *  For timing
 */
#include <sys/time.h>
#include <sys/resource.h>
// also for getpid:
#include <unistd.h> 
#include <time.h>

/**
 * for parallel solving
 */
#include <pthread.h>

struct SharedData {
  Riss::IncSolver* riss;     // handle to solver, to interrupt it if necessary
  Riss::IncSolver* guesser;  // handle to solver, to interrupt it if necessary
  int upperBound; // here we had the lowest known SAT result
  int lowerBound; // here we had the highest known UNSAT result
  int rissFrame, guesserFrame; // frames where the two solvers work on at the moment
  int rissSolved, guesserSolved; // indicate who has the solution ready
  pthread_t guesserThread; // thread handle to guesser thread
  
  Riss::vec< Riss::lbool >* upperBoundModel; // if an upper bound has been found by the guesser, store the model here, to be able to recover
  SharedData() : riss(0), guesser(0),upperBound(-1),lowerBound(-1),rissFrame(0),guesserFrame(0),rissSolved(0),guesserSolved(0),upperBoundModel(0) {}
};

static aiger * model = 0;
static const char * name = 0;
static PicoSAT * picosat = 0;
static Riss::IncSolver* riss = 0;

// structures that are necessary for preprocessing, and if x or y parameter are set also for postprocessing
static Riss::Solver* ppSolver;
static Riss::IncSolver* incsolver;
static Coprocessor::Preprocessor* outerPreprocessor;
static Coprocessor::CP3Config* cp3config;

static unsigned firstlatchidx = 0, first_andidx = 0;

typedef struct Latch { int lit, next; } Latch;
typedef struct Fairness { int lit, sat; } Fairness;
typedef struct Justice { int nlits, sat; Fairness * lits; } Justice;

typedef struct State {
  int time;
  int * inputs;
  Latch * latches;
  int * _ands;
  int * bad, onebad;
  int * constraints, sane;
  Justice * justice; int onejustified;
  Fairness * fairness; int allfair;
  int join, loop, assume;
} State;

static int falseLit = -1; /// literal that will be always false, so that it can be used in the initialization of literals

static double totalTime = 0;

static State * states = 0;
static int nstates = 0, szstates = 0, * join = 0;
static char * bad = 0, * justice = 0;

static int maxk=0; // upper bound for search
static int verbose = 0, move = 0, quiet = 0, nowitness = 0, nowarnings = 0, lazyEncode = 0,useShift=0, // options
  useRiss = 0, useCP3 = 0, denseVariables = 0, dontFreezeInput = 0, dontFreezeBads = 0, printTime=0, checkInitialBad = 0, abcDir = 0;
static int wildGuesser = 0, wildGuessStart = 10, wildGuessInc = 2, wildGuessIncInc = 1; // guess sequence: 10, 30, 60, ...
static int tuning = 0, tuneSeed = 0;
static std::string useABC = "";
static std::string abcCommand = "";
static char* dumpPPformula = 0;
static int nvars = 0;

/**
 *  this will be used to encode the formula with shifting!
 */
class ShiftFormula {
public:
  vector<int> initUnits;		// units to initialize the initial circuit - will not be used for simplification, but freezed (to generate the same clauses for bad states,fairness,justice,...)
  vector<int> formula;			// formula that contains all the clauses to be shifted per iteration
  vector<int> inputs;			// the input literals
  vector<int> latch;			// current latch literals
  vector<int> latchNext;		// next latch literals
  vector<int> currentBads;		// bad state literals of current formula
  vector<int> todoEqualities;		// equalities that have to be added to the formula before solving
//   int prevSane, sane;			// sane literals
//   int prevLoop,loop;			// loop literals
//   vector< vector< int > > justice;	// justice literals per justice constraint
//   vector< int > fairness;		// fairness literals
  int currentAssume;			// literal that is used as assuption for this round
  
  int initialMaxVar;	// use to reconstruct inputs and all that
  int afterPPmaxVar;   // use to shift the formula
  
  ShiftFormula () {}
  
  /// clear current state
  void clear() { formula.clear(); inputs.clear(); latch.clear(); currentBads.clear(); todoEqualities.clear(); }
  
  void produceFormula( int k ) {
    
  }
};

/// base structure for performing the shift operation with the (simplified) formula
static ShiftFormula shiftFormula;

static void die (const char *fmt, ...) {
  va_list ap;
  fputs ("*** [aigbmc] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);

  if( tuning ) {
    std::cout << "Result for ParamILS: " 
    << "CRASHED" << ", "
    << cpuTime() - totalTime << ", " // time running until printing this info
    << 0 << ", "
    << 10000.0 << ", "
    << tuneSeed  // seed
    << std::endl;
    std::cout.flush();
    std::cerr << "Result for ParamILS: " 
    << "CRASHED" << ", "
    << cpuTime() - totalTime << ", " // time running until printing this info
    << 0 << ", "
    << 10000.0 << ", "
    << tuneSeed  // seed
    << std::endl;
    std::cerr.flush();
  }
  exit (1);
}

static void msg (int level, const char *fmt, ...) {
  va_list ap;
  if (quiet || verbose < level) return;
  fputs ("[aigbmc] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

static void wrn (const char *fmt, ...) {
  if( nowarnings ) return; // print only if enabled
  va_list ap;
  fputs ("[aigbmc] WARNING ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

// static inline double cpuTime(void) {
//     struct rusage ru;
//     getrusage(RUSAGE_SELF, &ru);
//     return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000; }
// 
// 
// static inline double wallClockTime(void)
// {
//     struct timespec timestamp;
//     clock_gettime(CLOCK_MONOTONIC, &timestamp);
//     return ((double) timestamp.tv_sec) + ((double) timestamp.tv_nsec / 1000000000);
// }

/** class that measures the time between creation and destruction of the object, and adds it*/
class MethodTimer {
  double* pointer;
public:
  MethodTimer( double* timer ) : pointer( timer ) { *pointer = cpuTime() - *pointer;}
  ~MethodTimer() { *pointer = cpuTime() - *pointer; }
};

static void add (int lit, bool addToShift = true) { 
  if( addToShift ) shiftFormula.formula.push_back( lit ); // write into formula
  if( useRiss ) riss->add( lit );
  else picosat_add (picosat, lit);
}

static void assume (int lit) { 
  if( useRiss ) riss->assume(lit);
  else picosat_assume (picosat, lit);
}

/** solve with resource limit */
static int sat ( int limit = -1 ) { 
  // msg (2, "solve time with SAT solver: %d", nstates);
  int ret;
  if( useRiss ) ret = riss->sat(limit);
  else ret = picosat_sat (picosat, -1);
  msg (2, "return code of SAT solver: %d", ret);
  return ret;
}

static int deref (int lit) { 
  int ret;
  if( useRiss ) ret = riss->deref(lit);
  else ret = picosat_deref (picosat, lit);
  msg(3,"deref lit %d to value %d", lit, ret);
  return ret;
}

/** initialize everything, give solvers the possibility to read the parameters! */
static void init ( int& argc, char **& argv ) {
  msg (2, "init SAT solver");
  if( useRiss ) riss = new Riss::IncSolver(argc,argv);
  else picosat = picosat_init ();
  model = aiger_init ();
}

static void reset () {
  State * s;
  int i, j;
  for (i = 0; i < nstates; i++) {
    s = states + i;
    free (s->inputs);
    free (s->latches);
    free (s->_ands);
    free (s->bad);
    free (s->constraints);
    for (j = 0; j < model->num_justice; j++) free (s->justice[j].lits);
    free (s->justice);
    free (s->fairness);
  }
  free (states);
  free (bad);
  free (justice);
  free (join);
  if( useRiss ) { delete riss; riss = 0; }
  else picosat_reset (picosat);
  aiger_reset (model);
}

static int newvar () { return ++nvars; }

static int import (State * s, unsigned ulit) {
  unsigned uidx = ulit/2;
  int res, idx;
  assert (ulit <= 2*model->maxvar + 1);
  if (!uidx) idx = -1;
  else if (uidx < firstlatchidx) idx = s->inputs[uidx - 1];
  else if (uidx < first_andidx) idx = s->latches[uidx - firstlatchidx].lit;
  else idx = s->_ands[uidx - first_andidx];
  assert (idx);
  res = (ulit & 1) ? -idx : idx;
  return res;
}

static void unit (int lit, bool addToShift = true) { add (lit,addToShift); add (0,addToShift); }

static void binary (int a, int b, bool addToShift = true) { add (a,addToShift); add (b,addToShift); add (0,addToShift); }

static void ternary (int a, int b, int c, bool addToShift = true) {
  add (a,addToShift); add (b,addToShift); add (c,addToShift); add (0,addToShift);
}

static void quaterny (int a, int b, int c, int d, bool addToShift = true) {
  add (a,addToShift); add (b,addToShift); add (c,addToShift); add (d,addToShift); add (0,addToShift);
}

static void _and (int lhs, int rhs0, int rhs1, bool addToShift = true) {
  binary (-lhs, rhs0, addToShift);
  binary (-lhs, rhs1, addToShift);
  ternary (lhs, -rhs0, -rhs1, addToShift);
}

/** tell the sat solver about the equivalence of the two literals!
 */
static void _eq (int lhs, int rhs, bool addToShift = true) {
  binary (lhs, -rhs, addToShift);
  binary (-lhs, rhs, addToShift);
}

/** shift the number of the variable that belongs to the literal by the given number */
static int varadd( int l1, int distance ) {
  return l1 < 0 ? l1 - distance : l1 + distance;  
}

/** encode the graph into CNF (add the next frame, based on the nstates variables)
 *  @param actuallyEnode can disable handing the formula over to the sat solver
 */
static int encode ( bool actuallyEnode = true ) {
  int time = nstates, /// time is set to the current state
      lit;
  aiger_symbol * symbol;
  State * res, * prev;
  aiger_and * u_and;
  unsigned reset;
  int i, j;
  if (nstates == szstates)
    states = realloc (states, ++szstates * sizeof *states);
  nstates++; /// work on next state
  res = states + time;
  memset (res, 0, sizeof *res);
  res->time = time;

  res->latches = malloc (model->num_latches * sizeof *res->latches);
  shiftFormula.latch.resize(model->num_latches,0); /// create vector of latch literals
  shiftFormula.latchNext.resize(model->num_latches,0); /// create vector of latch literals
  shiftFormula.currentBads.resize( model->num_bad,0); /// create vector of bad literals
  
/// setup this rounds latches
  if (time) {
    prev = res - 1;
    for (i = 0; i < model->num_latches; i++) {
      if( lazyEncode ) {
	res->latches[i].lit = newvar();
	// reserve fresh variable, set this variable equivalent to previous rounds variable!
	shiftFormula.todoEqualities.push_back(res->latches[i].lit);
	shiftFormula.todoEqualities.push_back(prev->latches[i].next);
	if( actuallyEnode ) shiftFormula.latch[i] = res->latches[i].lit;
      } else {
	res->latches[i].lit = prev->latches[i].next; /// combine the literals of this iteration with the latch literals of last iteration
	if( actuallyEnode ) shiftFormula.latch[i] = shiftFormula.latchNext[i]; /// shift latches!
      }
      if( actuallyEnode ) msg(2,"set latch %d to lit %d",i,shiftFormula.latch[i]);
    }
  } else {
    prev = 0;
    for (i = 0; i < model->num_latches; i++) {
      symbol = model->latches + i;
      reset = symbol->reset;
      if (!reset) lit = -1; // should be false lit -> use equivalence to false lit !
      else if (reset == 1) lit = 1; // should be true-lit -> use equivalence to !falseLit !
      else {
	if (reset != symbol->lit)
	  die ("can only handle constant or uninitialized reset logic");
	lit = newvar ();
      }
      
      
      if( lazyEncode && ( lit == 1 || lit == -1 ) ) {
	int realLit = newvar();
	// add Equivalence to initialization of very first formula!!
	if( actuallyEnode ) {
	  if( lit == 1 ) {
	    shiftFormula.todoEqualities.push_back(-falseLit);
	    shiftFormula.todoEqualities.push_back(realLit);
	  } else {
	    shiftFormula.todoEqualities.push_back(falseLit);
	    shiftFormula.todoEqualities.push_back(realLit);
	  }
	}
	msg(3,"connected latch %d with the literal %d and with the value %d",i,realLit,lit);
	lit = realLit;
      }
      
      
      res->latches[i].lit = lit;
      msg(2,"set latch %d to lit %d",i,lit);
      if( actuallyEnode ) shiftFormula.latch[i] = lit;
    }
  }

  /// get a fresh set of input variables
  res->inputs = malloc (model->num_inputs * sizeof *res->inputs);
  for (i = 0; i < model->num_inputs; i++) {
    res->inputs[i] = newvar ();
    if( actuallyEnode ) shiftFormula.inputs.push_back( res->inputs[i] );
  }
  
  /// encode the whole formula one more time!
  res->_ands = malloc (model->num_ands * sizeof *res->_ands);
  for (i = 0; i < model->num_ands; i++) {
    lit = newvar ();
    res->_ands[i] = lit;
    u_and = model->ands + i;
    if( actuallyEnode ) _and (lit, import (res, u_and->rhs0), import (res, u_and->rhs1)); /// all 1 and -1 are fixed, and can be handled like that!
    else { import (res, u_and->rhs0); import (res, u_and->rhs1); } // fake the newvar calls
  }

  /// get values for latches
  for (i = 0; i < model->num_latches; i++) {
    int importedLit = import (res, model->latches[i].next);
    res->latches[i].next = importedLit;
    msg(3,"index %d of latch %d in time step %d results in literal %d",model->latches[i].next,i,time,res->latches[i].next);
    if( verbose > 2 && (res->latches[i].next == 1 || res->latches[i].next == -1) ) cerr << "c found unused latch with index " << i << endl;
    if( actuallyEnode ) shiftFormula.latchNext[i] = res->latches[i].next;
  }

  res->assume = newvar ();
  if( actuallyEnode ) shiftFormula.currentAssume = res->assume;

  /// take care of bad states ( seem to be set for each iteration )
  if (model->num_bad) {
    res->bad = malloc (model->num_bad * sizeof *res->bad);
    for (i = 0; i < model->num_bad; i++) {
      res->bad[i] = import (res, model->bad[i].lit); /// check how this variable is created
      if( actuallyEnode ) shiftFormula.currentBads[i] = res->bad[i]; // store bad literals for output! have to be the of the current last state!
    }
    if (model->num_bad > 1) {
      res->onebad = newvar ();
      if( actuallyEnode ) {
	add (-res->onebad);
	for (i = 0; i < model->num_bad; i++) add (res->bad[i]); // no need to be touched during shift!
	add (0);
      }
    } else res->onebad = res->bad[0];
  }

  /// take care of other constraints
  static bool didNumConstraints = false;
  if (model->num_constraints) {
    if( !didNumConstraints ) { cerr << "WARNING: CANNOT HANDLE NUM_CONSTRAINTS YET!" << endl; didNumConstraints = true; }
    res->constraints =
      malloc (model->num_constraints * sizeof *res->constraints);
    for (i = 0; i < model->num_constraints; i++)
      res->constraints[i] = import (res, model->constraints[i].lit);
    res->sane = newvar ();
    if( actuallyEnode ) {
      for (i = 0; i < model->num_constraints; i++)
	binary (-res->sane, res->constraints[i]);
      if (time) binary (-res->sane, prev->sane); /// compare with previous round!
      binary (-res->assume, res->sane);
    }
  }

  /// handle justice constraint
  static bool didJustice = false;
  if (model->num_justice) {
    assert( false && "cannot handle justice constraints yet" );
    exit (30);
  }

  /// handle fairness criteria
  static bool didFairness = false;
  if (model->num_justice && model->num_fairness) {
    assert( false && "cannot handle justice constraints yet" );
    exit (30);    
  }

  assert (model->num_bad || model->num_justice);
  /// justified or bad have to be fulfilled
  if( actuallyEnode ) {
    add (-res->assume);
    if (model->num_bad) add (res->onebad);
    // if (model->num_justice) add (res->onejustified); /// here we do not take care for justice!
    add (0);
  }

  msg (1, "encoded %d", time);
  return res->assume; /// return the single literal that has to be
}

static int isnum (const char * str) {
  const char * p = str;
  if (!isdigit (*p++)) return 0;
  while (isdigit (*p)) p++;
  return !*p;
}

static const char * usage =
"usage: aigbmc [<model>][<maxk>] [OPTIONS]\n"
"\n"
"-h         print this comm_and line option summary\n"
"--help     prints the help of the SAT solver, if riss is used\n"
#ifndef TEST_BINARY
"-bmc_v     increase verbose level\n"
#endif
"-bmc_m     use outputs as bad state constraint\n"
"-bmc_n     do not print witness\n"
"-bmc_q     be quite (impies '-n')\n"
"-bmc_c     check whether there are properties (return 1, otherwise 0)\n"
"-bmc_l     use lazy coding\n"
"-bmc_s     use shifting instead of reencoding (implies lazy encoding)\n"
"-bmc_r     use riss as SAT solver\n"
"-bmc_p     use CP3 as simplifier (implies using riss and shifting)\n"
#ifndef TEST_BINARY
"-bmc_dbgp  dump formula and everything else before preprocessing\n"
#endif
"-bmc_d     dense after preprocessing (implies using riss and CP3)\n"
"-bmc_x     modify preprocess (implies using riss and CP3)\n"
"-bmc_y     modify preprocess (implies using riss and CP3)\n"
"-bmc_z     more reasoning before search\n"
"-bmc_t     print time needed per bound\n"
"-bmc_ml X  specify memory limit in MB\n"
"-bmc_w     use wild guesser thread (implies using riss, lazy and shifting)\n"
"-bmc_ws  X initial guess frame (implies using guesser)\n"
"-bmc_wi  X initial guess increase (implies using guesser)\n"
"-bmc_wii X inc/dec of guess distance (implies using guesser)\n"
#ifdef USE_ABC
"-bmc_a     use the ABC tool to simplify the circuit before solving, next parameter has to specify a tmp file location!\n"
"-bmc_ad    use the ABC tool to simplify the circuit before solving, next parameter has to specify a tmp directory location!\n"
"-bmc_ac    give a special command(-sequence, separated by :) to ABC. default is dc2 other possible: dc2, drwsat, scorr\n"
#endif
"-bmc_tune  enable the output for tuning with paramILS/SMAC\n"
"\n\n the model name should be given first, then, the max. bound should be given\n"
" other options might be given as well. These options are forwarded to the SAT solver\n"
;

static void print (int lit) {
  int val = deref (lit), ch;
  if (val < 0) ch = '0';
  else if (val > 0) ch = '1';
  else ch = 'x';
  putc (ch, stdout);
}

/** print the given value, without deref */
static void printV (int val) {
  int ch;
  if (val < 0) ch = '0';
  else if (val > 0) ch = '1';
  else ch = 'x';
  putc (ch, stdout);
}

static void nl () { putc ('\n', stdout); }

/** perform wild guessing according to the given parameter*/
void* wildGuessMethod (void* threadData) {
  pthread_attr_t attr;       // create a joinable thread
  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  SharedData& sharedData = * (SharedData*) threadData;

  int encodeFromHereFrame = 0; // so far nothing is in the solver (neede for reset after UNSAT)
  int currentFrame = sharedData.lowerBound > wildGuessStart ? sharedData.lowerBound : wildGuessStart;
  if( currentFrame < 1 ) currentFrame = 1; // 0 is proven by the other thread already!
  
  assert( lazyEncode && "method does work only with lazy encoding" );
  
  Riss::IncSolver* guessSolver = sharedData.guesser;
  int lit = shiftFormula.currentAssume;
  std::vector<int> thisCheckClause; // collect all assumption literals that are not known yet as clause
  while( true ) {
    if( currentFrame > maxk ) {
      if ( sharedData.lowerBound < maxk ) currentFrame = maxk;
      else break; // lower bound is maxk -> done solving
    }

    thisCheckClause.clear();
    const int tmpLB = sharedData.lowerBound;
    int thisRoundLowerBound = tmpLB < 0 ? 0 : tmpLB; // needed to calculate the real lower bound for later!
    // give formula to solver
    if( encodeFromHereFrame == 0 ) { // after full reset, also pass other things to solver!
      guessSolver->clearSolver(); // reset "everything" inside the solver!
      guessSolver->add(1);guessSolver->add(0); // since we do not need to do anything else with the formula, we can add the unit clause immediately
      for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) guessSolver->add( shiftFormula.formula[i] );
      for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i+=2 ){
	guessSolver->add(shiftFormula.todoEqualities[i]); guessSolver->add(-shiftFormula.todoEqualities[i+1]);guessSolver->add(0);
	guessSolver->add(-shiftFormula.todoEqualities[i]); guessSolver->add(shiftFormula.todoEqualities[i+1]);guessSolver->add(0);
      }
      guessSolver->add(-lit); guessSolver->add(0); // add unit clause! // TODO has to be enabled!
    }
    // encode all following frames!
    const int shiftDist = shiftFormula.afterPPmaxVar - 1;
    for( int k = encodeFromHereFrame == 0 ? 1 : encodeFromHereFrame; k <= currentFrame; ++ k )
    {
      const int thisShift = k * shiftDist;
      const int lastShift = (k-1) * shiftDist;
	// new assume literal - if we want to solve a higher bound, we can add this unit immediately
        lit = shiftFormula.currentAssume + thisShift;
	if( k != currentFrame ) {
	  if( k <= thisRoundLowerBound ) {
	    guessSolver->add(-lit);guessSolver->add(0); // add the negated unit, if its known already // TODO has to be enabled!
	  } else {
	    thisCheckClause.push_back( lit ); // assume this level could be violated
	  }
	}
	
	// shift the formula!
	int thisClauses = 0;
	for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
	  const int& cl = shiftFormula.formula[i];
	  if( cl >= -1 && cl <= 1 ) {
	    guessSolver->add (cl); // handle constant units, and the end of clause symbol
	    if( cl == 0 ) thisClauses ++;
	  }
	  else guessSolver->add( varadd(cl, thisShift));
	}
	// ensure that latches behave correctly in the next iteration
	for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
	  const int lhs = ( shiftFormula.latch[i] == 1 || shiftFormula.latch[i] == -1 ) ? shiftFormula.latch[i] : varadd(shiftFormula.latch[i], thisShift);
	  const int rhs = ( shiftFormula.latchNext[i] == 1 || shiftFormula.latchNext[i] == -1 ) ? shiftFormula.latchNext[i] : varadd(shiftFormula.latchNext[i], lastShift);
	  guessSolver->add(-lhs);guessSolver->add(rhs);guessSolver->add(0);
	  guessSolver->add(lhs);guessSolver->add(-rhs);guessSolver->add(0);
	}
      if ( k == currentFrame ) // here we are to solve the current formula, this is the last iteration! have to set "encodeFromHereFrame"
      {
	thisCheckClause.push_back( lit );
	bool foundBad = false; // indicate whether solving resulted in 10!
	sharedData.guesserFrame = k; // my position
	// if lower bound is already above k, skip this iteration!
	if( sharedData.guesserFrame < sharedData.lowerBound ) { // other solver has prooven this lowerbound already!
	  msg(1,"WG: solving frame %d was already done - set unsat automatically", sharedData.guesserFrame );
	  foundBad = false;
	  encodeFromHereFrame = k; // can keep everything, since no assumption clause has been added
	  continue;
	} else {
	  msg(1,"WG: solving frame %d with assumption clause of size %d", sharedData.guesserFrame, thisCheckClause.size() );
	  assert( thisCheckClause.size() > 0 && "size of the check clause should be larger than 0, otherwise, we add the empty clause immediately!" );
	  for( int i = 0 ; i < thisCheckClause.size() ; ++i ){
	    guessSolver->add( thisCheckClause[i] );
	  }
	  guessSolver->add(0);
	  encodeFromHereFrame = 0; // for next iteration
	  const int retCode = guessSolver->sat ();
	  foundBad = ( retCode == 10); // otherwise, we need to test the current bound!
	  if( retCode != 20 && retCode != 10 ) break; // interrupt might have been called
	}
	if( foundBad ) {
	  // one of the bad outputs can be set to true -> find it and set it as upper bound!
	  // update the other solver!
	  if( sharedData.lowerBound + 1 == k ) { // this is the result, since k-1 has been shown to be unsat already!
	    sharedData.guesserSolved = 1;
	    sharedData.upperBound = k; // lowerst SAT result
	    msg(1,"WG: found solution at frame %d %d %d", sharedData.guesserFrame,sharedData.upperBound,k);
	    sharedData.riss->interrupt();
	    break;
	  } else { // found upper bound, now get rid of the formula and perform binary search backwards!
	    bool foundSatLit = false;
	    for( int i = 0; i < thisCheckClause.size(); ++ i ) {
	      if( guessSolver->deref(thisCheckClause[i]) == 1 ) {
		msg(1,"literal for output at level %d is satisfied", thisRoundLowerBound);
		sharedData.upperBound = thisRoundLowerBound + i + 1; // use the first sat literal as upper bound!
		foundSatLit = true;
		break;
	      }
	    }
	    assert(foundSatLit && "one of the literals in the clause has to be satisfied!" );
	    if( sharedData.upperBoundModel == 0 ) sharedData.upperBoundModel = new Riss::vec<Riss::lbool>();
	    guessSolver->exportModel( sharedData.upperBoundModel ); // export the model for this level, so that it can be used later
	    thisRoundLowerBound = thisRoundLowerBound < sharedData.lowerBound ? sharedData.lowerBound : thisRoundLowerBound; // update, if the other thread did something there!
	    currentFrame = (thisRoundLowerBound + sharedData.upperBound) / 2; // binary search - floor the value (no +1)
	    msg(1,"WG: found SAT result at frame %d which revealed new upper bound %d, try next %d, current lower bound", sharedData.guesserFrame,sharedData.upperBound, currentFrame,sharedData.lowerBound);
	    assert( currentFrame < sharedData.upperBound && "do not solver same formula twice!" );
	  }
	  break;
	} else {
	  if( sharedData.lowerBound < k ) sharedData.lowerBound = k; // update the lower bound!
	  if( sharedData.upperBound == -1 ) {
	    currentFrame += wildGuessInc; // increase to next level
	    wildGuessInc += wildGuessIncInc; // increase the increase value!
	  } else {
	    if( sharedData.guesserFrame + 1 == sharedData.upperBound ) { // act as if we would have solved this level right now
	      guessSolver->importModel( sharedData.upperBoundModel ); // export the model for this level, so that it can be used later  
	      sharedData.guesserFrame = k + 1; // set the according frame
	      sharedData.guesserSolved = 1;
	      sharedData.riss->interrupt();
	      break;
	    }
	    currentFrame = (sharedData.lowerBound + sharedData.upperBound) / 2;
	    assert( (currentFrame < sharedData.upperBound || sharedData.rissSolved ) && "do not solver same formula twice, if we have not solved it yet!" );
	  }
	  encodeFromHereFrame = 0; // always need to reset after sat call, because there is the assume clause
	  msg(1,"WG: found new lower bound %d, try next: %d, current upper bound %d", sharedData.guesserFrame, currentFrame, sharedData.upperBound);
	}
	break; // need to reencoded the formula, before it can be solved again
      } 
    } // end of encoding for loop
    if( sharedData.guesserSolved || sharedData.rissSolved ) break; // drop out of the thread here
    pthread_testcancel(); // check whether we should be aborted
  }
  return 0;
}

int main (int argc, char ** argv) {
  totalTime = cpuTime() - totalTime;
  int i=0, j=0, k=0, lit=0, found=0;
  
  bool noProperties = false, checkProperties = false; // are there any properties inside the circuit?
  
  const char * err;
  for (i = 1; i < argc; i++) {
    if ( !strcmp (argv[i], "-h") || !strcmp (argv[i], "--help")) {
      // usage for BMC
      printf ("%s", usage);
      // give SAT solver the chance to print its help
      init( argc,argv );
      reset();
      // terminate program
      exit (0);
    } 
#ifndef TEST_BINARY
    else if (!strcmp (argv[i], "-bmc_v")) verbose++;
#endif
    else if (!strcmp (argv[i], "-bmc_m")) move = 1;
    else if (!strcmp (argv[i], "-bmc_n")) nowitness = 1;
    else if (!strcmp (argv[i], "-bmc_q")) quiet = 1;
    else if (!strcmp (argv[i], "-bmc_nw")) nowarnings = 1;
    else if (!strcmp (argv[i], "-bmc_tune")) {
      tuning = 1;
      if( ++i < argc && isnum(argv[i]) ) tuneSeed = atoi( argv[i] );
    }
    else if (!strcmp (argv[i], "-bmc_l")) lazyEncode = 1;
    else if (!strcmp (argv[i], "-bmc_s")) { useShift = 1; lazyEncode = 1;} // strong dependency!
    else if (!strcmp (argv[i], "-bmc_r")) useRiss = 1;
    else if (!strcmp (argv[i], "-bmc_p")) { useCP3=1,useRiss = 1;useShift = 1; lazyEncode = 1; }
#ifndef TEST_BINARY
    else if (!strcmp (argv[i], "-bmc_dbgp")) { 
      ++ i;
      if( i < argc ) {
	dumpPPformula = argv[i];
	msg(0,"set debug PP file to %s",dumpPPformula);
      }
    }
#endif
    else if (!strcmp (argv[i], "-bmc_d")) { useCP3=1,useRiss = 1;denseVariables=1;  useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_x")) { useCP3=1,useRiss = 1;dontFreezeInput=1; useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_y")) { useCP3=1,useRiss = 1;dontFreezeBads=1;  useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_z")) {
      ++ i;
      if( i < argc && isnum(argv[i]) ) { 
	checkInitialBad = atoi( argv[i] );
	msg(1,"set bad state analysis resource limit to %d", checkInitialBad );
      }
    }
    else if (!strcmp (argv[i], "-bmc_t")) printTime = 1;
    else if (!strcmp (argv[i], "-bmc_ml")) { // set a memory limit!!
      ++i;
      int memLim = 0;
      if( i < argc && isnum(argv[i]) ) memLim = atoi( argv[i] );
    // Set limit on virtual memory:
        if (memLim != 0){
            rlim_t new_mem_lim = (rlim_t)memLim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    wrn("could not set resource limit: Virtual memory.");
            } else {
	      msg(0,"set memory limit to %d MB", memLim);
	    }
	} else {
	  wrn("no memory limit specified");
	}
    }
    else if (!strcmp (argv[i], "-bmc_w")) {
      wildGuesser = 1; useRiss = 1; useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_ws")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessStart = atoi( argv[i] );
      wildGuesser = 1; useRiss = 1; useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_wi")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessInc = atoi( argv[i] );
      wildGuesser = 1; useRiss = 1; useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_wii")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessIncInc = atoi( argv[i] );
      wildGuesser = 1; useRiss = 1; useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_c")) checkProperties = true;
#ifdef USE_ABC
    else if (!strcmp (argv[i], "-bmc_a")) {
      ++i;
      if( i < argc ) { useABC = std::string(argv[i]); abcDir = 0; }
      else wrn("not enough parameters to set the tmp file for ABC");
    }
    else if (!strcmp (argv[i], "-bmc_ad")) {
      ++i;
      if( i < argc ) { useABC = std::string(argv[i]); abcDir = 1; }
      else wrn("not enough parameters to set the tmp directory for ABC");
    }
    else if (!strcmp (argv[i], "-bmc_ac")) {
      ++i;
      if( i < argc ) abcCommand = std::string(argv[i]);
      else wrn("not enough parameters to set the command for ABC");
    }    
    
#endif
    // the other parameters might be for the SAT solver
    else if (isnum (argv[i]) && maxk == 0) {
      maxk = atoi (argv[i]);
    } else if (argv[i][0] == '-') {
      wrn( "unknown bmc parameter '%s'", argv[i] );
      // die ("invalid comm_and line option '%s'", argv[i]);
    } else if (name && maxk >= 0) {
      wrn( "unknown bmc argument '%s'", argv[i] );
      // die ("unexpected argument '%s'", argv[i]);
    } else if( !name) name = argv[i];
  }
  if (maxk <= 0) maxk = 1 << 30; // set a very high bound as default (for 32bit machines)!
  msg (1, "aigbmc bounded model checker");
  msg (1, "maxk = %d", maxk);

  double ppAigTime = 0, ppCNFtime = 0, reasonTime = 0;
  vector<double> boundtimes;
  
  // init solvers, is necessary with command line parameters
  init (argc,argv);
  
  int initialLatchNum = 0; // number of latches in initial file. If ABC changes the number of latches, this number is important to print the right witness!
  
/**
 *  use ABC for simplification
 */
// #define USE_ABC
#ifdef USE_ABC
  // simplification with ABC?
  if( useABC.size() > 0 ) { // there is a file or location
    MethodTimer aigTimer ( &ppAigTime );
    if(!name) { // reject reading from stdin
      wrn("will not read from stdin, since ABC ");  
      exit(30);std::string s;
    }
    // setup the command line for abc
    if( abcCommand.size() == 0 ) {
      abcCommand = "\"dc2\"";
      msg(1,"set abc command to default command %s", abcCommand.c_str() );
    }
    // if the scorr command is part of the command chain, the initial number of latches has to be set
    if( abcCommand.find("scorr") != std::string::npos ) {
      // parse the aig file and read the number of latches!
      std::ifstream myfile;
      myfile.open (name);
      if(!myfile) { wrn("error in opening file %s",name); exit (30); }
      int fm,fi,fl,fo,fa;
      std::string sstr;
      while( sstr != "aag" && sstr != "aig") {
	if(!myfile) break; // reached end of file?
	myfile >> sstr;
      }
      if( myfile ) {
	myfile >> fm >> fi >> fl >> fo >> fa;
	//std::cerr << " parsed MILOA: " << fm << ", " << fi << ", " << fl << ", " << fo << ", " << fa << std::endl;
	initialLatchNum = fl;
	msg(1,"set number of initial latches to %d - initial MILOA: %d %d %d %d %d",fl,fm,fi,fl,fo,fa);
      } else {
	wrn("could not extract the number of latches from %s",name);
      }
      myfile.close();
    }
    // start abc
    void * pAbc;
    Abc_Start(); // turn on ABC
    pAbc = Abc_FrameGetGlobalFrame();
    std::string abcmd = std::string("read ") + std::string(name);
    if ( Cmd_CommandExecute( pAbc, abcmd.c_str() ) )
    {
        wrn( "ABC cannot execute command \"%s\".\n", abcmd.c_str() );
        exit(30);
    }

    while ( abcCommand.size() > 0 ) {
      std::string thisCommand;
      if( abcCommand.find(':') != std::string::npos ) {
	int findP = abcCommand.find(":");
        thisCommand =  abcCommand.substr(0,findP); // extract first token
        abcCommand.erase(0,findP+1); // remove token from the first position
      } else {
	thisCommand = abcCommand;
	abcCommand = "";
      }
      msg(1,"execute abc command(s) %s, remaining: %s", thisCommand.c_str(), abcCommand.c_str() );
      if ( true &&  Cmd_CommandExecute( pAbc, thisCommand.c_str() ) )
      {
	  wrn( "ABC cannot execute command %s.\n", thisCommand.c_str() );
	  exit(30);
      }
    }
    // writing to a directory -> create file name based on PID of the current process!
    if( abcDir == 1 ) {
      std::stringstream filename;
      filename << useABC << ( useABC[useABC.size()-1] == '/' ? "tmp-" : "/tmp-") << getpid() << ".aig";
      useABC = filename.str();
      msg(1,"write temporary file %s", useABC.c_str());
    }
    
    abcmd = std::string("write ") + std::string(useABC);
    if ( Cmd_CommandExecute( pAbc, abcmd.c_str() ) )
    {
        wrn( "ABC cannot execute command \"%s\".\n", abcmd.c_str() );
        exit(30);
    }
    Abc_Stop(); // turn off ABC
    msg(1,"change the name of the aiger file to read from %s to %s",name,useABC.c_str());
    name = useABC.c_str(); // do not change useABC after this point any more!
  }
  msg(0,"aig preprocessing tool %lf seconds", ppAigTime);
#endif
  
  msg (1, "reading from '%s'", name ? name : "<stdin>");
  if (name) err = aiger_open_and_read_from_file (model, name);
  else err = aiger_read_from_file (model, stdin), name = "<stdin>";
  if (err) die ("parse error reading '%s': %s", name, err);
  
#ifdef USE_ABC
  // delete the temporary file as soon as its not needed any more!
  if( abcDir == 1 ) {
    if( 0 != remove( useABC.c_str() ) ) { // delete the temporary file
      wrn("could not remove temporary file from temporary directory: %s", useABC.c_str() );
    }
  }
#endif
  
  
  msg (1, "MILOA = %u %u %u %u %u",
       model->maxvar,
       model->num_inputs,
       model->num_latches,
       model->num_outputs,
       model->num_ands);
  if (!model->num_bad && !model->num_justice && model->num_constraints)
    wrn ("%u environment constraints but no bad nor justice properties",
         model->num_constraints);
  if (!model->num_justice && model->num_fairness)
    wrn ("%u fairness constraints but no justice properties",
         model->num_fairness);
  if (move) { 
    if (model->num_bad)
      wrn ("will not move outputs if bad state properties exists");
    else if (model->num_constraints)
      wrn ("will not move outputs if environment constraints exists");
    else if (!model->outputs)
      wrn ("not outputs to move");
    else {
      wrn ("using %u outputs as bad state properties", model->num_outputs);
      for (i = 0; i < model->num_outputs; i++)
	aiger_add_bad (model, model->outputs[i].lit, 0);
    }
  }
  msg (1, "BCJF = %u %u %u %u",
       model->num_bad,
       model->num_constraints,
       model->num_justice,
       model->num_fairness);
  
  if( checkProperties ) {
    cerr << "c bad " << model->num_bad << " output " << model->num_outputs << endl;
    if (!model->num_bad && !model->num_justice) exit( 0 );
    else exit( 1 );
  }
  
  
  if (!model->num_bad && !model->num_justice) {
    wrn ("no properties");
    noProperties = true;
    // goto DONE;
    wrn ("still try to solve the circuit");
  }
  aiger_reencode (model);
  firstlatchidx = 1 + model->num_inputs;
  first_andidx = firstlatchidx + model->num_latches;
  msg (2, "reencoded model");
  bad = calloc (model->num_bad, 1);
  justice = calloc (model->num_justice, 1);
  
  if (model->num_justice) {
    assert( false && "cannot handle justice constraints!" );
    exit(30);
  }

  /**
   * 
   *  THIS IS THE ACTUAL METHOD TO WORK WITH!
   *  AT THE MOMENT: the formula is extended by re-encoding it again _and again
   * 
   *  AIM: encode initial formula once, freeze input/output variables, preprocess, shift this formula again _and again!
   * 
   */

  // first loop iteration unrolled!
  const int constantUnit = newvar ();

  shiftFormula.clear(); 
  
  boundtimes.push_back(0);boundtimes[0] = cpuTime() - boundtimes[0]; // start measuring the time for bound 0
  lit = encode ();
  boundtimes[0] = cpuTime() - boundtimes[0]; // interrupt measuring the time for bound 0
  shiftFormula.currentAssume = lit;
  shiftFormula.initialMaxVar = nvars;
  shiftFormula.afterPPmaxVar = nvars;
  // TODO: here, simplification and compression could be performed!

  if( useCP3 ) {
    MethodTimer cnfTimer( &ppCNFtime );
    cp3config = new Coprocessor::CP3Config;
    // add more options here?
    cp3config->opt_enabled = true; cp3config->opt_verbose=0;
    cp3config->opt_bve=true; cp3config->opt_bve_verbose=0; 
    cp3config->opt_printStats = false;
    cp3config->parseOptions( argc,argv ); // parse the configuration!
    if( denseVariables ) { // enable options for CP3 for densing variables
      cp3config->opt_dense = true; cp3config->opt_dense_store_forward = 1; 
    }
    Riss::CoreConfig config = riss->getConfig();
    config.hk = false; // do not use laHack during preprocessing! (might already infere that the output lit is false -> unroll forever)
    ppSolver = new Riss::Solver (config);
    incsolver = new Riss::IncSolver (ppSolver,config);
    outerPreprocessor = new Coprocessor::Preprocessor ( ppSolver, *cp3config );
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) incsolver->add( shiftFormula.formula[i] );
    
    if( verbose > 1 ) cerr << "c call freeze variables ... " << endl;
    outerPreprocessor->freezeExtern( 1 ); // do not remove the unit (during dense!)
    if( !dontFreezeInput ) for( int i = 0 ; i < shiftFormula.inputs.size(); ++i ) outerPreprocessor->freezeExtern( shiftFormula.inputs[i] );
    for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) outerPreprocessor->freezeExtern( shiftFormula.latch[i] );
    for( int i = 0 ; i < shiftFormula.latchNext.size(); ++i ) outerPreprocessor->freezeExtern( shiftFormula.latchNext[i] );
    outerPreprocessor->freezeExtern( shiftFormula.currentAssume < 0 ? -shiftFormula.currentAssume : shiftFormula.currentAssume );
    if( !dontFreezeBads ) for( int i = 0 ; i < shiftFormula.currentBads.size(); ++i ) outerPreprocessor->freezeExtern( shiftFormula.currentBads[i] );
    // for debugging interference with CP3
    if( dumpPPformula ) {
      ofstream formulaFile;
      formulaFile.open( (string(dumpPPformula) + string(".cnf")).c_str() ); // file for write formula to
      if(!formulaFile) wrn("could not open formula dump file");
      else {
	for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) {
	  const int l = shiftFormula.formula[i];
	  if( l == 0 ) formulaFile << "0" << endl;
	  else formulaFile << l << " ";
	}
	formulaFile.close();
      }
      ofstream freezeFile;
      freezeFile.open( (string(dumpPPformula) + string(".freeze")).c_str() ); // file for write formula to
      if(!freezeFile) wrn("could not open formula dump file");
      else {
	if( !dontFreezeInput ) for( int i = 0 ; i < shiftFormula.inputs.size(); ++i ) freezeFile << shiftFormula.inputs[i] << " 0" << endl;
	for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) freezeFile <<  shiftFormula.latch[i] << " 0" << endl;
	for( int i = 0 ; i < shiftFormula.latchNext.size(); ++i ) freezeFile << shiftFormula.latchNext[i] << " 0" << endl;
	freezeFile <<  (shiftFormula.currentAssume < 0 ? -shiftFormula.currentAssume : shiftFormula.currentAssume) << " 0" << endl;
	freezeFile.close();
      }
    }
    
    if( verbose > 1 ) cerr << "c call preprocess ... " << endl;
    outerPreprocessor->preprocess();
    if( !ppSolver->okay() ) {
      msg(0,"initial formula has no bad state (found even without initialization)"); 
      exit(0);
    }
    // test whether readding works without modifying the formula
    if( verbose > 1 ) { cerr << "c before formula: " << endl; riss->printFormula(); }
    // alles gut!
    riss->clearSolver();
    outerPreprocessor->dumpFormula( shiftFormula.formula ); // actively changing the formula!
    if( verbose > 1 ) cerr << "c add formula to riss" << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) riss->add( shiftFormula.formula[i] );
    // TODO: reduce maxVar according to shrink! ... thus, shift distance will be adopted
    shiftFormula.afterPPmaxVar = ppSolver->nVars();
    if( verbose > 1 ) cerr << "c after variables: " << shiftFormula.afterPPmaxVar << endl;
    // riss->setMaxVar( nvars );
    if( denseVariables ) { // refresh knowledge about the extra literals (input,output/bad,latches,assume)
      int tmpLit = outerPreprocessor->giveNewLit( shiftFormula.currentAssume );
      if( verbose > 1 ) cerr << "move assume from " << shiftFormula.currentAssume << " to " << tmpLit << endl;
      shiftFormula.currentAssume = tmpLit;
      if( !dontFreezeInput ) { // no need to shift input bits, if they will be restored properly after a solution has been found
	for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) {
	  tmpLit = outerPreprocessor->giveNewLit( shiftFormula.inputs[i] );
	  if( verbose > 1 ) cerr << "move input from " <<  shiftFormula.inputs[i]   << " to " << tmpLit << endl;
	  shiftFormula.inputs[i] = tmpLit;
	}
      }
      for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ){
	tmpLit = outerPreprocessor->giveNewLit( shiftFormula.latch[i] );
	if( verbose > 1 ) cerr << "move latch from " << shiftFormula.latch[i] << " to " << tmpLit << endl;
	shiftFormula.latch[i] = tmpLit;
      }
      for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ){
	tmpLit = outerPreprocessor->giveNewLit(shiftFormula.latchNext[i]);
	if( verbose > 1 ) cerr << "move latchN from " << shiftFormula.latchNext[i] << " to " << tmpLit << endl;
	shiftFormula.latchNext[i] = tmpLit;
      }
      if( !dontFreezeBads ) { // dont freeze bad stats, if they are restored after a solution has been found
	for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ){
	  tmpLit = outerPreprocessor->giveNewLit( shiftFormula.currentBads[i] );
	  if( verbose > 1 ) cerr << "move bad from " <<   shiftFormula.currentBads[i]  << " to " << tmpLit << endl;
	  shiftFormula.currentBads[i] = tmpLit;
	}
      }
      // also shift equivalences that are not known by the solver and preprocessor yet (but are present during search)
      for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i++ ) {
	tmpLit = outerPreprocessor->giveNewLit( shiftFormula.todoEqualities[i] );
	if( verbose > 1 ) cerr << "move eq from " <<   shiftFormula.todoEqualities[i]  << " to " << tmpLit << endl;
	shiftFormula.todoEqualities[i] = tmpLit;
      }
    }
    if( verbose > 1 ) {
      cerr << "c maxVar["<<k<<"]:     " << nvars << endl;
      cerr << "c assume["<<k<<"]:     " << shiftFormula.currentAssume << endl;
      cerr << "c inputs["<<k<<"]:     " << shiftFormula.inputs.size() << ": "; for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) cerr << " " << shiftFormula.inputs[i]; cerr << endl;
      cerr << "c latches["<<k<<"]:    " << shiftFormula.latch.size() << ": "; for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) cerr << " " << shiftFormula.latch[i]; cerr << endl;
      cerr << "c latchNext["<<k<<"]:  " << shiftFormula.latchNext.size() << ": "; for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ) cerr << " " << shiftFormula.latchNext[i]; cerr << endl;
      cerr << "c thisBad["<<k<<"]:  " << shiftFormula.currentBads.size() << ": "; for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ) cerr << " " << shiftFormula.currentBads[i]; cerr << endl;
      cerr << "c formula["<<k<<"]:    " << endl;
      for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
	cerr << " " << shiftFormula.formula[i];
	if( shiftFormula.formula[i] == 0 ) cerr << endl;
      }
      cerr << "c after formula (inside riss): " << endl;
      riss->printFormula();
    }
  }
  msg(0,"cnf preprocessing tool %lf seconds", ppCNFtime);
  // TODO after compression, update all the variables/literals lists in the shiftformula structure!
  
  // we could check whether the circuit is completely safe, be assuming the top level unit, ignoring the initialization of latches, and assuming the single bad states to be true
  // if UNSAT, we can unroll the circuit for ever
  // if SAT, we cannot tell anything, except that a latch initialization was found, which can result in a bad state
  // TODO what to do with multiple bad states?
  if( checkInitialBad && !dontFreezeBads && shiftFormula.currentBads.size() == 1 ) // only possible if bad is not frozen!
  {
    reasonTime = cpuTime() - reasonTime; // start timer
    msg(0,"analyze bad state");
    int satResult = 10;
    int iterations = 0;
    do {
      iterations ++;
      assume( 1 ); // top level unit is not set yet, but is essential
      int badlit = shiftFormula.currentAssume; // take care of moving variables around!
      assume( -badlit ); // check whether this state can be reached
      satResult  = sat ( checkInitialBad ) ; // give the number of conflicts to search maximally!
      if( satResult == 10 ) {
	msg(0,"found a latch configuration for reaching the bad state");
	// TODO add a clause to the solver that disallows this combination
	/*
	add( -badlit, false ); // give to solver, but not to shiftFormula! have extra assumption here! e.g. 2!
	for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) {
	  if( deref(shiftFormula.latch[i]) != 0 ) { // if the latch has no state, there is no need to add it to the clause!
	    int nl = -1 * shiftFormula.latch[i] * deref(shiftFormula.latch[i] );
	    msg(1, "latch[%d] = %d has value %d; thus, add lit %d to the clause",i, shiftFormula.latch[i], deref(shiftFormula.latch[i] ), nl );
	    add( nl, false );
	  }
	}
	add( 0 );
	*/
	break;
      } else if (satResult == 20 ) {
	if( iterations == 1 ) msg(0,"found no latch configuration for reaching the bad state - can unroll infinitely!");
	else  msg(0,"there exists exactly %d latch configurations that lead to the bad state", iterations - 1);
      } else {
	msg(0,"could not analyze bad state properties completely within resource bounds. So far, found %d potential latch configurations.", iterations - 1);
      }
    } while( satResult == 10 );
    reasonTime = cpuTime() - reasonTime; // stop timer
    msg(1,"needed %lf seconds for bad state reasoning",reasonTime );
  }

  lit = shiftFormula.currentAssume; // literal might have changed!
  int shiftDist = shiftFormula.afterPPmaxVar - 1;  // shift depends on the variables that are actually in the formula that is used for solving
  
  if( lazyEncode )
    for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i+=2 )
      _eq( shiftFormula.todoEqualities[i], shiftFormula.todoEqualities[i+1], false );
  
  SharedData sharedData;
  if( wildGuesser ) {
    // spin off extra thread, which performs guessing!
    sharedData.riss = riss;
    sharedData.guesser = new Riss::IncSolver (new Riss::Solver (riss->getConfig()), riss->getConfig()); // FIXME this causes a memory leak for sure! (cannot delete the solver)
    pthread_create( & sharedData.guesserThread, 0, wildGuessMethod , &sharedData );
  }
  
  // stay here for 10 seconds - to debug the other thread! TODO remove after debug!
  msg(1,"start solving at frame 0");
  
  boundtimes[0] = cpuTime() - boundtimes[0]; // continue measuring the time for bound 0    
  unit ( constantUnit );   // the literal 1 will always be mapped to true!! add only in the very first iteration!
  assume (lit);
  assert ( k == 0 && "the first iteration has to be done first!" );
  if( verbose > 2 ) { 
    cerr << "c maxVar["<<k<<"]:     " << nvars << endl;
    cerr << "c assume["<<k<<"]:     " << shiftFormula.currentAssume << endl;
    cerr << "c inputs["<<k<<"]:     " << shiftFormula.inputs.size() << ": "; for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) cerr << " " << shiftFormula.inputs[i]; cerr << endl;
    cerr << "c latches["<<k<<"]:    " << shiftFormula.latch.size() << ": "; for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) cerr << " " << shiftFormula.latch[i]; cerr << endl;
    cerr << "c latchNext["<<k<<"]:  " << shiftFormula.latchNext.size() << ": "; for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ) cerr << " " << shiftFormula.latchNext[i]; cerr << endl;
    cerr << "c thisBad["<<k<<"]:  " << shiftFormula.currentBads.size() << ": "; for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ) cerr << " " << shiftFormula.currentBads[i]; cerr << endl;
    cerr << "c formula["<<k<<"]:    " << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
      cerr << " " << shiftFormula.formula[i];
      if( shiftFormula.formula[i] == 0 ) cerr << endl;
    }
    cerr << "c equis: " << endl;
      // ensure that latches behave correctly in the next iteration
      for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i+=2 ) {
	cerr << " " << shiftFormula.todoEqualities[i]  << " == " <<  shiftFormula.todoEqualities[i+1] << endl;
    }
  }
  bool foundBadStateFast =  (sat () == 10);
  boundtimes[0] = cpuTime() - boundtimes[0]; // stop measuring the time for bound 0
  if( printTime ) msg(0,"solved bound %d in %lf seconds", 0, boundtimes[k]);
  if( foundBadStateFast ) {
    sharedData.rissSolved = 1; // we found the solution!
    sharedData.upperBound = 0; // lowerst SAT result
    goto finishedSolving;
  }
  sharedData.lowerBound = 0;
  unit (-lit, false); // do not add this unit to the shift formula!
  
  msg (2, "re-encode model");
  if( !useShift ) { // old or new method?
    for (k = 1; k <= maxk; k++) {
      if( sharedData.guesserSolved ) {
	msg(1,"stop solving at frame %d because wg solved the formula", k);
	break; // do not continue, if the guesser thread solved the problem!
      }
      boundtimes.push_back(0);
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      shiftFormula.clear(); /// carefully here, since one unit has been added already!
      lit = encode ();
      
      if( lazyEncode )
	for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i+=2 )
	  _eq( shiftFormula.todoEqualities[i], shiftFormula.todoEqualities[i+1] );
      
      assume (lit);
      if( verbose > 2 ) { 
	cerr << "c maxVar["<<k<<"]:     " << nvars << endl;
	cerr << "c assume["<<k<<"]:     " << shiftFormula.currentAssume << endl;
	cerr << "c inputs["<<k<<"]:     " << shiftFormula.inputs.size() << ": "; for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) cerr << " " << shiftFormula.inputs[i]; cerr << endl;
	cerr << "c latches["<<k<<"]:    " << shiftFormula.latch.size() << ": "; for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) cerr << " " << shiftFormula.latch[i]; cerr << endl;
	cerr << "c latchNext["<<k<<"]:  " << shiftFormula.latchNext.size() << ": "; for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ) cerr << " " << shiftFormula.latchNext[i]; cerr << endl;
	cerr << "c thisBad["<<k<<"]:  " << shiftFormula.currentBads.size() << ": "; for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ) cerr << " " << shiftFormula.currentBads[i]; cerr << endl;
	cerr << "c formula["<<k<<"]:    " << endl;
	for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
	  cerr << " " << shiftFormula.formula[i];
	  if( shiftFormula.formula[i] == 0 ) cerr << endl;
	}
      }
      bool foundBad ; // indicate whether solving resulted in 10!
      sharedData.rissFrame = k; // my position
      // if lower bound is already above k, skip this iteration!
      if( sharedData.rissFrame < sharedData.lowerBound ) { foundBad = false; riss->clearAssumptions(); }
      else foundBad = ( sat () == 10); // otherwise, we need to test the current bound!
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      if( printTime ) msg(0,"solved bound %d in %lf seconds", k, boundtimes[k]);
      if( foundBad ) {
	sharedData.rissSolved = 1; // we found the solution!
	sharedData.upperBound = k; // lowerst SAT result
	break;
      }
      if( sharedData.lowerBound < k ) sharedData.lowerBound = k; // update the lower bound!
      unit (-lit); // add as unit, since it is ensured that it cannot be satisfied (previous call)
    }
  } else {
    for (k = 1; k <= maxk; k++) {
      if( sharedData.guesserSolved ) {
	msg(1,"stop solving at frame %d because wg solved the formula", k);
	break; // do not continue, if the guesser thread solved the problem!
      }
      boundtimes.push_back(0);
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      // lit = encode ( false ); // TODO if printing the witness also works without the original structures, this line can be removed!

      const int thisShift = k * shiftDist;
      const int lastShift = (k-1) * shiftDist;
      
      if( verbose > 2 ) {
        cerr << "c shiftDist:           " << shiftDist << endl;
	cerr << "c maxVar["<<k<<"]:     " << nvars << endl;
	cerr << "c assume["<<k<<"]:     " << shiftFormula.currentAssume + thisShift << endl;
	cerr << "c inputs["<<k<<"]:     " << shiftFormula.inputs.size() << ": ";           for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) cerr << " " << varadd(shiftFormula.inputs[i], thisShift); cerr << endl;
	cerr << "c latches["<<k<<"]:    " << shiftFormula.latch.size() << ": ";             for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) cerr << " " << varadd(shiftFormula.latch[i], thisShift); cerr << endl;
	cerr << "c latchNext["<<k<<"]:  " << shiftFormula.latchNext.size() << ": "    ; for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ) cerr << " " << varadd(shiftFormula.latchNext[i], thisShift); cerr << endl;
	cerr << "c thisBad["<<k<<"]:  "   << shiftFormula.currentBads.size() << ": "; for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ) cerr << " " << shiftFormula.currentBads[i]; cerr << endl;
      }
	// shift the formula!
	int thisClauses = 0;
	if( verbose > 2 )cerr << "c formula: " << endl;
	for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
	  const int& cl = shiftFormula.formula[i];
	  if( cl >= -1 && cl <= 1 ) {
	    add (cl,false); // handle constant units, and the end of clause symbol
	    if( cl == 0 ) thisClauses ++;
	  }
	  else add( varadd(cl, thisShift),false); // do not add to the shiftformula again!
	  if( verbose > 2 ) {
	    if( cl != 0 ) cerr << " " << varadd(cl, thisShift) ;
	    else cerr << " " << 0 << endl;
	  }
	}
	
	if( verbose > 2 )cerr << "c equis: " << endl;
	// ensure that latches behave correctly in the next iteration
	for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
	  const int lhs = ( shiftFormula.latch[i] == 1 || shiftFormula.latch[i] == -1 ) ? shiftFormula.latch[i] : varadd(shiftFormula.latch[i], thisShift);
	  const int rhs = ( shiftFormula.latchNext[i] == 1 || shiftFormula.latchNext[i] == -1 ) ? shiftFormula.latchNext[i] : varadd(shiftFormula.latchNext[i], lastShift);
	  if( verbose > 2 ) cerr << " " << lhs  << " == " <<  rhs << endl;
	  _eq( lhs, rhs, false );
	}
      

      lit = shiftFormula.currentAssume + thisShift;
      msg(3,"solve iteration %d with assumption literal %d and %d shifted clauses\n",k,lit,thisClauses);
      assume (lit);
      bool foundBad ; // indicate whether solving resulted in 10!
      sharedData.rissFrame = k; // my position
      // if lower bound is already above k, skip this iteration!
      if( sharedData.rissFrame < sharedData.lowerBound ) { foundBad = false; riss->clearAssumptions(); }
      else foundBad = ( sat () == 10); // otherwise, we need to test the current bound!
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      if( printTime ) msg(0,"solved bound %d in %lf seconds", k, boundtimes[k]);
      if( foundBad ) {
	sharedData.rissSolved = 1; // we found the solution!
	sharedData.upperBound = k; // lowerst SAT result
	break;
      }
      if( sharedData.lowerBound < k ) sharedData.lowerBound = k; // update the lower bound!
      unit (-lit,false); // add as unit, since it is ensured that it cannot be satisfied (previous call)
      
    }
  }
  
  // jump here if a solution has been found!
finishedSolving:;
  

  if( wildGuesser ) {
    if( sharedData.rissSolved && sharedData.guesser != 0 ) sharedData.guesser->interrupt();
    void* status;
    pthread_join( sharedData.guesserThread, &status); // wait for the other thread
    
    if( !sharedData.rissSolved ) {
      assert( sharedData.guesserSolved && "one of the two solver has have solved the instance" );
      Riss::IncSolver* tmp = riss;
      riss = sharedData.guesser; // if the other solver solved the instance, swap solvers!
      sharedData.guesser = tmp;
    }
  }
  if( sharedData.guesserSolved && !sharedData.rissSolved ) {
    k = sharedData.guesserFrame; // set right bound
    msg(1,"formula finally solved by results of wg");
  }

  /**
   *  PRINT MODEL/WITNESS, if necessary
   */
  cerr << "[aigbmc] pp time summary: aig: " << ppAigTime << " cnf: " << ppCNFtime << endl;
  if( printTime ) {
    cerr << "[aigbmc] solve time summary: ";
    for( int i = 0 ; i < boundtimes.size(); ++ i ) cerr << " " << boundtimes[i];
    cerr << endl;
  }
  
  if (quiet) goto DONE;
  if (k <= maxk) {	
    printf ("1\n");
    fflush (stdout);
    if (nowitness) goto DONE;
    found = 0;
    
    if( useShift ) {
      const int thisShift = k * shiftDist;
      const int lastShift = (k-1) * shiftDist;
      // print the line that states the bad outputs, that have been reached
      if( dontFreezeBads ) { // if inputs have not been frozen, we need to recover them per frame before printing their value!
	  const int frameShift = k * shiftDist; // want to access the very last frame
	  Riss::vec<lbool> frameModel ( shiftFormula.afterPPmaxVar, l_False ); // TODO put this vector higher!
	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + frameShift); // treat very first value always special, because its not moved!
	    if( verbose > 3 ) cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << v << endl;
	    frameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  outerPreprocessor->extendModel( frameModel ); // recover the original model for this frame, and also the inputs
	  if( verbose > 3 ) { // print extra info for debugging
	    cerr << "c after extendModel (model size: " << frameModel.size() << ")" << endl;
	    for( int j = 1 ; j <= shiftFormula.initialMaxVar; ++ j ) { // get the right part of the actual model
	      cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << (frameModel[j-1] == l_True ? 1 : -1) << endl;
	    }
	    for (j = 0; j < shiftFormula.currentBads.size(); j++) { // show numbers of literals that will be accessed
	      int tlit = shiftFormula.currentBads[j];
	      cerr << "c bad[" << j << "] = " << tlit << endl;
	    }
	  }
	  for (j = 0; j < shiftFormula.currentBads.size(); j++) { // print the actual values!
	    int tlit = shiftFormula.currentBads[j];
	    int v = 0;
	    if(tlit>0 ) {
	      if( frameModel[ tlit -1 ] == l_True) v = 1;
	    }
	    else if(frameModel[ -tlit -1 ] == l_False ) v=1;
	    if( v = 1 ) {
	      printf ("b%d", j), found++;
	    }
	    assert( (shiftFormula.currentBads.size() > 1 || found > 0) && "only one output -> find immediately" );
	  }
	  assert (found && "there has to be found something!");
      } else {
	for (i = 0; i < model->num_bad; i++) {
	  int thisBad = shiftFormula.currentBads[i] + thisShift;
	  if (deref ( thisBad  ) > 0)
	    printf ("b%d", i), found++;
	}
	assert (found && "there has to be found something!");
      }
      assert ((model->num_bad || model->num_justice) && "there has to be some property that has been found!" );
      nl (); // print new line, to give the initialization of the latches
      
      /// nothing special to do with latches, because those are always frozen, and thus never touched
      for (i = 0; i < model->num_latches; i++){
	if( initialLatchNum > 0 &&  deref(shiftFormula.latch[i]) != -1 ) wrn("latch[%d],var %d is not initialized to 0, but scorr has been used during simplifying the circuit",i,shiftFormula.latch[i]);
	print ( shiftFormula.latch[i] ); // this function prints 0 or 1 depending on the current assignment of this variable!
      }
      for( ; i < initialLatchNum; ++ i ) {
	printV(-1); // ABC assumes all latches to be initialized to 0!
      }
      nl (); // print new line to give the sequence of inputs
      
      if( dontFreezeInput && verbose > 3 ) {
	cerr << "c frame offset: 1" << endl
	     << "c solve frame size: " << shiftFormula.afterPPmaxVar - 1 << endl
	     << "c before pp frame size: " << shiftFormula.initialMaxVar - 1 << endl;
	riss->printModel();
      }
      
      for (i = 0; i <= k; i++) { // for each step give the inputs that lead to the bad state
	if( dontFreezeInput ) { // if inputs have not been frozen, we need to recover them per frame before printing their value!
	  const int frameShift = i * shiftDist;
	  Riss::vec<lbool> frameModel ( shiftFormula.afterPPmaxVar, l_False ); // TODO put this vector higher!

	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + frameShift); // treat very first value always special, because its not moved!
	    if( verbose > 3 ) cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << v << endl;
	    frameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  
	  outerPreprocessor->extendModel( frameModel ); // recover the original model for this frame, and also the inputs

	  if( verbose > 3 ) { // print extra info for debugging
	    cerr << "c after extendModel (model size: " << frameModel.size() << ")" << endl;
	    for( int j = 1 ; j <= shiftFormula.initialMaxVar; ++ j ) { // get the right part of the actual model
	      cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << (frameModel[j-1] == l_True ? 1 : -1) << endl;
	    }
	    for (j = 0; j < shiftFormula.inputs.size(); j++) { // show numbers of literals that will be accessed
	      int tlit = shiftFormula.inputs[j];
	      cerr << "c input[" << j << "] = " << tlit << endl;
	    }
	  }
	  
	  for (j = 0; j < shiftFormula.inputs.size(); j++) { // print the actual values!
	    int tlit = shiftFormula.inputs[j];
	    if(tlit>0) printV ( frameModel[ tlit -1 ] == l_False ? -1 : 1  ); // print input j of iteration i
	    else  printV ( frameModel[ -tlit -1 ] == l_True ? -1 : 1  ); // or complementary literal
	  }
	  
	} else { // if inputs have been frozen, their is no need to treat them special
	  for (j = 0; j < model->num_inputs; j++)
	    print ( shiftFormula.inputs[j] + shiftDist * i ); // print input j of iteration i
	}
	nl ();
      }
      printf (".\n"); // indicate end of witness
      fflush (stdout);
    } else {
      // "old" way of printing the witness with all structures from the tool
      assert (nstates == k + 1);
      for (i = 0; i < model->num_bad; i++) {
	if (deref (states[k].bad[i]) > 0)
	  printf ("b%d", i), found++;
	 // cerr << "c check bad state for: " << states[k].bad[i] << endl;
      }
      assert (found);
      assert (model->num_bad || model->num_justice);
      nl ();
      for (i = 0; i < model->num_latches; i++){
	print (states[0].latches[i].lit);
      	if( initialLatchNum > 0 &&  deref(states[0].latches[i].lit) != -1 ) wrn("latch[%d],var %d is not initialized to 0, but scorr has been used during simplifying the circuit",i,shiftFormula.latch[i]);
      }
      for( ; i < initialLatchNum; ++ i ) { // take care of missing latches
	printV(-1); // ABC assumes all latches to be initialized to 0!
      }
      nl ();
      for (i = 0; i <= k; i++) {
	for (j = 0; j < model->num_inputs; j++)
	  print (states[i].inputs[j]);
	nl ();
      }
      printf (".\n");
      fflush (stdout);
    } 
  }
  else printf ("2\n"), fflush (stdout);
DONE:
 
//Result for ParamILS: <solved>, <runtime>, <runlength>, <quality>, <seed>,
//<additional rundata>

  // print result for Tuner
  if( tuning ) {
    std::cout << "Result for ParamILS: " 
    << ((k <= maxk) ? "SAT" : "ABORT") << ", " // if the limit we are searching for is not high enough, this is a reason to abort!
    << cpuTime() - totalTime << ", "
    << k << ", "
    << 10000.0 / (double)(k+1) << ", " // avoid dividing by 0!
    << tuneSeed 
    << std::endl;
    std::cout.flush();
    std::cerr << "Result for ParamILS: " 
    << ((k <= maxk) ? "SAT" : "ABORT") << ", " // if the limit we are searching for is not high enough, this is a reason to abort!
    << cpuTime() - totalTime << ", "
    << k << ", "
    << 10000.0 / (double)(k+1) << ", " // avoid dividing by 0!
    << tuneSeed
    << std::endl;
    std::cerr.flush();
  }
  
  reset ();
  
  return 0;
}
