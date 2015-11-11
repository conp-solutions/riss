/*****************************************************************************************[Main.cc]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
				CRIL - Univ. Artois, France
				LRI  - Univ. Paris Sud, France
 
Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------

Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2015, Norbert Manthey, All rights reserved.

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>

#include "riss/utils/System.h"
#include "riss/utils/ParseUtils.h"
#include "riss/utils/Options.h"
#include "riss/utils/AutoDelete.h"
#include "riss/core/Dimacs.h"
#include "riss/core/Solver.h"

#include "proofcheck/ProofChecker.h"

#include "riss/utils/version.h"

using namespace Riss;

ProofChecker* pc; // pointer to proof checker

//=================================================================================================

static bool receivedInterupt = false;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { pc->interupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("c *** INTERRUPTED ***\n");
//     if (solver->verbosity > 0){
//         printStats(*solver);
//         printf("\n"); printf("c *** INTERRUPTED ***\n"); }
//     solver->interrupt();
    if( receivedInterupt ) _exit(1);
    else receivedInterupt = true;
}


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
  
  setUsageHelp("USAGE: %s [options] <formula-file> <proof1> [<proof2> ... <proofn>]\n\n  where format may be either in plain or gzipped DIMACS.\n\n  Returns 0, if the proof can be verified, 1 otherwise");
  // Extra options:
  //
  IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more, ...).", 1, IntRange(0, 5));
  IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
  IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

  BoolOption   opt_drat        ("PROOFCHECK", "drat",        "verify DRAT instead of DRUP", true);
  BoolOption   opt_first       ("PROOFCHECK", "first",       "check RAT only for first literal", true);
  BoolOption   opt_backward    ("PROOFCHECK", "backward",    "use backward checking", true);
  BoolOption   opt_verifyUnsat ("PROOFCHECK", "verifyUnsat", "verify the empty clause (otherwise check proof only)", true);
  IntOption    opt_threads     ("PROOFCHECK", "threads",     "number of threads that are used for verification.\n", 1, IntRange(0, INT32_MAX));
  BoolOption   opt_stdin       ("PROOFCHECK", "useStdin",    "scan on stdin for further proof parts (files first)", false);
  
    try {

	  bool foundHelp = ::parseOptions (argc, argv ); // parse all global options
	  if( foundHelp ) exit(0);
	  
	  signal(SIGINT, SIGINT_exit);
	  signal(SIGXCPU,SIGINT_exit);

	  // Set limit on CPU-time:
	  if (cpu_lim != INT32_MAX){
	      rlimit rl;
	      getrlimit(RLIMIT_CPU, &rl);
	      if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
		  rl.rlim_cur = cpu_lim;
		  if (setrlimit(RLIMIT_CPU, &rl) == -1)
		      printf("c WARNING! Could not set resource limit: CPU-time.\n");
	      } }

	  // Set limit on virtual memory:
	  if (mem_lim != INT32_MAX){
	      rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
	      rlimit rl;
	      getrlimit(RLIMIT_AS, &rl);
	      if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
		  rl.rlim_cur = new_mem_lim;
		  if (setrlimit(RLIMIT_AS, &rl) == -1)
		      printf("c WARNING! Could not set resource limit: Virtual memory.\n");
	      } }

	  printf("c ======================[ proofcheck %5s  %13s ]================================================\n", solverVersion, gitSHA1);
	  printf("c | Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
	  printf("c =========================================================================================================\n");
	  
	  // not enough parameters given, at least a formula and one proof
	  if( argc < 2 ) {
	    printUsageAndExit(argc, argv);
	    exit(1);
	  }
	  
	  // create checker object
// 	  cerr << "c opt_drat: " << (bool)opt_drat << " backward: " << (bool)opt_backward << " first: " << (bool)opt_first  << " threads: " << (int) opt_threads << endl;
	  ProofChecker proofChecker( opt_drat, opt_backward, opt_threads, opt_first );
	  proofChecker.setVerbosity( verb ); // tell about verbosity
	  pc = &proofChecker;
	  
	  // parse the formula
	  printf("c parse the formula\n");
	  gzFile in = gzopen( argv[1], "rb");
	  if( ! in ) {
	    printf("c WARNING: could not open formula file %s\n", argv[1] );
	    printf("s NOT VERIFIED\n");
	    exit(1);
	  } else {
	    parse_DIMACS(in, proofChecker);
	    gzclose(in);
	  }
	  
	  // tell checker that the end of the formula has been reached - from now on there are learned clauses
	  proofChecker.setReveiceFormula( false );
	  vec<Lit> dummy;
	  
	  // the formula is unsatisfiable by unit propagation, print result and return with correct exit code
	  if( proofChecker.checkClauseDRUP( dummy , false ) ) {
	    printf("s UNSATISFIABLE\n");
	    printf("s VERIFIED\n");
	    exit(0);
	  }
	  
	  // parse proofs
	  int proofParts = 0;
	  bool drupProof = true;
	  for( int i = 2; i < argc; ++ i ) {
	    proofParts ++;
	    printf("c parse proof part [%d] from file %s\n", proofParts, argv[i] );
	    gzFile in = gzopen(argv[i], "rb");
	    if( ! in ) {
	      printf("c WARNING: could not open file %s\n", argv[i] );
	    } else {
	      ProofStyle proofStyle = parse_proof(in, proofChecker);
	      if( proofStyle == dratProof && !opt_drat ) {
		printf ("c WARNING given proof format is said to be stronger than the enabled verification\n");
	      }
	      gzclose(in);
	      drupProof = drupProof && ( proofStyle == Riss::drupProof ); // check whether the given proof is claimed to be in the less expensive format
	    }
	  }
	  
	  // scan for the final part of the proof on stdin
	  if( opt_stdin ) {
	    proofParts ++;
	    printf("c parse proof part [%d] from stdin\n", proofParts );
	    gzFile in =  gzdopen(0, "rb");
	    ProofStyle proofStyle = parse_proof(in, proofChecker);
	    if( proofStyle == dratProof && !opt_drat ) {
	      printf ("c WARNING given proof format is said to be stronger than the enabled verification\n");
	    }
	    gzclose(in);
	    drupProof = drupProof && ( proofStyle == Riss::drupProof ); // check whether the given proof is claimed to be in the less expensive format
	  }

	  // check whether parsing worked
	  bool successfulVerification = proofChecker.parsingOk();
	  
	  if( opt_verifyUnsat ) {
	    successfulVerification = successfulVerification && proofChecker.emptyPresent();
	    
	    if( !successfulVerification ) {
	      printf ("c WARNING: empty clause not present\n");
	      printf ("s NOT VERIFIED\n");
	      return 1;
	    }
	    
	    // all proof files claimed to be DRUP, so disable expensive DRAT data structures
	    if( drupProof ) proofChecker.setDRUPproof();
	    successfulVerification = proofChecker.verifyProof();
	  }
	    
	  if( successfulVerification ) {
	    printf ("s VERIFIED\n");
	    return 0;
	  } else {
	    printf ("s NOT VERIFIED\n");
	    return 1;
	  }
	  
    } catch (OutOfMemoryException&){  // something went wrong
	printf("c Warning: caught an exception\n");
	printf ("s NOT VERIFIED\n");
        exit(1); 
    }
}
