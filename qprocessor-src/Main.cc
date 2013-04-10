/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2013,      Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "core/Dimacs.h"
#include "simp/SimpSolver.h"

#include "coprocessor-src/Coprocessor.h"

#include "qprocessor-src/QDimacs.h"

using namespace Minisat;
using namespace Coprocessor;

//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    printf("c restarts              : %"PRIu64"\n", solver.starts);
    printf("c conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    printf("c decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    printf("c propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    printf("c conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) printf("c Memory used           : %.2f MB\n", mem_used);
    printf("c CPU time              : %g s\n", cpu_time);
}


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("c *** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        printStats(*solver);
        printf("\n"); printf("c *** INTERRUPTED ***\n"); }
    _exit(1); }


//=================================================================================================
// Main:

int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        
        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        BoolOption   pre    ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
        StringOption dimacs ("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));
	
        parseOptions(argc, argv, true);
        
        Solver  S;
        double      initial_time = cpuTime();

        S.verbosity = verb;
        
        solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        // signal(SIGINT, SIGINT_exit);
        // signal(SIGXCPU,SIGINT_exit);

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
    
	  if (argc == 1)
	      printf("c Reading from standard input... Use '--help' for help.\n");

	  gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
	  if (in == NULL)
	      printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
	  
	  if (S.verbosity > 0) {
	      printf("c ===============================[    Qprocessor    ]============================\n");
	      printf("c | Norbert Manthey. The use of the tool is limited to research only!           |\n");
	  }
	  
	      if (S.verbosity > 0) {
		  printf("c |                                                                             |\n");
		  printf("c ============================[ Problem Statistics ]=============================\n");
		  printf("c |                                                                             |\n"); }
	    
	    vector<quantification> quantifiers;
	    parse_QDIMACS(in, S,quantifiers);
	    gzclose(in);

	    if (S.verbosity > 0){
	        printf("c |  Number of quantifiers:  %12d                                       |\n", quantifiers.size());
		printf("c |  Number of variables:    %12d                                       |\n", S.nVars());
		printf("c |  Number of clauses:      %12d                                       |\n", S.nClauses()); }
	    
	    double parsed_time = cpuTime();
	    if (S.verbosity > 0)
		printf("c |  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

	    // Change to signal-handlers that will only notify the solver and allow it to terminate
	    // voluntarily:
	    signal(SIGINT, SIGINT_interrupt);
	    signal(SIGXCPU,SIGINT_interrupt);

	    Preprocessor preprocessor( &S );
	    preprocessor.preprocess();
	    
	    double simplified_time = cpuTime();
	    if (S.verbosity > 0){
		printf("c |  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
		printf("c |                                                                             |\n"); }

	    // do coprocessing here!

	    if (dimacs){
		if (S.verbosity > 0)
		    printf("c ==============================[ Writing QDIMACS ]===============================\n");
		
		  FILE* res = fopen(dimacs, "wb") ;
		
		  int vars = 0; int cls = 0;
		  preprocessor.getCNFinfo(vars,cls);
		  fprintf(res,"p cnf %u %i\n", vars, cls);
		  // print all the quantifiers again!
		  for( int i = 0 ; i < quantifiers.size(); ++ i ) {
		   fprintf(res,"%c ", quantifiers[i].kind);
		   for( int j = 0 ; j < quantifiers[i].lits.size(); ++ j )
		   { 
		     const int l = toInt(quantifiers[i].lits[j]);
		     if (l % 2 == 0) fprintf(res, "%i ", (l / 2) + 1);
		     else fprintf(res, "-%i ", (l/2) + 1);
		   }
		   fprintf(res, "0\n");
		  }
		  // print the remaining formula!
		  preprocessor.printFormula(res, true);
	    }
	    
        cerr.flush(); cout.flush();
#ifdef NDEBUG
        exit(0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (0);
#endif
    } catch (OutOfMemoryException&){
        printf("c ===============================================================================\n");
	printf("c WARNING: caught an exception \n");
        printf("s UNKNOWN\n");
        exit(0);
    }
}