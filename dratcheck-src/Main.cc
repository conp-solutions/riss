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

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "utils/AutoDelete.h"
#include "core/Dimacs.h"
#include "core/Solver.h"

#include "coprocessor-src/Coprocessor.h"

#include "VERSION" // include the file that defines the solver version

using namespace Minisat;

//=================================================================================================

static bool receivedInterupt = false;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("c *** INTERRUPTED ***\n");
//     if (solver->verbosity > 0){
//         printStats(*solver);
//         printf("\n"); printf("c *** INTERRUPTED ***\n"); }
    solver->interrupt();
    if( receivedInterupt ) _exit(1);
    else receivedInterupt = true;
}


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
  
  setUsageHelp("USAGE: %s [options] <formula-file> <proof1> [<proof2> ... <proofn>]\n\n  where format may be either in plain or gzipped DIMACS.\n");
  // Extra options:
  //
  IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
  IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
  IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

  BoolOption   opt_drat       ("MAIN", "drat",    "verify DRAT instead of DRUP", true);
  BoolOption   opt_first      ("MAIN", "first",   "check RAT only for first literal", true);
  BoolOption   opt_backward   ("MAIN", "quiet",   "Do not print the model", false);
  
# error IMPLEMENT DRUP/DRAT checking here
  
    try {

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
  
	  gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");

	      printf("c ==========================[     dratcheck %5.2f     ]====================================================\n", solverVersion);
	      printf("c | Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
	      printf("c =========================================================================================================\n");
	      

	  parse_DIMACS(in, S);
	  gzclose(in);
	  FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

	  
  #ifdef NDEBUG
	  exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
  #else
	  return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
  #endif
	
	
    } catch (OutOfMemoryException&){
	printf("c Warning: caught an exception\n");
        exit(0);
    }
}
