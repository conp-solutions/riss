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

#include "riss/utils/System.h"
#include "riss/utils/ParseUtils.h"
#include "riss/utils/Options.h"
#include "riss/core/Dimacs.h"
#include "pcasso/SplitterSolver.h"

#include "pcasso/Master.h"

// for parallelization
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "pcasso/version.h"

using namespace Pcasso;

//=================================================================================================


void printStats(SplitterSolver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    fprintf(stderr, "restarts              : %"PRIu64"\n", solver.starts);
    fprintf(stderr, "conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   / cpu_time);
    fprintf(stderr, "decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions * 100 / (float)solver.decisions, solver.decisions   / cpu_time);
    fprintf(stderr, "propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations / cpu_time);
    fprintf(stderr, "conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals) * 100 / (double)solver.max_literals);
    if (mem_used != 0) { fprintf(stderr, "Memory used           : %.2f MB\n", mem_used); }
    fprintf(stderr, "cCPU time              : %g s\n", cpu_time);
}


static Master* master;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
// static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).

// static void SIGINT_exit(int signum)
// {
//     fprintf(stderr, "\n"); fprintf(stderr, "*** INTERRUPTED ***\n");
//     master->shutdown();
// }
//    if (solver->verbosity > 0){
//       printStats(*solver);
//       fprintf( stderr,"\n"); fprintf( stderr,"*** INTERRUPTED ***\n"); }
//    _exit(1); }

// #error TODO FIXME react on signal, or disable signals completely

struct parameter {
    IntOption    verb;
    BoolOption   pre;
    StringOption dimacs;
};

struct shareStruct {
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

int main(int argc, char** argv)
{

    setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
    // fprintf( stderr,"This is MiniSat 2.0 beta\n");

    #if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    fprintf(stderr, "WARNING: for repeatability, setting FPU to use double precision\n");
    #endif
    // Extra options:
    //

    IntOption    verb("MAIN", "verb",   "Verbosity level of running solvers (0=silent, 1=some, 2=more).", 0, IntRange(0, 2));
    BoolOption   pre("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
    StringOption dimacs("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
    IntOption    cpu_lim("MAIN", "cpu-lim", "Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
    IntOption    mem_lim("MAIN", "mem-lim", "Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));
    BoolOption   opt_checkModel("MAIN", "checkModel", "verify model inside the solver before printing (if input is a file)", false);
    BoolOption   opt_modelStyle("MAIN", "oldModel",   "present model on screen in old format", false);
    BoolOption   opt_quiet("MAIN", "quiet",      "Do not print the model", false);

    bool foundHelp = ::parseOptions(argc, argv, true);
    if (foundHelp) { exit(0); }   // stop after printing the help information

    // Set limit on CPU-time:
    if (cpu_lim != INT32_MAX) {
        rlimit rl;
        getrlimit(RLIMIT_CPU, &rl);
        if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max) {
            rl.rlim_cur = cpu_lim;
            if (setrlimit(RLIMIT_CPU, &rl) == -1) {
                fprintf(stderr, "WARNING! Could not set resource limit: CPU-time.\n");
            }
        }
    }

    // Set limit on virtual memory:
    if (mem_lim != INT32_MAX) {
        rlim_t new_mem_lim = (rlim_t)mem_lim * 1024 * 1024;
        rlimit rl;
        getrlimit(RLIMIT_AS, &rl);
        if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max) {
            rl.rlim_cur = new_mem_lim;
            if (setrlimit(RLIMIT_AS, &rl) == -1) {
                fprintf(stderr, "WARNING! Could not set resource limit: Virtual memory.\n");
            }
        }
    }

    fprintf(stderr, "c ===============================[ Pcasso  %13s ]=================================================\n", Pcasso::gitSHA1);
    fprintf(stderr, "c | PCASSO - a Parallel, CooperAtive Sat SOlver                                                           |\n");
    fprintf(stderr, "c |                                                                                                       |\n");
    fprintf(stderr, "c | Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
    fprintf(stderr, "c | Contributors:                                                                                         |\n");
    fprintf(stderr, "c |     Davide Lanti (clause sharing, extended conflict analysis)                                         |\n");
    fprintf(stderr, "c |     Ahmed Irfan  (LA partitioning, information sharing      )                                         |\n");
    fprintf(stderr, "c |                                                                                                       |\n");

    Master::Parameter p;
    p.pre = pre;
    p.verb = verb;

    Master pcassoMasert(p);
    master = &pcassoMasert;
    
#error have working signal for hybrid pcasso
//     signal(SIGINT, SIGINT_exit);
//     signal(SIGXCPU, SIGINT_exit);

    if (argc == 1) {
        printf("c Reading from standard input... Use '--help' for help.\n");
    }
    gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
    if (in == nullptr) {
        printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
    }

    parse_DIMACS(in, pcassoMasert.getGlobalSolver());
    gzclose(in);

    FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : nullptr;
    if (!pcassoMasert.getGlobalSolver().simplify()) {
            if (res != nullptr) {
                if (opt_modelStyle) { fprintf(res, "UNSAT\n"), fclose(res); }
                else { fprintf(res, "s UNSATISFIABLE\n"), fclose(res); }
            }
            // add the empty clause to the proof, close proof file
            // choose among output formats!
            if (opt_modelStyle) { printf("UNSAT"); }
            else { printf("s UNSATISFIABLE\n"); }
            cout.flush(); cerr.flush();
            exit(20);
        }

   vec<Lit> dummy;
   lbool ret = pcassoMasert.solveLimited(dummy); // solve formula with master, and given assumptions

       // check model of the formula
        if (ret == l_True && opt_checkModel && argc != 1) {   // check the model if the formla was given via a file!
            if (check_DIMACS(in, *(pcassoMasert.model))) {
                printf("c verified model\n");
            } else {
                printf("c model invalid -- turn answer into UNKNOWN\n");
                ret = l_Undef; // turn result into unknown, because the model is not correct
            }
        }

        // print solution to screen
        if (opt_modelStyle) { printf(ret == l_True ? "SAT\n" : ret == l_False ? "UNSAT\n" : "UNKNOWN\n"); }
        else { printf(ret == l_True ? "s SATISFIABLE\n" : ret == l_False ? "s UNSATISFIABLE\n" : "s UNKNOWN\n"); }

        // put empty clause on proof
        // if (ret == l_False && m.getDrupFile() != nullptr) { fprintf(m.getDrupFile(), "0\n"); }

        // print solution into file
        if (res != nullptr) {
            if (ret == l_True) {
                if (opt_modelStyle) { fprintf(res, "SAT\n"); }
                else { fprintf(res, "s SATISFIABLE\nv "); }
                for (int i = 0; i < pcassoMasert.model->size(); i++)
                {
                    fprintf(res, "%s%s%d", (i == 0) ? "" : " ", (*(pcassoMasert.model)[i] == l_True) ? "" : "-", i + 1);
                }
                fprintf(res, " 0\n");
            } else if (ret == l_False) {
                if (opt_modelStyle) { fprintf(res, "UNSAT\n"); }
                else { fprintf(res, "s UNSATISFIABLE\n"); }
            } else if (opt_modelStyle) { fprintf(res, "UNKNOWN\n"); }
            else { fprintf(res, "s UNKNOWN\n"); }
            fclose(res);
        }

        // print model to screen
        if (! opt_quiet && ret == l_True && res == nullptr) {
            if (!opt_modelStyle) { printf("v "); }
            for (int i = 0; i < pcassoMasert.model->size(); i++)
            {
                printf("%s%s%d", (i == 0) ? "" : " ", ( (*pcassoMasert.model)[i] == l_True) ? "" : "-", i + 1);
            }
            printf(" 0\n");
        }

        cout.flush(); cerr.flush();

   #ifdef NDEBUG
   exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
   #else
   return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
   #endif

}

