/***************************************************************************
Copyright (c) 2014, Norbert Manthey

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

#ifndef SHIFTBMC_H
#define SHIFTBMC_H

#include "aiger.h"

// enable this to disable all the output that might tell about the content of the implementation
// #define TEST_BINARY

extern "C" { // we are compiling with G++, however, picosat is C code, as well as the library interfaces of the other two solvers
  #include "picosat.h" // is found because an include path is set from the outside (makefile)
  #include "../lib-src/librissc.h" 
  #include "../lib-src/libprissc.h" 
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
 *  For using coprocessor 3 as formula simplification
 */
#include "../lib-src/libcoprocessorc.h"

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

inline 
double cpuTime(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000; 
}


/**
 *  this will be used to encode the formula with shifting!
 */
class ShiftFormula {
public:
  vector<int> initUnits;		// units to initialize the initial circuit - will not be used for simplification, but freezed (to generate the same clauses for bad states,fairness,justice,...)
  vector<int> formula;			// formula that contains all the clauses to be shifted per iteration
  vector<int> inputs;			// the input literals
  vector<int> outputs;			// the input literals
  vector<int> latch;			// current latch literals
  vector<int> originalLatch;		// current latch literals (original variable indexes of original formula
  vector<int> latchNext;		// next latch literals
  vector<int> currentBads;		// bad state literals of current formula
  vector<int> initEqualities;		// equalities that have to be added to the formula before solving
  vector<int> loopEqualities;		// equalities that have to be added to fuse two time frames (not to initialize the first time frame!)

  int currentAssume;			// literal that is used as assuption for this round
  vector<int> mergeAssumes;		// in case of merging multiple timeframes, this vector stores the variables of the 
  int mergeShiftDist;			// use this value for freezing extra variables
  vector<int> mergeBads;		// bad state literals of merged formula
  
  int clauses; // count the number of clauses that are present in the current formula
  
  int initialMaxVar;	// use to reconstruct inputs and all that
  int afterPPmaxVar;   // use to shift the formula
  
  void clearFormula() { formula.clear(); clauses = 0; }
  
  ShiftFormula () : clauses(0) {}
  
  /// clear current state
  void clear() { formula.clear(); inputs.clear(); latch.clear(); currentBads.clear(); initEqualities.clear(); loopEqualities.clear(); }
  
  void produceFormula( int k ) {
    
  }
};

/** class that measures the time between creation and destruction of the object, and adds it*/
class MethodTimer {
  double* pointer;
public:
  MethodTimer( double* timer ) : pointer( timer ) { *pointer = cpuTime() - *pointer;}
  ~MethodTimer() { *pointer = cpuTime() - *pointer; }
};



const char * usage =
"usage: shift-bmc [<model>][<maxk>] [OPTIONS]\n"
"\n"
"-h         print this comm_and line option summary\n"
#ifndef TEST_BINARY
"-bmc_v     increase verbose level\n"
#endif
"-bmc_k     set maxk as usual option\n"
"-bmc_m     use outputs as bad state constraint\n"
"-bmc_np    still process, even if there are no properties given (might be buggy)\n"
"-bmc_n     do not print witness\n"
"-bmc_q     be quite (implies '-n')\n"
"-bmc_c     check whether there are properties (return 1, otherwise 0)\n"
"-bmc_so    output formula stats only\n"
"-bmc_dd    do not allow too small circuit\n"
"-bmc_l     use lazy coding\n"
"-bmc_s     use shifting instead of reencoding (implies lazy encoding)\n"
"-bmc_p X   use CP3 as simplifier (implies using shifting) with the given configuration X\n"
"-bmc_r      use Riss as SAT solver (default is PicoSAT) (disables previous Priss)\n"
#ifndef TEST_BINARY
"-bmc_pr X   use Priss as SAT solver with X threads (1..64) (default is PicoSAT) (disables previous Riss)\n"
#endif
"-bmc_rpc    use given preset configuration for Riss (implies using Riss)\n"
#ifndef TEST_BINARY
"-bmc_dbgp  dump formula and everything else before preprocessing\n"
#endif
"-bmc_d     dense after preprocessing (implies using CP3)\n"
"-bmc_x     dont freeze input variables (implies using  CP3)\n"
"-bmc_y     dont freeze bad-state variables (implies using CP3)\n"
#ifndef TEST_BINARY
// merge, and budget solving
"-bmc_mf X  merge X frames into one\n"
"-bmc_mp X  use CNF simplification before merging with preset configuration X (e.g. AUTO for automatic selection)\n"
"-bmc_fc X  number of conflicts to be allowed for one frame\n"
"-bmc_fi X  increase of number of conflicts, if a frame cannot be solved\n"
#endif
"-bmc_t     print time needed per bound\n"
"-bmc_ml X  specify memory limit in MB\n"
#ifdef USE_ABC
"-bmc_a     use the ABC tool to simplify the circuit before solving, next parameter has to specify a tmp file location!\n"
"-bmc_ad    use the ABC tool to simplify the circuit before solving, next parameter has to specify a tmp directory location!\n"
"-bmc_ac    give a special command(-sequence, separated by ':') to ABC. default is AUTO other possible: AUTO, dc2, drwsat, scorr,dc2:scorr,...\n"
#endif
#ifndef TEST_BINARY
"-bmc_outCNF print formula to solve bound k into the given file (formula will not have a valid p-line, works only with preprocessing, should not be used)"
"-bmc_tune  enable the output for tuning with paramILS/SMAC\n"
#endif
"\n\n the model name should be given first, then, the max. bound should be given\n"
" other options might be given as well. These options are forwarded to the SAT solver/CNF simplifier\n"
;


static int isnum (const char * str) {
  const char * p = str;
  if (!isdigit (*p++)) return 0;
  while (isdigit (*p)) p++;
  return !*p;
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


#endif
