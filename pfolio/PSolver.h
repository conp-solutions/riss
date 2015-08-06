/***************************************************************************************[PSolver.h]
Copyright (c) 2014,      Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_Minisat_PSolver_h
#define RISS_Minisat_PSolver_h

#include "riss/core/Solver.h"
#include "riss/core/CoreConfig.h"
#include "riss/core/Communication.h"
#include "coprocessor/CP3Config.h"

#include "pfolio/PfolioConfig.h"

#include "pthread.h"


namespace Riss
{

class PSolver
{

    Riss::PfolioConfig* privateConfig; // do be able to construct object without modifying configuration
    bool deleteConfig;
    Riss::PfolioConfig& pfolioConfig;  // configuration for this portfolio solver

    bool initialized;
    int threads;

    Coprocessor::CP3Config*    globalSimplifierConfig; /// configuration object for global simplifier
    Coprocessor::Preprocessor* globalSimplifier;       /// global object that initially runs simplification on the formula for all solvers together

    Riss::vec<Riss::Solver*> solvers;
    CoreConfig*              configs;   // the configuration for each solver
    Coprocessor::CP3Config*  ppconfigs; // the configuration for each preprocessor

    CommunicationData* data;      // major data object that takes care of the sharing
    Communicator** communicators; // interface between controller and SAT solvers
    pthread_t* threadIDs;         // pthread handles for the threads

    ProofMaster* proofMaster;     // in a portfolio setup, use the proof master for generating DRUP proofs
    OnlineProofChecker* opc;      // check the proof on the fly during its creation

    std::string defaultConfig;                     // name of the configuration that should be used
    std::string defaultSimplifierConfig;           // name of the configuration that should be used by the global simplification
    std::vector< std::string > incarnationConfigs; // strings of incarnation configurations

    // Output for DRUP unsat proof
    FILE* drupProofFile;

  public:

    PSolver(PfolioConfig* externalConfig = 0, const char* configName = 0, int externalThreads = -1) ;

    ~PSolver();

    /** return the handle for the drup file */
    FILE* getDrupFile();

    /** set the handle for the global DRUP file */
    void setDrupFile(FILE* drupFile);

    int verbosity, verbEveryConflicts; // how much information to be printed

    Riss::vec<Riss::lbool> model;             // If problem is satisfiable, this std::vector contains the model (if any).
    Riss::vec<Riss::Lit>   conflict;          // If problem is unsatisfiable (possibly under assumptions),  this std::vector represent the final conflict clause expressed in the assumptions.

    //
    // Control of parallel behavior
    //
    CoreConfig& getConfig(const int solverID);

    Coprocessor::CP3Config& getPPConfig(const int solverID);

    /** set global pp config */
    void setGlobalSimplifierConfig(const std::string& _config) { defaultSimplifierConfig = _config; }

    //
    // solve the formula in parallel, including communication and all that
    //
    /** Search for a model that respects a given set of assumptions
     *  Note: when this method is called the first time, the solver incarnations are created
     */
    Riss::lbool solveLimited(const Riss::vec<Riss::Lit>& assumps);

    //
    // executed only for the first solver (e.g. for parsing and simplification)
    //
    //

    /** The current number of original clauses of the 1st solver. */
    int nClauses() const;

    /** The current number of variables of the 1st solver. */
    int nVars() const;

    /** The current number of total literals in the formula of the 1st solver. */
    int nTotLits() const;

    /** reserve space for enough variables in the first solver */
    void reserveVars(Riss::Var v);

    /** Removes already satisfied clauses in the first solver */
    bool simplify();

    //
    // executed for all present solvers:
    //
    //

    /** Add a new variable with parameters specifying variable mode to all solvers */
    Riss::Var  newVar(bool polarity = true, bool dvar = true, char type = 'o');

    /** Add a clause to the solver without making superflous internal copy. Will change the passed std::vector 'ps'.
     *  @return false, if the addition of the clause results in an unsatisfiable formula
     */
    bool addClause_(Riss::vec<Riss::Lit>& ps);

    /** Add a clause to the online proof checker.. Not implemented for parallel solver */
    void addInputClause_(Riss::vec<Riss::Lit>& ps);

    void interrupt(); // Trigger a (potentially asynchronous) interruption of the solver.

    void setConfBudget(int64_t x); // set number of conflicts for the next search run

    void budgetOff(); // reset the search bugdet

    /** parse the combined configurations
     * Format: [N]configN[N+1]configN+1...
     * and split them into the data strcuture incarnationConfigs
     */
    void parseConfigurations(const std::string& combinedConfigurations);

  protected:

    /** initialize all the thread configurations
     */
    void createThreadConfigs();

    /** initialize all the threads
     * @return false, if the initialization failed
     */
    bool initializeThreads();

    /** start solving all tasks with the given number of threads
     */
    void start();

    /** the master thread sleeps until some thread is done
     * @param waitState wait until the given condition is met
     *  note: will sleep, and sleep again, until the criterion is reached
     */
    void waitFor(const WaitState waitState);

    /** interrupt all solvers that are running at the moment once they reached level 0
     * @param forceRestart forces all solvers to perform a restart
     * note: calling thread will sleep until all threads are interrupted and waiting
     */
    void interrupt(const bool forceRestart);

    /** continue with the current work
     *  note: simply releases all waiting workers once
     */
    void continueWork();

    /** stops all parallel workers and kill their processes
     * note: afterwards, no other operations should be executed any more (for now)
     */
    void kill();
};

inline FILE* PSolver::getDrupFile()
{
    return drupProofFile;
}


} // namespace Riss

#endif
