/***************************************************************************
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

#include "aiger.h"

extern "C" { // we are compiling with G++, however, picosat is C code
  #include "../picosat/picosat.h"
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
#include <iostream>

/**
 *  For using riss, CP3, ...
 */
#include "../core/BMCwrapper.h"
#include "../coprocessor-src/Coprocessor.h"

static aiger * model = 0;
static const char * name = 0;
static PicoSAT * picosat = 0;
static Minisat::IncSolver* riss = 0;

// structures that are necessary for preprocessing, and if x or y parameter are set also for postprocessing
static Minisat::Solver* ppSolver;
static Minisat::IncSolver* incsolver;
static Coprocessor::Preprocessor* preprocessor;
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


static State * states = 0;
static int nstates = 0, szstates = 0, * join = 0;
static char * bad = 0, * justice = 0;

static int verbose = 0, move = 0, quiet = 0, nowitness = 0, lazyEncode = 0,useShift=0, 
  useRiss = 0, useCP3 = 0, denseVariables = 0, dontFreezeInput = 0, dontFreezeBads = 0; // options
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
  va_list ap;
  fputs ("[aigbmc] WARNING ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

static void add (int lit, bool addToShift = true) { 
  if( addToShift ) shiftFormula.formula.push_back( lit ); // write into formula
  if( useRiss ) riss->add( lit );
  else picosat_add (picosat, lit);
}

static void assume (int lit) { 
  if( useRiss ) riss->assume(lit);
  else picosat_assume (picosat, lit);
}

static int sat () { 
  msg (2, "solve time with SAT solver: %d", nstates);
  int ret;
  if( useRiss ) ret = riss->sat();
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
  if( useRiss ) riss = new Minisat::IncSolver(argc,argv);
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
"-h       print this comm_and line option summary\n"
"--help   prints the help of the SAT solver, if riss is used\n"
"-bmc_v    increase verbose level\n"
"-bmc_m    use outputs as bad state constraint\n"
"-bmc_n    do not print witness\n"
"-bmc_q    be quite (impies '-n')\n"
"-bmc_c    check whether there are properties (return 1, otherwise 0)\n"
"-bmc_l    use lazy coding\n"
"-bmc_s    use shifting instead of reencoding (implies lazy encoding)\n"
"-bmc_r    use riss as SAT solver\n"
"-bmc_p    use CP3 as simplifier (implies using riss and shifting)\n"
"-bmc_d    dense after preprocessing (implies using riss and CP3)\n"
"-bmc_x    modify preprocess (implies using riss and CP3)\n"
"-bmc_y    modify preprocess (implies using riss and CP3)\n"
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

int main (int argc, char ** argv) {
  int i=0, j=0, k=0, maxk=0, lit=0, found=0;
  
  bool noProperties = false, checkProperties = false; // are there any properties inside the circuit?
  
  const char * err;
  maxk = -1;
  for (i = 1; i < argc; i++) {
    if ( !strcmp (argv[i], "-h") || !strcmp (argv[i], "--help")) {
      // usage for BMC
      printf ("%s", usage);
      // give SAT solver the chance to print its help
      init( argc,argv );
      reset();
      // terminate program
      exit (0);
    } else if (!strcmp (argv[i], "-bmc_v")) verbose++;
    else if (!strcmp (argv[i], "-bmc_m")) move = 1;
    else if (!strcmp (argv[i], "-bmc_n")) nowitness = 1;
    else if (!strcmp (argv[i], "-bmc_q")) quiet = 1;
    else if (!strcmp (argv[i], "-bmc_l")) lazyEncode = 1;
    else if (!strcmp (argv[i], "-bmc_s")) { useShift = 1; lazyEncode = 1;} // strong dependency!
    else if (!strcmp (argv[i], "-bmc_r")) useRiss = 1;
    else if (!strcmp (argv[i], "-bmc_p")) { useCP3=1,useRiss = 1; }
    else if (!strcmp (argv[i], "-bmc_d")) { useCP3=1,useRiss = 1;denseVariables=1; }
    else if (!strcmp (argv[i], "-bmc_x")) { useCP3=1,useRiss = 1;dontFreezeInput=1; }
    else if (!strcmp (argv[i], "-bmc_y")) { useCP3=1,useRiss = 1;dontFreezeBads=1; }
    else if (!strcmp (argv[i], "-bmc_c")) checkProperties = true;
    // the other parameters might be for the SAT solver
    else if (argv[i][0] == '-') {
      wrn( "unknown bmc parameter '%s'", argv[i] );
      // die ("invalid comm_and line option '%s'", argv[i]);
    } else if (name && maxk >= 0) {
      wrn( "unknown bmc argument '%s'", argv[i] );
      // die ("unexpected argument '%s'", argv[i]);
    } else if (maxk < 0 && isnum (argv[i]) && maxk == 0) maxk = atoi (argv[i]);
    else if( !name) name = argv[i];
  }
  if (maxk < 0) maxk = 10;
  msg (1, "aigbmc bounded model checker");
  msg (1, "maxk = %d", maxk);

  // init solvers, is necessary with command line parameters
  init (argc,argv);
  
  msg (1, "reading from '%s'", name ? name : "<stdin>");
  if (name) err = aiger_open_and_read_from_file (model, name);
  else err = aiger_read_from_file (model, stdin), name = "<stdin>";
  if (err) die ("parse error reading '%s': %s", name, err);
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
  
  lit = encode ();
  shiftFormula.currentAssume = lit;
  shiftFormula.initialMaxVar = nvars;
  shiftFormula.afterPPmaxVar = nvars;
  // TODO: here, simplification and compression could be performed!

  if( useCP3 ) {
    
    cp3config = new Coprocessor::CP3Config;
    // add more options here?
    cp3config->opt_enabled = true; cp3config->opt_verbose=0;
    cp3config->opt_bve=true; cp3config->opt_bve_verbose=0; 
    cp3config->opt_printStats = true;
    if( denseVariables ) { // enable options for CP3 for densing variables
      cp3config->opt_dense = true; cp3config->opt_dense_store_forward = 1; 
    }
    Minisat::CoreConfig config = riss->getConfig();
    ppSolver = new Minisat::Solver (config);
    incsolver = new Minisat::IncSolver (ppSolver,config);
    preprocessor = new Coprocessor::Preprocessor ( ppSolver, *cp3config );
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) incsolver->add( shiftFormula.formula[i] );
    
    cerr << "c call freeze variables ... " << endl;
    preprocessor->freezeExtern( 1 ); // do not remove the unit (during dense!)
    if( !dontFreezeInput ) for( int i = 0 ; i < shiftFormula.inputs.size(); ++i ) preprocessor->freezeExtern( shiftFormula.inputs[i] );
    for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) preprocessor->freezeExtern( shiftFormula.latch[i] );
    for( int i = 0 ; i < shiftFormula.latchNext.size(); ++i ) preprocessor->freezeExtern( shiftFormula.latchNext[i] );
    preprocessor->freezeExtern( shiftFormula.currentAssume < 0 ? -shiftFormula.currentAssume : shiftFormula.currentAssume );
    if( !dontFreezeBads ) for( int i = 0 ; i < shiftFormula.currentBads.size(); ++i ) preprocessor->freezeExtern( shiftFormula.currentBads[i] );
    cerr << "c call preprocess ... " << endl;
    preprocessor->preprocess();
    // test whether readding works without modifying the formula
    cerr << "c before formula: " << endl;
    riss->printFormula();
    // alles gut!
    riss->clearSolver();
    preprocessor->dumpFormula( shiftFormula.formula ); // actively changing the formula!
    cerr << "c add formula to riss" << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) riss->add( shiftFormula.formula[i] );
    // TODO: reduce maxVar according to shrink! ... thus, shift distance will be adopted
    shiftFormula.afterPPmaxVar = ppSolver->nVars();
    cerr << "c after variables: " << shiftFormula.afterPPmaxVar << endl;
    // riss->setMaxVar( nvars );
    if( denseVariables ) { // refresh knowledge about the extra literals (input,output/bad,latches,assume)
      int tmpLit = preprocessor->giveNewLit( shiftFormula.currentAssume );
      cerr << "move assume from " << shiftFormula.currentAssume << " to " << tmpLit << endl;
      shiftFormula.currentAssume = tmpLit;
      if( !dontFreezeInput ) { // no need to shift input bits, if they will be restored properly after a solution has been found
	for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) {
	  tmpLit = preprocessor->giveNewLit( shiftFormula.inputs[i] );
	  cerr << "move input from " <<  shiftFormula.inputs[i]   << " to " << tmpLit << endl;
	  shiftFormula.inputs[i] = tmpLit;
	}
      }
      for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ){
	tmpLit = preprocessor->giveNewLit( shiftFormula.latch[i] );
	cerr << "move latch from " << shiftFormula.latch[i] << " to " << tmpLit << endl;
	shiftFormula.latch[i] = tmpLit;
      }
      for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ){
	tmpLit = preprocessor->giveNewLit(shiftFormula.latchNext[i]);
	cerr << "move latchN from " << shiftFormula.latchNext[i] << " to " << tmpLit << endl;
	shiftFormula.latchNext[i] = tmpLit;
      }
      if( !dontFreezeBads ) { // dont freeze bad stats, if they are restored after a solution has been found
	for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ){
	  tmpLit = preprocessor->giveNewLit( shiftFormula.currentBads[i] );
	  cerr << "move bad from " <<   shiftFormula.currentBads[i]  << " to " << tmpLit << endl;
	  shiftFormula.currentBads[i] = tmpLit;
	}
      }
      // also shift equivalences that are not known by the solver and preprocessor yet (but are present during search)
      for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i++ ) {
	tmpLit = preprocessor->giveNewLit( shiftFormula.todoEqualities[i] );
	cerr << "move eq from " <<   shiftFormula.todoEqualities[i]  << " to " << tmpLit << endl;
	shiftFormula.todoEqualities[i] = tmpLit;
      }
    }
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
  // TODO after compression, update all the variables/literals lists in the shiftformula structure!
  
  lit = shiftFormula.currentAssume; // literal might have changed!
  int shiftDist = shiftFormula.afterPPmaxVar - 1;  // shift depends on the variables that are actually in the formula that is used for solving
  
  if( lazyEncode )
    for( int i = 0 ; i < shiftFormula.todoEqualities.size(); i+=2 )
      _eq( shiftFormula.todoEqualities[i], shiftFormula.todoEqualities[i+1], false );
  
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
  if (sat () == 10) goto finishedSolving;
  unit (-lit, false); // do not add this unit to the shift formula!
  

  
  msg (2, "re-encode model");
  if( !useShift ) { // old or new method?
    for (k = 1; k <= maxk; k++) {
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
      if (sat () == 10) break;
      unit (-lit); // add as unit, since it is ensured that it cannot be satisfied (previous call)
    }
  } else {
    for (k = 1; k <= maxk; k++) {
      lit = encode ( false ); // TODO if printing the witness also works without the original structures, this line can be removed!

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
      if (sat () == 10) break;
      unit (-lit,false); // add as unit, since it is ensured that it cannot be satisfied (previous call)
    }
  }
  
  // jump here if a solution has been found!
finishedSolving:;
  
  /**
   *  PRINT MODEL/WITNESS, if necessary
   */
  
  
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
	  Minisat::vec<lbool> frameModel ( shiftFormula.afterPPmaxVar, l_False ); // TODO put this vector higher!
	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + frameShift); // treat very first value always special, because its not moved!
	    if( verbose > 3 ) cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << v << endl;
	    frameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  preprocessor->extendModel( frameModel ); // recover the original model for this frame, and also the inputs
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
	    if(tlit>0 && frameModel[ tlit -1 ] == l_True) v = 1;
	    else if(frameModel[ -tlit -1 ] == l_False ) v=1;
	    if( v = 1 ) {
	      printf ("b%d", j), found++;
	    }
	  }
      } else {
	for (i = 0; i < model->num_bad; i++) {
	  int thisBad = shiftFormula.currentBads[i] + thisShift;
	  if (deref ( thisBad  ) > 0)
	    printf ("b%d", i), found++;
	}
      }
      assert (found && "there has to be found something!");
      assert ((model->num_bad || model->num_justice) && "there has to be some property that has been found!" );
      nl (); // print new line, to give the initialization of the latches
      
      /// nothing special to do with latches, because those are always frozen, and thus never touched
      for (i = 0; i < model->num_latches; i++)
	print ( shiftFormula.latch[i] ); // this function prints 0 or 1 depending on the current assignment of this variable!
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
	  Minisat::vec<lbool> frameModel ( shiftFormula.afterPPmaxVar, l_False ); // TODO put this vector higher!

	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + frameShift); // treat very first value always special, because its not moved!
	    if( verbose > 3 ) cerr << "c var " << (j==1 ? 1 : j + frameShift) << " with value " << v << endl;
	    frameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  
	  preprocessor->extendModel( frameModel ); // recover the original model for this frame, and also the inputs

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
	 cerr << "c check bad state for: " << states[k].bad[i] << endl;
      }
      assert (found);
      assert (model->num_bad || model->num_justice);
      nl ();
      for (i = 0; i < model->num_latches; i++)
	print (states[0].latches[i].lit);
      
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
  reset ();
  return 0;
}
