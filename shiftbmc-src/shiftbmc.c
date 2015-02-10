/***************************************************************************
Copyright (c) 2014, Norbert Manthey

Copyright (c) 2011, Armin Biere, Johannes Kepler University, Austria.


The above copyright notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
***************************************************************************/

#include "shiftbmc.h"

static aiger * model = 0;
static const char * name = 0;

// these two SAT solvers can be used
static PicoSAT * picosat = 0;
static void* riss = 0, *priss = 0; // handles to SAT solver libraries

// structures that are necessary for preprocessing, and if x or y parameter are set also for postprocessing
static void* outerPreprocessor, *innerPreprocessor;

static unsigned firstlatchidx = 0, first_andidx = 0;

static int falseLit = -1; /// literal that will be always false, so that it can be used in the initialization of literals

static double totalTime = 0;

static State * states = 0;
static int nstates = 0, szstates = 0, * join = 0;
static char * bad = 0, * justice = 0;

static int maxk=0; // upper bound for search
static int verbose = 0, move = 0, processNoProperties = 0 ,quiet = 0, nowitness = 0, nowarnings = 0, lazyEncode = 0,useShift=0, // options
  useRiss = 0, usePriss = 0, useCP3 = 0, denseVariables = 0, dontFreezeInput = 0, dontFreezeBads = 0, printTime=0, checkInitialBad = 0, abcDir = 0, stats_only = 0, debugWitness = 0;
static char* rissPresetConf = 0;
static char* innerPPConf = 0, *outerPPConf = 0;
static int wildGuesser = 0, wildGuessStart = 10, wildGuessInc = 2, wildGuessIncInc = 1; // guess sequence: 10, 30, 60, ...
static int tuning = 0, tuneSeed = 0;
static bool checkProperties = false;
static std::string useABC = "";
static std::string abcCommand = "";
static char* dumpPPformula = 0;
static int nvars = 0;
static int mergeFrames = 1; // number of time frames that should be merged together
static int frameConflictBudget = -1 ; // number of conflicts that are allowed to be used per time frame
static int doNotAllowToSmallCircuits = 0;	// useful when tool is analyzed with a delta debugger
static float frameConflictsInc = 4;	// factor how initialFrameConflicts grow when a frame cannot be solved
static int simplifiedCNF = 0; // count how often the CNF has been simplified already
static std::string formulaFile; // write the formula into a file instead of solving the formula (only after preprocessing)
static int nonZeroInitialized = 0;	// there are uninitilized latched -> do not use SCORR as preprocessing method

/// base structure for performing the shift operation with the (simplified) formula
static ShiftFormula shiftFormula;

static void die (const char *fmt, ...) {
  va_list ap;
  fputs ("*** [shift-bmc] ", stderr);
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
  exit (37);
}

static void msg (int level, const char *fmt, ...) {
  va_list ap;
  if (quiet || verbose < level) return;
  fputs ("[shift-bmc] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}

static void wrn (const char *fmt, ...) {
  if( nowarnings ) return; // print only if enabled
  va_list ap;
  fputs ("[shift-bmc] WARNING ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
}


static void add (int lit, bool addToShift = true) { 
  if( addToShift ) { 
    shiftFormula.formula.push_back( lit ); // write into formula
    if( lit == 0) shiftFormula.clauses ++;
  }
  if( riss != 0 ) riss_add( riss, lit );
  else if( priss != 0 ) priss_add( priss, lit );
  else picosat_add (picosat, lit);
//  fprintf(stderr, "%d ", lit );
//  if( lit == 0 ) fprintf(stderr,"\n");
}

static void assume (int lit) { 
//  fprintf(stderr, "assume literal: %d ", lit );
  if( riss != 0 ) riss_assume( riss, lit );
  else if( priss != 0 ) priss_assume( priss, lit );
  else picosat_assume (picosat, lit);
}

/** solve with resource limit */
static int sat ( int limit = -1 ) { 
  // msg (2, "solve time with SAT solver: %d", nstates);
  int ret;
  if( riss != 0 ) ret = riss_sat( riss, limit );
  else if( priss != 0 ) ret = priss_sat( priss, limit );
  else ret = picosat_sat (picosat, limit);
  msg (2, "return code of SAT solver: %d", ret);
  return ret;
}

static int deref (int lit) { 
  int ret;
  if( riss != 0 ) ret = riss_deref( riss, lit );
  else if( priss != 0 ) ret = priss_deref(priss, lit );
  else ret = picosat_deref (picosat, lit);
  msg(3,"deref lit %d to value %d", lit, ret);
  return ret;
}

/** initialize everything, give solvers the possibility to read the parameters! */
static void init ( int& argc, char **& argv ) {
  msg (2, "init SAT solver");
  if( useRiss ) riss = riss_init(rissPresetConf);
  else if ( usePriss > 0 ) {
    cerr << "c init priss with " << usePriss << " threads" << endl ;
    priss = priss_init( usePriss, "BMC" );
  }
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
  if( riss != 0 ) riss_destroy( riss );
  else if ( priss != 0 ) priss_destroy( priss );
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
	shiftFormula.loopEqualities.push_back(res->latches[i].lit);
	shiftFormula.loopEqualities.push_back(prev->latches[i].next);
	if( actuallyEnode ) shiftFormula.latch[i] = res->latches[i].lit;
      } else {
	res->latches[i].lit = prev->latches[i].next; /// combine the literals of this iteration with the latch literals of last iteration
	if( actuallyEnode ) shiftFormula.latch[i] = shiftFormula.latchNext[i]; /// shift latches!
      }
      if( actuallyEnode ) msg(2,"set latch %d to lit %d",i,shiftFormula.latch[i]);
    }
  } else {
    prev = 0;
    int initpos=0,initneg=0,initundef=0;
    for (i = 0; i < model->num_latches; i++) {
      symbol = model->latches + i;
      reset = symbol->reset;
      msg(2, "reset for latch: %d nr: %d lit: %d \n", reset,i, symbol->lit );
      if (!reset) { lit = -1;initneg++;} // should be false lit -> use equivalence to false lit !
      else if (reset == 1) { lit = 1; initpos ++; }  // should be true-lit -> use equivalence to !falseLit !
      else {
	initundef ++;
	if (reset != symbol->lit)
	  die ("can only handle constant or uninitialized reset logic");
	lit = newvar ();
      }
      
      
      if( lazyEncode && ( lit == 1 || lit == -1 ) ) {
	int realLit = newvar();
	// add Equivalence to initialization of very first formula!!
	if( actuallyEnode ) {
	  if( lit == 1 ) {
	    shiftFormula.initEqualities.push_back(-falseLit);
	    shiftFormula.initEqualities.push_back(realLit);
	  } else {
	    shiftFormula.initEqualities.push_back(falseLit);
	    shiftFormula.initEqualities.push_back(realLit);
	  }
	}
	msg(3,"connected latch %d with the literal %d and with the value %d",i,realLit,lit);
	lit = realLit;
      }
      
      
      res->latches[i].lit = lit;
      msg(2,"set latch %d to lit %d",i,lit);
      if( actuallyEnode ) shiftFormula.latch[i] = lit;
    }
    msg(1,"initialized latchs:  %d neg, %d pos, %d undef\n",initneg,initpos,initundef);
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
    exit (38);
  }

  /// handle fairness criteria
  static bool didFairness = false;
  if (model->num_justice && model->num_fairness) {
    assert( false && "cannot handle justice constraints yet" );
    exit (39);    
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


static void print (int lit) {
  int val = deref (lit), ch;
  if (val < 0) ch = '0';
  else if (val > 0) ch = '1';
  else ch = 'x';
  putc (ch, stdout);
}


int parseOptions(int argc, char ** argv) 
{
  for (int i = 1; i < argc; i++) {
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
    
    else if (!strcmp (argv[i], "-bmc_k")) {
      if( ++i < argc && isnum(argv[i]) ) maxk = atoi( argv[i] );
    }
    else if (!strcmp (argv[i], "-bmc_m")) move = 1;
    else if (!strcmp (argv[i], "-bmc_np")) processNoProperties = 1;
    else if (!strcmp (argv[i], "-bmc_n")) nowitness = 1;
    else if (!strcmp (argv[i], "-bmc_q")) quiet = 1;
    else if (!strcmp (argv[i], "-bmc_nw")) nowarnings = 1;
    else if (!strcmp (argv[i], "-bmc_dd")) doNotAllowToSmallCircuits = 1;
    else if (!strcmp (argv[i], "-bmc_tune")) {
      tuning = 1;
      if( ++i < argc && isnum(argv[i]) ) tuneSeed = atoi( argv[i] );
    }
    else if (!strcmp (argv[i], "-bmc_l")) lazyEncode = 1;
    else if (!strcmp (argv[i], "-bmc_s")) { useShift = 1; lazyEncode = 1;} // strong dependency!
    else if (!strcmp (argv[i], "-bmc_r")) { useRiss = 1; usePriss = 0; }
    else if (!strcmp (argv[i], "-bmc_rpc")) { // use preset configuration for riss
      useRiss = 1;
      if( ++i < argc ) rissPresetConf = argv[i];
    } 
    else if (!strcmp (argv[i], "-bmc_pr")) { 
      usePriss = 1; 
      if( ++i < argc ) usePriss = atoi( argv[i] );
      if( usePriss == 0 ) usePriss = 1;
      msg(0,"set parallel solver with %d threads",usePriss);
      useRiss = 0; 
    }
    else if (!strcmp (argv[i], "-bmc_p")) { 
      useCP3=1,useShift = 1; lazyEncode = 1; 
      ++ i;
      if( i < argc ) { 
	outerPPConf = argv[i];
	msg(1,"pp outer frame with config %s", innerPPConf );
      }
    }
#ifndef TEST_BINARY
    else if (!strcmp (argv[i], "-bmc_dbgp")) { 
      ++ i;
      if( i < argc ) {
	dumpPPformula = argv[i];
	msg(0,"set debug PP file to %s",dumpPPformula);
      }
    }
    else if (!strcmp (argv[i], "-bmc_dbgW")) { debugWitness=1; }
#endif
    else if (!strcmp (argv[i], "-bmc_d")) { useCP3=1,  denseVariables=1;  useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_x")) { useCP3=1,  dontFreezeInput=1; useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_y")) { useCP3=1,  dontFreezeBads=1;  useShift=1; lazyEncode=1; }
    else if (!strcmp (argv[i], "-bmc_z")) {
      ++ i;
      if( i < argc && isnum(argv[i]) ) { 
	checkInitialBad = atoi( argv[i] );
	msg(1,"set bad state analysis resource limit to %d", checkInitialBad );
      }
    }
    else if (!strcmp (argv[i], "-bmc_mf")) {
      ++ i;
      if( i < argc && isnum(argv[i]) ) { 
	mergeFrames = atoi( argv[i] );
	msg(1,"merge multiple frames, namely %d", mergeFrames );
      }
    }
    else if (!strcmp (argv[i], "-bmc_mp")) {
      ++ i;
      if( i < argc ) { 
	innerPPConf = argv[i];
	msg(1,"pp inner frame with config %s", innerPPConf );
      }
    }
    else if (!strcmp (argv[i], "-bmc_fc")) {
      ++ i;
      if( i < argc && isnum(argv[i]) ) { 
	frameConflictBudget = atoi( argv[i] );
	msg(1,"initial conflicts per frame: %d", frameConflictBudget );
      }
    }
    else if (!strcmp (argv[i], "-bmc_fi")) {
      ++ i;
      if( i < argc && isnum(argv[i]) ) { 
	frameConflictsInc = atof( argv[i] );
	msg(1,"conflicts increase in case of failure: %f", frameConflictsInc );
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
      wildGuesser = 1;  useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_ws")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessStart = atoi( argv[i] );
      wildGuesser = 1;  useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_wi")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessInc = atoi( argv[i] );
      wildGuesser = 1;  useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_wii")) {
      if( ++i < argc && isnum(argv[i]) ) wildGuessIncInc = atoi( argv[i] );
      wildGuesser = 1;  useShift = 1; lazyEncode = 1;
    }
    else if (!strcmp (argv[i], "-bmc_c")) checkProperties = true;
    else if (!strcmp (argv[i], "-bmc_so")) stats_only = true;
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
    else if (!strcmp (argv[i], "-bmc_outCNF")) {
      ++i;
      if( i < argc ) formulaFile = std::string(argv[i]);
      else wrn("not enough parameters to set the output formula");
    }    
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
}


int ABC_simplify(double& ppAigTime, int& initialLatchNum)
{
// #define USE_ABC
#ifdef USE_ABC

//
// possible commands for ABC that have been recommended:
//
// drwsat
//
// dc2
// &get; &syn2; &put
// &get; &syn3; &put
// &get; &syn4; &put
//
// scorr
// &get; &scorr; &put
//


  // simplification with ABC?
  if( useABC.size() > 0 ) { // there is a file or location
    MethodTimer aigTimer ( &ppAigTime );
    if(!name) { // reject reading from stdin
      wrn("will not read from stdin, since ABC ");  
      exit(40);
    }
    // if the scorr command is part of the command chain, the initial number of latches has to be set
    bool foundScorr = false;
    int fm,fi,fl,fo,fa;
    if( true ) { // always extract the info about the original file!
      // parse the aig file and read the number of latches!
      std::ifstream myfile;
      myfile.open (name);
      if(!myfile) { wrn("error in opening file %s",name); exit (41); }
      std::string sstr;
      while( sstr != "aag" && sstr != "aig") {
				if(!myfile) break; // reached end of file?
				myfile >> sstr;
      }
      if( myfile ) {
	myfile >> fm >> fi >> fl >> fo >> fa;
	msg(0,"initial MILOA: %d %d %d %d %d  -- set number of initial latches to %d - ",fm,fi,fl,fo,fa,fl);
      } else {
	wrn("could not extract the number of latches from %s",name);
      }
      myfile.close();
    }
    

    // setup the command line for abc
    if( abcCommand.size() == 0 || abcCommand == "AUTO" ) {
      const int maxABCTime = 25; // alternative is 10
      msg(0,"select ABC commands to take at most %d seconds (on known circuits)", maxABCTime);
      // set a technique NOT, if both bounds are reached (hence, if one is not reached, set the best technique!)
      if( maxABCTime == 10 ) {
	     if( fa <=  10000  || fl <=    90 ) abcCommand = "dc2:dc2:scorr";
	else if( fa <=  20000  || fl <=   100 ) abcCommand = "dc2:scorr";
	else if( fa <=  30000  || fl <=   180 ) abcCommand = "dc2:&get; &scorr; &put";
	else if( fa <=  40000  || fl <=  1500 ) abcCommand = "dc2:dc2";
	else if( fa <= 100000  || fl <=  2000 ) abcCommand = "dc2";
	else if( fa <= 250000  || fl <= 15000 ) abcCommand = "&get;&syn2;&put:&get;&syn3;&put:&get;&syn4;&put";
	else { 
	  abcCommand = ""; // instsance is too large to select an automatic command!
	  msg(0,"combination of latches and AND-gates is assumed to consume too much AIG simplification time");
	}	
      } else if (maxABCTime == 25 ) { 
	     if( fa <=  11000  || fl <=   180 ) abcCommand = "dc2:dc2:scorr";
	else if( fa <=  30000  || fl <=   180 ) abcCommand = "dc2:scorr";
	else if( fa <=  90000  || fl <=  3500 ) abcCommand = "dc2:&get; &scorr; &put";
	else if( fa <=  75000  || fl <=  7000 ) abcCommand = "&get;&syn2;&put:&get;&syn3;&put:&get;&syn4;&put:scorr";
	else if( fa <= 100000  || fl <=  7000 ) abcCommand = "&get;&syn2;&put:&get;&syn3;&put:&get;&syn4;&put:&get; &scorr; &put";
	else if( fa <= 115000  || fl <= 10000 ) abcCommand = "dc2:dc2";
	else if( fa <= 125000  || fl <= 15000 ) abcCommand = "dc2";
	else if( fa <= 400000  || fl <= 50000 ) abcCommand = "&get;&syn2;&put:&get;&syn3;&put:&get;&syn4;&put";
	else if( fa <= 600000  || fl <= 70000 ) abcCommand = "drwsat";
	else { 
	  abcCommand = ""; // instsance is too large to select an automatic command!
	  msg(0,"combination of latches and AND-gates is assumed to consume too much AIG simplification time");
	}
      }
      msg(1,"set abc command to default command %s", abcCommand.c_str() );
    }
    
    // set number of initial latches, but only, if scorr is executed, and if there are no nonZero initialized latches
    if( (foundScorr || abcCommand.find("scorr") != std::string::npos) && nonZeroInitialized == 0 ) { // scorr is not used when there are nonZeroInitialized latches
      initialLatchNum = fl;
    }
    
    // start abc
    void * pAbc;
    Abc_Start(); // turn on ABC
    pAbc = Abc_FrameGetGlobalFrame();
    std::string abcmd = std::string("read ") + std::string(name);
    
    if ( Cmd_CommandExecute( pAbc, abcmd.c_str() ) )
    {
	  wrn( "ABC cannot execute command \"%s\".\n", abcmd.c_str() );
	  exit(42);
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
      
      
      if( nonZeroInitialized == 0 || thisCommand.find("scorr") == string::npos ) {
	msg(1,"execute abc command(s) %s, remaining: %s", thisCommand.c_str(), abcCommand.c_str() );
	if ( true &&  Cmd_CommandExecute( pAbc, thisCommand.c_str() ) )
	{
	    wrn( "ABC cannot execute command %s.\n", thisCommand.c_str() );
	    exit(43);
	}
      } else {
	wrn( "do not execute ABC command \"%s\" with %d nonZeroInitialized latches.\n", thisCommand.c_str(), nonZeroInitialized );
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
        exit(44);
    }
    Abc_Stop(); // turn off ABC
    msg(1,"change the name of the aiger file to read from %s to %s",name,useABC.c_str());
    name = useABC.c_str(); // do not change useABC after this point any more!
  }
  msg(0,"aig preprocessing tool %lf seconds", ppAigTime);
#endif
}


int ABC_cleanup()
{
#ifdef USE_ABC
  // delete the temporary file as soon as its not needed any more!
  if( abcDir == 1 ) {
    if( 0 != remove( useABC.c_str() ) ) { // delete the temporary file
      wrn("could not remove temporary file from temporary directory: %s", useABC.c_str() );
    }
  }
#endif
};

void printState(int k)
{
    cerr << "c maxVar["<<k<<"]:     " << nvars << endl;
    cerr << "c assume["<<k<<"]:     " << shiftFormula.currentAssume << endl;
    cerr << "c inputs["<<k<<"]:     " << shiftFormula.inputs.size() << ": "; for( int i = 0 ; i < shiftFormula.inputs.size(); ++ i ) cerr << " " << shiftFormula.inputs[i]; cerr << endl;
    cerr << "c latches["<<k<<"]:    " << shiftFormula.latch.size() << ": "; for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ) cerr << " " << shiftFormula.latch[i]; cerr << endl;
    cerr << "c latchNext["<<k<<"]:  " << shiftFormula.latchNext.size() << ": "; for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ) cerr << " " << shiftFormula.latchNext[i]; cerr << endl;
    cerr << "c thisBad["<<k<<"]:  " << shiftFormula.currentBads.size() << ": "; for( int i = 0 ; i < shiftFormula.currentBads.size(); ++ i ) cerr << " " << shiftFormula.currentBads[i]; cerr << endl;
    cerr << "c initial max var: " << shiftFormula.initialMaxVar << endl;
    cerr << "c initial after pp max var: " << shiftFormula.afterPPmaxVar << endl;
    cerr << "c merge shift dist: " << shiftFormula.mergeShiftDist << endl;
    cerr << "c formula["<<k<<"]:    " << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
      cerr << " " << shiftFormula.formula[i];
      if( shiftFormula.formula[i] == 0 ) cerr << endl;
    }
    cerr << "c equis: " << endl;
      // ensure that latches behave correctly in the next iteration
    for( int i = 0 ; i < shiftFormula.loopEqualities.size(); i+=2 ) {
	cerr << " " << shiftFormula.loopEqualities[i]  << " == " <<  shiftFormula.loopEqualities[i+1] << endl;
    }
    for( int i = 0 ; i < shiftFormula.initEqualities.size(); i+=2 ) {
	cerr << " " << shiftFormula.initEqualities[i]  << " == " <<  shiftFormula.initEqualities[i+1] << endl;
    } 
}

int simplifyCNF(int &k, void* preprocessorToUse, double& ppCNFtime) 
{
    MethodTimer cnfTimer( &ppCNFtime );
    simplifiedCNF ++;
    // add formula to preprocessor!
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) {
      CPaddLit( preprocessorToUse, shiftFormula.formula[i] );
    }
    
    if( verbose > 1 ) cerr << "c call freeze variables ... " << endl;
    CPfreezeVariable( preprocessorToUse,  1 ); // do not remove the unit (during dense!)
    if( !dontFreezeInput ) for( int i = 0 ; i < shiftFormula.inputs.size(); ++i ) CPfreezeVariable( preprocessorToUse,  shiftFormula.inputs[i] );
    for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
      msg(2,"freeze assumption literal %d", shiftFormula.latch[i]);
      CPfreezeVariable( preprocessorToUse,  shiftFormula.latch[i] );
    }
    for( int i = 0 ; i < shiftFormula.latchNext.size(); ++i ) CPfreezeVariable( preprocessorToUse,  shiftFormula.latchNext[i] );
    msg(2,"freeze assumption literal %d", shiftFormula.currentAssume);
    CPfreezeVariable( preprocessorToUse,  shiftFormula.currentAssume < 0 ? -shiftFormula.currentAssume : shiftFormula.currentAssume ); // do not alter the assumption variable!
    
    if( !dontFreezeBads ) {// should not be done!
    	for( int i = 0 ; i < shiftFormula.currentBads.size(); ++i ) CPfreezeVariable( preprocessorToUse,  shiftFormula.currentBads[i] );
    	for( int i = 0 ; i < shiftFormula.mergeBads.size(); ++i ) CPfreezeVariable( preprocessorToUse,  shiftFormula.mergeBads[i] );
    }
	
    // freez input equalitiy variables!
    for( int i = 0 ; i < shiftFormula.initEqualities.size(); i++ ) { 
      if(shiftFormula.initEqualities[i] > 0 ) CPfreezeVariable( preprocessorToUse, shiftFormula.initEqualities[i] );
      else  CPfreezeVariable( preprocessorToUse, - shiftFormula.initEqualities[i] );
    }
    // freez loop equalitiy variables!
    for( int i = 0 ; i < shiftFormula.loopEqualities.size(); i++ ) { 
      if(shiftFormula.loopEqualities[i] > 0 ) CPfreezeVariable( preprocessorToUse, shiftFormula.loopEqualities[i] );
      else  CPfreezeVariable( preprocessorToUse, - shiftFormula.loopEqualities[i] );
    }

    // perform simplification on the formula
    if( verbose > 1 ) cerr << "c call preprocess ... " << endl;
    CPpreprocess(preprocessorToUse);
    if( !CPok(preprocessorToUse) ) {
	msg(0,"initial formula has no bad state (found even without initialization)"); 
	printf( "0\n" );
	for( int i = 0 ; i < model->num_bad; ++ i ) printf("b%d",i);
	printf("\n.\n" ); // print witness, and terminate!
	fflush( stdout );
	exit(1);
    }
    
    // everything ok, re-setup the SAT solver!
    if( riss != 0 ) {
      riss_destroy (riss); // restart the solver!
      riss = riss_init(rissPresetConf);
    } else if ( priss != 0 ) {
      priss_destroy( priss );
      cerr << "c init priss with " << usePriss << " threads" << endl ;
      priss = priss_init(usePriss, "BMC" );
    } else {
      picosat_reset (picosat); // restart the solver!
//       picosat = picosat_init();
    }

    // get formula back from preprocessor
    shiftFormula.clearFormula(); // actively changing the formula!
    while ( CPhasNextOutputLit( preprocessorToUse ) ) {
      const int nextLit = CPnextOutputLit(preprocessorToUse);
      shiftFormula.formula.push_back( nextLit );
      shiftFormula.clauses = (nextLit == 0 ? shiftFormula.clauses + 1 : shiftFormula.clauses );
    }

    if( verbose > 1 ) cerr << "c add formula to SAT solver" << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) add( shiftFormula.formula[i], false );
    const int afterPPmaxVar = CPnVars(preprocessorToUse);
    if( verbose > 1 ) cerr << "c after variables: " << afterPPmaxVar << endl;
    
    if( denseVariables ) { // refresh knowledge about the extra literals (input,output/bad,latches,assume)
      int tmpLit = CPgetReplaceLiteral( preprocessorToUse,  shiftFormula.currentAssume );
      if( verbose > 1 ) cerr << "c current assume translated from " << shiftFormula.currentAssume << " to " << tmpLit <<endl;
      if( tmpLit != 0 ) shiftFormula.currentAssume = tmpLit; // if the shift literal is 0, then no dense information is present (or the literal is missing, which would be more complicated to track...)

      if( shiftFormula.originalLatch.size() == 0 ) {
	shiftFormula.originalLatch = shiftFormula.latch; // memorize original latch variables for printing witness with not initialized latches
      }


      for( int i = 0 ; i < shiftFormula.latch.size(); ++ i ){
	tmpLit = CPgetReplaceLiteral( preprocessorToUse,  shiftFormula.latch[i] );
	if( verbose > 1 ) cerr << "move latch from " << shiftFormula.latch[i] << " to " << tmpLit << endl;
	if( tmpLit != 0 ) shiftFormula.latch[i] = tmpLit;
      }

      for( int i = 0 ; i < shiftFormula.latchNext.size(); ++ i ){
	tmpLit = CPgetReplaceLiteral( preprocessorToUse, shiftFormula.latchNext[i]);
	if( verbose > 1 ) cerr << "move latchN from " << shiftFormula.latchNext[i] << " to " << tmpLit << endl;
	if( tmpLit != 0 ) shiftFormula.latchNext[i] = tmpLit;
      }
      // also shift equivalences that are not known by the solver and preprocessor yet (but are present during search)
      for( int i = 0 ; i < shiftFormula.initEqualities.size(); i++ ) {
				tmpLit = CPgetReplaceLiteral( preprocessorToUse,  shiftFormula.initEqualities[i] );
				if( tmpLit != 0 ) shiftFormula.initEqualities[i] = tmpLit;
      }
      for( int i = 0 ; i < shiftFormula.loopEqualities.size(); i++ ) {
				tmpLit = CPgetReplaceLiteral( preprocessorToUse,  shiftFormula.loopEqualities[i] );
				if( tmpLit != 0 ) shiftFormula.loopEqualities[i] = tmpLit;
      }
    }
    
    // check whether single output is already set to false ...
    if( CPlitFalsified(preprocessorToUse, shiftFormula.currentAssume ) ) {
      msg(1, "current bad is already falsified! - circuit has to be safe!" );
	printf( "u %d\n0\n", maxk );
	for( int i = 0 ; i < model->num_bad; ++ i ) printf("b%d",i);
	printf("\n.\n" ); // print witness, and terminate!
	fflush( stdout );
	exit(20);
    }
    if( verbose > 1 ) printState(k);
    
    return afterPPmaxVar;

}

int restoreSimplifyModel( void* usedPreprocessor, std::vector<uint8_t>& model)
{
  // extend model without copying current model twice
  CPresetModel( usedPreprocessor ); // reset current internal model
  for( int j = 0 ; j < model.size(); ++ j ) {
if(debugWitness)     cerr << "c push model " << j << " : " << (model[j] == l_True ? 1 : 0) << endl;
    CPpushModelBool( usedPreprocessor, model[j]  == l_True ? 1 : 0 );
  }
  CPpostprocessModel( usedPreprocessor );
  const int modelVars = CPmodelVariables( usedPreprocessor );
  model.clear(); // reset current model, so that it can be filled with data from the preprocessor
  for( int j = 0 ; j < modelVars; ++ j ) {
    uint8_t ret  = CPgetFinalModelLit(usedPreprocessor) > 0 ? l_True : l_False;
    model.push_back(ret);
if(debugWitness)     cerr << "c get model " << j << " : " << (int)ret << endl;
  }
  return modelVars;
}

void 
printWitness(int k, int shiftDist, int initialLatchNum)
{
    int found = 0;
    
    std::vector<uint8_t> fullFrameModel;
    std::vector<uint8_t> mergeFrameModel;
    
    // calculated buggy frame
    int actualFaultyFrame = (k * mergeFrames); // this frame has been proven to be good
    
    if(debugWitness) cerr << "c extract BAD state variables" << endl;
    
    if( useShift )
    {
      const int thisShift = k * shiftDist;
      const int lastShift = (k-1) * shiftDist;
      //
      // print the line that states the bad outputs, that have been reached
      //
      if( dontFreezeBads || true ) { // always uncover the whole model, to be able to split it into the inner frames!
	
	// if inputs have not been frozen, we need to recover them per frame before printing their value!
	const int frameShift = k * shiftDist; // want to access the very last frame
	fullFrameModel.assign ( shiftFormula.afterPPmaxVar, l_False );
	for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
		const int v = deref(j==0 ? 1 : j + frameShift + 1); // treat very first value always special, because its not moved!
		fullFrameModel[j] = v < 0 ? l_False : l_True; // model does not treat field 0!
	}
	
	    if( debugWitness && verbose > 1 ) {
	      cerr << "c pre RESTORE frame model[" << -1 << "] : " << endl;
	      for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { 
		cerr << (fullFrameModel[j] == l_True ? j+1 : -j -1) << " ";
	      }
	      cerr << endl;
	    }
	
	//
	// extend model for global frame  (without copying current model twice)
	//
	if( outerPreprocessor != 0 ) {
	  const int modelVars = restoreSimplifyModel( outerPreprocessor, fullFrameModel);
	  if( debugWitness ) cerr << "c last (multi-) frame has " << modelVars << " variables" << endl;
	}
	const int mergeFrameSize = shiftFormula.mergeShiftDist;  // == ( fullFrameModel.size()  ) / mergeFrames;
	assert( mergeFrames * mergeFrameSize <= fullFrameModel.size() && "an exact number of frames has been merged, hence the model sizes should also fit!" );

	if( debugWitness && verbose > 1 ) {
	  cerr << "c initial frame model[" << 0 << "] : " << endl;
	  for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { 
	    cerr << (fullFrameModel[j] == l_True ? j+1 : -j -1) << " ";
	  }
	  cerr << endl;
	}
	
	//
	// if frames have been merged, process each single frame now
	//
	
	for( int localFrame = 0; localFrame < mergeFrames; ++localFrame ) { // find the smallest frame that is actually broken! 
	    int innerModelVars  = mergeFrameModel.size();
	    if( mergeFrames == 1 ) {
	      if( debugWitness ) cerr << "c use outer model with PP: " << (int)(innerPreprocessor != 0)  << endl;
	      mergeFrameModel.swap( fullFrameModel );  // nothing has been merged, simply use the fullFrameModel
	    } else {
	      mergeFrameModel.clear();
	      mergeFrameModel.push_back( fullFrameModel[0] ); // copy the constant unit
	      for( int j = 1; j <= mergeFrameSize; ++ j )
		mergeFrameModel.push_back( fullFrameModel[ localFrame*mergeFrameSize + j] ); // copy all the other variables for the current (inner) frame
	      // has an inner simplifier been used? then undo its simplifications as well!
	      if( innerPreprocessor != 0 ) {
		if( debugWitness ) cerr << "c restore inner model with PP: " << (int)(innerPreprocessor != 0) << endl;
		restoreSimplifyModel( innerPreprocessor, mergeFrameModel); // restore model from the inner preprocessor, if an inner preprocessor has been used
	      }
	    }
	    
	    // check whether any bad variable is set
	    for (int j = 0; j < shiftFormula.currentBads.size(); j++) {
		int tlit = shiftFormula.currentBads[j];
		int v = 0; // by default the bad state is false
		if(tlit>0 ) { if( mergeFrameModel[ tlit -1 ] == l_True) v = 1; } 
		else { if(mergeFrameModel[ -tlit -1 ] == l_False ) v=1; }
		if( v = 1 ) { printf ("b%d", j), found++;} // print the current bad state
		assert( (shiftFormula.currentBads.size() > 1 || found > 0) && "only one output -> find immediately" );
	    }
	    if( found > 0 ) { // this inner frame is the first frame where things brake
	      actualFaultyFrame += localFrame;
	      break;	// as soon as one bad state has been printed, stop printing bad states!
	    }
	}
	if( debugWitness ) cerr << "c found bad state at " << actualFaultyFrame << endl;

      } else { // bad variables have been frozen, hance simply use the moved value!
	assert( false && "this code should never be reached!");
      }
      assert ((model->num_bad || model->num_justice) && "there has to be some property that has been found!" );
      nl (); // print new line, to give the initialization of the latches
      
      //
      // print initial Latch configuration
      //
      
#if 0
      /// nothing special to do with latches, because those are always frozen, and thus never touched
      int latchNumber = 0;
      vector<int>& witnessLatches = shiftFormula.originalLatch.size() == 0 ? shiftFormula.latch : shiftFormula.originalLatch;
      for (latchNumber = 0; latchNumber < model->num_latches; latchNumber++){
	if( initialLatchNum > 0 &&  deref(witnessLatches[latchNumber]) != -1 ) wrn("latch[%d],var %d is not initialized to 0, but scorr has been used during simplifying the circuit",latchNumber,witnessLatches[latchNumber]);
	print ( witnessLatches[latchNumber] ); // this function prints 0 or 1 depending on the current assignment of this variable!
      }
      for( ; latchNumber < initialLatchNum; ++ latchNumber ) { // print the latches that might be removed by ABC
	printV(-1); // ABC and HWMCC assumes all latches to be initialized to 0!
      }
      nl (); // print new line to give the sequence of inputs

#else
      
      /**
       *  extract value of all the latches
       */
      if( debugWitness ) cerr << "c extract BAD latches" << endl;
      
      { // for each step give the inputs that lead to the bad state
	int globalFrame = 0;
	{ // if inputs have not been frozen, we need to recover them per frame before printing their value!
	  const int globalFrameShift = globalFrame * shiftDist;
	  
	  fullFrameModel.assign ( shiftFormula.afterPPmaxVar, l_False );
	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + globalFrameShift); // treat very first value always special, because its not moved!
	    fullFrameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  
	  if( debugWitness && verbose > 1 ) {
	    cerr << "c initial frame model[" << 1 << "] : " << endl;
	    for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { 
	      cerr << (fullFrameModel[j] == l_True ? j+1 : -j -1) << " ";
	    }
	    cerr << endl;
	  }
	  
	  int mergeFrameSize =0;
	  if( outerPreprocessor != 0 ) {

	    if( debugWitness && verbose > 1 ) {
	      cerr << "c pre RESTORE frame model[" << 1 << "] : " << endl;
	      for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { 
		cerr << (fullFrameModel[j] == l_True ? j+1 : -j -1) << " ";
	      }
	      cerr << endl;
	    }
	    
	    const int modelVars = restoreSimplifyModel( outerPreprocessor, fullFrameModel);
	    mergeFrameSize = shiftFormula.mergeShiftDist;  // == ( fullFrameModel.size()  ) / mergeFrames;
	    
	    if( debugWitness && verbose > 1 ) {
	      cerr << "c post RESTORE frame model[" << 1 << "] : " << endl;
	      for( int j = 0 ; j < shiftFormula.afterPPmaxVar; ++ j ) { 
		cerr << (fullFrameModel[j] == l_True ? j+1 : -j -1) << " ";
	      }
	      cerr << endl;
	    }
	    
	    assert( mergeFrames * mergeFrameSize <= fullFrameModel.size() && "an exact number of frames has been merged, hence the model sizes should also fit!" );
	  } else {
	    mergeFrameSize = shiftDist;
	  }
	  int localFrame = 0;
	  { // find the smallest frame that is actually broken! 
	      // setup the model for this local frame
	      if( mergeFrames == 1 ) {
		mergeFrameModel.swap( fullFrameModel );  // nothing has been merged, simply use the fullFrameModel
		if( debugWitness ) cerr << "c simply use outer frame model" << endl;
	      }
	      else {
		mergeFrameModel.clear();
		mergeFrameModel.push_back( fullFrameModel[0] ); // copy the constant unit
		for( int j = 1; j <= mergeFrameSize; ++ j )
		  mergeFrameModel.push_back( fullFrameModel[ localFrame*mergeFrameSize + j] ); // copy all the other variables for the current (inner) frame
		// has an inner simplifier been used? then undo its simplifications as well!
		if( innerPreprocessor != 0 ) restoreSimplifyModel( innerPreprocessor, mergeFrameModel); // restore model from the inner preprocessor, if an inner preprocessor has been used
	      }
	      
	      if( debugWitness && verbose > 1 ) {
		cerr << "c initial inner frame model : " << endl;
		for( int j = 0 ; j < mergeFrameModel.size(); ++ j ) { 
		  cerr << (mergeFrameModel[j] == l_True ? j+1 : -j -1) << " ";
		}
		cerr << endl;
	      }
	      
	      // print the actual values
	      if( shiftFormula.originalLatch.size() > 0 ) msg(0,"use original latch variables");
	      vector<int>& witnessLatches = shiftFormula.originalLatch.size() == 0 ? shiftFormula.latch : shiftFormula.originalLatch;
	      assert( witnessLatches.size() == model->num_latches && "the number of latches has to match");
	      int latchNumber = 0;
	      for (; latchNumber < witnessLatches.size(); latchNumber++) { // print the actual values!
		int tlit = witnessLatches[latchNumber];
		if(tlit>0) printV ( mergeFrameModel[ tlit -1 ] == l_False ? -1 : 1  );
		else  printV ( mergeFrameModel[ -tlit -1 ] == l_True ? -1 : 1  ); 
	      }
	      if( debugWitness ) cerr << "c print not initialized latches from " << latchNumber << " to " << initialLatchNum << endl;
	      for( ; latchNumber < initialLatchNum; ++ latchNumber ) { // print the latches that might have been removed by ABC
		printV(-1); // ABC and HWMCC assumes all latches to be initialized to 0!
	      }
	      nl (); // printed the value of all latches
	  }
	}
      }
#endif
      
      /**
       *  end of extracting the value of all latches
       */
      
      if( debugWitness ) cerr << "c extract input sequences" << endl;
      
      
      //
      // print the inputs for all iterations
      //
      for (int globalFrame = 0; globalFrame <= k; globalFrame++) { // for each step give the inputs that lead to the bad state
	if( true || dontFreezeInput ) { // if inputs have not been frozen, we need to recover them per frame before printing their value!
	  const int globalFrameShift = globalFrame * shiftDist;
	  
	  fullFrameModel.assign ( shiftFormula.afterPPmaxVar, l_False );
	  for( int j = 1 ; j <= shiftFormula.afterPPmaxVar; ++ j ) { // get the right part of the actual model
	    const int v = deref(j==1 ? 1 : j + globalFrameShift); // treat very first value always special, because its not moved!
	    fullFrameModel[j-1] = v < 0 ? l_False : l_True; // model does not treat field 0!
	  }
	  int mergeFrameSize =0;
	  if( outerPreprocessor != 0 ) {
	    const int modelVars = restoreSimplifyModel( outerPreprocessor, fullFrameModel);
	    mergeFrameSize = shiftFormula.mergeShiftDist;  // == ( fullFrameModel.size()  ) / mergeFrames;
	    assert( mergeFrames * mergeFrameSize <= fullFrameModel.size() && "an exact number of frames has been merged, hence the model sizes should also fit!" );
	  } else {
	    mergeFrameSize = shiftDist;
	  }
	  
	  for( int localFrame = 0; localFrame < mergeFrames; ++localFrame ) { // find the smallest frame that is actually broken! 

	      // no need to print the input for the other frames that have been used for the calculation
	      if( globalFrame * mergeFrames + localFrame > actualFaultyFrame ) break;
	      // setup the model for this local frame
	      if( mergeFrames == 1 ) mergeFrameModel.swap( fullFrameModel );  // nothing has been merged, simply use the fullFrameModel
	      else {
		mergeFrameModel.clear();
		mergeFrameModel.push_back( fullFrameModel[0] ); // copy the constant unit
		for( int j = 1; j <= mergeFrameSize; ++ j )
		  mergeFrameModel.push_back( fullFrameModel[ localFrame*mergeFrameSize + j] ); // copy all the other variables for the current (inner) frame
		// has an inner simplifier been used? then undo its simplifications as well!
		if( innerPreprocessor != 0 ) restoreSimplifyModel( innerPreprocessor, mergeFrameModel); // restore model from the inner preprocessor, if an inner preprocessor has been used
	      }
	      // print the actual values
	      for (int j = 0; j < shiftFormula.inputs.size(); j++) { // print the actual values!
		int tlit = shiftFormula.inputs[j];
		if(tlit>0) printV ( mergeFrameModel[ tlit -1 ] == l_False ? -1 : 1  ); // print input j of iteration i
		else  printV ( mergeFrameModel[ -tlit -1 ] == l_True ? -1 : 1  ); // or complementary literal
	      }
	      nl ();
	  }
	} else { // if inputs have been frozen, their is no need to treat them special
	  assert( false && "all models have to be uncovered always!" );
	}
      }
      printf (".\n"); // indicate end of witness
      fflush (stdout);
    } else {
      int i = 0;
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
				for (int j = 0; j < model->num_inputs; j++)
					print (states[i].inputs[j]);
				nl ();
      }
      printf (".\n");
      fflush (stdout);
    } 
    
}
  



int main (int argc, char ** argv) {
  msg(0, "shiftbmc, Norbert Manthey, 2014");
  msg(0, "based on the aigbmc bounded model checker");

//   msg(0, "include temporal induction: simulate circuit with initial latch setup + simulate, record all states and hash them, check for convergence, turn 0/1 into X if not converging, continue. stable signals/alternating signals: check signals during execution. validate on circuit with initial latches=X, and perform iterations");
//   msg(0, "debug shift+preprocess with armin");
//   exit(0);


  totalTime = cpuTime() - totalTime;
  int i=0, k=0, lit=0, found=0;
  
  bool noProperties = false; // are there any properties inside the circuit?
  
  const char * err;
  parseOptions(argc,argv);
  if (maxk <= 0) maxk = 1 << 30; // set a very high bound as default (for 32bit machines)!
  msg (1, "maxk = %d", maxk);

  double ppAigTime = 0, outerPPtime = 0, innerPPtime = 0, reasonTime = 0;
  vector<double> boundtimes;
  
  // init solvers, is necessary with command line parameters
  init (argc,argv);
  
  int initialLatchNum = 0; // number of latches in initial file. If ABC changes the number of latches, this number is important to print the right witness!
  
/**
 *  check AIG circuit for nonZeroInitialized latches
 */
 if( true ) {
  msg (1, "reading from '%s'", name ? name : "<stdin>");
  if (name) err = aiger_open_and_read_from_file (model, name);
  else err = aiger_read_from_file (model, stdin), name = "<stdin>";
  if (err) die ("parse error reading '%s': %s", name, err);

  {
    unsigned i, j, lit;
    aiger_symbol * sym;
    int initpos = 0, initundef = 0;
    {
      for (i = 0; i < model->num_latches; i++) {
	sym = model->latches + i;
	if (sym->reset == 1 || sym->reset == sym->lit) {
	  nonZeroInitialized ++;
	  if( sym->reset == 1 ) initpos ++; 
	  else initundef ++;
	}
      }
    }
    msg(1,"found %d nonZeroInitialized latches(non: %d, pos %d)\n", nonZeroInitialized,initundef, initpos );
  }
  aiger_reset (model);
  model = aiger_init ();
  
  if( false && nonZeroInitialized > 0 ) {
    mergeFrames = 1;
    msg(0,"set merge frames to fall back value %d due to non zero initialized latches\n", mergeFrames);
  }
 }
  
  
/**
 *  use ABC for simplification
 */
  if( nonZeroInitialized == 0  && model->num_outputs != 0 && model->num_inputs != 0)
    ABC_simplify(ppAigTime, initialLatchNum); // measure CPU time, get number of latches in the initial AIG problem

  if( initialLatchNum != 0 ) cerr << "c set initial latches to " << initialLatchNum << endl;  
    
  msg (1, "reading from '%s'", name ? name : "<stdin>");
  if (name) err = aiger_open_and_read_from_file (model, name);
  else err = aiger_read_from_file (model, stdin), name = "<stdin>";
  if (err) die ("parse error reading '%s': %s", name, err);

  {
    unsigned i, j, lit,nonZeroInitialized2=0;
    aiger_symbol * sym;
    int initpos = 0, initundef = 0;
    {
      for (i = 0; i < model->num_latches; i++) {
	sym = model->latches + i;
	if (sym->reset == 1 || sym->reset == sym->lit) {
	  nonZeroInitialized2 ++;
	  if( sym->reset == 1 ) initpos ++; 
	  else initundef ++;
	}
      }
    }
    msg(1,"found %d nonZeroInitialized latches(non: %d, pos %d)\n", nonZeroInitialized2,initundef, initpos );
  }
  
  if( nonZeroInitialized == 0  && model->num_outputs != 0 && model->num_inputs != 0)
    ABC_cleanup();
  
  // print stats
  msg (0, "encode MILOA = %u %u %u %u %u",model->maxvar,model->num_inputs,model->num_latches,model->num_outputs,model->num_ands);
  msg (1, "encode BCJF = %u %u %u %u",model->num_bad,model->num_constraints,model->num_justice,model->num_fairness);
       
  if( doNotAllowToSmallCircuits ) { // here aigdd cannot produce too small circuits
    if( model->num_inputs < 2 || model->num_latches < 2 || model->num_outputs != 1 || model->num_ands <2 ) exit (30);
  }
  
  // check circuit
  if (!model->num_bad && !model->num_justice && model->num_constraints)
    wrn ("%u environment constraints but no bad nor justice properties",
         model->num_constraints);
  if (!model->num_justice && model->num_fairness)
    wrn ("%u fairness constraints but no justice properties",
         model->num_fairness);
         
  if (!model->num_bad && !model->num_justice && ! processNoProperties) {
    wrn ("no properties - abort solving the current instance (can be processed with bmc_np option");
    exit(31); // do not exit with std exit codes!!
  }
         
  // use output as bad state, if not bad state exists yet!    
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
	
	
			msg (1, "after move: BCJF = %u %u %u %u",
				   model->num_bad,
				   model->num_constraints,
				   model->num_justice,
				   model->num_fairness);
		}
  }

  // check properties
  if( checkProperties ) {
    cerr << "c bad " << model->num_bad << " output " << model->num_outputs << endl;
    exit( 0 );
  }
  
  // check properties once more
  if (!model->num_bad && !model->num_justice) {
    wrn ("no properties");
    noProperties = true;
    // goto DONE;
    wrn ("still try to solve the circuit");
  }
  
  // normalize the circuit!
  aiger_reencode (model);
  
  // get the first numbers
  firstlatchidx = 1 + model->num_inputs;
  first_andidx = firstlatchidx + model->num_latches;
  msg (2, "reencoded model");
  bad = calloc (model->num_bad, 1);
  justice = calloc (model->num_justice, 1);
  
  if (model->num_justice) {
    assert( false && "cannot handle justice constraints!" );
    exit(32);
  }

  /**
   * 
   *  THIS IS THE ACTUAL METHOD TO WORK WITH!
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
  shiftFormula.mergeShiftDist = nvars;
  // TODO: here, simplification and compression could be performed!

  //
  //
  // merge multiple frames
  //
  //
  int newMaxVar  = nvars;
  msg(1,"[1] transition-formula stats: %d clauses, %d vars, %d inputs, %d latch-vars(sum input and output)", shiftFormula.clauses, newMaxVar, shiftFormula.inputs.size(), shiftFormula.latch.size()+shiftFormula.latchNext.size());
  
  // have an "AUTO" in form of -1 here as well!
  
  if( mergeFrames > 1 ) {
      
      msg(1,"simplify inner frame");
      
      // setup the preprocessor for the inner frame
      if( innerPPConf != 0 ) {
	if( string(innerPPConf) == "AUTO" ) {
	  const int ands = model->num_ands, clss = shiftFormula.clauses; // use number of and gates and clauses to select the technique
	       if( clss <   75000   || ands <   28000 ) innerPPConf = "BMC_FULL";
	  else if( clss <  100000   || ands <   35000 ) innerPPConf = "BMC_BVEPRBAST";
	  else if( clss <  180000   || ands <   60000 ) innerPPConf = "BMC_FULLNOPRB";
	  else if( clss <  275000   || ands <  100000 ) innerPPConf = "BMC_BVEUHDAST";
	  else if( clss <  390000   || ands <  150000 ) innerPPConf = "BMC_BVEBVAAST";
	  else if( clss < 2000000   || ands <  850000 ) innerPPConf = "BMC_BVECLE";
	  else if( clss < 2500000   || ands < 1200000 ) innerPPConf = "BMC_BEBE";
	  else innerPPConf = 0; // if the formula is too large, disable innerPPconf
	}
	
	if( innerPPConf != 0 ) {
	  msg(1,"init PP with %s", innerPPConf);
	  innerPreprocessor = CPinit(innerPPConf);
	  // CPparseOptions( innerPreprocessor,&argc,argv, 0 ); // TODO: remove after debug!
	  //CPparseOptionString( innerPreprocessor, "-cp3_dense_forw -cp3_dense_debug=2" );
	  msg(1,"simplify");
	  // currentAssume
	  newMaxVar = simplifyCNF(k, innerPreprocessor, innerPPtime);
	  msg(0,"inner CNF preprocessing tool %lf seconds", innerPPtime);
	} else {
	  msg(0,"no inner preprocessing due to too large formula");
	}
      }
      msg(1,"[2] transition-formula stats: %d clauses, %d vars, %d inputs, %d latch-vars(sum input and output)", shiftFormula.clauses, newMaxVar, shiftFormula.inputs.size(), shiftFormula.latch.size()+shiftFormula.latchNext.size());
      
      msg(1,"merge multiple frames, namely %d\n", mergeFrames);
      // if merging is activated, then we need one extra variable (which is then shifted as well ... 
      shiftFormula.mergeAssumes.push_back( shiftFormula.currentAssume );	// memorize for frame 0
      const int mergeAssume = newMaxVar; // newvar(); // fresh variable for combined bad-state
      newMaxVar ++;
      shiftFormula.initialMaxVar = newMaxVar;
      shiftFormula.afterPPmaxVar = newMaxVar;

      const int shiftDist = shiftFormula.afterPPmaxVar - 1;  // shift depends on the variables that are actually in the formula that is used for solving
      shiftFormula.mergeShiftDist = shiftDist;
      
      if( verbose > 2 ){ cerr << "c before merge:" << endl; printState(0); }
      
      const int initialFormulaClauses = shiftFormula.formula.size();
      const int initialInputs = shiftFormula.inputs.size();
      const int initialBads = shiftFormula.currentBads.size();
      for (k = 1; k < mergeFrames; k++) { // frame 0 is already there, not add the remaining frames!
	  const int thisShift = k * shiftDist, lastShift = (k-1) * shiftDist;

	  // shift the formula!
	  int thisClauses = 0;
	  for( int i = 0 ; i < initialFormulaClauses; ++ i ) // in each iteration only shift the clauses of the initial iteration!
	  {
	    const int cl = shiftFormula.formula[i]; // literal to be shifted
	    if( cl >= -1 && cl <= 1 ) {
	      shiftFormula.formula.push_back( cl ); add(cl,false); // handle constant units, and the end of clause symbol
	      if( cl == 0 ) { thisClauses ++; shiftFormula.clauses ++; }
	    }
	    else { 
	      int newLit = varadd(cl, thisShift);
	      shiftFormula.formula.push_back( newLit ); add( newLit, false ); // do not add to the shiftformula again!
	    }
	  }
	  // ensure that latches behave correctly in the next iteration
	  for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
	    const int lhs = ( shiftFormula.latch[i] == 1 || shiftFormula.latch[i] == -1 ) ? shiftFormula.latch[i] : varadd(shiftFormula.latch[i], thisShift);
	    const int rhs = ( shiftFormula.latchNext[i] == 1 || shiftFormula.latchNext[i] == -1 ) ? shiftFormula.latchNext[i] : varadd(shiftFormula.latchNext[i], lastShift);
	    // encode lhs <-> rhs as two binary clauses
	    shiftFormula.formula.push_back( lhs  );add( lhs,false );
	    shiftFormula.formula.push_back( -rhs );add( -rhs,false );
	    shiftFormula.formula.push_back( 0    );add( 0,false );shiftFormula.clauses ++;
	    shiftFormula.formula.push_back( -lhs );add( -lhs,false );
	    shiftFormula.formula.push_back( rhs  );add( rhs,false );
	    shiftFormula.formula.push_back( 0    );add( 0,false );shiftFormula.clauses ++;
	  }
	  shiftFormula.mergeAssumes.push_back( shiftFormula.currentAssume + thisShift ); // memorize for frame k
	  
	  // no need to memorize inputs or bad state variables!
	  /*
	  for( int i = 0 ; i < initialInputs; ++ i ) {
	    int inputI = ( shiftFormula.inputs[i] == 1 || shiftFormula.inputs[i] == -1 ) ? shiftFormula.inputs[i] : varadd(shiftFormula.inputs[i], thisShift);
	    shiftFormula.inputs.push_back( inputI ); // add the inputs of this frame to the merged frame!
	  }
	  
  	  for( int i = 0 ; i < initialBads; ++ i ) {
  	    int badI = ( shiftFormula.currentBads[i] == 1 || shiftFormula.currentBads[i] == -1 ) ? shiftFormula.currentBads[i] : varadd(shiftFormula.currentBads[i], thisShift);
  	    shiftFormula.mergeBads.push_back( badI  ); // add the currentBads of this frame to the merged frame!
   	  }
   	  */

	  // also shift constant unit ...
// 	  shiftFormula.formula.push_back( thisShift  );add( thisShift,false );
// 	  shiftFormula.formula.push_back( -1 );add( -1,false );
// 	  shiftFormula.formula.push_back( 0  );add( 0,false );
// 	  shiftFormula.formula.push_back( -thisShift );add( -thisShift,false );
// 	  shiftFormula.formula.push_back( 1  );add( 1,false );
// 	  shiftFormula.formula.push_back( 0  );add( 0,false );
      }
      k = 0;
      // the bad property is now represented by the first mergeAssume variable
      shiftFormula.currentAssume = mergeAssume;
      lit = mergeAssume;
      
      // add clauses that force the merge-assume variable to be true, as soon as one of the other literals is true
      shiftFormula.formula.push_back( - shiftFormula.currentAssume ); // mergeAssume = true -> one original assume has to be true
      add( - shiftFormula.currentAssume, false );
      for( int i = 0; i < shiftFormula.mergeAssumes.size(); ++ i ) { 
	shiftFormula.formula.push_back(  shiftFormula.mergeAssumes[i] ); 
	add( shiftFormula.mergeAssumes[i], false );
      }
      shiftFormula.formula.push_back( 0 );add( 0, false );shiftFormula.clauses ++;
      
      for( int i = 0; i < shiftFormula.mergeAssumes.size(); ++ i ) {
	shiftFormula.formula.push_back( - shiftFormula.mergeAssumes[i] ); add( - shiftFormula.mergeAssumes[i], false );
	shiftFormula.formula.push_back( shiftFormula.currentAssume ); add( shiftFormula.currentAssume, false );
	shiftFormula.formula.push_back( 0 ); add( 0,false );shiftFormula.clauses ++;
      }
      
      // rewrite the latchNext variables to be the latchNext variables of the last frame
      const int thisShift = (mergeFrames-1) * shiftDist;
      cerr << "c shift latchNext with " << thisShift << endl;
      for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
	const int rhs = ( shiftFormula.latchNext[i] == 1 || shiftFormula.latchNext[i] == -1 ) ? shiftFormula.latchNext[i] : varadd(shiftFormula.latchNext[i], thisShift);
	shiftFormula.latchNext[i] = rhs;
      }
      
      // tell system about new variables
      newMaxVar = 1 + newMaxVar * mergeFrames; // + 1 - mergeFrames; // because there is the extra unit variable
      shiftFormula.initialMaxVar = newMaxVar;
      shiftFormula.afterPPmaxVar = newMaxVar;
      if( verbose > 2 ){ cerr << "c after merge:" << endl; printState(0); }
      msg(1,"[3] transition-formula stats: %d clauses, %d vars, %d inputs, %d latch-vars(sum input and output)", shiftFormula.clauses, newMaxVar, mergeFrames * shiftFormula.inputs.size(), shiftFormula.latch.size()+shiftFormula.latchNext.size());
  }
  

  if( useCP3 ) {
    
    
    // if( simplifiedCNF == 0 ) { // use preprocessing setup for inner frame
	if( outerPPConf == 0 || string(outerPPConf) == "AUTO" ) {
	  const int vars = newMaxVar, clss = shiftFormula.clauses; // use number of and gates and clauses to select the technique
	       if( clss <   75000   || vars <   28000 ) outerPPConf = "BMC_FULL";
	  else if( clss <  100000   || vars <   35000 ) outerPPConf = "BMC_BVEPRBAST";
	  else if( clss <  180000   || vars <   70000 ) outerPPConf = "BMC_FULLNOPRB";
	  else if( clss <  275000   || vars <  125000 ) outerPPConf = "BMC_BVEUHDAST";
	  else if( clss <  390000   || vars <  160000 ) outerPPConf = "BMC_BVEBVAAST";
	  else if( clss < 2000000   || vars <  840000 ) outerPPConf = "BMC_BVECLE";
	  else if( clss < 2500000   || vars < 1100000 ) outerPPConf = "BMC_BEBE";
	  else outerPPConf = 0; // if the formula is too large, disable innerPPconf
	}
    //} else {
    // and have something different otherwise
    //}
    
    if( outerPPConf != 0 ) {
      msg(0,"init outer CNF simplification with configuration %s", outerPPConf );
      outerPreprocessor = CPinit(outerPPConf);
    // add more options here?
    
//    CPparseOptions( outerPreprocessor,&argc,argv, 0 ); // parse the configuration!
//     if( denseVariables ) { // enable options for CP3 for densing variables
//       int myargc = 8; // number of elements of parameters
//       const char * myargv [] = {"pseudobinary","-enabled_cp3","-up","-subsimp","-bve","-dense","-cp3_dense_debug"};
//       // cp3parseOptions( preprocessor,argc,argv ); // parse the configuration!
//     } else {
//       int myargc = 6; // number of elements of parameters
//       const char * myargv [] = {"pseudobinary","-enabled_cp3","-up","-subsimp","-bve"};
//       // cp3parseOptions( preprocessor,argc,argv ); // parse the configuration!      
//     }
      shiftFormula.afterPPmaxVar = simplifyCNF(k,outerPreprocessor,outerPPtime);
    } else {
      msg(0,"no outer preprocessing due to too large formula");
    }
  }
  
  msg(0,"outer CNF preprocessing tool %lf seconds", outerPPtime);
  // TODO after compression, update all the variables/literals lists in the shiftformula structure!
  


  msg(1,"[4] transition-formula stats: %d clauses, %d vars, %d inputs, %d latch-vars(sum input and output)", shiftFormula.clauses, shiftFormula.afterPPmaxVar, mergeFrames * shiftFormula.inputs.size(), shiftFormula.latch.size()+shiftFormula.latchNext.size());
  cerr << "[shift-bmc] pp time summary: aig: " << ppAigTime << " inner-cnf: " << innerPPtime << " outer-cnf: " << outerPPtime << endl;
  if( stats_only ) exit(0);

  // semi-global variables
  lit = shiftFormula.currentAssume; // literal might have changed!
  const int shiftDist = shiftFormula.afterPPmaxVar - 1;  // shift depends on the variables that are actually in the formula that is used for solving
  
  
  // dump the output formula ...
  std::ofstream outputCNF;
  if( formulaFile != "" ) {
    outputCNF.open ( formulaFile.c_str(), std::fstream::out | std::fstream::trunc );
    if ( ! outputCNF ) {
      msg(0,"could not open file %s\n", formulaFile.c_str() );
      exit(3);
    }
    
    if( verbose > 1 ) cerr << "c write simplified first frame to solver" << endl; // as done in the simplify method
    outputCNF << "c SHIFBMC formula dump for bound " << maxk << endl;
    cerr << "c SHIFBMC formula dump for bound " << maxk << endl;
    for( int i = 0 ; i < shiftFormula.formula.size(); ++i ) {
      if( shiftFormula.formula[i] == 0 ) outputCNF << "0" << endl; // new line
      else outputCNF << shiftFormula.formula[i] << " "; // have a space after each literal
    }
    // convert into printing into file
    for( int i = 0 ; i < shiftFormula.initEqualities.size(); i+=2 ) {
      outputCNF << - shiftFormula.initEqualities[i] << " "  << shiftFormula.initEqualities[i+1] << " 0" << endl;
      outputCNF << shiftFormula.initEqualities[i] << " "  << - shiftFormula.initEqualities[i+1] << " 0" << endl;
    }
    outputCNF << constantUnit << " 0" << endl;   // constant unit
    // take care of bound
    if( maxk == 0 ) outputCNF << (lit != 0 ? lit : 1) << " 0" << endl;
    else if( lit != 0 ) outputCNF << -lit << " 0" << endl; 
    
    if( maxk != 0 ) {
      for (k = 1; (k * mergeFrames)  <= maxk; k++) {
	cerr << "c SHIFBMC process bound " << (k * mergeFrames) << endl;
	const int thisShift = k * shiftDist;
	const int lastShift = (k-1) * shiftDist;
	  // shift the formula!
	  int thisClauses = 0;
	  for( int i = 0 ; i < shiftFormula.formula.size(); ++ i ) {
	    const int& cl = shiftFormula.formula[i];
	    if( cl >= -1 && cl <= 1 ) {
	      outputCNF << (cl,false) << " "; // handle constant units, and the end of clause symbol
	      if( cl == 0 ) outputCNF << endl;
	    }
	    else outputCNF << varadd(cl, thisShift) << " "; // do not add to the shiftformula again!
	  }
	  // ensure that latches behave correctly in the next iteration
	  for( int i = 0 ; i < shiftFormula.latch.size(); ++i ) {
	    const int lhs = ( shiftFormula.latch[i] == 1 || shiftFormula.latch[i] == -1 ) ? shiftFormula.latch[i] : varadd(shiftFormula.latch[i], thisShift);
	    const int rhs = ( shiftFormula.latchNext[i] == 1 || shiftFormula.latchNext[i] == -1 ) ? shiftFormula.latchNext[i] : varadd(shiftFormula.latchNext[i], lastShift);
	    outputCNF << -lhs << " " << rhs << " 0" << endl;
	    outputCNF << lhs << " " << -rhs << " 0" << endl;
	  }
	lit = shiftFormula.currentAssume + thisShift;
	if( (k * mergeFrames) >= maxk ) {
	  cerr << "c write positive assume unit clause for bound " << (k * mergeFrames) << " : " << lit << endl;
	  outputCNF << (lit) << " 0" << endl; // set positive only for the last bound! (this is the literal that would be assumed)
	} else {
	  cerr << "c write negative assume unit clause for bound " << (k * mergeFrames) << " : " << -lit << endl;
	  outputCNF << (-lit) << " 0" << endl; // otherwise, for lower bounds its assumed that there cannot be a bug
	}
      }
    }
    outputCNF.close();
    msg( 0, "finished dumping CNF file\n" );
    exit(0);
  }
  
/*
 *
 *  solve the initial state here!
 *
 */

  
  
  // after first PP call, do write equalities between initialized latches and first latch inputs!
  if( lazyEncode )
    for( int i = 0 ; i < shiftFormula.initEqualities.size(); i+=2 ) {
      _eq( shiftFormula.initEqualities[i], shiftFormula.initEqualities[i+1], false );
//      if( verbose > 0 ) cerr << "c eq: " << shiftFormula.initEqualities[i] << " == " << shiftFormula.initEqualities[i+1] << endl;
    }
  
  msg(1,"start solving at frame 0");
  
  boundtimes[0] = cpuTime() - boundtimes[0]; // continue measuring the time for bound 0    
  unit ( constantUnit, false );   // the literal 1 will always be mapped to true!! add only in the very first iteration!
  assume (lit != 0 ? lit : 1); // only if lit != 0, but still assume something - thus, simply assume the known unit literal! TODO check whether more information can be derived if the assumption literal does not play a role in the circuit!
  assert ( k == 0 && "the first iteration has to be done first!" );
  if( verbose > 2 ) printState(k); 
  int satReturn = sat ( frameConflictBudget );
  const bool foundBadStateFast = (satReturn == 10);
  if( satReturn == 0 && frameConflictBudget == -1 ) {
    wrn("sat call at frame 0 returned UNKNOWN\n");
    exit(33);
  } else if (satReturn == 0 && frameConflictBudget != -1 ) {
    msg(1,"failed to solve bound %d within %d conflicts\n", k, frameConflictBudget );
    frameConflictBudget = frameConflictBudget * frameConflictsInc;
  }
  boundtimes[0] = cpuTime() - boundtimes[0]; // stop measuring the time for bound 0
  if( printTime ) msg(0,"solved bound %d in %lf seconds", mergeFrames - 1, boundtimes[k]);
  if( foundBadStateFast ) {
    cerr << "c found bad state within first time frame" << endl;
    goto finishedSolving;
  }
  // only if we assumed the literal!
  if( lit != 0 ) unit (-lit, false); // do not add this unit to the shift formula!
  
  /* print safe bound for deep bound track */
  printf ("u%d\n", mergeFrames - 1 );fflush (stdout);
  
/*
 *
 *  solve all upcoming states here!
 *
 */
  msg (2, "re-encode model");
  if( !useShift ) { // old or new method?
    for (k = 1; (k * mergeFrames)  <= maxk; k++) {
      boundtimes.push_back(0);
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      shiftFormula.clear(); /// carefully here, since one unit has been added already!
      lit = encode ();
      cerr << "c after encode " << shiftFormula.loopEqualities.size() << " loop eq lits" << endl;
      if( lazyEncode ) { // fuse the two time frames!
	for( int i = 0 ; i < shiftFormula.loopEqualities.size(); i+=2 ) _eq( shiftFormula.loopEqualities[i], shiftFormula.loopEqualities[i+1] );
      }
      assume (lit);
      if( verbose > 2 ) printState(k); 
      const int satReturn = sat (frameConflictBudget);
      const bool foundBad = (satReturn  == 10); // // indicate whether solving resulted in 10! otherwise
      if( satReturn == 0 && frameConflictBudget == -1 ) {
	wrn("sat call at frame %d returned UNKNOWN\n",k);
	exit(34);
      } else if (satReturn == 0 && frameConflictBudget != -1 ) {
	msg(1,"failed to solve bound %d within %d conflicts\n", k, frameConflictBudget );
	frameConflictBudget = frameConflictBudget * frameConflictsInc;
      }
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      if( printTime ) msg(0,"solved bound %d in %lf seconds", mergeFrames - 1 + (k * mergeFrames), boundtimes[k]);
      if( foundBad ) break;
      unit (-lit); // add as unit, since it is ensured that it cannot be satisfied (previous call)
      /* print safe bound for deep bound track */
      printf ("u%d\n", mergeFrames - 1 + (k * mergeFrames) );fflush (stdout);
    }
  } else {
    for (k = 1; (k * mergeFrames)  <= maxk; k++) {
      boundtimes.push_back(0);
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0

      const int thisShift = k * shiftDist;
      const int lastShift = (k-1) * shiftDist;
      
      if( verbose > 2 ) printState(k);
      
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
      const int satReturn = sat (frameConflictBudget);
      const bool foundBad = (satReturn  == 10); // // indicate whether solving resulted in 10! otherwise
      if( satReturn == 0 && frameConflictBudget == -1 ) {
	wrn("sat call at frame %d returned UNKNOWN\n",k);
	exit(35);
      } else if (satReturn == 0 && frameConflictBudget != -1 ) {
	msg(1,"failed to solve bound %d within %d conflicts\n", k, frameConflictBudget );
	frameConflictBudget = frameConflictBudget * frameConflictsInc;
      }
      boundtimes[k] = cpuTime() - boundtimes[k]; // stop measuring the time for bound 0
      if( printTime ) msg(0,"solved bound %d in %lf seconds", mergeFrames - 1 + (k * mergeFrames), boundtimes[k]);
      if( foundBad ) break;
      unit (-lit,false); // add as unit, since it is ensured that it cannot be satisfied (previous call)
      /* print safe bound for deep bound track */
      printf ("u%d\n", mergeFrames - 1 + (k * mergeFrames) );fflush (stdout);
    }
  }
  
  // jump here if a solution has been found!
finishedSolving:;
  
  /**
   *  PRINT MODEL/WITNESS, if necessary
   */
  
  if( printTime ) {
    cerr << "[shift-bmc] solve time summary: ";
    for( int i = 0 ; i < boundtimes.size(); ++ i ) cerr << " " << boundtimes[i];
    cerr << endl;
  }
  
  if (mergeFrames - 1 + (k * mergeFrames) <= maxk) {	
    cerr << "found a bad state around bound " << mergeFrames - 1 + (k * mergeFrames) << endl;
  }
  
  if (quiet) goto DONE;
  if (mergeFrames - 1 + (k * mergeFrames) <= maxk) {	
    printf ("1\n");
    fflush (stdout);
    if (nowitness) goto DONE;
    cerr << "c print witness with initial latches " << initialLatchNum << endl;
    printWitness(k, shiftDist, initialLatchNum);
  } else {
    printf ("2\n");
    fflush (stdout);  
  }
DONE:
 
//Result for ParamILS: <solved>, <runtime>, <runlength>, <quality>, <seed>,
//<additional rundata>

  // print result for Tuner
  if( tuning ) {
    std::cout << "Result for ParamILS: " 
    << ((mergeFrames - 1 + (k * mergeFrames) <= maxk) ? "SAT" : "ABORT") << ", " // if the limit we are searching for is not high enough, this is a reason to abort!
    << cpuTime() - totalTime << ", "
    << mergeFrames - 1 + (k * mergeFrames) << ", "
    << 10000.0 / (double)(mergeFrames - 1 + (k * mergeFrames)+1) << ", " // avoid dividing by 0!
    << tuneSeed 
    << std::endl;
    std::cout.flush();
    std::cerr << "Result for ParamILS: " 
    << ((mergeFrames - 1 + (k * mergeFrames) <= maxk) ? "SAT" : "ABORT") << ", " // if the limit we are searching for is not high enough, this is a reason to abort!
    << cpuTime() - totalTime << ", "
    << mergeFrames - 1 + (k * mergeFrames) << ", "
    << 10000.0 / (double)(mergeFrames - 1 + (k * mergeFrames)+1) << ", " // avoid dividing by 0!
    << tuneSeed
    << std::endl;
    std::cerr.flush();
  }
  
  reset ();
  
  // if( k == 0 ) return 20; // initial circuit is faulty already (without using any latches and unrolling)
  if( mergeFrames - 1 + (k * mergeFrames) <= maxk) return 1; // found a bug
  else return 0;
}
  
