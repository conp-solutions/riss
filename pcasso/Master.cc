/*****************************************************************************************[master.cc]
Copyright (c) 2012,      Norbert Manthey, Antti Hyvärinen

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

/** Davide's notes
   In master we never access the shared_pool. The clauses on the path
   leading to the current node are marked as unsafe and added to the
   node's constraints.
 **/

#include <time.h>
#include <sched.h> // to pin threads to cores
#include <sys/stat.h>
#include <string>

#include "pcasso/Master.h"
#include "pcasso/SplitterSolver.h"
#include "pcasso/SolverPT.h"
#include "pcasso/SolverRiss.h"
#include "pcasso/SolverPriss.h"
#include "pcasso/InstanceSolver.h"
#include "pcasso/LookaheadSplitting.h"
#include "pcasso/vsidsSplitting.h"
#include "pcasso/version.h"

using namespace Riss;
using namespace std;

namespace Pcasso
{

// ## begin of automatically handled section
std::string splitString = std::string("how to split the tree: ")
                          + std::string("simple,")
                          + std::string("vsidsScattering,")
                          + std::string("lookaheadSplitting,")
                          + std::string("score,")
                          // ## add your new splitting here (above)
                          ;
// ## number of splittings, will be increased automatically when a new splitting is added via the script
static const int splitMethodNumbers = 3 ;
// ## end of automatically handled section


// Davide> Options for clauses sharing
static IntOption opt_pool_size("SPLITTER + SHARING", "shpool-size", "Specifies the size of the shared clauses pools, in kb\n", 5000, IntRange(0, INT32_MAX));


static BoolOption opt_conflict_killing("SPLITTER + SHARING", "c-killing", "Conflict killing", false);

static IntOption     split_mode("SPLITTER", "split-mode",  splitString.c_str(), 2, IntRange(0, splitMethodNumbers));
static DoubleOption  work_timeout("SPLITTER", "work-timeout", "timeout for worker theards (seconds).\n", -1, DoubleRange(-1, true, 2000000000, true));
static IntOption     work_conflicts("SPLITTER", "work-conflicts", "limit for conflicts in a working thread.\n", -1, IntRange(-1, INT32_MAX));
static DoubleOption  split_timeout("SPLITTER", "split-timeout", "timeout for splitter theards (seconds).\n", 1024, DoubleRange(-1, true, 2000000000, true));
static IntOption     work_threads("SPLITTER", "threads", "number of threads that should be used.\n", 1, IntRange(1, 64));
//static IntOption     solve_mode        ("SPLITTER", "solve-mode","how to solve child nodes (0=usual, 1=simplification)\n", 0, IntRange(0, 1));
static IntOption     maxSplitters("SPLITTER", "max-splitters", "how many splitters can be used simultaneously\n", 1, IntRange(1, 1024));
static BoolOption    loadSplit("SPLITTER", "load-split", "If there is nothing to solve,but to split, split!.\n", false);
static BoolOption    stopUnsatChilds("SPLITTER", "stop-children", "If a child formula is known to be unsat, stop that solver.\n", false);
static IntOption     phase_saving_mode("SPLITTER", "phase-saving-mode", "how to handle the phase saving information (0=not,1=use last)\n", 0, IntRange(0, 1));
static IntOption     activity_mode("SPLITTER", "activity-mode", "how to handle the activity information (0=not,1=use last)\n", 0, IntRange(0, 1));
static IntOption     keepToplevelUnits("SPLITTER", "forward-units", "propagate found toplevel units downwards (0=no, 1=yes)\n", 1, IntRange(0, 1));
static IntOption     seed("SPLITTER", "seed", "Seed to initialize randomization\n", 0, IntRange(0, INT32_MAX));
static Int64Option   data_space("SPLITTER", "data-lim", "total bytes reserved for internal data of the solvers.\n", INT64_MAX, Int64Range(0, INT64_MAX));
static BoolOption    opt_scheme_pp("SPLITTER", "plain-scheme", "Use the plain partitioning paralellization scheme.\n", false);
static BoolOption    printModel("SPLITTER", "model", "Display the model for the formula\n", false);
static BoolOption    printTree("SPLITTER", "print-tree", "Print the solve-tree.\n", false);
static Int64Option   MSverbosity("SPLITTER", "verbosity", "verbosity of the master\n", 0, Int64Range(0, INT64_MAX));
static BoolOption    Portfolio("SPLITTER", "portfolio",  "Portfolio mode.\n", false);
static Int64Option   PortfolioLevel("SPLITTER", "portfolioL", "Perform Portfolio until specified level\n", 0, Int64Range(0, INT64_MAX));     // depends on option above!
static BoolOption    UseHardwareCores("SPLITTER", "usehw",  "Use Hardware, pin threads to cores\n", false);
static BoolOption    priss("SPLITTER", "use-priss",  "Uses Priss as instance solver\n", false);

static StringOption prissConfig("WORKER - CONFIG", "priss-config", "config string used to initialize priss incarnations", 0);
static StringOption rissConfig("WORKER - CONFIG", "riss-config", "config string used to initialize riss incarnations", 0);

static vector<unsigned short int> hardwareCores; // set of available hardware cores, used to pin the threads to cores

// instantiate static members of the class


Master::Master(Parameter p) :

    defaultSolverConfig((const char*)prissConfig == 0 ? "" : prissConfig),
    defaultPfolioConfig((const char*)rissConfig == 0 ? ""  : rissConfig),

    maxVar(0),
    model(0),
    param(p),
    threads(work_threads),
    space_lim(data_space / threads),
    communicationLock(1),
    sleepLock(1),
    masterState(working),
    done(false),
    timeout(work_timeout),
    res(0),
    plainpart(opt_scheme_pp)
{
    // decrease available elements in semaphore down to 0 -> next call will be blocking
    sleepLock.wait();

    threadData = new ThreadData[threads];


    createdNodeID = statistics.registerI("createdNodes");
    loadSplitID = statistics.registerI("loadSplits");
    unsolvedNodesID = statistics.registerI("unsolvedNodes");
    splitSolvedNodesID = statistics.registerI("splitSolvedNodes");
    retryNodesID = statistics.registerI("retryNodes");
    treeHeightID = statistics.registerI("treeHeight");
    evaLevelID = statistics.registerI("evaLevel");
    stoppedUnsatID = statistics.registerI("stoppedUnsatChildren");
    TotalConflictsID = statistics.registerI("TotalConflicts");
    TotalDecisionsID = statistics.registerI("TotalDecisions");
    TotalRestartsID = statistics.registerI("TotalRestarts");
    splitterCallsID = statistics.registerI("splitterCalls");
    //solverMaxTimeID = statistics.registerD("solverMaxTime");
    nConflictKilledID = statistics.registerI("nConflictKilled");

    if (UseHardwareCores) {
        hardwareCores.clear();
        cpu_set_t mask;
        sched_getaffinity(0, sizeof(cpu_set_t), &mask);
        for (int i = 0; i < sizeof(cpu_set_t) << 3; ++i) // add all available cores to the system
            if (CPU_ISSET(i, &mask)) { hardwareCores.push_back(i); }
        cerr << "c detected available cores: " << hardwareCores.size() << endl;
        if (hardwareCores.size() < threads) {
            cerr << "c WARNING: less physical cores that available threads, core-pinning will be disabled" << endl;
            hardwareCores.clear();
        }
    }
}


Master::~Master()  // TODO Dav> See if this can be re-enabled, when you join
{
    // free allocated literals
    for (unsigned int i = 0 ; i < originalFormula.size(); ++i) {
        delete [] originalFormula[i].lits;
    }
    // clear vector
    originalFormula.clear();
    delete [] threadData;
}


int
Master::main(int argc, char **argv)
{
    fprintf(stderr, "c ===============================[ Pcasso  %13s ]=================================================\n", Pcasso::gitSHA1);
    fprintf(stderr, "c | PCASSO - a Parallel, CooperAtive Sat SOlver                                                           |\n");
    fprintf(stderr, "c |                                                                                                       |\n");
    fprintf(stderr, "c | Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
    fprintf(stderr, "c | Contributors:                                                                                         |\n");
    fprintf(stderr, "c |     Davide Lanti (clause sharing, extended conflict analysis)                                         |\n");
    fprintf(stderr, "c |     Ahmed Irfan  (LA partitioning, information sharing      )                                         |\n");
    fprintf(stderr, "c |                                                                                                       |\n");

    string filename = (argc == 1) ? "" : argv[1];

    // // FILE* out_state_file = fopen("data.txt", "a"); // Davide> for statistics

    // fprintf( out_state_file, "Solving on %s\n", filename.c_str() ); // Davide> for statistics
    // fclose( out_state_file ); // Davide> for statistics

    // setup everything
    parseFormula(filename);
    if (MSverbosity > 1) { fprintf(stderr, "parsed %d clauses with %d variables\n", (int)originalFormula.size(), maxVar); }

    // open output file, in case of a non-valid filename, res will be 0 as well
    res = (argc >= 3) ? fopen(argv[2], "wb") : 0;
    if (argc >= 3) {
        fprintf(stderr, "The output file name should be %s\n", argv[2]);
    }
    // push the root node to the solve and split queues
    if (!plainpart) {
        solveQueue.push_back(&root);
    }

    splitQueue.push_back(&root);

    // run the real work loop
    return run();
}

int Master::run()
{
    int solution = 0;

    // initialize random number generator
    srand(seed);

    // workloop of the master
    // fill the queue
    // check the state of the tree
    // solve the next node in the tree
    if (MSverbosity > 0) { fprintf(stderr, "M: start main loop\n"); }
    while (!done) {

        int idles = 0;  // important, because this indicates that there can be done more in the next round!
        int workers = 0, splitters = 0, uncleans = 0;

	// check state of threads
        for (unsigned int i = 0 ; i < threads; ++i) {
            if (threadData[i].s == splitting) { splitters++; }
            if (threadData[i].s == idle) { idles++; }
            if (threadData[i].s == working) { workers++; }
            if (threadData[i].s == unclean) {

                // Davide> Free memory
                if (threadData[i].result == 20 && stopUnsatChilds) {
                    // The children are NOT running, so I can delete every pool
                    lock();
                    threadData[i].nodeToSolve->setState(TreeNode::unsat, true);
                    killUnsatChildren(i);  // says master has to be in lock
                    unlock();
                }

                uncleans++;
                // take care of unclean thread data
                threadData[i].s = idle;
                threadData[i].result = 0;
                threadData[i].nodeToSolve = 0;
                if (threadData[i].solver != nullptr) {
                    delete threadData[i].solver;
                }
                threadData[i].solver = nullptr;
                if (threadData[i].handle != 0) { pthread_join(threadData[i].handle, nullptr); }
                threadData[i].handle = 0;
            }
        }

        // print some information
        if (MSverbosity > 1) {
            fprintf(stderr, "==== STATE ====\n");
            fprintf(stderr, "solveQ: %d nodes = {", (int)solveQueue.size());
            for (uint32_t i = 0 ; i < solveQueue.size(); ++i) {
                fprintf(stderr, " %d", solveQueue[i]->id());
            }
            fprintf(stderr, " }\n");
            fprintf(stderr, "splitQ: %d nodes = {", (int)splitQueue.size());
            for (uint32_t i = 0 ; i < splitQueue.size(); ++i) {
                fprintf(stderr, " %d", splitQueue[i]->id());
            }
            fprintf(stderr, " }\n");
            fprintf(stderr, "THREADS: i: %d, s: %d w: %d u: %d \n", idles, splitters, workers, uncleans);
            fprintf(stderr, "====\n");
        } else if (MSverbosity > 0) {
            fprintf(stderr, "STATE: solveQ: %d, splitQ: %d  -- Threads: i: %d, s: %d w: %d u: %d \n", (int)solveQueue.size(), (int)splitQueue.size(), idles, splitters, workers, uncleans);
        }
        if (MSverbosity > 3) { root.print(); } // might become quite huge

        // uncleans have been turned into idle again
        idles = idles + uncleans;

        // check the state of the search tree
        {
            // model might be increased right now

            lock();
            if (model != 0) {
                solution = 10;
                unlock();
                fprintf(stderr, "c A model has been submitted!\n");
            }
            unlock();

            // check the tree for UNSAT/SAT
            if (MSverbosity > 1) { fprintf(stderr, "M: CHECK TREE\n"); }
            root.evaluate(*this);
            if (root.getState() == TreeNode::unsat) {
                // assign the according solution
                solution = 20;
                // jump out of the workloop
                break;
            }
            if (solution == 10 || root.getState() == TreeNode::sat) {
                // assign the according solution
                solution = 10;
                break;
            }

            // there is nothing to do for sat or unknown, because sat will be noticed if the model has been given
        }

        // can only start more threads, if there are some available
        if (idles > 0) {

            // more splitting?
            if (solveQueue.size() < threads) {
                unsigned int splitter = 0;
                for (unsigned int i = 0 ; i < threads; ++i) {
                    if (threadData[i].s == splitting) { splitter++; }
                }
                if (MSverbosity > 1) { fprintf(stderr, "M: current splitters: %d queue size: %d threads %d\n", splitter, (int)splitQueue.size(), threads); }
                if (splitter < maxSplitters && splitQueue.size() > 0) {
                    // check the size of the queue and whether the splitter is already running TODO: think about introducing several splitter
                    // in case it is too short, create more childs
                    // do splitting
                    if (MSverbosity > 1) { fprintf(stderr, "M: splitt a node\n"); }

                    // only split nodes that are not already unsatisfied
                    TreeNode* node = 0;
                    bool fail = false;
                    do {
                        if (splitQueue.size() == 0) { fail = true; break; }
                        else {
                            node = splitQueue.front();
                            splitQueue.pop_front();
                        }
                    } while (node->getState() == TreeNode::unsat);
                    PcassoDebug::PRINT_NOTE("M: SPLITTING NODE ");
                    PcassoDebug::PRINTLN_NOTE(node->getPosition());
                    if (!fail) {
                        assert(node != 0);
                        if (!splittFormula(node)) { continue; }     // if it fails, retry!
                    }
                }
            }

            // if there is something to solve, solve it!
            unsigned int i = 0 ;
            bool fail = false;
            for (; i < threads; ++i) {
                if (threadData[i].s == idle && solveQueue.size() > 0) {
                    if (MSverbosity > 1) { fprintf(stderr, "M: solve the next node\n"); }
                    if (!solveNextQueueEle()) {
                        fail = true; break;
                    }
                };
            }
            if (fail) { continue; }   // if one of the instance creations failed, start loop again and check states of workers

            // if there is nothing to solve, but more nodes to be split, split them!
            if (loadSplit && solveQueue.size() == 0 && splitQueue.size() > 0) {
                unsigned int i = 0 ;
                bool fail = false;
                for (; i < threads; ++i) {
                    if (threadData[i].s == idle && splitQueue.size() > 0) {
                        // only split nodes that are not already unsatisfied
                        TreeNode* node = 0;
                        bool localFail = false;
                        do {
                            if (splitQueue.size() == 0) { localFail = true; break; }
                            else {
                                node = splitQueue.front();
                                splitQueue.pop_front();
                            }
                        } while (node->getState() == TreeNode::unsat);

                        if (!localFail) {
                            assert(node != 0);
                            // TODO Davide> IS this the best we can do ?
                            if (!splittFormula(node)) { continue; }   // if it fails, retry!
                            else {
                                statistics.changeI(loadSplitID, 1);   // count how many load splits have been performed
                                if (MSverbosity > 1) { fprintf(stderr, "M: scheduled load split\n"); }
                            }
                        }
                        fail = fail || localFail;
                    };
                }
                if (fail) { continue; }   // if one of the instance creations failed, start loop again
            }

            // TODO: what should happen, if all queues are empty and not all threads do have work?
            if (solveQueue.size() == 0 && splitQueue.size() == 0) {
                unsigned int i = 0;
                for (; i < threads; ++i) {
                    if (threadData[i].s == splitting) { break; }   // TODO SHOULDN'T BE IDLE ?? DAVIDE>
                    if (threadData[i].s == working) { break; }   // do not stop if some worker is doing something
                }
                // if there is a thread that is still doing something, we did not run out of work!
                if (i == threads) {
                    fprintf(stderr, "\n***\n*** RUN OUT OF WORK - return unknown?\n***\n\n");

                    int idles = 0, workers = 0, splitters = 0, uncleans = 0;
                    for (unsigned int i = 0 ; i < threads; ++i) {
                        if (threadData[i].s == splitting) { splitters++; }
                        else if (threadData[i].s == idle) { idles++; }
                        else if (threadData[i].s == working) { workers++; }
                        else if (threadData[i].s == unclean) { uncleans++; }
                    }

                    fprintf(stderr, "c idle: %d working: %d splitting: %d unclean: %d\n", idles, workers, splitters, uncleans);

                    root.evaluate(*this);
                    if (root.getState() == TreeNode::unsat) {
                        // assign the according solution
                        solution = 20;
                        fprintf(stderr, "found UNSAT of tree\n");
                        // jump out of the workloop
                    } else if (solution == 10 || root.getState() == TreeNode::sat) {
                        // assign the according solution
                        solution = 10;
                        fprintf(stderr, "found SAT of tree\n");
                    }

                    fprintf(stderr, "solution unknown!\n");
                    exit(3); // make sure we find it outside of the solver again
                }
            }


        } else {
            if (MSverbosity > 1) { fprintf(stderr, "M: all threads are in work\n"); }
        }

        // if there is nothing more to do (splitter runs, all tasks are assigned, no free cores)
        // goto sleep until something happens (call to notify)
        if (MSverbosity > 1) { fprintf(stderr, "M: sleeping\n"); }

        goSleeping();

        if (MSverbosity > 1) { fprintf(stderr, "M: wakeup\n"); }
        // continue with the work after waking up again

    } // END while (!done)

    // handle the solution
    // check whether sat has been found
    if (MSverbosity > 0) { fprintf(stderr, "M: solved formula with result %d\n", solution); }
    // // Davide> handle statistics
    // for( unsigned int i = 0; i < root.size(); i++ ){
    //   cout << i << endl;
    //   statistics.changeI(sum_clauses_pools_lv1ID, root.getChild(i)->lv_pool->shared_clauses.size() );
    // }

    // Davide> write statistics
    // // FILE* out_state_file = fopen("data.txt", "a"); // Davide> for statistics
    // if( solution == 10 )
    //   fprintf(out_state_file, "SAT %g\n", cpuTime()); // Davide> for statistics
    // if( solution == 20 )
    //   fprintf(out_state_file, "UNSAT %g\n", cpuTime()); // Davide> for statistics

    // fclose(out_state_file);

    assert(model != 0 || solution != 10);

    if (printModel) {

        if (res != nullptr) { fprintf(stderr, "c write model to file\n"); }

        if (model != 0) {
            assert(solution == 10 && "the solution has to be 20");
            fprintf(stderr, "c SATISFIABLE\n");

            // fprintf(out_state_file, "SAT %g\n", cpuTime()); // Davide> for statistics

            // print model only, if a file has been specified
            if (res != nullptr) {
                fprintf(res, "s SATISFIABLE\nv ");
                /*
                if( model.size() != maxVar ){
                fprintf( stderr,"different model sizes: model: %d variables: %d", model.size(), maxVar);
                }
                 */
                for (int i = 0; i < maxVar; i++) {
                    if ((*model)[i] != l_Undef) {
                        fprintf(res, "%s%s%d", (i == 0) ? "" : " ", ((*model)[i] == l_True) ? "" : "-", i + 1);
                    }
                    /*
                    else
                    fprintf(res, "%s%s%d", (i==0)?"":" ", (true)?"":"-", i+1);
                     */
                }
                fprintf(res, " 0\n");
            } else {
                fprintf(stdout, "s SATISFIABLE\nv ");
                /*
                if( model.size() != maxVar ){
                fprintf( stdout,"different model sizes: model: %d variables: %d", model.size(), maxVar);
                }
                 */
                for (int i = 0; i < maxVar; i++) {
                    if ((*model)[i] != l_Undef) {
                        fprintf(stdout, "%s%s%d", (i == 0) ? "" : " ", ((*model)[i] == l_True) ? "" : "-", i + 1);
                    }
                    /*
                    else
                    fprintf(res, "%s%s%d", (i==0)?"":" ", (true)?"":"-", i+1);
                     */
                }
                fprintf(stdout, " 0\n");
            }
        } else if (solution == 20) {
            fprintf(stdout, "UNSATISFIABLE\n");

            // fprintf(out_state_file, "UNSAT %g\n", cpuTime()); // Davide> for statistics

            fprintf(stdout, "UNSATISFIABLE\n");

            if (res != nullptr) { fprintf(res, "s UNSATISFIABLE\n"); }
            fprintf(stdout, "s UNSATISFIABLE\n");
        } else {
            fprintf(stdout, "s INDETERMINATE\n");
        }


        // fclose(out_state_file); // Davide> for statistics

    }

    if (res != 0) { fclose(res); }

    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        perror("clock_gettime()");
        ts.tv_sec = -1;
        ts.tv_nsec = -1;
    }

    double canc_t = ts.tv_sec + ts.tv_nsec / 1000000000.0;

    // cancel all threads
    for (unsigned int i = 0 ; i < threads ; ++i) {
        //    int err = 0;
        fprintf(stderr, "c try to cancel thread %d\n", i);
        // if( threadData[i].s == idle || threadData[i].handle == 0 || threadData[i].solver == nullptr) continue;
        if (threadData[i].solver == 0) { continue; }
        threadData[i].solver->interrupt(); // Yeah, it takes time.
        // threadData[i].solver->kill();
        PcassoDebug::PRINTLN_NOTE("thread KILLED");
        if (threadData[i].s == idle || threadData[i].handle == 0) { continue; }
        // pthread_cancel(threadData[i].handle);
        //if( err == ESRCH ) fprintf( stderr, "c specified thread does not exist\n");
    }

    // TODO Davide> REDO THIS, if POSSIBLE
    // join all threads (avoid segmentation fault due to calls to destructor of master object)
    for (unsigned int i = 0 ; i < threads ; ++i) {
        int* status = 0;
        int err = 0;
        if (MSverbosity > 1) { fprintf(stderr, "c try to join thread %d\n", i); }
        //    if( threadData[i].s == idle || threadData[i].handle == 0 ) continue;
        if (threadData[i].solver != nullptr) {
            if (threadData[i].handle != 0) {
                err = pthread_join(threadData[i].handle, (void**)&status);
                if (err == EINVAL) { fprintf(stderr, "c tried to cancel wrong thread\n"); }
                if (err == ESRCH) { fprintf(stderr, "c specified thread does not exist\n"); }
                if (MSverbosity > 1) { fprintf(stderr, "c joined thread %d\n", i); }
            }
            delete threadData[i].solver;
        }
    }

    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        perror("clock_gettime()");
        ts.tv_sec = -1;
        ts.tv_nsec = -1;
    }

    if (MSverbosity > 0) {
        fprintf(stderr, "c Waited for threads for %f secs\n", (ts.tv_sec + ts.tv_nsec / 1000000000.0) - canc_t);
    }

    if (MSverbosity > 2 || printTree) {
        fprintf(stderr, "\n\nfinal tree:\n");
        root.print();
    }

    int treeHeight = root.getSubTreeHeight();
    statistics.changeI(treeHeightID, treeHeight);
    int evaLevel = root.getEvaLevel();
    //statistics.changeI( evaLevelID, evaLevel );

    string statOutput;
    statistics.print(statOutput);

    fprintf(stderr, "c statistics:\n%s\n", statOutput.c_str());

    fprintf(stderr, "c solved instance with result %d\n", solution);

    printf("c CPU time              : %g s\n", cpuTime());
    printf("c memory                : %g MB\n", memUsedPeak());

    // report found value to calling method
    return solution;
}

void
Master::stopThread(uint32_t threadId)
{

    // Solver will terminate gracefully next time it restarts.
    // It can require a lot of time.
    //lock();
    if (threadData[threadId].solver != 0) { threadData[threadId].solver->interrupt(); }
    //unlock();
    //
    //        if(pthread_cancel(threadData[threadId].handle) == 0){
    //          pthread_join(threadData[threadId].handle, 0);
    //        }
}

bool
Master::solveNextQueueEle()
{
    // make all threads joinable
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    // find an idle thread
    unsigned int i = 0 ;
    for (; i < threads; ++i) {
        if (threadData[i].s == idle) { break; }
    }
    if (i == threads) {
        fprintf(stderr, "was not able to allocate a thread for the next task\n");
        return false;
    }
    // setup data for thread
    threadData[i].id = i;
    threadData[i].master = this;
    threadData[i].s = working;
    threadData[i].timeout = timeout;
    threadData[i].conflicts = work_conflicts;
    // check under the lock, whether there are still elements in the queue
    bool fail = false;
    lock();
    // look for the next node, that can be solved (is not already unsatisfiable)
    do {
        if (solveQueue.size() == 0) { fail = true; break; }
        else {
            threadData[i].nodeToSolve = solveQueue.front();
            solveQueue.pop_front();
        }
    } while (threadData[i].nodeToSolve->getState() == TreeNode::unsat); // TODO: might also work with checking for "no state". should not be "sat" either
    unlock();
    if (fail) {
        threadData[i].s = idle;
        return false;
    }

    // create one thread
    if (MSverbosity > 1) { fprintf(stderr, "create thread [%d] for solving\n", i); }
    assert(threadData[i].nodeToSolve != 0);
    int rc = pthread_create(&(threadData[i].handle), &attr, Master::solveInstance, (void *)  & (threadData[i]));
    if (rc) {
        printf("ERROR; return code from pthread_create() is %d for thread %d\n", rc, i);
        // add the tree element back to the queue
        lock();
        solveQueue.push_front(threadData[i].nodeToSolve);
        unlock();
        // set the state of the current core back to idle
        threadData[i].s = idle;
        return false;
    }
    pthread_attr_destroy(&attr);
    return true;
}

bool
Master::splittFormula(TreeNode* data)
{
    // create extra thread
    // this thread has to reset the splitter state
    // make all threads joinable
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    // find an idle thread
    unsigned int i = 0 ;
    for (; i < threads; ++i) {
        if (threadData[i].s == idle) { break; }
    }
    if (i == threads) {
        fprintf(stderr, "was not able to allocate a thread for the next task\n");
        return false;
    }
    // setup data for thread
    threadData[i].id = i;
    threadData[i].master = this;
    threadData[i].s = splitting;
    threadData[i].nodeToSolve = data;   // this is the node to split

    assert(threadData[i].nodeToSolve != 0);
    assert(threadData[i].handle == 0);
    // create one thread
    if (MSverbosity > 1) { fprintf(stderr, "create another thread[%d]  for splitting\n", i); }
    int rc = pthread_create(&(threadData[i].handle), &attr, Master::splitInstance, (void *)  & (threadData[i]));
    if (rc) {
        fprintf(stderr, "ERROR; return code from pthread_create() is %d for thread %d\n", rc, i);
        // set the state of the current core back to idle
        threadData[i].s = idle;
        return false;
    }
    pthread_attr_destroy(&attr);
    return true;
}

void*
Master::solveInstance(void* data)
{
    // set thread cancelable
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    // extract parameter data again
    ThreadData& tData = *((ThreadData*)data);
    Master& master = *(tData.master);

    if (MSverbosity > 0) {
        fprintf(stderr, "solve instance with id %d that solves node %d\n", tData.id, tData.nodeToSolve->id());
        fprintf(stderr, "thread %d starts solving a node at %lld with the node solve time %lld\n", tData.id, Master::getCpuTimeMS(), tData.nodeToSolve->solveTime);
    }
    PcassoDebug::PRINT_NOTE("STARTED TO SOLVE NODE ");
    PcassoDebug::PRINT_NOTE(tData.nodeToSolve->getPosition());
    PcassoDebug::PRINT_NOTE(" with PT_Level ");
    PcassoDebug::PRINTLN_NOTE(tData.nodeToSolve->getPTLevel());

    if (UseHardwareCores && hardwareCores.size() > 0) {  // pin this thread to the specified core
        cpu_set_t mask;
        CPU_ZERO(&mask); CPU_SET(tData.id, &mask);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
            PcassoDebug::PRINTLN_NOTE("Failed to pin thread to core");
        }
    }

    long long int cputime = Master::getCpuTimeMS();


    // create a solver object
    InstanceSolver* solver;
    if (!priss) {
        solver = new SolverRiss(& master.defaultSolverConfig);
    } else {
        // TODO: how many threads for priss? commandline option?
//         @Franzi: Norbert: yes! and a way to control it per level
//         either: always a fixed number, or:
//         X on level 0, and X/2 on all the other levels, calculate from number of threads given to pcasso, and the chosen policy
        solver = new SolverPriss(& master.defaultPfolioConfig, 2);
    }
    assert(tData.solver == nullptr);
    tData.solver = solver;

//    // Davide> Give the pt_level to the solver
    solver->curPTLevel = tData.nodeToSolve->getPTLevel();
    solver->unsatPTLevel = tData.nodeToSolve->getPTLevel();

    if (Portfolio && tData.nodeToSolve->getLevel() <= PortfolioLevel) {   // a portfolio node should be solved
        const int nodeLevel = tData.nodeToSolve->getLevel();
        if (nodeLevel > 0) {   // root node is solved as usually
            cerr << "c solve portfolio node at level " << nodeLevel << endl;
            solver->setupForPortfolio(nodeLevel);
        }
    }

    // setup the parameters, setup varCnt
    solver->setVerbosity(tData.master->param.verb);
    if (master.varCnt() >= (unsigned int) solver->nVars()) solver->reserveVars( master.varCnt() );
    while (master.varCnt() >= (unsigned int) solver->nVars()) {
        solver->newVar();
    }

    // setup communication
    
//    // Davide> Initialize the shared indeces to zero
//    for (unsigned int i = 0; i <= solver->curPTLevel; i++)   // Davide> I put my level, also
//    { solver->shared_indeces.push_back(0); }

//    // Davide> Create the pool object for this node, and
//    //         push it in the set of pools
//    PcassoDavide::LevelPool* pool = new PcassoDavide::LevelPool(opt_pool_size);
//    // pool->setCode(S.position);
//    tData.nodeToSolve->lv_pool = pool;

//    // Davide> Associate the pt_node to the Solver
//    solver->tnode = tData.nodeToSolve;
    
    
    // setup activity and polarity of the solver (pseudo-clone)
    // put phase saving information
    // set up?
    if( phase_saving_mode != 0 ) {
    master.lock();
      if( master.polarity.size() == 0) master.polarity.growTo( master.maxVar, false );
      else {
    // copy information to solver, use only variables that are available
    int min = solver->nVars();
    min = min <= master.polarity.size() ? min : master.polarity.size();

    for( int i = 0 ; i < min; ++ i ) {
      solver->setPolarity(i, master.polarity[i]);
    }
      }
    master.unlock();
    }
    if( activity_mode != 0 ) {
    master.lock();
      if( master.activity.size() == 0) master.activity.growTo( master.maxVar, false );
      else {
      // copy information to solver, use only variables that are available
      int min = solver->nVars();
      min = min <= master.activity.size() ? min : master.activity.size();

      for( int i = 0 ; i < min; ++ i ) {
	    solver->setActivity(i, master.activity[i] );
      }
      }
    master.unlock();
    }
    
    

    
    // feed the instance into the solver
    assert(tData.nodeToSolve != 0);
    vec<Lit> lits;

    // add constraints to solver
    vector< pair<vector<Lit>*, unsigned int> > clauses;
    tData.nodeToSolve->fillConstraintPathWithLevels(&clauses);  // collect all clauses from all nodes to the tree
    for (unsigned int i = 0 ; i < clauses.size(); ++i) {
        lits.clear();
        for (unsigned int j = 0 ; j < clauses[i].first->size(); ++ j) {
            lits.push((*clauses[i].first)[j]);
        }
        solver->addClause_(lits, clauses[i].second); // add them to the solver with the right dependency level
    }

    // add the formula to the solver
    for (unsigned int i = 0 ; i < master.formula().size(); ++i) {
        lits.clear();
        for (int j = 0 ; j < master.formula()[i].size; ++j) {
            lits.push(master.formula()[i].lits[j]);
        }
        solver->addClause_(lits, 0);// add master formula to the solver with the right dependency level
    }

    // copy the procedure as in the original main function
    int ret = 0;
    int initialUnits = 0;

    vec<Lit> dummy;
    initialUnits = solver->getNumberOfTopLevelUnits();
    if (MSverbosity > 1) { fprintf(stderr, "c start search with %d units\n", initialUnits); }

    // solve the instance with a timeout?
    if (tData.timeout != -1)   { solver->setTimeOut(tData.timeout); }
    if (tData.conflicts != -1) { solver->setConfBudget(tData.conflicts); }

    // TODO introduce conflict limit for becoming deterministic!
    // solve the formula
    lbool solution = solver->solveLimited(dummy);
    ret = solution == l_True ? 10 : solution == l_False ? 20 : 0;
    master.lock();
    if (tData.result == 20) { ret = 20; }

    // tell statistics
    if (ret != 10 && ret != 20) {
        statistics.changeI(master.unsolvedNodesID, 1);
    } else {
        if (tData.nodeToSolve != 0)  // prevent using 0 pointers
        { tData.nodeToSolve->solveTime = Master::getCpuTimeMS() - cputime; }
    }

    // take care about the output
    if (MSverbosity > 1) { fprintf(stderr, "finished working on an instance (node %d, level %d)\n", tData.nodeToSolve->id(), tData.nodeToSolve->getLevel()); }


    // take care about statistics
    // since solver is dead, there is no need to have a lock for reading statistics!
    // since we are in the master, there is also no need to lock changing statistics here, however, its there ...
    statistics.changeI(master.TotalConflictsID, solver->getConflicts());
    statistics.changeI(master.TotalDecisionsID, solver->getDecisions());
    statistics.changeI(master.TotalRestartsID, solver->getStarts());
    Statistics& localStat = solver->localStat;
    for (unsigned si = 0; si < localStat.size(); ++ si) {
        if (localStat.isInt(si)) {
            unsigned thisID = statistics.reregisterI(localStat.getName(si));
            statistics.changeI(thisID, localStat.giveInt(si));
        } else {
            unsigned thisID = statistics.reregisterD(localStat.getName(si));
            statistics.changeD(thisID, localStat.giveDouble(si));
        }
    }

    if (ret == 10) {
        if (tData.nodeToSolve != 0) {
            fprintf(stderr, "============SOLUTION FOUND BY NODE %d AT PARTITION LEVEL %d============\n",
                    tData.nodeToSolve->id(), tData.nodeToSolve->getLevel());
        }
        if (tData.nodeToSolve != 0) { tData.nodeToSolve->setState(TreeNode::sat); }
        vec< lbool > solverModel;
        solver->getModel(solverModel);   // TODO: give a reference to the actual model back, and give this to the master directly
        master.submitModel(solverModel); // just copy model to master
    } else if (ret == 20) {
        if (opt_conflict_killing && solver->unsatPTLevel < tData.nodeToSolve->getPTLevel()) {
            statistics.changeI(master.nConflictKilledID, 1);

            if (tData.nodeToSolve != 0) {
                int i = tData.nodeToSolve->getPTLevel() - solver->unsatPTLevel; // use the level of which the solver just showed that its subtree is unsatisfiable
                while (i > 0) {
                    TreeNode* node = tData.nodeToSolve->getFather();
                    node->setState(TreeNode::unsat);
                    --i;
                }
            }
        }
        if (tData.nodeToSolve != 0) { tData.nodeToSolve->setState(TreeNode::unsat); }
    } else {
        // result of solver is "unknown"
        if (tData.nodeToSolve != 0) {
            if (master.plainpart && tData.nodeToSolve->getState() == TreeNode::unknown) {
                tData.nodeToSolve->setState(TreeNode::retry);
            }
        }

        if (keepToplevelUnits > 0) {
            int toplevelVariables = 0;
            toplevelVariables = solver->getNumberOfTopLevelUnits();

            if (toplevelVariables > 0 && MSverbosity > 1) { fprintf(stderr, "found %d topLevel units\n", toplevelVariables - initialUnits); }
            if (tData.nodeToSolve != 0) {
                for (int i = initialUnits ; i < toplevelVariables; ++i) {  // TODO have an extra data type to describe unit with level in the node
                    Lit currentLit = solver->trailGet(i);  //(*trail)[i];
                    vector<Lit>* clause = new vector<Lit>(1);
                    (*clause)[0] = currentLit;

                    // Davide> I modified this in order to include information about
                    // literal pt_levels
		    // TODO use the pt level to add the units to the correct nodes (use write-lock for the tree!)
                    tData.nodeToSolve->addNodeUnaryConstraint(clause, solver->getLiteralPTLevel(currentLit) ); // add the unit with the level
                }
            }
        }
        
	// write back activity and polarity of the solver (pseudo-clone)
	// put phase saving information
	if( phase_saving_mode != 0 ) {
	master.lock();
	  if( master.polarity.size() == 0) master.polarity.growTo( master.maxVar, false );
	  else {
	// copy information to solver, use only variables that are available
	int min = solver->nVars();
	min = min <= master.polarity.size() ? min : master.polarity.size();

	for( int i = 0 ; i < min; ++ i ) {
	  master.polarity[i] = solver->getPolarity(i );
	}
	  }
	master.unlock();
	}
	if( activity_mode != 0 ) {
	master.lock();
	  if( master.activity.size() == 0) master.activity.growTo( master.maxVar, false );
	  else {
	  // copy information to solver, use only variables that are available
	  int min = solver->nVars();
	  min = min <= master.activity.size() ? min : master.activity.size();

	  for( int i = 0 ; i < min; ++ i ) {
	    master.activity[i] = solver->getActivity(i );
	  }
	  }
	master.unlock();
	}
        
    }

    tData.result = ret;
    tData.s = unclean;

    // Delete pools

    // set the own state to unclean
    if (MSverbosity > 1) { fprintf(stderr, "run instance with id %d, state=%d, result=%d\n", tData.id, tData.s, tData.result); }
    if (MSverbosity > 2) { fprintf(stderr, "done with the work (%d), wakeup master again\n", ret); }
    // wake up the master so that it can check the result
    if (MSverbosity > 0 && tData.nodeToSolve != 0) { fprintf(stderr, "thread %d calls notify from solving after spending time on the node: %lld\n" , tData.id, tData.nodeToSolve->solveTime); }

    master.unlock();
    master.notify();
    return (void*)ret;
}

void*
Master::splitInstance(void* data)
{
    // set thread cancelable
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    // extract parameter data again
    ThreadData& tData = *((ThreadData*)data);
    Master& master = *(tData.master);

    if (MSverbosity > 1) { fprintf(stderr, "create a split thread[%d] that splits node %d\n", tData.id, tData.nodeToSolve->id()); }
    if (UseHardwareCores && hardwareCores.size() > 0) {  // pin this thread to the specified core
        cpu_set_t mask;
        CPU_ZERO(&mask); CPU_SET(tData.id, &mask);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
            PcassoDebug::PRINTLN_NOTE("Failed to pin thread to core");
        }
    }

    long long int cputime = Master::getCpuTimeMS();
    int ret = 0;
    vector< vector< vector<Lit>* >* > childConstraints;

    if (Portfolio && tData.nodeToSolve->getLevel() < PortfolioLevel) {  // perform portfolio only until this level!
        master.lock(); // ********************* START CRITICAL ***********************
        childConstraints.push_back(new vector<vector<Lit>*>);
        tData.nodeToSolve->setState(TreeNode::unknown);
        tData.nodeToSolve->expand(childConstraints);
        master.addNode(tData.nodeToSolve->getChild(0));
        tData.result = ret;
        tData.s = unclean;
        master.unlock(); // --------------------- END CRITICAL --------------------- //
        master.notify();
        return (void*)ret;
    }

    vector< vector<Lit>* > validConstraints;
    // create a solver object
    SplitterSolver* S;
    if (split_mode == 1) {
        S = new VSIDSSplitting(& master.defaultSolverConfig);
        //((VSIDSSplitting *)S)->setTimeOut(split_timeout);
    }
    if (split_mode == 2) {
        S = new LookaheadSplitting(& master.defaultSolverConfig);
        ((LookaheadSplitting *)S)->setTimeOut(split_timeout);
    }
    tData.solver = S;
    // setup the parameters
    S->verbosity = tData.master->param.verb;


    // feed the instance into the solver
    while (master.varCnt() >= (unsigned int)S->nVars()) { S->newVar(); }

    vector< vector<Lit>* > clauses;
    assert(tData.nodeToSolve != 0);
    tData.nodeToSolve->fillConstraintPath(&clauses);
    vec<Lit> lits;
    for (unsigned int i = 0 ; i < clauses.size(); ++i) {
        lits.clear();
        for (unsigned int j = 0 ; j < clauses[i]->size(); ++ j) {
            lits.push((*clauses[i])[j]);
        }
        S->addClause_(lits);
    }
    for (unsigned int i = 0 ; i < master.formula().size(); ++i) {
        lits.clear();
        for (int j = 0 ; j < master.formula()[i].size; ++j) {
            lits.push(master.formula()[i].lits[j]);
        }
        S->addClause_(lits);
    }

    if (!S->okay()) {
        fprintf(stderr, "reading split instance resulted in error (node %d)\n", tData.nodeToSolve->id());
#warning: this is ok! (according to LA-splitting author Ahmed)
        ret = 20;
        // tell statistics
        statistics.changeI(master.splitSolvedNodesID, 1);
        tData.nodeToSolve->solveTime = Master::getCpuTimeMS() - cputime;
    } else {
        vec<vec<vec<Lit>* >* > *splits = 0;
        vec<vec<Lit>* > *valid = 0;

        lbool solution = l_Undef;

        // split an instance - just in case add an exception
        //try {
        // ## begin automatically handled code
        // ## selecting split mode
        if (split_mode == 0) {   //solution = S->simpleSplit(&splits, nullptr, split_timeout);
        } else if (split_mode == 1) {
            solution = ((VSIDSSplitting *)S)->vsidsScatteringSplit(&splits, nullptr, split_timeout);
        } else if (split_mode == 2) {
            solution = ((LookaheadSplitting *)S)->produceSplitting(&splits, &valid);
        } else if (split_mode == 3) {   //solution = S->score(&splits, &valid, split_timeout);
            // ## add your new split-method here (above)

            // ## end automatically handled code
        } else {
            fprintf(stderr, "unknown split method %d, exit.\n", (int)split_mode);
            exit(0);
        }
        /*} catch (ControlledOutOfMemoryException &e) {
        if(MSverbosity > 1) fprintf(stderr, "Controlled memory out on node %d during splitting\n", tData.nodeToSolve->id());
        exit(127);
        } */

        // tell statistics
        PcassoDebug::PRINT_NOTE("c add to created nodes: ");
        PcassoDebug::PRINTLN_NOTE((*splits).size());
        statistics.changeI(master.createdNodeID, (*splits).size());
        statistics.changeI(master.splitterCallsID, 1);

        // only if there is a valid vector, use it!
        if (valid != 0) {
            for (int i = 0; i < (*valid).size(); i++) {
                vec<Lit> *tmp_cls = (*valid)[i];
                validConstraints.push_back(new vector<Lit>);
                for (int j = 0; j < (*tmp_cls).size(); j++) {
                    Lit l = (*tmp_cls)[j];
                    validConstraints[i]->push_back(l);
                    //                  fprintf( stderr,"%s%d ", (sign(l) ? "-" : ""),
                    //                                var(l) + 1);
                }
                //              fprintf( stderr,"0\n");
                delete tmp_cls;
            }

            // delete the valid vector again!
            delete valid;
        }
        //          fprintf( stderr,"\n");

        for (int i = 0; i < (*splits).size(); i++) {
            if (MSverbosity > 1) { fprintf(stderr, "Split %d\n", i); }
            vec<vec<Lit>*> *tmp_cnf = (*splits)[i];
            childConstraints.push_back(new vector<vector<Lit>*>);
            for (int j = 0; j < (*tmp_cnf).size(); j++) {
                vec<Lit> *tmp_cls = (*tmp_cnf)[j];
                childConstraints[i]->push_back(new vector<Lit>);
                for (int k = 0; k < (*tmp_cls).size(); k++) {
                    Lit l = (*tmp_cls)[k];
                    (*childConstraints[i])[j]->push_back(l);
                    //                  fprintf( stderr,"%s%d ", (sign(l) ? "-" : ""),
                    //                                var(l) + 1);
                }
                //              fprintf( stderr,"0\n");
                delete tmp_cls;
            }
            //          fprintf( stderr,"\n");
            delete tmp_cnf;
        }
        // if sat has been found, return SAT
        if (solution == l_True) {
            // tell statistics
            statistics.changeI(master.splitSolvedNodesID, 1);
            ret = 10;
            tData.nodeToSolve->solveTime = Master::getCpuTimeMS() - cputime;
        }
        // if no split has been found, return unsat
        else if (childConstraints.size() == 0) {
            // tell statistics
            statistics.changeI(master.splitSolvedNodesID, 1);
            ret = 20;
            tData.nodeToSolve->solveTime = Master::getCpuTimeMS() - cputime;
        }
    }
    // take care about the output
    if (MSverbosity > 1) { fprintf(stderr, "finished splitting an instance, resulting in %d childs (node %d)\n", (int)childConstraints.size(), tData.nodeToSolve->id()); }

    master.lock(); // ********************* START CRITICAL ***********************
    // expand the node

    /** take care about statistics
     * since solver is dead, there is no need to have a lock for reading statistics!
     * since we are in the master, there is also no need to lock changing statistics here, however, its there ...
     */
    if (split_mode == 2) {
        Statistics& localStat = ((LookaheadSplitting *)S)->localStat;//currently only using statistics for lookahead splitting
        for (unsigned si = 0; si < localStat.size(); ++ si) {
            if (localStat.isInt(si)) {
                unsigned thisID = statistics.reregisterI(localStat.getName(si));
                statistics.changeI(thisID, localStat.giveInt(si));
            } else {
                unsigned thisID = statistics.reregisterD(localStat.getName(si));
                statistics.changeD(thisID, localStat.giveDouble(si));
            }
        }
    }

    // add childs to the queue
    if (MSverbosity > 2) { fprintf(stderr, "split a node with result %d into %d childs\n", ret, (int)childConstraints.size()); }
    // handle the outcome of solving
    if (ret == 10) {
        master.submitModel(S->model);
        tData.nodeToSolve->setState(TreeNode::sat);
        // nothing more to do, master will take care of the submitted model
    } else if (ret == 20) {
        tData.nodeToSolve->setState(TreeNode::unsat, true);
        // shut down all threads that are running below that node (necessary?)
    } else {
        // simply set the node to the unknown state
        tData.nodeToSolve->setState(TreeNode::unknown);
        for (unsigned int i = 0; i < validConstraints.size(); i++) {
            tData.nodeToSolve->addNodeConstraint(validConstraints[i]);
        }
        // expand the node
        tData.nodeToSolve->expand(childConstraints);
        for (unsigned int i = 0; i < childConstraints.size(); i++) {
            delete childConstraints[i];
        }
        // add childs to both queues
        for (unsigned int i = 0 ; i < tData.nodeToSolve->size(); ++ i) {
            master.addNode(tData.nodeToSolve->getChild(i));
        }
    }
    tData.result = ret;
    tData.s = unclean;

    master.unlock(); // --------------------- END CRITICAL --------------------- //
    //  delete S;

    // set the own state to unclean
    if (MSverbosity > 1) {
        fprintf(stderr, "finished splitting with %d\n", ret);
        // wake up the master so that it can check the result
        fprintf(stderr, "thread %d calls notify from splitting\n" , tData.id);
    }
    master.notify();

    return (void*)ret; // turn 32bit number into 32/64 bit pointer
}

void
Master::addNode(TreeNode* t)
{
    solveQueue.push_back(t);
    if (!plainpart) {
        splitQueue.push_back(t);
    }
}

void
Master::submitModel(const vec<lbool>& fullModel)
{
    // only store one model
    if (model == 0)  {
        // if( MSverbosity > 0 )
        fprintf(stderr, "c model submitted\n");
        model = new vec< lbool >;
        fullModel.copyTo(*model);
    }
}

const vector< Master::clause >&
Master::formula() const
{
    return originalFormula;
}

const unsigned int
Master::varCnt() const
{
    return maxVar;
}

void Master::notify()
{
    if (MSverbosity > 0) {
        fprintf(stderr, "wake up master with %d waiters\n", sleepLock.getWaiters());
    }
    // do nothing, if master is already working
    //while( masterState == sleeping ){
    // otherwise wake it up
    sleepLock.unlock();
    //}
}

void Master::goSleeping()
{
    //fprintf( stderr,"goto sleep with %d wait calls to the semaphore\n", sleepLock.getWaiters() );
    // masterState = sleeping;

    sleepLock.wait();

    // masterState = working;
    //fprintf( stderr,"woke up with %d wait calls to the semaphore\n", sleepLock.getWaiters() );
}

void
Master::lock()
{
    communicationLock.wait();
}


void
Master::unlock()
{
    communicationLock.unlock();
}

int
Master::parseFormula(string filename)
{
    gzFile in = (filename.size() == 0) ? gzdopen(0, "rb") : gzopen(filename.c_str(), "rb");
    if (in == nullptr) {
        fprintf(stderr, "ERROR! Could not open file: %s\n", filename.size() == 0 ? "<stdin>" : filename.c_str()), exit(1);
    } else {
        // open file
        StreamBuffer stream(in);
        //parse_DIMACS_main(stream, S);

        vec<Lit> lits;
        int vars    = 0;
        int clauses = 0;
        int cnt     = 0;
        for (;;) {
            skipWhitespace(stream);
            if (*stream == EOF) { break; }
            else if (*stream == 'p') {
                if (eagerMatch(stream, "p cnf")) {
                    vars    = parseInt(stream);
                    clauses = parseInt(stream);
                    // SATRACE'06 hack
                    // if (clauses > 4000000)
                    //     S.eliminate(true);
                } else {
                    fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *stream), exit(3);
                }
            } else if (*stream == 'c' || *stream == 'p') {
                skipLine(stream);
            } else {
                // parse the clause
                cnt++;
                // readClause(in, lits);
                int     parsed_lit, var;
                lits.clear();
                for (;;) {
                    parsed_lit = parseInt(stream);
                    if (parsed_lit == 0) { break; }
                    var = abs(parsed_lit) - 1;
                    while (var >= maxVar) { maxVar++; }
                    lits.push((parsed_lit > 0) ? mkLit(var) : ~mkLit(var));
                }
                // add clause to original formula
                clause c;
                c.size = lits.size();
                c.lits = new Lit [ c.size ];
                for (int i =  0; i < c.size; i++) { c.lits[i] = lits[i]; }
                originalFormula.push_back(c);
            }
        }
        /*
        if (vars != S.nVars())
        fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of variables.\n");
        if (cnt  != clauses)
        fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of clauses.\n");
         */
        std::sort(originalFormula.begin(), originalFormula.end(), clause_size(originalFormula));//AHMED> to get more clause reduction for child nodes
    }
    gzclose(in);
    return 0;
}
void
Master::killUnsatChildren(unsigned int i)
{
    // MASTER HAS TO BE IN LOCK HERE!
    uint32_t solvedUnsat = 0;
    TreeNode* solvedNode = threadData[i].nodeToSolve;
    for (unsigned int j = 0 ; j < threads; j++) {
        if (j == i) { continue; }
        if (threadData[j].nodeToSolve == 0) { continue; }

        TreeNode* curNode = threadData[j].nodeToSolve;

        if (solvedNode->isAncestorOf((*curNode))) {
            PcassoDebug::PRINTLN_DEBUG("PREPARING TO KILL A NODE");
            solvedUnsat ++;
            stopThread(j);
            // threadData[j].s = unclean;
            PcassoDebug::PRINTLN_DEBUG("KILLED");
            //              notify();
        }
    }
    if (solvedUnsat > 0) {
        if (MSverbosity > 0 && solvedUnsat > 0) { fprintf(stderr, "M: killed %d nodes that solved unsatisfiable nodes\n", solvedUnsat); }
        statistics.changeI(stoppedUnsatID, solvedUnsat);
    } else {
        PcassoDebug::PRINTLN_NOTE("M: DELETING POOLS ");
        assert(threadData[i].nodeToSolve != 0);
        threadData[i].nodeToSolve->removePoolRecursive(true);
    }
}
//void
//Master::removeUnsatNodesFromQueues(){
//  for( int i = 0; i < solveQueue.size(); i ++ ){
//      assert(solveQueue[i] != 0 && "removeUnsatNodesFromQueues");
//      if( solveQueue[i]->getState() == TreeNode::unsat ){
//          std::cout<<"Removed UNSAT NODE" << solveQueue[i]->id() <<" FROM SOLVEQUEUE" << std::endl;
//          solveQueue.erase(solveQueue.begin() + i);
//      }
//  }
//  for( int i = 0; i < splitQueue.size(); i++ ){
//      assert(splitQueue[i] != 0 && "removeUnsatNodesFromQueues");
//      if( splitQueue[i]->getState() == TreeNode::unsat ){
//          std::cout<<"Removed UNSAT NODE" << solveQueue[i]->id() <<" FROM SPLITQUEUE" << std::endl;
//          splitQueue.erase(splitQueue.begin() + i);
//      }
//  }
//}

} // namespace Pcasso
