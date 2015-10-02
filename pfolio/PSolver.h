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

/** forward declaration */
class EnumerateMaster;
  
class PSolver
{

    Riss::PfolioConfig* privateConfig; // do be able to construct object without modifying configuration
    bool deleteConfig;
    Riss::PfolioConfig& pfolioConfig;  // configuration for this portfolio solver

    bool initialized;     // indicate whether everything has been setup already
    bool simplified;      // indicate whether global formula has been simplified with global preprocessor already
    bool killed;          // killed all childs already?
    int threads;
    int winningSolver;     // id of the thread of the solver that won

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

    EnumerateMaster* modelMaster; // object that controls parallel model enumeration
    
    std::string defaultConfig;                     // name of the configuration that should be used
    std::string defaultSimplifierConfig;           // name of the configuration that should be used by the global simplification
    std::vector< std::string > incarnationConfigs; // strings of incarnation configurations

    std::vector<unsigned short int> hardwareCores; // list of available cores for this parallel solver
    
    // communicate with external solvers
    ClauseRingBuffer* externBuffer;            // special buffer that should be used to send clauses to
    ClauseRingBuffer* externSpecialBuffer;     // special buffer that should be used to send clauses to
    
    /** store original formula for incarnations that do not want to use global preprocessing */
    class OriginalFormula {
    public: 
      vec<Lit>    trail;    // trail for learned clause minimization
      vec<CRef>   clauses;  // List of problem clauses.
      ClauseAllocator ca; // clause allocator
      vec<double> activity; // A heuristic measurement of the activity of a variable.
      Heap<Solver::VarOrderLt> order_heap;  // A priority queue of variables ordered with respect to the variable activity.
      vec<Solver::VarFlags>    varFlags;    // state of variables
      vec<Solver::VarData>     vardata;     // Stores reason and level for each variable.
      int nVars;
      
      OriginalFormula(const vec<Lit>&  originaltrail, const vec<CRef>& originalclauses, const ClauseAllocator& originalca, const int vars,
	const vec<double>& originalactivity,
	const Heap<Solver::VarOrderLt>& originalorder_heap,
	const vec<Solver::VarFlags>&    originalvarFlags,
	const vec<Solver::VarData>&     originalvardata
      ) : nVars(vars)
      , order_heap(Solver::VarOrderLt(activity))
      {
	originaltrail.copyTo( trail );
	originalclauses.copyTo( clauses );
	originalca.copyTo( ca );
	originalactivity.copyTo(activity);
	originalorder_heap.copyOrderTo(order_heap);
	originalvarFlags.copyTo( varFlags );
	originalvardata.copyTo(  vardata  );
      }
      
      ~OriginalFormula () {
	vardata.clear(true);
	varFlags.clear(true);
	order_heap.clear(true);
	activity.clear(true);
	ca.clear(true);
	clauses.clear(true);
	trail.clear(true);
      }
    };
    OriginalFormula* originalFormula; // data of original formula after parsing (if not set to be used, equal to nullptr)
    
    
    CommunicationData* externalData;    // pointer to the data, that is shared among all threads
    TreeReceiver* externalParent;       // handle to communcation of parent node
    
    // Output for DRUP unsat proof
    FILE* drupProofFile;

  public:

    PSolver(PfolioConfig* externalConfig = nullptr, const char* configName = nullptr, int externalThreads = -1) ;

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
    void setGlobalSimplifierConfig(const string& _config) { defaultSimplifierConfig = _config; }

    //
    // solve the formula in parallel, including communication and all that
    //
    /** Search for a model that respects a given set of assumptions
     *  Note: when this method is called the first time, the solver incarnations are created
     */
    Riss::lbool solveLimited(const Riss::vec<Riss::Lit>& assumps);

    /** simplify given formula with the global preprocessor 
     *  (only once, sets simplified flag)
     *  @return state of the formula, adds model, if state is l_true
     */
    Riss::lbool simplifyFormula();

    /** use global simplifier to re-setup given model */
    void extendModel( Riss::vec< Riss::lbool>& externalModel );
    
    
    /** use these buffers when initializin the solver to send clauses to, also cross link own buffers back */
    void setExternBuffers(ClauseRingBuffer* getBuffer, ClauseRingBuffer* getSpecialBuffer);
	
    //
    // executed only for the first solver (e.g. for parsing and simplification)
    //
    //

    /** The current number of original clauses of the 1st solver. */
    int nClauses() const;

    /** return reference to the clause with the given index */
    Clause& GetClause( int index ) const ;
    
    /** The current number of variables of the 1st solver. */
    int nVars() const;

    /** The current number of total literals in the formula of the 1st solver. */
    int nTotLits() const;

    /** reserve space for enough variables in the first solver */
    void reserveVars(Riss::Var v);

    /** Removes already satisfied clauses in the first solver */
    bool simplify();
    
    /** execute for first solver, or winning solver, if there is a winning solver */
    int getNumberOfTopLevelUnits();
    
    /** execute for first solver, or winning solver, if there is a winning solver */
    Lit trailGet( int index );

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
    
    /** overwrite a thread configuration from the outside, the thread will work on the original formula then
     * @param preferredSequentialConfig configuration to be set with "setPreset" for the given thread
     * @param thread worker number that should use this configuration
     */
    void overwriteAsIndependent(const string& preferredSequentialConfig, int thread);

    /** set CommunicationData from the outside, to be used to setup the solver
     *  Note: when this solver is shut down, nothing is deleted additionally
     */
    void setExternalCommunication(Communicator* com);
    
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

public:
    /** stops all parallel workers and kill their processes
     * note: afterwards, no other operations should be executed any more (for now)
     */
    void kill();
    
    /** tell pfolio solver about enumeration */
    void setEnumnerationMaster(EnumerateMaster* modelMaster);

};

inline FILE* PSolver::getDrupFile()
{
    return drupProofFile;
}


} // namespace Riss

#endif
