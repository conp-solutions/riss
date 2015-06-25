/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2013,      Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>

#include "riss/utils/System.h"
#include "riss/utils/ParseUtils.h"
#include "riss/utils/Options.h"
#include "riss/core/Dimacs.h"
#include "riss/simp/SimpSolver.h"

#include "coprocessor/Coprocessor.h"
#include "mprocessor/WDimacs.h"
#include "riss/utils/version.h" // include the file defining the solver version

using namespace Riss;
using namespace Coprocessor;
using namespace std;

//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    printf("c restarts              : %"PRIu64"\n", solver.starts);
    printf("c conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   / cpu_time);
    printf("c decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions * 100 / (float)solver.decisions, solver.decisions   / cpu_time);
    printf("c propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations / cpu_time);
    printf("c conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals) * 100 / (double)solver.max_literals);
    printf("c Memory used           : %.2f MB\n", mem_used);
    printf("c CPU time              : %g s\n", cpu_time);
}


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum)
{
    printf("\n"); printf("c *** INTERRUPTED ***\n");
    if (solver->verbosity > 0) {
        printStats(*solver);
        printf("\n"); printf("c *** INTERRUPTED ***\n");
    }
    _exit(1);
}


//=================================================================================================
// Main:

int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");

        // Extra options:
        //
        IntOption    verb("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        // BoolOption   pre    ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
        StringOption dimacs("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
        IntOption    cpu_lim("MAIN", "cpu-lim", "Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim", "Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));
        StringOption opt_config("MAIN", "config", "Use a preset configuration", 0);
        BoolOption   opt_cmdLine("MAIN", "cmd", "print the relevant options", false);

        const char* _cat = "COPROCESSOR 3";
        StringOption undoFile(_cat, "undo",   "write information about undoing simplifications into given file (and var map into X.map file)");
        BoolOption   post(_cat, "post",   "perform post processing", false);
        StringOption modelFile(_cat, "model",  "read model from SAT solver from this file");
        IntOption    opt_search(_cat, "search", "perform search until the given number of conflicts", 1, IntRange(0, INT32_MAX));

        CoreConfig coreConfig;
        Coprocessor::CP3Config cp3config;

        bool foundHelp = coreConfig.parseOptions(argc, argv);
        foundHelp = cp3config.parseOptions(argc, argv) || foundHelp;
        ::parseOptions(argc, argv);   // parse all global options
        if (foundHelp) { exit(0); }  // stop after printing the help information
        coreConfig.setPreset(string(opt_config == 0 ? "" : opt_config));
        cp3config.setPreset(string(opt_config == 0 ? "" : opt_config));

        if (opt_cmdLine) {  // print the command line options
            std::stringstream s;
            coreConfig.configCall(s);
            cp3config.configCall(s);
            configCall(argc, argv, s);
            cerr << "c tool-parameters: " << s.str() << endl;
            exit(0);
        }

        Solver S(&coreConfig);
        S.setPreprocessor(&cp3config); // tell solver about preprocessor

        double      initial_time = cpuTime();

        S.verbosity = verb;

        solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU, SIGINT_exit);

        // Set limit on CPU-time:
        if (cpu_lim != INT32_MAX) {
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max) {
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1)
                { printf("c WARNING! Could not set resource limit: CPU-time.\n"); }
            }
        }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX) {
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024 * 1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max) {
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                { printf("c WARNING! Could not set resource limit: Virtual memory.\n"); }
            }
        }

        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        if (!post) {

            if (argc == 1)
            { printf("c Reading from standard input... Use '--help' for help.\n"); }

            gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
            if (in == NULL)
            { printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1); }

            if (S.verbosity > 0) {
                printf("c =========================[ Mprocessor %s  %13s ]=============================================\n", solverVersion, gitSHA1);
                printf("c |  Norbert Manthey. The use of the tool is limited to research only!                                    |\n");
                printf("c | Contributors:                                                                                         |\n");
                printf("c |     Kilian Gebhard: Implementation of BVE, Subsumption, Parallelization                               |\n");
                printf("c ============================[ Problem Statistics ]=======================================================\n");
                printf("c |                                                                                                       |\n");
            }


            // parse the formula, and furthermore also freeze all relaxation literals!
            vec<Weight> literalWeights; // have a vector that knows the weights for all the (relaxation) variables
            unsigned int originalVariables = parse_WCNF(in, S, literalWeights);
            const unsigned int fullVariables = S.nVars(); // all variables, including relaxation variables

            gzclose(in);

            if (S.verbosity > 0) {
                printf("c |  Number of variables:  %12d                                                                   |\n", S.nVars());
                printf("c |  Number of clauses:    %12d                                                                   |\n", S.nClauses());
            }

            double parsed_time = cpuTime();
            if (S.verbosity > 0)
            { printf("c |  Parse time:           %12.2f s                                                                 |\n", parsed_time - initial_time); }

            // Change to signal-handlers that will only notify the solver and allow it to terminate
            // voluntarily:
            signal(SIGINT, SIGINT_interrupt);
            signal(SIGXCPU, SIGINT_interrupt);

            // if we are densing, the the units should be rewritten and kept, and, the soft clauses need to be rewritten!
            if (cp3config.opt_dense) {
                cp3config.opt_dense_keep_assigned = true; // keep the units on the trail!
                cp3config.opt_dense_store_forward = true; // store a forward mapping
            }

            Preprocessor preprocessor(&S , cp3config);
            preprocessor.preprocess();

            double simplified_time = cpuTime();
            if (S.verbosity > 0) {
                printf("c |  Simplification time:  %12.2f s                                                                 |\n", simplified_time - parsed_time);
                printf("c |                                                                                                       |\n");
            }

            // TODO: do not reduce the variables withing the formula!
            if (dimacs) {
                if (S.verbosity > 0)
                { printf("c ==============================[ Writing DIMACS ]=========================================================\n"); }
                //S.toDimacs((const char*)dimacs);
                FILE* wcnfFile = fopen((const char*) dimacs, "wb");
                if (wcnfFile == NULL) {
                    cerr << "c WARNING: could not open file " << (const char*) dimacs << " to write the formula" << endl;
                    exit(3);
                }
                // calculate the header
                const int vars = S.nVars(); // might be smaller already due to dense
                int cls = S.trail.size() + S.clauses.size();
                Weight top = 0;
                for (Var v = 0 ; v < fullVariables; ++ v) {  // for each weighted literal, have an extra clause! use variables before preprocessing (densing)
                    cls = (literalWeights[toInt(mkLit(v))] != 0 ? cls + 1 : cls);
                    cls = (literalWeights[toInt(~mkLit(v))] != 0 ? cls + 1 : cls);
                    top += literalWeights[toInt(mkLit(v))] + literalWeights[toInt(~mkLit(v))];
                }
                top ++;

                // if the option dense is used, these literals are already rewritten by "dense"
                // write header
                fprintf(wcnfFile, "p wcnf %d %d %ld\n", vars, cls, top);
                // write the hard clauses into the formula!
                // print assignments
                for (int i = 0; i < S.trail.size(); ++i) {
                    {
                        fprintf(wcnfFile, "%ld ", top);
                        fprintf(wcnfFile, "%s%d", sign(S.trail[i]) ? "-" : "", var(S.trail[i]) + 1);
                        fprintf(wcnfFile, " 0\n");
                    }
                }
                // print clauses
                for (int i = 0; i < S.clauses.size(); ++i) {
                    const Clause& c = S.ca[S.clauses[i]];
                    if (c.mark()) { continue; }
                    stringstream s;
                    for (int i = 0; i < c.size(); ++i)
                    { s << c[i] << " "; }
                    s << "0" << endl;
                    fprintf(wcnfFile, "%ld %s", top , s.str().c_str());
                }

                // if the option dense is used, these literals need to be adopted!
                // write all the soft clauses (write after hard clauses, because there might be units that can have positive effects on the next tool in the chain!)
                for (Var v = 0 ; v < fullVariables; ++ v) {  // for each weighted literal, have an extra clause!
                    Lit l = mkLit(v);
                    Lit nl = preprocessor.giveNewLit(l);
                    cerr << "c lit " << l << " is compress lit " << nl << endl;
                    if (nl != lit_Undef && nl != lit_Error) { l = nl; }
                    else if (nl == lit_Undef && cp3config.opt_dense) {
                        cerr << "c WARNING: soft literal " << l << " has been removed from the formula" << endl;
                        continue;
                    }
                    if (literalWeights[toInt(mkLit(v))] != 0) {  // setting literal l to true ha a cost, hence have unit!
                        fprintf(wcnfFile, "%ld %d 0\n", literalWeights[toInt(mkLit(v))], var(l) + 1); // use the weight of the not rewritten literal!
                    }
                    if (literalWeights[toInt(~mkLit(v))] != 0) {  // setting literal ~l to true ha a cost, hence have unit!
                        fprintf(wcnfFile, "%ld %d 0\n", literalWeights[toInt(~mkLit(v))], -var(l) - 1); // use the weight of the not rewritten literal!
                    }
                }
                // close the file
                fclose(wcnfFile);
            }

            if ((const char*)undoFile != 0) {
                if (S.verbosity > 0)
                { printf("c =============================[ Writing Undo Info ]=======================================================\n"); }
                preprocessor.writeUndoInfo(string(undoFile), originalVariables);
            }

            if (!S.okay()) {
                if (res != NULL) { fprintf(res, "s UNSATISFIABLE\n"); fclose(res); cerr << "s UNSATISFIABLE" << endl; }
                else { printf("s UNSATISFIABLE\n"); }
                if (S.verbosity > 0) {
                    printf("c =========================================================================================================\n");
                    printf("c Solved by simplification\n");
                    printStats(S);
                    printf("\n");
                }
                cerr.flush(); cout.flush();
                #ifdef NDEBUG
                exit(20);     // (faster than "return", which will invoke the destructor for 'Solver')
                #else
                return (20);
                #endif
            } else {
                if (res != NULL) { fprintf(res, "s UNKNOWN\n"); fclose(res); cerr << "s UNKNOWN" << endl; }
                #ifdef NDEBUG
                exit(0);     // (faster than "return", which will invoke the destructor for 'Solver')
                #else
                return (0);
                #endif
            }
        } else {
//
// process undo here!
//
            Preprocessor preprocessor(&S , cp3config);
            if (undoFile == 0) {
                cerr << "c ERROR: no undo file specified" << endl;
                exit(1);
            }

            int solution = preprocessor.parseModel(modelFile == 0 ? string("") : string(modelFile));
            bool good = solution != -1;
            good = good && preprocessor.parseUndoInfo(string(undoFile));
            if (!good) { cerr << "c will not continue because of above errors" << endl; return 1; }

            if (solution == 10) {
                preprocessor.extendModel(S.model);
                if (res != NULL) {
                    printf("s SATISFIABLE\n");
                    fprintf(res, "s SATISFIABLE\nv ");
                    for (int i = 0; i < preprocessor.getFormulaVariables(); i++)
                        if (S.model[i] != l_Undef)
                        { fprintf(res, "%s%s%d", (i == 0) ? "" : " ", (S.model[i] == l_True) ? "" : "-", i + 1); }
                    fprintf(res, " 0\n");
                } else {
                    printf("s SATISFIABLE\nv ");
                    for (int i = 0; i < preprocessor.getFormulaVariables(); i++)
                        if (S.model[i] != l_Undef)
                        { printf("%s%s%d", (i == 0) ? "" : " ", (S.model[i] == l_True) ? "" : "-", i + 1); }
                    printf(" 0\n");
                }
                cerr.flush(); cout.flush();
                #ifdef NDEBUG
                exit(10);     // (faster than "return", which will invoke the destructor for 'Solver')
                #else
                return (10);
                #endif
            } else if (solution == 20) {
                if (res != NULL) { fprintf(res, "s UNSATISFIABLE\n"), fclose(res); }
                printf("s UNSATISFIABLE\n");
                cerr.flush(); cout.flush();
                #ifdef NDEBUG
                exit(20);     // (faster than "return", which will invoke the destructor for 'Solver')
                #else
                return (20);
                #endif
            } else {
                if (res != NULL) { fprintf(res, "s UNKNOWN\n"), fclose(res); }
                printf("s UNKNOWN\n");
            }

        }

        cerr.flush(); cout.flush();
        #ifdef NDEBUG
        exit(0);     // (faster than "return", which will invoke the destructor for 'Solver')
        #else
        return (0);
        #endif
    } catch (OutOfMemoryException&) {
        printf("c =========================================================================================================\n");
        printf("s UNKNOWN\n");
        exit(0);
    }
}
