/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>
#include "CNFClassifier.h"

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "core/Dimacs.h"
#include "core/Solver.h"

#include "coprocessor-src/Coprocessor.h"

#include "VERSION" // include the file that defines the solver version

using namespace Minisat;
using namespace Coprocessor;

//=================================================================================================
static bool receivedInterupt = false;

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("c *** INTERRUPTED ***\n");
    if( receivedInterupt ) _exit(1);
    else receivedInterupt = true;
}


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
	
	// parse the options
        parseOptions(argc, argv, true);
        
        CoreConfig coreConfig;
        coreConfig.parseOptions(argc, argv, true);

	
        Solver S(coreConfig);
	
        double      initial_time = cpuTime();
        S.verbosity = verb;

        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
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

	FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

	  if (argc == 1)
	      printf("c Reading from standard input... Use '--help' for help.\n");

	  gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
	  if (in == NULL)
	      printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
	  
	  if (S.verbosity > 0) {
  	      printf("c ==========================[   CNF Classifier %5.2f   ]===================================================\n", solverVersion);
	      printf("c |  Norbert Manthey. The use of the tool is limited to research only!                                    |\n");
  	      printf("c | Contributors:                                                                                         |\n");
	      printf("c |     Enrique Matos Alfonso: Implementation of the classifier                                           |\n");
	      printf("c ============================[ Problem Statistics ]=======================================================\n");
	      printf("c |                                                                                                       |\n"); }
	      
	    parse_DIMACS(in, S);
	    gzclose(in);

	    if (S.verbosity > 0){
		printf("c |  Number of variables:  %12d                                                                  |\n", S.nVars());
		printf("c |  Number of clauses:    %12d                                                                   |\n", S.nClauses()); }
	    
	    double parsed_time = cpuTime();
	    if (S.verbosity > 0)
		printf("c |  Parse time:           %12.2f s                                                                 |\n", parsed_time - initial_time);
	 
 /**
  * 
  *  Norbert: Here comes you code. 
  *  Most easy way is to extract everything from the SAT solver, and pass it to some other class without the need to be a friend of the solver class.
  *  I already implemented some basic class for you, where you can add the features and their extraction. 
  * 
  */
	   CNFClassifier classifier( S.ca, S.clauses, S.nVars() );
	   vector<double> features = classifier.extractFeatures();
	   vector<string> names = classifier.featureNames();
	   
	   // print features
	   for( int i = 0 ; i < features.size(); ++ i ) {
	     cerr << "c " << names[i] << " " << features[i] << endl; 
	   }
	   
  /**
   *  given the features, here should come a call to a classifier, to return the ID/name/someString of the class, that has been chosen, and maybe a propability how sure the algorithm is (if possible)
   *    for learning the classifier, another method might be specified, which can read all the features of many instances at once!
   */
  
  
  /**
   *  that's it for now
   */

    } catch (OutOfMemoryException&){
        printf("c =========================================================================================================\n");
        printf("s UNKNOWN\n");
        exit(0);
    }
}
