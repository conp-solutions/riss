/*****************************************************************************************[master.h]
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

#ifndef RISS_PCASSO_MASTER_H
#define RISS_PCASSO_MASTER_H

#define COMPILE_SPLITTER

// splitter
#include "riss/utils/LockCollection.h"
#include "pcasso/PartitionTree.h"
#include "pcasso/ISolver.h"

// Minisat
#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/utils/Options.h"
#include "riss/utils/ParseUtils.h"
#include "riss/utils/Statistics-mt.h"
#include "riss/utils/System.h"

// libs
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <zlib.h>
#include <vector>
#include <deque>
#include <cstdio>
#include <algorithm>    // std::sort

// using namespace std;
// using namespace Pcasso;
// using namespace Riss;

namespace Pcasso
{

class Master
{
  public: // Davide
    // void removeUnreachableNodes(TreeNode* node);
    /** Stops all the nodes that are known to be unsat */
    void killUnsatChildren(unsigned int i);
    // void removeUnsatNodesFromQueues();
    // void killUnsatChildren(int tDataIndex);

    /** configuration with which each solver is initialized */
    static Riss::CoreConfig defaultSolverConfig;

    struct Parameter {
        int    verb;
        bool   pre;
        std::string dimacs;
    };

  private:
    // Non-limiting memory limit
    //MLim  ub_lim;

    // represents the state of a thread
    // describe what the thread does at the moment
    // after work this will be set to unclean
    // only the master thread is allowed to set this back to idle and reuse it
    enum state { idle = 1, working = 2, splitting = 3, unclean = 4, sleeping = 5 };

    // to be stored in original formula
    struct clause {
        int size;
        Riss::Lit* lits;
    };

    struct clause_size {
        std::vector<clause>& vectorClause;
        clause_size(std::vector<clause>& vc) : vectorClause(vc) {}
        bool operator()(clause x, clause y)
        {
            return x.size < y.size;
        }
    };

    // data that is passed to the thread that is responsible for solving the instance
    struct ThreadData {
        TreeNode* nodeToSolve;  // node of the partition tree that has to be solved next
        int id;         // number from 0 to thread-1 to identify thread
        int result;         // outcome of the search (10 SAT, 20 UNSAT, 0 UNKNOWN)
        double timeout;     // in seconds, -1 no timeout
        int conflicts;      // -1 no limit
        pthread_t handle;       // handle to the thread
        Master* master;     // handle to the master class, for backward communication (e.g. wakeup)
        Master::state s;        // state of the thread
        ISolver* solver;         // The most recent solver
        ThreadData() : nodeToSolve(0), id(-1), result(0), timeout(-1), conflicts(-1), handle((pthread_t)0), s(idle), master(0), solver(NULL) {}
    };

    // to maintain the original formula
    int maxVar;
    std::vector< clause > originalFormula;
    Riss::vec<Riss::lbool>* model;       // model that will be produced by one of the child nodes
    //std::vector<char> polarity;   // preferred polarity from the last solver, for the next solvers
    // std::vector<double> activity; // activity of each variable

    // to maintain the parameters that have been specified on the commandline
    Parameter param;

    unsigned int threads;   // number of threads
    int64_t      space_lim;       // Total number of bytes reserved for Riss::ClauseAllocator and Riss::vec in each solver
    ThreadData* threadData; // data for each thread

    // to lock whenever some critical section has to be executed
    ComplexLock communicationLock;
    // to wake up the master / to let the master sleep if there is no work to do
    ComplexLock sleepLock;
    // state of the master thread
    volatile state masterState;

    // printed solution?
    bool done;

    double timeout;

    // handle for the result file
    FILE* res;

    bool plainpart;

    // partition tree and work queue
    TreeNode root;
    std::deque<TreeNode*> solveQueue;
    std::deque<TreeNode*> splitQueue;


    // wakes up the master thread such that it can check for the reason
    // NOTE: the reason needs to be stated in the ThreadData block of the calling thread
    void notify();

    // sets the master into sleep state
    void goSleeping();


    /** std::dequeues the next instance from the queue to the next free core
     * @param return false, if no core was available. Thus, thread and solver instance will not be created.
     */
    bool solveNextQueueEle();

    // call a new instance of the solver, that solves the instance
    // parameter is assumed to be:  ThreadData* data
    static void* solveInstance(void* data);

    // call a new instance of the solver, that splits the instance
    // parameter is assumed to be:  ThreadData* data
    static void* splitInstance(void* data);

    /** creates an extra instance for splitting a node in the tree
     *   takes care of all child nodes, if UNSAT is found
     *   retruns asynchrnously
     */
    bool splittFormula(TreeNode* data);

    /** adds a newly created TreeNode to the two workqueues
     *  NOTE: not thread safe
     */
    void addNode(TreeNode*);

    /* parses formula into originalFormulaVector
     * @param filename if empty, parse from stdin
     */
    int parseFormula(std::string filename);

    // return the formula for reading (for the threads)
    const std::vector< clause >& formula() const ;
    // return number of variables for reading
    const unsigned int varCnt() const;

    /** submits the model to the master
     *   NOTE: the model has to be complete and not only be the model of one of the nodes in the tree, not thread safe
     */
    void submitModel(const Riss::vec<Riss::lbool>& fullModel);

    // lock access to shared data for any thread
    void lock();

    // unlock access to shared data again (should be called only be the lock owner)
    void unlock();

    /** interrupts a thread, if it is known that this thread solves an unsatisfiable node
     * NOTE: kills and joins the thread
     */
    void stopThread(uint32_t threadId);

  public:

    // setup the object correctly
    Master(Parameter p);
    // free all the resources again
    ~Master();

    // to be called from the main function
    int main(int argc, char** argv);

    // setup everything and handle the conrol loop
    int run();

    void shutdown() { done = true; notify(); }

    friend class TreeNode;

    static long long int getCpuTimeMS()
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
            perror("clock_gettime()");
            ts.tv_sec = -1;
            ts.tv_nsec = -1;
        }
        return (long long int) ts.tv_sec * 1000 + ts.tv_nsec / 1000000.0;
    }

    // statistics section
  public:
    unsigned createdNodeID;
    unsigned unsolvedNodesID;
    unsigned splitSolvedNodesID;
    unsigned retryNodesID;
    unsigned treeHeightID;
    unsigned evaLevelID;
    unsigned loadSplitID;
    unsigned stoppedUnsatID;
    unsigned TotalConflictsID;
    unsigned TotalDecisionsID;
    unsigned TotalRestartsID;
    unsigned splitterCallsID;
    unsigned solverMaxTimeID;
    unsigned nConflictKilledID;
};

} // namespace Pcasso

#endif
