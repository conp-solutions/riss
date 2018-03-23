
/*****************************************************************************************[Main.cc]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
                CRIL - Univ. Artois, France
                LRI  - Univ. Paris Sud, France

Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------

Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2013, Norbert Manthey, LGPL v2, see LICENSE

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
#include "riss/utils/version.h" // include the file that defines the solver version
#include "riss/core/Dimacs.h"

#include "riss/core/Solver.h"

#include "coprocessor/Coprocessor.h"

#include "riss/core/EnumerateMaster.h" // for model enumeration

using namespace Riss;
using namespace std;

//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();

    double mem_used = memUsedPeak();
    printf("c restarts              : %" PRIu64 " (%" PRIu64 " conflicts in avg)\n", solver.starts, solver.starts == 0 ? 0 : solver.conflicts / solver.starts);
    printf("c blocked restarts      : %" PRIu64 " (multiple: %" PRIu64 ") \n", solver.nbstopsrestarts, solver.nbstopsrestartssame);
    printf("c last block at restart : %" PRIu64 "\n", solver.lastblockatrestart);
    printf("c nb ReduceDB           : %" PRIu64 "\n", solver.nbReduceDB);
    printf("c nb removed Clauses    : %" PRIu64 "\n", solver.nbRemovedClauses);
    printf("c nb learnts DL2        : %" PRIu64 "\n", solver.nbDL2);
    printf("c nb learnts size 2     : %" PRIu64 "\n", solver.nbBin);
    printf("c nb learnts size 1     : %" PRIu64 "\n", solver.nbUn);

    printf("c conflicts             : %-12" PRIu64 "   (%.0f /sec)\n", solver.conflicts, cpu_time == 0 ? 0 : solver.conflicts / cpu_time);
    printf("c decisions             : %-12" PRIu64 "   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, solver.decisions == 0 ? 0 : (float)solver.rnd_decisions * 100 / (float)solver.decisions, cpu_time == 0 ? 0 : solver.decisions / cpu_time);
    printf("c propagations          : %-12" PRIu64 "   (%.0f /sec)\n", solver.propagations, cpu_time == 0 ? 0 : solver.propagations / cpu_time);
    printf("c conflict literals     : %-12" PRIu64 "   (%4.2f %% deleted)\n", solver.tot_literals, solver.max_literals == 0 ? 0 : (solver.max_literals - solver.tot_literals) * 100 / (double)solver.max_literals);
    printf("c nb reduced Clauses    : %" PRIu64 "\n", solver.nbReducedClauses);

    printf("c Memory used           : %.2f MB\n", mem_used);

    printf("c CPU time              : %g s\n", cpu_time);
}


static Solver* solver;

static bool receivedInterupt = false;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum)
{
    printf("\n"); printf("c *** INTERRUPTED ***\n");
//     if (solver->verbosity > 0){
//         printStats(*solver);
//         printf("\n"); printf("c *** INTERRUPTED ***\n"); }
    solver->interrupt();
    if (receivedInterupt) { _exit(1); }
    else { receivedInterupt = true; }
}


//=================================================================================================
// Main:


int main(int argc, char** argv)
{

    setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
    // Extra options:
    //
    IntOption    verb("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
    IntOption    vv("MAIN", "vv",   "Verbosity every vv conflicts", 10000, IntRange(1, INT32_MAX));
    IntOption    cpu_lim("MAIN", "cpu-lim", "Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
    IntOption    mem_lim("MAIN", "mem-lim", "Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

    StringOption proofFile("PROOF", "proof", "Write a proof trace into the given file", 0);
    StringOption opt_proofFormat("PROOF", "proofFormat", "Do print the proof format (print o line with the given format, DRUP or DRAT)", "DRAT");


    StringOption opt_config("MAIN", "config", "Use a preset configuration", "700");
    IntOption    opt_maxConflicts("MAIN", "maxConflicts", "Limit the number of conflicts for the search.\n", -1, IntRange(-1, INT32_MAX));
    BoolOption   opt_checkModel("MAIN", "checkModel", "verify model inside the solver before printing (if input is a file)", false);
    BoolOption   opt_modelStyle("MAIN", "oldModel",   "present model on screen in old format", false);
    BoolOption   opt_quiet("MAIN", "quiet",      "Do not print the model", false);
    BoolOption   opt_parseOnly("MAIN", "parseOnly", "abort after parsing", false);
    BoolOption   opt_cmdLine("MAIN", "cmd", "print the relevant options", false);
    BoolOption   opt_showParam("MAIN", "showUnusedParam", "print parameters after parsing", false);
    IntOption    opt_helpLevel("MAIN", "helpLevel", "Show only partial help.\n", -1, IntRange(-1, INT32_MAX));
    BoolOption   opt_autoconfig("MAIN", "auto", "pick a configuratoin automatically", false);

    IntOption    opt_assumeFirst("MAIN", "assumeFirst", "Assume the first X positive literals for search.", 0, IntRange(0, INT32_MAX));

    IntOption    opt_tuneGranularity("PARAMETER CONFIGURATION", "pcs-granularity", "Sample intervals into given number of values, 0=use intervals.\n", 0, IntRange(0, INT32_MAX));
    IntOption    opt_tuneLevel("PARAMETER CONFIGURATION", "pcs-dLevel", "dependency level to be considered (-1 = all).\n", -1, IntRange(-1, INT32_MAX));
    StringOption opt_tuneFile("PARAMETER CONFIGURATION", "pcs-file",   "File to write configuration to (exit afterwards)", 0);

    Int64Option  opt_enumeration("MODEL ENUMERATION", "models",     "number of models to be found (0=all)\n", -1, Int64Range(-1, INT64_MAX));
    IntOption    opt_enuMinimize("MODEL ENUMERATION", "modelMini",  "minimize blocking clause (0=no,1=from full,2=also from blocking)\n", 2, IntRange(0, 2));
    BoolOption   opt_enumPrintOFT("MODEL ENUMERATION", "enuOnline", "print model as soon as it has been found", true);
    BoolOption   opt_enumPRnbt("MODEL ENUMERATION", "models-NBT", "use backtracking enumeration (after first model)", false);
    StringOption opt_projectionFile("MODEL ENUMERATION", "modelScope", "file that stores enumeration projection\n", 0);
    StringOption opt_modelFile("MODEL ENUMERATION", "modelsFile", "file to store models to\n", 0);
    StringOption opt_fullModelFile("MODEL ENUMERATION", "fullModels", "file to store full models to\n", 0);
    StringOption opt_DNFfile("MODEL ENUMERATION", "dnf-file",   "file to store (reduced) DNF\n",  0);

    try {

        //
        // here the solver starts with its actual work ...
        //
        bool foundHelp = ::parseOptions(argc, argv);   // parse all global options
        CoreConfig* coreConfig = new CoreConfig(string(opt_config == 0 ? "" : opt_config));
        Coprocessor::CP3Config* cp3config = new Coprocessor::CP3Config(string(opt_config == 0 ? "" : opt_config));
        foundHelp = coreConfig->parseOptions(argc, argv, false, opt_helpLevel) || foundHelp;
        foundHelp = cp3config->parseOptions(argc, argv, false, opt_helpLevel) || foundHelp;
        if (foundHelp) { exit(0); }  // stop after printing the help information

        // print pcs information into file
        if (0 != (const char*)opt_tuneFile) {
            FILE* pcsFile = fopen((const char*) opt_tuneFile, "wb");  // open file
            fprintf(pcsFile, "# PCS Information for riss (core) %s  %s \n#\n#\n# Global Parameters\n#\n#\n", solverVersion, gitSHA1);
            // ::printOptions(pcsFile, opt_tuneLevel,opt_tuneGranularity); // do not print the global options, as those are usually not relevant for tuning
            fprintf(pcsFile, "\n\n#\n#\n# Search Parameters\n#\n#\n");
            coreConfig->printOptions(pcsFile, opt_tuneLevel, opt_tuneGranularity);
            fprintf(pcsFile, "\n\n#\n#\n# Simplification Parameters\n#\n#\n");
            cp3config->printOptions(pcsFile, opt_tuneLevel, opt_tuneGranularity);
            fprintf(pcsFile, "\n\n#\n#\n# Dependencies \n#\n#\n");
//             fprintf(pcsFile, "\n\n#\n#\n# Global Dependencies \n#\n#\n");
//             ::printOptionsDependencies(pcsFile, opt_tuneLevel);
            fprintf(pcsFile, "\n\n#\n#\n# Search Dependencies \n#\n#\n");
            coreConfig->printOptionsDependencies(pcsFile, opt_tuneLevel, opt_tuneGranularity);
            fprintf(pcsFile, "\n\n#\n#\n# Simplification Dependencies \n#\n#\n");
            cp3config->printOptionsDependencies(pcsFile, opt_tuneLevel, opt_tuneGranularity);
            fclose(pcsFile);
            exit(0);
        }

        if (opt_cmdLine) {  // print the command line options
            std::stringstream s;
            coreConfig->configCall(s);
            cp3config->configCall(s);
            configCall(argc, argv, s);
            cerr << "c tool-parameters: " << s.str() << endl;
            exit(0);
        }

        if (opt_showParam) {  // print remaining parameters
            cerr << "c call after parsing options: ";
            for (int i = 0 ; i < argc; ++i) { cerr << " " << argv[i]; }
            cerr << endl;
        }
        Solver* S = new Solver(coreConfig);
        S->setPreprocessor(cp3config); // tell solver about preprocessor

        double initial_time = cpuTime();

        S->verbosity = verb;
        S->verbEveryConflicts = vv;

        #if defined(__linux__)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
        if (verb > 0) { printf("c WARNING: for repeatability, setting FPU to use double precision\n"); }
        #endif

        solver = S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU, SIGINT_exit);

        // Set limit on CPU-time:
        if (cpu_lim != INT32_MAX) {
            receivedInterupt = true; // make sure the next interrupt hits exit
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max) {
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1) {
                    printf("c WARNING! Could not set resource limit: CPU-time.\n");
                }
            }
        }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX) {
            receivedInterupt = true; // make sure the next interrupt hits exit
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024 * 1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max) {
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1) {
                    printf("c WARNING! Could not set resource limit: Virtual memory.\n");
                }
            }
        }

        if (argc == 1) {
            printf("c Reading from standard input... Use '--help' for help.\n");
        }

        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == nullptr) {
            printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
        }

        if (S->verbosity > 0) { // print only once!
            printf("c =================[ riss (core) %s  %30s ]===================================\n", solverVersion, gitSHA1);
            printf("c | Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
            printf("c | Based on Minisat 2.2 and Glucose 2.1  -- thanks!                                                      |\n");
            printf("c | Contributors:                                                                                         |\n");
            printf("c |      Kilian Gebhardt                                                                                  |\n");
            printf("c |      Lucas Kahlert, Franziska Krüger, Aaron Stephan                                                   |\n");
            printf("c ============================[ Problem Statistics ]=======================================================\n");
            printf("c |                                                                                                       |\n");
        }

        #ifdef CLASSIFIER
        if (opt_autoconfig) {  // do configuration based on integrated configuration database

            if ((argc == 1)) {
                printf("c cannot autoconfig with formula on stdin\n");
                exit(0);
            }

            // TODO FIXME set string according to current database "automatically".
            // Here, the best configuration for the "cut" formulas should be used.
            // The below limits should also be set automatically.
            string config = "505-O";

            parse_DIMACS(in, *S);

            if (S->nClauses() < 4000000 || S->nVars() < 1900000 || S->nTotLits() < 12000000) {

                //gzclose(in);

                CNFClassifier* cnfclassifier = new CNFClassifier(S->ca, S->clauses, S->nVars());
                cnfclassifier->setVerb(verb);
                cnfclassifier->setComputingClausesGraph(false);
                cnfclassifier->setComputingResolutionGraph(false);
                cnfclassifier->setComputingRwh(true);
                cnfclassifier->setComputeBinaryImplicationGraph(true);
                cnfclassifier->setComputeConstraints(true);
                cnfclassifier->setComputeXor(false);
                cnfclassifier->setQuantilesCount(4);
                cnfclassifier->setComputingVarGraph(false);
                cnfclassifier->setAttrFileName(nullptr);
                cnfclassifier->setComputingDerivative(true);

                config = cnfclassifier->getConfig(*S);
                // get new autoconfigured config

                delete cnfclassifier;

            }

            delete S;
            delete coreConfig;
            delete cp3config;

            // set up the new configuration
            coreConfig = new CoreConfig(config.c_str());
            cp3config = new Coprocessor::CP3Config(config.c_str());   // use new and pointer

            // reset the solver with the new configuration
            S = new Solver(coreConfig);
            S->setPreprocessor(cp3config); // tell solver about preprocessor
            S->verbosity = verb;
            S->verbEveryConflicts = vv;

            gzclose(in); // reopening the formula file. (old one refers to EOF)
            in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
            if (S->verbosity > 0) { printf("c |  Config: %12s                                                                                |\n", config.c_str()); }
        }
        #endif // CLASSIFIER

        // open file for proof
        S->proofFile = (proofFile) ? (string(proofFile) == "stderr" ? stderr : fopen((const char*) proofFile, "wb")) : nullptr ;
        if (opt_proofFormat && strlen(opt_proofFormat) > 0 && S->proofFile != nullptr) { fprintf(S->proofFile, "o proof %s\n", (const char*)opt_proofFormat); }    // we are writing proofs of the given format!

        parse_DIMACS(in, *S);
        //printf("\n%d\n", S->nClauses());
        gzclose(in);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : nullptr;

        double parsed_time = cpuTime();
        if (S->verbosity > 0) {
            printf("c |  Number of variables:       %12d                                                              |\n", S->nVars());
            printf("c |  Number of clauses:         %12d                                                              |\n", S->nClauses());
            printf("c |  Number of total literals:  %12d                                                              |\n", S->nTotLits());
            printf("c |  Parse time:                %12.2f s                                                            |\n", parsed_time - initial_time);
            printf("c |                                                                                                       |\n");
        }

        if (opt_parseOnly) { exit(0); }  // simply stop here!

        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        //signal(SIGINT, SIGINT_interrupt);
        //signal(SIGXCPU,SIGINT_interrupt);

        if (!S->simplify()) {
            if (res != nullptr) {
                if (opt_modelStyle) { fprintf(res, "UNSAT\n"), fclose(res); }
                else { fprintf(res, "s UNSATISFIABLE\n"), fclose(res); }
                res = nullptr;
            }
            // add the empty clause to the proof, close proof file
            if (S->proofFile != nullptr) {
                lbool validProof = S->checkProof(); // check the proof that is generated inside the solver
                if (verb > 0) { cerr << "c checked proof, valid= " << (validProof == l_Undef ? "?  " : (validProof == l_True ? "yes" : "no ")) << endl; }
                fprintf(S->proofFile, "0\n");
                if (S->proofFile != stderr) { fclose(S->proofFile); }
            }
            if (S->verbosity > 0) {
                printf("c =========================================================================================================\n");
                printf("c Solved by unit propagation\n");
                printStats(*S);
            }

            // choose among output formats!
            if (opt_modelStyle) { printf("UNSAT"); }
            else { printf("s UNSATISFIABLE\n"); }
            cout.flush(); cerr.flush();
            exit(20);
        }

        vec<Lit> dummy;
        // tell solver about the number of conflicts it is allowed to use (for the current iteration)
        if (opt_maxConflicts != -1) { S->setConfBudget(opt_maxConflicts); }
        // assume first literals of the formula
        if (opt_assumeFirst > 0) {
            const int maxV = opt_assumeFirst > S->nVars() ? S->nVars() : opt_assumeFirst;
            for (int i = 0 ; i < maxV; ++i) { dummy.push(mkLit(i, false)); }
        }

        Riss::EnumerateMaster* modelMaster = nullptr;
        if (opt_enumeration != -1) {
            modelMaster = new Riss::EnumerateMaster(S->nVars());
            modelMaster->setMaxModels(opt_enumeration);
            modelMaster->setModelMinimization((int) opt_enuMinimize);
            if (opt_enumPRnbt) { modelMaster->activateNaiveBacktrackingEnumeration(); }

            if (S->getPreprocessor() != nullptr) { modelMaster->setPreprocessor(S->getPreprocessor()) ; }
            if ((const char*) opt_projectionFile != 0) { modelMaster->setProjectionFile((const char*) opt_projectionFile); }
            modelMaster->initEnumerateModels(); // for this method, coprocessor and projection have to be known already!
            modelMaster->setPrintEagerly(opt_enumPrintOFT);

            if ((const char*) opt_modelFile != 0) { modelMaster->setModelFile((const char*) opt_modelFile); }
            if ((const char*) opt_fullModelFile != 0) { modelMaster->setFullModelFile((const char*) opt_fullModelFile); }
            if ((const char*) opt_DNFfile != 0) { modelMaster->setDNFfile((const char*) opt_DNFfile); }

            S->setEnumnerationMaster(modelMaster);   // finally, tell the solver about the enumeration master
        }

        // solve the formula (with the possible created assumptions)
        lbool ret = S->solveLimited(dummy);
        S->budgetOff(); // remove budget again!

        if (modelMaster != nullptr) {  // handle model enumeration
            if (S->verbosity > 0) { printf("c found models: %ld\n", modelMaster->foundModels()); }
            if (modelMaster->foundModels() > 0) {
                modelMaster->writeStreamToFile("", false); // for now, print all models to stderr is fine
                printf("s SATISFIABLE\n");
                if (res != nullptr) { fclose(res); res = nullptr; } // TODO: write result into output file!
                exit(30);
            }
        }

        // have we reached UNKNOWN because of the limited number of conflicts? then continue with the next loop!
        if (ret == l_Undef) {
            if (res != nullptr) { fclose(res); res = nullptr; }
            if (S->proofFile != nullptr && S->proofFile != stderr) {
                fclose(S->proofFile);   // close the current file
                S->proofFile = fopen((const char*) proofFile, "w"); // remove the content of that file
                fclose(S->proofFile);   // close the file again
            }
        }

        // print stats
        if (S->verbosity > 0) {
            printStats(*S);
        }

        // check model of the formula
        if (ret == l_True && opt_checkModel && argc != 1) {  // check the model if the formla was given via a file!
            gzFile in = gzopen(argv[1], "rb"); // re-read file
            if (check_DIMACS(in, S->model)) {
                printf("c verified model\n");
            } else {
                printf("c model invalid -- turn answer into UNKNOWN\n");
                assert(false && "model should be correct");
                ret = l_Undef; // turn result into unknown, because the model is not correct
            }
            gzclose(in);
        }

        // print solution to screen
        if (opt_modelStyle) { printf(ret == l_True ? "SAT\n" : ret == l_False ? "UNSAT\n" : "UNKNOWN\n"); }
        else { printf(ret == l_True ? "s SATISFIABLE\n" : ret == l_False ? "s UNSATISFIABLE\n" : "s UNKNOWN\n"); }

        // put empty clause on proof
        if (ret == l_False && S->proofFile != nullptr) {
            #ifdef DRATPROOF
            lbool validProof = S->checkProof(); // check the proof that is generated inside the solver
            if (verb > 0) { cerr << "c checked proof, valid= " << (validProof == l_Undef ? "?  " : (validProof == l_True ? "yes" : "no ")) << endl; }
            #endif
            fprintf(S->proofFile, "0\n");
        }

        // print solution into file
        if (res != nullptr) {
            if (ret == l_True) {
                if (opt_modelStyle) { fprintf(res, "SAT\n"); }
                else { fprintf(res, "s SATISFIABLE\nv "); }
                for (int i = 0; i < S->model.size(); i++)
                    //  if (S->model[i] != l_Undef) // treat undef simply as falsified (does not matter anyways)
                {
                    fprintf(res, "%s%s%d", (i == 0) ? "" : " ", (S->model[i] == l_True) ? "" : "-", i + 1);
                }
                fprintf(res, " 0\n");
            } else if (ret == l_False) {
                if (opt_modelStyle) { fprintf(res, "UNSAT\n"); }
                else { fprintf(res, "s UNSATISFIABLE\n"); }
            } else if (opt_modelStyle) { fprintf(res, "UNKNOWN\n"); }
            else { fprintf(res, "s UNKNOWN\n"); }
            fclose(res); res = nullptr;
        }

        // print model to screen
        if (! opt_quiet && ret == l_True && res == nullptr) {
            if (!opt_modelStyle) { printf("v "); }
            for (int i = 0; i < S->model.size(); i++)
                //  if (S->model[i] != l_Undef) // treat undef simply as falsified (does not matter anyways)
            {
                printf("%s%s%d", (i == 0) ? "" : " ", (S->model[i] == l_True) ? "" : "-", i + 1);
            }
            printf(" 0\n");
        }

        cout.flush(); cerr.flush();

        #ifdef NDEBUG
        exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
        #else
        return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
        #endif



    } catch (OutOfMemoryException&) {
        // printf("c ===============================================================================\n");
        printf("c Warning: caught an exception\n");
        if (opt_modelStyle) { printf("UNKNOWN\n"); }
        else { printf("s UNKNOWN\n"); }
        exit(0);
    }
}
