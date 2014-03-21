/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2012,      Norbert Manthey

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

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
#include <sys/resource.h>

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "core/Dimacs.h"
#include "pcasso-src/SplitterSolver.h"

#include "pcasso-src/Master.h"

// for parallelization
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

using namespace Pcasso;

//=================================================================================================


void printStats(SplitterSolver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    fprintf( stderr,"restarts              : %"PRIu64"\n", solver.starts);
    fprintf( stderr,"conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    fprintf( stderr,"decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    fprintf( stderr,"propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    fprintf( stderr,"conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) fprintf( stderr,"Memory used           : %.2f MB\n", mem_used);
    fprintf( stderr,"CPU time              : %g s\n", cpu_time);
}


static Master* master;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
// static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).

static void SIGINT_exit(int signum) {
    fprintf( stderr,"\n"); fprintf( stderr,"*** INTERRUPTED ***\n");
    master->shutdown();
}
//    if (solver->verbosity > 0){
//       printStats(*solver);
//       fprintf( stderr,"\n"); fprintf( stderr,"*** INTERRUPTED ***\n"); }
//    _exit(1); }


struct parameter {
	IntOption    verb;
	BoolOption   pre;
	StringOption dimacs;
};

struct shareStruct{
	parameter* param;
	int result;
	int threadnr;
	int* printedAlready;
	pthread_mutex_t* mutex;
	sem_t* wakeSem;
	shareStruct():
		result(0),
		threadnr(0)
	{}
};


// declare
void* solverMain(void* pointer);


int main(int argc, char** argv){

	setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
	// fprintf( stderr,"This is MiniSat 2.0 beta\n");
        
#if defined(__linux__)
	fpu_control_t oldcw, newcw;
	_FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
	fprintf( stderr,"WARNING: for repeatability, setting FPU to use double precision\n");
#endif
	// Extra options:
	//

	IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
	BoolOption   pre    ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
	StringOption dimacs ("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
	IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
	IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

	parseOptions(argc, argv, true);	
        
	// Set limit on CPU-time:
        if (cpu_lim != INT32_MAX){
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1)
                    fprintf( stderr,"WARNING! Could not set resource limit: CPU-time.\n");
            } }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX){
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    fprintf( stderr,"WARNING! Could not set resource limit: Virtual memory.\n");
            } }
	
	Master::Parameter p;
	p.pre = pre;
	p.verb = verb;
	if(dimacs){
	  p.dimacs = (const char*)dimacs;
	} else {
	   p.dimacs = "";
	}
	
	Master m(p);
        master = &m;
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU,SIGINT_exit);

	return m.main( argc, argv );
/*

	parameter p;
  

	int threads = 2;
	shareStruct shares[threads];


	p.argc = argc;
	p.argv = argv;
	
	// first one that prints will set it to 1 -> no other can print its solution
	int printedAlready = 0;
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	
	sem_t sleepSem;
	sem_init(&(sleepSem), 0, 0);

	pthread_t threaddata [threads];
	// Make threads Joinable for sure.
	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);
	
	int ret = 0;
	for( int i = 0 ; i < threads ; ++i ){
		shares[i].param=&p;
		shares[i].printedAlready = &printedAlready;
		shares[i].mutex = &mutex;
		shares[i].wakeSem = &sleepSem;
		
		int rc = pthread_create(&(threaddata[i]), &attr, solverMain, (void *)  &(shares[i]));
		if (rc) {
			fprintf( stderr,"ERROR; return code from pthread_create() is %d for thread %d\n",rc,i);
		}
	}

	// wait for one of the parallel solvers to return a solution
	sem_wait( &sleepSem );

	for( int i = 0 ; i < threads ; ++i ){
		int* status = 0;
		int err = 0;
		fprintf( stderr,"c try to join thread %d\n", i);
		err = pthread_join(threaddata[i], (void**)&status);
		if( err == EINVAL ) fprintf( stderr,"c tried to cancel wrong thread\n");
		if( err == ESRCH ) fprintf( stderr, "c specified thread does not exist\n");
		fprintf( stderr,"c joined thread %d\n", i);
		fprintf( stderr,"Solver %d returned value %d\n", i , shares[i].result );
	}

	pthread_mutex_destroy(&mutex);
	
	for( int i = 0 ; i < threads ; ++i ){
		if( shares[i].result!=0) return shares[i].result;
	}
	fprintf( stderr,"done\n");
	return ret;
	*/

}

//=================================================================================================
// Main:

void* solverMain(void* pointer)
{
  #if 0
		shareStruct* share = (shareStruct*) pointer;
		parameter& p = *(share->param);
		
		// enable canceling
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);

    try {

        SimpSolver  S;
        double      initial_time = cpuTime();

        if (!p.pre) S.eliminate(true);

        S.verbosity = p.verb;
        
        solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU,SIGINT_exit);


        
        if (p.argc == 1)
            fprintf( stderr,"Reading from standard input... Use '--help' for help.\n");

        gzFile in = (p.argc == 1) ? gzdopen(0, "rb") : gzopen(p.argv[1], "rb");
        if (in == NULL)
            fprintf( stderr,"ERROR! Could not open file: %s\n", p.argc == 1 ? "<stdin>" : p.argv[1]), exit(1);
        
        if (S.verbosity > 0){
            fprintf( stderr,"============================[ Problem Statistics ]=============================\n");
            fprintf( stderr,"|                                                                             |\n"); }
        
        parse_DIMACS(in, S);
        gzclose(in);
        

        if (S.verbosity > 0){
            fprintf( stderr,"|  Number of variables:  %12d                                         |\n", S.nVars());
            fprintf( stderr,"|  Number of clauses:    %12d                                         |\n", S.nClauses()); }
        
        double parsed_time = cpuTime();
        if (S.verbosity > 0)
            fprintf( stderr,"|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        signal(SIGINT, SIGINT_interrupt);
        signal(SIGXCPU,SIGINT_interrupt);

        S.eliminate(true);
        double simplified_time = cpuTime();
        if (S.verbosity > 0){
            fprintf( stderr,"|  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
            fprintf( stderr,"|                                                                             |\n"); }

        if (!S.okay()){
					// print? -> mutex!
        	pthread_mutex_lock( share->mutex );
        	if( *(share->printedAlready) == 0 ){
		      	*(share->printedAlready) = 1;        
		      	FILE* res = (p.argc >= 3) ? fopen(p.argv[2], "wb") : NULL;
		        if (res != NULL) ffprintf( stderr,res, "UNSAT\n"), fclose(res);
            
            if (S.verbosity > 0){
                fprintf( stderr,"===============================================================================\n");
                fprintf( stderr,"Solved by simplification\n");
                printStats(S);
                fprintf( stderr,"\n"); 
						}
				    fprintf( stderr,"UNSATISFIABLE\n");

          }
				  pthread_mutex_unlock( share->mutex );
				  share->result = 20;
				  sem_post( share->wakeSem );
          return 0;
        }

        if (p.dimacs){
            if (S.verbosity > 0)
                fprintf( stderr,"==============================[ Writing DIMACS ]===============================\n");
            S.toDimacs((const char*)p.dimacs);
            if (S.verbosity > 0)
                printStats(S);
            exit(0);
        }

        vec<Lit> dummy;
        lbool ret = S.solveLimited(dummy);
        
        // do print only, if no other thread has already printed 
        pthread_mutex_lock( share->mutex );
        if( *(share->printedAlready) == 0 ){
        	*(share->printedAlready) = 1;
		      if (S.verbosity > 0){
		          printStats(S);
		          fprintf( stderr,"\n"); }
		      fprintf( stderr,ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
		      
		      FILE* res = (p.argc >= 3) ? fopen(p.argv[2], "wb") : NULL;
		      if (res != NULL){
		          if (ret == l_True){
		              fprintf(res, "SAT\n");
		              for (int i = 0; i < S.nVars(); i++)
		                  if (S.model[i] != l_Undef)
		                      fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
		              fprintf(res, " 0\n");
		          }else if (ret == l_False)
		              fprintf(res, "UNSAT\n");
		          else
		              fprintf(res, "INDET\n");
		          fclose(res);
		      }
				}
				pthread_mutex_unlock( share->mutex );

        // let thread starter know about return value
				share->result = (int)(ret == l_True ? 10 : ret == l_False ? 20 : 0);
				sem_post( share->wakeSem );
        return 0;

    } catch (OutOfMemoryException&){
        fprintf( stderr,"===============================================================================\n");
        fprintf( stderr,"INDETERMINATE\n");
        sem_post( share->wakeSem );
        exit(0);
    }
    #endif
    return 0;
}
