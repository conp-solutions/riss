/****************************************************************************************[Solver.h]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
                CRIL - Univ. Artois, France
                LRI  - Univ. Paris Sud, France

Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2013, Norbert Manthey, LGPL v2, see LICENSE

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

#ifndef RISS_Minisat_Solver_h
#define RISS_Minisat_Solver_h

#include "riss/mtl/Vec.h"
#include "riss/mtl/Heap.h"
#include "riss/mtl/Alg.h"
#include "riss/utils/Options.h"
#include "riss/utils/System.h"
#include "riss/utils/Compression.h"
#include "riss/core/SolverTypes.h"
#include "riss/core/BoundedQueue.h"
#include "riss/core/Constants.h"
#include "riss/core/CoreConfig.h"

//
// choose which bit width should be used
// (used in level-X-look-ahead and FM)
//
#define DONT_USE_128_BIT
#ifndef DONT_USE_128_BIT
    #define LONG_INT __uint128_t
    #define USEABLE_BITS 127
#else
    #define LONG_INT uint64_t
    #define USEABLE_BITS 63
#endif

// forward declarations
//
namespace Coprocessor
{
class Preprocessor;
class CP3Config;
class CoprocessorData;
class Propagation;
class BoundedVariableElimination;
class Probing;
class Symmetry;
class RATElimination;
class FourierMotzkin;
class ExperimentalTechniques;
class ModPrep;
class BIG;
}

#ifdef PCASSO
namespace Pcasso
{
class SolverRiss;
}
#endif


// since template methods need to be in headers ...
extern Riss::IntOption opt_verboseProof;
extern Riss::BoolOption opt_rupProofOnly;

namespace Riss
{

class Communicator;
class OnlineProofChecker;
class IncSolver;

class EnumerateMaster;

//=================================================================================================
// Solver -- the main class:

class Solver
{

    friend class Coprocessor::Preprocessor;
    friend class Coprocessor::Propagation;
    friend class Coprocessor::BoundedVariableElimination;
    friend class Coprocessor::CoprocessorData;
    friend class Coprocessor::Probing;
    friend class Coprocessor::Symmetry;
    friend class Coprocessor::RATElimination;
    friend class Coprocessor::FourierMotzkin;
    friend class Coprocessor::ExperimentalTechniques;
    friend class Coprocessor::ModPrep;
    friend class Riss::IncSolver; // for bmc

    #ifdef PCASSO
    friend class Pcasso::SolverRiss; // SolverRiss is allowed to access all the solver data structures
    #endif

    CoreConfig* privateConfig; // do be able to construct object without modifying configuration
    bool deleteConfig;
    CoreConfig& config;
  public:

    /** part of the solve method that is executed */
    enum SolveCallType {
        full = 0,
        simplificationOnly = 1,
        afterSimplification = 2,
        initializeOnly = 3,
    };
    // Constructor/Destructor:
    //
    Solver(CoreConfig* externalConfig = 0, const char* configName = 0);


    ~Solver();
    /// tell the solver to delete the configuration it just received
    void setDeleteConfig() { deleteConfig = true; }

    /** indicate whether this incarnation is working on the original formula in pfolio*/
    bool independent() const { return privateConfig->opt_useOriginal; }

    // Problem specification:
    //

    Var     newVar(bool polarity = true, bool dvar = true, char type = 'o');     // Add a new variable with parameters specifying variable mode.
    void    reserveVars(Var v);


    bool    addClause(const vec<Lit>& ps);                      /// Add a clause to the solver.

    bool    addClause(const Clause& ps);                        /// Add a clause to the solver (all clause invariants do not need to be checked)
    bool    addEmptyClause();                                   /// Add the empty clause, making the solver contradictory.
    bool    addClause(Lit p);                                   /// Add a unit clause to the solver.
    bool    addClause(Lit p, Lit q);                            /// Add a binary clause to the solver.
    bool    addClause(Lit p, Lit q, Lit r);                     /// Add a ternary clause to the solver.


    bool    addClause_(vec<Lit>& ps, bool noRedundancyCheck = false);                           /// Add a clause to the solver without making superflous internal copy. Will
    /// change the passed vector 'ps'.
    void    addInputClause_(vec<Lit>& ps);                      /// Add a clause to the online proof checker

    /** integrate the given clause into the current state of the SAT solver
     *  @param clause vector with the literals of the clause
     *  @return l_False, if adding the clause turns the formula of the solver unsatisfiable, l_True, if addig the clause did not fail
     */
    lbool   integrateNewClause(vec<Lit>& clause);

    /** find and keep common prefix for given assumptions and current assumptions, adjusts backtracking level accordingly to enusre safe continue of search
     *
     * If there have not been assumptions before, or the current decision level is 0, 0 is returned immediately.
     *
     *  @param nextAssumptions ordered list of assumption literals that should be used for the next solve iteration
     *                         Note: assumptions will be reordered according to current search state
     *  @return number of assumptions that can be kept (assumptions that are same in this vector and in currently set assumptions)
     *                     Note: equals the search decision level that remains in the solver state
     */
    int     integrateAssumptions(vec<Lit>& nextAssumptions);

    // Solving:
    //
    bool    simplify();                             /// Removes already satisfied clauses.
    bool    solve(const vec<Lit>& assumps);         /// Search for a model that respects a given set of assumptions.
    lbool   solveLimited(const Riss::vec< Riss::Lit >& assumps, const SolveCallType preprocessCall = full);  /// Search for a model that respects a given set of assumptions (With resource constraints).
    bool    solve();                                /// Search without assumptions.
    bool    solve(Lit p);                           /// Search for a model that respects a single assumption.
    bool    solve(Lit p, Lit q);                    /// Search for a model that respects two assumptions.
    bool    solve(Lit p, Lit q, Lit r);             /// Search for a model that respects three assumptions.
    bool    okay() const;                           /// FALSE means solver is in a conflicting state

    void    toDimacs(FILE* f, const vec<Lit>& assumps);                 // Write CNF to file in DIMACS-format.
    void    toDimacs(const char *file, const vec<Lit>& assumps);
    void    toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max);
    void printLit(Lit l);
    void printClause(CRef c);
    void dumpAndExit(const char* filename, bool doExit = true, bool fullState = false);  // print the current formula without assumptions (p line, trail, clauses)

    // Convenience versions of 'toDimacs()':
    void    toDimacs(const char* file);
    void    toDimacs(const char* file, Lit p);
    void    toDimacs(const char* file, Lit p, Lit q);
    void    toDimacs(const char* file, Lit p, Lit q, Lit r);

    // Variable mode:
    //
    void    setPolarity(Var v, bool b);     /// Declare which polarity the decision heuristic should use for a variable. Requires mode 'polarity_user'.
    bool    getPolarity(Var v);
    void    setDecisionVar(Var v, bool b);  /// Declare if a variable should be eligible for selection in the decision heuristic.
    // NuSMV: SEED
    void    setRandomSeed(double seed); // sets random seed (cannot be 0)
    // NuSMV: SEED END
    // NuSMV: PREF MOD
    vec<Var> preferredDecisionVariables;

    /*
     * Add a variable at the end of the list of preferred variables
     * Does not remove the variable from the standard ordering.
     */
    void addPreferred(Var v);

    /*
     * Clear vector of preferred variables.
     */
    void clearPreferred();
    // NuSMV: PREF MOD END


    // Read state:
    //
    lbool   value(Var x) const;             /// The current value of a variable.
    lbool   value(Lit p) const;             /// The current value of a literal.
    lbool   modelValue(Var x) const;        /// The value of a variable in the last model. The last call to solve must have been satisfiable.
    lbool   modelValue(Lit p) const;        /// The value of a literal in the last model. The last call to solve must have been satisfiable.
    int     nAssigns()      const;          /// The current number of assigned literals.
    int     nClauses()      const;          /// The current number of original clauses.
    int     nLearnts()      const;          /// The current number of learnt clauses.
    int     nVars()      const;             /// The current number of variables.
    int     nTotLits()      const;          /// The current number of total literals in the formula.
    int     nFreeVars()      const;

    // Resource contraints:
    //
    void    setConfBudget(int64_t x);
    void    setPropBudget(int64_t x);
    void    budgetOff();
    void    interrupt();          /// Trigger a (potentially asynchronous) interruption of the solver.
    void    clearInterrupt();     /// Clear interrupt indicator flag.

    // Memory managment:
    //
    virtual void garbageCollect();
    void    checkGarbage(double gf);
    void    checkGarbage();

    // Output for DRUP unsat proof
    FILE*               proofFile;

    // Extra results: (read-only member variable)
    //
    vec<lbool> model;             /// If problem is satisfiable, this vector contains the model (if any).
    vec<Lit>   conflict;          /// If problem is unsatisfiable (possibly under assumptions),
    /// this vector represent the final conflict clause expressed in the assumptions.
    vec<Lit>    oc;               /// vector to store clauses for before being added -- for DRUP output

    // Mode of operation:
    //
    int       verbosity;
    int       verbEveryConflicts;

    // check the polarity of all clauses
    bool posInAllClauses; // indicate that there is a positive literal in all added clauses (after reducing units)
    bool negInAllClauses; // indicate that there is a negative literal in all added clauses (after reducing units)
    void updatePosNeg(bool somePosInClause, bool somNegInClause);   /// if a clause is added in another way, the flags can be updated with this method accordingly

    /** class to calculate exponential moving averages
     * Series to control restarts, along "Evaluating CDCL Restart Schemes" by Biere and Fröhlich, POS 2015
     * Note: uses initialization, as described in the paper ( first alphas will be 2^{-step}, until 2^{-step} < alpha )
     * */
    class EMA
    {
        double value, alpha; // value, and update value of series
        int64_t steps;        // number of added elements
        bool initialized;    // indicate whether using actual alpha is ok now
      public:
        EMA(double _alpha = 0) : value(0), alpha(_alpha), steps(0), initialized(false) {}

        /** assign a new value for alpha, reset series */
        void reinit(double _alpha) { value = 0; alpha = _alpha ; steps = 0; initialized = false;}

        /** reset all values, keep alpha */
        void reset() { value = 0; steps = 0; initialized = false; assert(alpha != 0 && "should be initilized somehow");}

        /** add next value to series */
        void update(double g_i)
        {
            ++ steps;
//  std::cerr << "c update EMA with " << g_i << " at step " << steps << " with alpha=" << alpha << " from " << value;
            if (!initialized) {
                double compareAlpha = pow(2, - steps);
                if (compareAlpha <= alpha) { initialized = true; compareAlpha = alpha; }  // set initiliazed to true, so that we do not have to do this calculation any more
                value = compareAlpha * g_i + (1 - compareAlpha) * value;   // exponential update
            } else {
                value = alpha * g_i + (1 - alpha) * value;   // exponential update
            }
//  std::cerr << " to " << value << std::endl;
        }

        int64_t getSteps() const { return steps; }

        double getValue() const { return value; }
    };

    EMA slow_interpretationSizes; // collect all conflict levels
    EMA slow_LBDs;                // collect all clause LBDs
    EMA recent_LBD;               // collect all LBDs, slow moving average

    /// collection of all the values that are useful to have a hybrid restart strategy (Paper by Oh at SAT conference 2015)
    class RestartSwitchSchedule
    {
      public:
        int lubyRestarts, geometricRestarts, constantRestarts; // have extra counters for the restart types (luby, geometric, constant)
      private:
        int initial_restartScheduleSwitchInterval;       // initial size of the interval
        int restartScheduleSwitchInterval;               // current size of the interval to switch between dynamic and static -1 == never
        int restartScheduleSwitch_conflicts;             // next number of conflicts to switch the restart schedule -1 == never
        int lastSwitch;                                  // number of conflicts when the last switch happened
        bool performRestarts;                            // are we currently in a "noRestart" phase?

      public:
        RestartSwitchSchedule()
            : lubyRestarts(0), geometricRestarts(0), constantRestarts(0)
            , initial_restartScheduleSwitchInterval(300), restartScheduleSwitchInterval(300)
            , restartScheduleSwitch_conflicts(0), lastSwitch(0)
            , performRestarts(true)
        {}

        void initialize(int firstIntervalSize)
        {
            initial_restartScheduleSwitchInterval = firstIntervalSize;
            restartScheduleSwitchInterval = initial_restartScheduleSwitchInterval;
            setupNextFullInterval(0, 1, 0.666);   // setup first interval with given size
        }

        /** indicate whether the heuristic should be switched (at all) */
        bool heuristicSwitching() const { return restartScheduleSwitch_conflicts > 0; }

        /** hit interval bound, has to take care of interval and schedule now */
        bool reachedIntervalLimit(uint64_t searchConflicts) { return searchConflicts >= restartScheduleSwitch_conflicts; }

        /** check whether the current interval limit is for the second half of the interval
         * Note: useful to be checked after @reachedIntervalLimit returns true
         * @return true, if full interval is
         */
        bool finishedFullInterval() const { return restartScheduleSwitch_conflicts == lastSwitch + restartScheduleSwitchInterval; }

        /** setup all values for the next interval */
        void setupNextFullInterval(uint64_t searchConflicts, double opt_rswitch_interval_inc, double opt_dynamic_rtype_ratio)
        {
            lastSwitch = searchConflicts;
            restartScheduleSwitchInterval = (double)restartScheduleSwitchInterval * opt_rswitch_interval_inc;
            restartScheduleSwitch_conflicts = searchConflicts + (double)restartScheduleSwitchInterval * (opt_dynamic_rtype_ratio);
        }

        /** set the limits to execute the second half of the schedule */
        void setupSecondIntervalHalf()
        {
            restartScheduleSwitch_conflicts = lastSwitch + restartScheduleSwitchInterval;
        }

        void resetInterval()
        {
            if (heuristicSwitching()) {
                restartScheduleSwitchInterval = initial_restartScheduleSwitchInterval;
                setupNextFullInterval(0, 1, 1);  // setup first interval with given size
            }
        }

    } restartSwitchSchedule;

    /// Object that controls configuration of search, might be changed during search
    class SearchConfiguration
    {
      public:

        SearchConfiguration() :
            K(0.8)
            , R(1.4)
            , sizeLBDQueue(50)
            , sizeTrailQueue(5000)
            , firstReduceDB(4000)
            , incReduceDB(300)
            , specialIncReduceDB(1000)
            , lbLBDFrozenClause(30)
            , lbSizeMinimizingClause(30)
            , lbLBDMinimizingClause(6)
            , uhle_minimizing_size(0)
            , uhle_minimizing_lbd(6)
            , use_reverse_minimization(false)
            , lbSizeReverseClause(12)
            , lbLBDReverseClause(6)
            , var_decay(0.95)
            , var_decay_start(0.95)
            , var_decay_end(0.95)
            , var_decay_inc(0.01)
            , var_decay_distance(5000)
            , clause_decay(0.999)
            , ccmin_mode(2)
            , phase_saving(2)
            , restarts_type(0)
        {}

        double    K;
        double    R;
        double    sizeLBDQueue;
        double    sizeTrailQueue;

        // Constants for reduce DB
        int firstReduceDB;
        int incReduceDB;
        int specialIncReduceDB;
        unsigned int lbLBDFrozenClause;

        // Constant for reducing clause
        int lbSizeMinimizingClause;
        unsigned int lbLBDMinimizingClause;
        int uhle_minimizing_size;
        int uhle_minimizing_lbd;
        bool use_reverse_minimization; // has to be set explicitely here: ReverseMinimization.enabled
        int lbSizeReverseClause;
        int lbLBDReverseClause;

        double    var_decay;
        double    var_decay_start;    // have dynamic var decay (starting point)
        double    var_decay_end;      // end point
        double    var_decay_inc;      // increment by this value
        int       var_decay_distance; // increment every X conflicts
        double    clause_decay;

        int       ccmin_mode;         // Controls conflict clause minimization (0=none, 1=basic, 2=deep).
        int       phase_saving;       // Controls the level of phase saving (0=none, 1=limited, 2=full).

        int       restarts_type;       // choose series (dynamic, luby, geometric)
    } searchconfiguration;

    /// store all required things to change the configuration during search
    struct ConfigurationScheduler {
        vec<SearchConfiguration> searchConfigs; // list of configs that use used for iteration
        vec<int> searchConfigConflicts;         // number of conflicts allowed for the given configuration
        int lastConfigChangeConflict;           // store number of conflict when seeing last conflict


        int currentConfig;                      // index of currently used configuration
        float growFactor;                       // increase of all conflict limits after running each configuration once

        void reset(Riss::Solver::SearchConfiguration& searchConfiguration);
        void initConfigs(const Riss::Solver::SearchConfiguration& searchConfiguration, std::string schedule, float factor, int defaultC, int usualC);                     // initialize all configurations
        bool checkAndChangeSearchConfig(int conflicts, SearchConfiguration& searchConfiguration);       // takes care of whether the configuration should be changed at this restart, @return true, if new configuration was picked
        ConfigurationScheduler();
    } configScheduler;

    double    random_var_freq;
    double    random_seed;
    bool      rnd_pol;            // Use random polarities for branching heuristics.
    bool      rnd_init_act;       // Initialize variable activities with a small random value.
    double    garbage_frac;       // The fraction of wasted memory allowed before a garbage collection is triggered.


    // Statistics: (read-only member variable)
    //
    uint64_t nbRemovedClauses, nbReducedClauses, nbDL2, nbBin, nbUn, nbReduceDB, solves, starts, decisions, rnd_decisions, propagations, conflicts, nbstopsrestarts, nbstopsrestartssame, lastblockatrestart;
    uint64_t dec_vars, clauses_literals, learnts_literals, max_literals, tot_literals;

    void applyConfiguration(); // assigns relevant values of search configuration to data structures/counters
  protected:

    int performSimplificationNext;

    uint64_t nbLCM, nbLitsLCM, nbConflLits, nbLCMattempts, nbLCMsuccess, npLCMimpDrop, nbRound1Lits, nbRound2Lits, nbLCMfalsified;
    Clock LCMTime;

    bool simplifyLCM(); // learned clause minimization style inprocessing inside the solver, based on vivificatoin

    /** run all the simplification that is necessary for one lause
     * Note: this method assumes the clause is detached before being called
     * @param cr index of the clause to be processed (might be learned or original clause)
     * @param LCMconfig 5x5 number of what to do in forward and backward iteration of LCM (3*5+5 seems to be strongest)
     * @param fullySimplify allow to actually run vivification on that clause, otherwise, only falsified literals are removed
     * @return true, if the clause reference should be kept. In case false is returned, the clause is already removed from the solver
     */
    bool simplifyClause_viviLCM(const Riss::CRef cr, int LCMconfig, bool fullySimplify = true);

    int simplifyLearntLCM(Clause& c, int vivificationConfig); // simplify a non-watched clause, and perform vivification on it

    long curRestart;
    // Helper structures:
    //
  public:

    /** structure that allows to store the binary reason for a variable assignment implicitely*/
    struct ReasonStruct {

        unsigned data : 31;
        unsigned isBinary : 1;
        ReasonStruct() : data(0), isBinary(0) {}

        ReasonStruct(const CRef& cr) : data(cr), isBinary(0) {}
        ReasonStruct(const Lit& l) : data(toInt(l)), isBinary(1) {}

        void setReason(const CRef cr) { data = cr; isBinary = 0; }
        void setReason(const Lit l) { data = toInt(l); isBinary = 1; }

        CRef getReasonC() const { return data; }
        Lit getReasonL() const { return toLit(data); }

        bool isBinaryClause() const { return isBinary; }
    };

    struct VarData {
        ReasonStruct reason; // new reason struct
        int level;
        Lit dom;
        int32_t position; /// for hack
        #ifdef PCASSO
        unsigned dependencyLevel;
        #endif
        VarData() : reason(CRef_Undef), level(-1), dom(lit_Undef), position(-1)
            #ifdef PCASSO
            , dependencyLevel(0)
            #endif
        {}
        VarData(CRef r, int l, Lit li, int32_t p) : reason(r), level(l), dom(li), position(p)
            #ifdef PCASSO
            , dependencyLevel(0)
            #endif
        {}
        VarData(Lit r, int l, Lit li, int32_t p) : reason(r), level(l), dom(li), position(p)
            #ifdef PCASSO
            , dependencyLevel(0)
            #endif
        {}
    };

  protected:
    static inline VarData mkVarData(CRef cr, int l)
    {
        VarData d(cr, l, lit_Undef, -1);
        return d;
    }

    static inline VarData mkVarData(Lit reasonLiteral, int l)
    {
        VarData d(reasonLiteral, l, lit_Undef, -1);
        return d;
    }

  public:

    // movd watcher data structure to solver types!


    struct VarOrderLt {
        const vec<double>&  activity;
        bool operator()(Var x, Var y) const { return activity[x] > activity[y]; }
        VarOrderLt(const vec<double>&  act) : activity(act) { }
    };

  protected:
    // Solver state:
    //
    int lastIndexRed;
    bool                ok;               // If FALSE, the constraints are already unsatisfiable. No part of the solver state may be used!
    double              cla_inc;          // Amount to bump next clause with.
  public:
    vec<double>         activity;         // A heuristic measurement of the activity of a variable.
  protected:
    double              var_inc;          // Amount to bump next variable with.
  public: // TODO FIXME undo after debugging!
    OccLists<Lit, vec<Watcher>, WatcherDeleted> watches;          // 'watches[lit]' is a list of constraints watching 'lit' (will go there if literal becomes true).
    // no watchesBin, incorporated into watches

    /** structure to hande reverse minimization nicely
     *  uses data structures of solver for incomplete propagation
     */
    struct ReverseMinimization {
        vec<lbool> assigns;                                    // assignment for learned clause minimization
        vec<Lit>   trail;                                      // trail for learned clause minimization
        bool enabled;                                                 // indicate whether the technique is enabled

        int attempts;
        int revMindroppedLiterals;
        int revMinConflicts;
        int revMincutOffLiterals;
        int succesfulReverseMinimizations;

        ReverseMinimization(bool doUse) : enabled(doUse), attempts(0), revMindroppedLiterals(0), revMinConflicts(0), revMincutOffLiterals(0), succesfulReverseMinimizations(0) { }

        lbool value(const Var& x) const { return assigns[x]; }                         /// The current value of a variable.
        lbool value(const Lit& p) const { return assigns[var(p)] ^ sign(p); } /// The current value of a literal.
        void uncheckedEnqueue(const Lit& l) { assigns[ var(l) ] = sign(l) ? l_False : l_True; trail.push(l); }  /// add variable assignment

    } reverseMinimization;

    bool earlyAssumptionConflict; // abort incremental calls as soon as we know it conflicts

  public: // TODO: set more nicely, or write method!
    vec<CRef>           clauses;          // List of problem clauses.
    vec<CRef>           learnts;          // List of learnt clauses.

    struct VarFlags {
        lbool assigns;
        unsigned polarity: 1;
        unsigned decision: 1;
        unsigned seen: 1;
        unsigned extra: 2; // TODO: use for special variable (not in LBD) and do not touch!
        unsigned modifiedPositiveModels: 1; // modified the models of the positive literal of this variable
        unsigned modifiedNegativeModels: 1; // modified the models of the negative literal of this variable
        unsigned frozen: 1; // indicate that this variable cannot be used for simplification techniques that do not preserve equivalence
        #ifdef PCASSO
        unsigned varPT: 16; // partition tree level for this variable
        #endif
        VarFlags(char _polarity) : assigns(l_Undef), polarity(_polarity), decision(0), seen(0), extra(0), modifiedPositiveModels(0), modifiedNegativeModels(0), frozen(0)
            #ifdef PCASSO
            , varPT(0)
            #endif
        {}
        VarFlags() : assigns(l_Undef), polarity(1), decision(0), seen(0), extra(0), modifiedPositiveModels(0), modifiedNegativeModels(0), frozen(0)
            #ifdef PCASSO
            , varPT(0)
            #endif
        {}
    };

    /// all the data that is needed to handle equivalent literals, and the related methods
    class EquivalenceInfo
    {
        bool activeReplacements;                    // replaced by also points to offsets somewhere
        vec<Riss::Lit> equivalencesStack;   // stack of literal classes that represent equivalent literals which have to be processed
        #ifdef PCASSO
        vec<int> dependencyStack;           // store dependency for each SCC on the stack
        #endif
        Solver* solver;
      public:
        vec<Riss::Lit> replacedBy;          // stores which variable has been replaced by which literal
        vec<Riss::Lit> temporary;

        // methods
        EquivalenceInfo(Solver* _solver) : activeReplacements(false), solver(_solver) {}

        inline vec<Riss::Lit>& getEquivalenceStack() { return equivalencesStack; }

//         inline bool hasReplacements() const { return activeReplacements; }                     /// @return true means that there are variables pointing to other variables
        inline bool hasEquivalencesToProcess() const { return equivalencesStack.size() > 0; }

        inline void addEquivalenceClass(const Lit& a, const Lit& b, bool doShare = true
                                        #ifdef PCASSO
                                        , int dependencyLevel = -1
                                        #endif
                                       )
        {
            assert(var(a) < solver->nVars() && var(b) < solver->nVars() && "do not add variables that are larger than stack size");
            if (a != b) {
                equivalencesStack.push(a);
                equivalencesStack.push(b);
                equivalencesStack.push(Riss::lit_Undef);   // termination symbol!
                #ifdef PCASSO
                dependencyStack.push(dependencyLevel);
                #endif
                if (doShare && solver->isCommunicating()) {
                    temporary.clear();
                    temporary.push(a);
                    temporary.push(b);
                    #ifdef PCASSO
                    solver->updateSleep(&temporary, 2, dependencyLevel, false, true);
                    #else
                    solver->updateSleep(&temporary, 2, false, true);
                    #endif
                }
            }
        }
        template <class T>
        inline void addEquivalenceClass(const T& lits, bool doShare = true
                                        #ifdef PCASSO
                                        , int dependencyLevel = -1
                                        #endif
                                       )
        {
            for (int i = 0 ; i < lits.size(); ++ i) {
                assert(var(lits[i]) < solver->nVars() && "eq variables have to be in current formula");
                equivalencesStack.push(lits[i]);
            }
            equivalencesStack.push(Riss::lit_Undef);   // termination symbol!
            #ifdef PCASSO
            dependencyStack.push(dependencyLevel);
            #endif

            if (doShare && solver->isCommunicating()) {   // tell priss about shared equivalences
                temporary.clear();
                for (int i = 0 ; i < lits.size(); ++ i) { temporary.push(lits[i]); }
                #ifdef PCASSO
                solver->updateSleep(&temporary, lits.size(), dependencyLevel, false, true);
                #else
                solver->updateSleep(&temporary, lits.size(), false, true);
                #endif
            }
        }

        #ifdef PCASSO
        /** add received equivalence classes faster than checking each class on its own*/
        template <class T>
        inline void addEquivalenceClass(const T& lits, vec<int>& dependencyLevels)
        {
            int usedSCC = 0;
            for (int i = 0 ; i < lits.size(); ++ i) {
                equivalencesStack.push(lits[i]);
                if (lits[i] == lit_Undef) {  // reached end of an SCC?
                    assert(usedSCC < dependencyLevels.size() && "number of received scc has to fit");
                    dependencyStack.push(dependencyLevels[usedSCC++]);
                }
            }
            assert((lits.size() == 0 || lits[ lits.size() - 1 ] == lit_Undef) && "SCC should be separated by lit_Undef	 ");
        }
        #endif

        /** just return the next smaller reprentative */
        inline Lit getFirstReplacement(Lit l) const
        {
            if (var(l) >= replacedBy.size()) { return l; }
            return sign(l) ? ~replacedBy[var(l)] : replacedBy[var(l)];
        }

        /** return the smallest equivalent literal */
        inline Lit getReplacement(Lit l) const
        {
            if (var(l) >= replacedBy.size()) { return l; }
            while (var(replacedBy[var(l)]) != var(l)) { l = sign(l) ? ~replacedBy[var(l)] : replacedBy[var(l)]; }   // go down through the whole hierarchy!
            return l;
        }

        /** return the smallest equivalent literal */
        inline Lit getReplacement(const Var& v) const
        {
            if (v >= replacedBy.size()) { return mkLit(v, false); }
            return getReplacement(mkLit(v, false));
        }

        inline void growTo(const int& newSize)
        {
            for (Var v = replacedBy.size(); v < newSize; ++v) { replacedBy.push(mkLit(v, false)); }
        }

    } eqInfo;

    vec<VarFlags> varFlags;

//     vec<lbool>          assigns;          // The current assignments.
//     vec<char>           polarity;         // The preferred polarity of each variable.
//     vec<char>           decision;         // Declares if a variable is eligible for selection in the decision heuristic.
//     vec<char>           seen;


  public:
    /// set whether a variable can be used for simplification techniques that do not preserve equivalence
    void freezeVariable(const Var& v, const bool& frozen) { varFlags[v].frozen = frozen; }
    /// indicates that this variable cannot be used for simplification techniques that do not preserve equivalence
    bool isFrozen(const Var& v) const { return varFlags[v].frozen; }

    vec<Lit>            trail;            // Assignment stack; stores all assigments made in the order they were made.
    vec<VarData>        vardata;          // Stores reason and level for each variable.


//     vec<int>            nbpos;
    vec<int>            trail_lim;        // Separator indices for different decision levels in 'trail'.
    Compression         compression;      // if compression is enabled, this transformation info will be stored here

  protected:
    int                 qhead;            // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
    int                 realHead;         // indicate last literal that has been analyzed for unit propagation
    int                 simpDB_assigns;   // Number of top-level assignments since last execution of 'simplify()'.
    int64_t             simpDB_props;     // Remaining number of propagations that must be made before next execution of 'simplify()'.
    vec<Lit>            assumptions;      // Current set of assumptions provided to solve by the user.
  public:
    Heap<VarOrderLt>    order_heap;       // A priority queue of variables ordered with respect to the variable activity.
  protected:
    double              progress_estimate;// Set by 'search()'.
    bool                remove_satisfied; // Indicates whether possibly inefficient linear scan for satisfied clauses should be performed in 'simplify'.
    MarkArray           lbd_marker;

    #ifdef UPDATEVARACTIVITY
    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
    vec<Lit> lastDecisionLevel;
    #endif

  public: // TODO: set more nicely!
    ClauseAllocator     ca;
  protected:

    int nbclausesbeforereduce;            // To know when it is time to reduce clause database

    bqueue<unsigned int> trailQueue, lbdQueue; // Bounded queues for restarts.
    float sumLBD; // used to compute the global average of LBD. Restarts...


    // Temporaries (to reduce allocation overhead). Each variable is prefixed by the method in which it is
    // used, exept 'seen' wich is used in several places.
    //
    vec<Lit>            analyze_stack;
    vec<Lit>            analyze_toclear;
    vec<Lit>            add_tmp;
    unsigned long  MYFLAG;

//     vec<int> trailPos;          /// store the position where the variable is located in the trail exactly (for hack)

    // minisat style removal
    double max_learnts;
    double learntsize_factor;
    double learntsize_inc;
    double learntsize_adjust_start_confl;
    double learntsize_adjust_inc;
    double learntsize_adjust_confl;
    int    learntsize_adjust_cnt;

    Clock totalTime, propagationTime, analysisTime, preprocessTime, inprocessTime, extResTime, reduceDBTime, icsTime; // times for methods during search
    int preprocessCalls, inprocessCalls;    // stats

    // Resource contraints:
    //
    int64_t             conflict_budget;    // -1 means no budget.
    int64_t             propagation_budget; // -1 means no budget.
    bool                asynch_interrupt;

    // Main internal methods:
    //
    void     insertVarOrder(Var x);                                                    // Insert a variable in the decision order priority queue.

    Lit      pickBranchLit();                                                          // Return the next decision variable.
    void     newDecisionLevel();                                                       // Begins a new decision level.


    void     uncheckedEnqueue(Lit p, CRef from = CRef_Undef,                           // Enqueue a literal. Assumes value of literal is undefined.
                              bool addToProof = false, const unsigned dependencyLevel = 0);     // decide whether the method should furthermore add the literal to the proof, and whether the literal has an extra information (interegsting for decision level 0)
    void     uncheckedEnqueue(Lit p, Lit fromLit, bool addToProof = true, const unsigned dependencyLevel = 0); // same as the above method, but uses literal as the reason

    bool     enqueue(Lit p, CRef from = CRef_Undef);                                   // Test if fact 'p' contradicts current state, enqueue otherwise.

    CRef     propagate(bool duringAddingClauses = false);                              // Perform unit propagation. Returns possibly conflicting clause (during adding clauses, to add proof infos, if necessary)
    void     cancelUntil(int level);                                                   // Backtrack until a certain level.

    int      analyze(CRef confl, vec< Lit >& out_learnt, int& out_btlevel, unsigned int& lbd, unsigned& dependencyLevel);               // // (bt = backtrack, return is number of unit clauses in out_learnt. if 0, treat as usual!)
    void     analyzeFinal(Lit p, vec<Lit>& out_conflict);                              // COULD THIS BE IMPLEMENTED BY THE ORDINARIY "analyze" BY SOME REASONABLE GENERALIZATION?
    void     analyzeFinal(const Solver::ReasonStruct& conflictingClause, vec< Lit >& out_conflict, const Lit otherLit = lit_Undef); // in case of binary conflict, set the other lit!

    bool     litRedundant(Lit p, uint32_t abstract_levels, unsigned& dependencyLevel);                           // (helper method for 'analyze()')

    lbool    search(int nof_conflicts);                                                // Search for a given number of conflicts.

    void updateMetricsDuringAnalyze(const Lit p, const CRef cr, Clause& c, bool& foundFirstLearnedClause, unsigned int& dependencyLevel);   /// update metrics based on the current clause we are using

  public:
    /** Main solve method (assumptions given in 'assumptions')
     * @param preprocessCall control how to perform initialization and preprocessing
     *        0 usual behavior
     *        1 perform until preprocessing
     *        2 leave out everything above preprocessing
     * @return status of the formula
     */
    lbool    solve_(const SolveCallType preprocessCall = full);

  protected:
    void     reduceDB();                                                               // Reduce the set of learnt clauses.
    void     removeSatisfied(vec<CRef>& cs);                                           // Shrink 'cs' to contain only non-satisfied clauses.
  public:
    void     rebuildOrderHeap();
    void     varSetActivity(Var v, double value);      // set activity for a given variable
    double   varGetActivity(Var v) const;              // get activity for a given variable

  protected:
    // Maintaining Variable/Clause activity:
    //
    void     varDecayActivity();                       // Decay all variables with the specified factor. Implemented by increasing the 'bump' value instead.
    void     varBumpActivityD(Var v, double inc);      // Increase a variable with the current 'bump' value.
    void     varBumpActivity(Var v, double inverseRatio = 1) ;        // Increase a variable with the current 'bump' value, multiplied by 1 / (inverseRatio).
    void     claDecayActivity();                       // Decay all clauses with the specified factor. Implemented by increasing the 'bump' value instead.
    void     claBumpActivity(Clause& c, double inverseRatio = 1);     // Increase a clause with the current 'bump' value, multiplied by 1 / (inverseRatio).

    // Operations on clauses:
    //
  public:  // FIXME: could also declare PSolver as friend somewhere
    void     attachClause(CRef cr);                    // Attach a clause to watcher lists.
  protected:
    void     detachClause(CRef cr, bool strict = false);      // Detach a clause to watcher lists.

    void     removeClause(CRef cr, bool strict = false);      // Detach and free a clause.
    bool     locked(const Clause& c) const;            // Returns TRUE if a clause is a reason for some implication in the current state.
    bool     satisfied(const Clause& c) const;         // Returns TRUE if a clause is satisfied in the current state.
  public:
    bool addUnitClauses(const vec< Lit >& other);        // use the given trail as starting point, return true, if fails!
  protected:
    /** Calculates the Literals Block Distance, which is the number of
     *  different decision levels in a clause or list of literals.
     */
    template<typename T>
    int computeLBD(const T& lits, const int& litsSize);

    /** perform minimization with binary clauses of the formula
     *  @param lbd the current calculated LBD score of the clause
     *  @return true, if the clause has been shrinked
     */
    bool minimisationWithBinaryResolution(Riss::vec< Riss::Lit >& out_learnt, unsigned int& lbd, unsigned int& dependencyLevel);


    void     relocAll(ClauseAllocator& to);

    // Misc:
    //
  public:
    int      decisionLevel()      const;     // Gives the current decisionlevel.
  protected:
    uint32_t abstractLevel(Var x) const;     // Used to represent an abstraction of sets of decision levels.
    ReasonStruct& reason(Var x);
    const ReasonStruct& reason(Var x) const ;
    int      level(Var x) const;
    double   progressEstimate()      const;  // DELETE THIS ?? IT'S NOT VERY USEFUL ...
    bool     withinBudget()      const;

    vec<Lit> refineAssumptions;     // assumption vector used for refinement
    void     refineFinalConflict(); // minimize final conflict clause

    /** to handle termination from the outside by a callback function */
    void* terminationCallbackState;                            // state that should be passed to the calling method
    int (*terminationCallbackMethod)(void* terminationState);  // pointer to the callback method


    void* learnCallbackState;
    int learnCallbackLimit;
    void (*learnCallback)(void * state, int * clause);
    int *learnCallbackBuffer;

    /** send a clause via the learn call back
     * @param clauseToShare clause to be shared
     */
    void IPASIR_shareClause(const vec<Lit>& clauseToShare)
    {
        if (learnCallback != 0 && clauseToShare.size() <= learnCallbackLimit) {
            for (int i = 0; i < clauseToShare.size(); i++) {
                Lit lit = clauseToShare[i];
                learnCallbackBuffer[i] = sign(lit) ? -(var(lit) + 1) : (var(lit) + 1);
            }
            learnCallbackBuffer[clauseToShare.size()] = 0;
            learnCallback(learnCallbackState, learnCallbackBuffer);
        }
    }

    /** send a clause via the learn call back
     * @param clauseToShare clause to be shared
     */
    void IPASIR_shareUnit(const Lit clauseToShare)
    {
        if (learnCallback != 0 && 1 <= learnCallbackLimit) {
            learnCallbackBuffer[0] = sign(clauseToShare) ? -(var(clauseToShare) + 1) : (var(clauseToShare) + 1);
            learnCallbackBuffer[1] = 0;
            learnCallback(learnCallbackState, learnCallbackBuffer);
        }
    }

  public:
    /** set a callback to a function that should be frequently tested by the solver to be noticed that the current search should be interrupted
     * Note: the state has to be used as argument when calling the callback
     * @param terminationState pointer to an external state object that is used in the termination callback
     * @param terminationCallbackMethod pointer to an external callback method that indicates termination (return value is != 0 to terminate)
     */
    void setTerminationCallback(void* terminationState, int (*terminationCallback)(void*));

    /** set a call back function in the solver to call a function with each learned clause (less than a certain size)
     * @param state pointer to an external state object that is used in the termination callback
     * @param max_length max length of clauses to be shared
     * @param learn function that will process the shared learned clause
     */
    void setLearnCallback(void * state, int maxLength, void (*learn)(void * state, int * clause));

    /// use the set preprocessor (if present) to simplify the current formula
    lbool preprocess();
    /** print full solver state (trail,clauses,watch lists, acticities)*/
    void printFullSolverState();
  protected:
    lbool inprocess(lbool status); // inprocessing code
    lbool initSolve(int solves);   // set up the next call to solve

    void printHeader();
    void printSearchHeader();

    // for search procedure
    void printConflictTrail(Riss::CRef confl);
    void printSearchProgress();
    void updateDecayAndVMTF();

    /** takes care of the vector of entailed unit clauses, DRUP, ...
     * @return l_False, if adding the learned unit clause(s) results in UNSAT of the formula
     */
    lbool handleMultipleUnits(vec< Lit >& learnt_clause);

    /** handle learned clause, perform RER,ECL, extra analysis, DRUP, ...
     * @return l_False, if adding the learned unit clause(s) results in UNSAT of the formula
     */
    lbool handleLearntClause(Riss::vec< Riss::Lit >& learnt_clause, bool backtrackedBeyond, unsigned int nblevels, unsigned int& dependencyLevel);

    /** check whether a restart should be performed (return true, if restart)
     * Furthermore takes care of the restart heuristic use, switches heuristics if necessary
     * @param nof_conflicts limit can be increased by the method, if an agility reject has been applied
     */
    bool handleRestarts(int& nof_conflicts, const int conflictC);

    /** handle top level units for the unsatisfiability proof and clause sharing */
    void handleTopLevelUnits(const int& beforeTrail, int& proofTopLevels);

    /** update the heuristic that is responsible for triggering restarts and blocking them */
    void updateBlockRestartAndRemovalHeuristic(bool& blockNextRestart);

    /** perform conflict analysis based on the current conflict */
    lbool conflictAnalysis(const CRef confl, vec<Lit>& learnt_clause);

    /** check for commands from the outside, as well as for shared clauses
     + @return l_True, if everything is fine, l_False is we found UNSAT, l_Undef if the search should be interupted
     */
    lbool receiveInformation();

    /** perform search decision, including handling assumptions recoding model for enumeration, performing look ahead
     @return decision literal, lit_Error -> return with the given returnValue, lit_Undef -> continue with the next search loop iteration
     */
    Lit performSearchDecision(lbool& returnValue, vec<Lit>& tmp_Lits);

    /** remove learned clauses during search */
    void clauseRemoval();

    #ifdef DRATPROOF
    // DRUP proof
    bool outputsProof() const ;
    vec<Lit> exportedClause; // temporary storage to write literals to proof
    template <class T>
    void addToProof(const T& clause, bool deleteFromProof = false, Lit remLit = lit_Undef);     // write the given clause to the output, if the output is enabled
    void addUnitToProof(Lit l, bool deleteFromProof = false);   // write a single unit clause to the proof
    void addCommentToProof(const char* text, bool deleteFromProof = false); // write the text as comment into the proof!
    template <class T>
    bool checkClauseDRAT(const T& clause);
    template <class T>
    bool proofHasClause(const T& clause);
  public:
    lbool checkProof(); // if online checker is used, return whether the current proof is valid
  protected:
    #else // have empty dummy functions
    bool outputsProof() const { return false; }
    template <class T>
    void addToProof(const T& clause, bool deleteFromProof = false, Lit remLit = lit_Undef) const {};
    void addUnitToProof(Lit l, bool deleteFromProof = false) const {};
    void addCommentToProof(const char* text, bool deleteFromProof = false) const {};
    template <class T>
    bool checkClauseDRAT(const T& clause) { return true; }
    template <class T>
    bool proofHasClause(const T& clause) { return true; }
  public:
    lbool checkProof() const { return l_Undef; } // if online checker is used, return whether the current proof is valid
  protected:
    #endif

    /// check whether the given new clause already exists in the set of clauses given before
    bool clauseAlreadyExists(const vec<CRef>& clauses, vec<Lit>& clause);

    /*
     * restricted extended resolution (Audemard ea 2010)
     */

    enum rerReturnType {    // return type for the rer-implementation
        rerUsualProcedure = 0,    // do nothing special, since rer failed -- or attach the current clause because its unit on the current level
        rerMemorizeClause = 1,    // add the current learned clause to the data structure rerFuseClauses
        rerDontAttachAssertingLit = 2,    // do not enqueue the asserting literal of the current clause
        rerAttemptFailed = 3, // some extra method failed (e.g. find RER-ITE)
    };

    /// initialize the data structures for RER with the given clause
    void restrictedExtendedResolutionInitialize(const vec< Lit >& currentLearnedClause);

    /// @return true, if a clause should be added to rerFuseClauses
    rerReturnType restrictedExtendedResolution(Riss::vec< Riss::Lit >& currentLearnedClause, unsigned int& lbd, unsigned int& dependencyLevel);
    /// reset current state of restricted Extended Resolution
    void resetRestrictedExtendedResolution();
    /// check whether the new learned clause produces an ITE pattern with the previously learned clause (assumption, previousClause is sorted, currentClause is sorted starting from the 3rd literal)
    rerReturnType restrictedERITE(const Lit& previousFirst, const vec<Lit>& previousPartialClause, vec<Lit>& currentClause);
    /// initialize the rewrite info with the gates of the formula
    void rerInitRewriteInfo();
    /// replace the disjunction p \lor q with x
    void disjunctionReplace(Lit p, Lit q, const Lit& x, const bool& inLearned, const bool& inBinary);

    /// structure to store for each literal the literal for rewriting new learned clauses after an RER extension
    struct LitPair {
        Lit otherMatch, replaceWith;
        LitPair(const Lit& l1, const Lit& l2) : otherMatch(l1), replaceWith(l2) {};
        LitPair() : otherMatch(lit_Undef), replaceWith(lit_Undef) {}
        void reset() { otherMatch = lit_Undef; replaceWith = lit_Undef; }
    };
    vec< LitPair > erRewriteInfo; /// vector that stores the information to rewrite new learned clauses

    /** fill the current variable assignment into the given vector */
    void fillLAmodel(vec<LONG_INT>& pattern, const int steps, vec<Var>& relevantVariables, const bool moveOnly = false);  // fills current model into variable vector

    /** perform la hack, return false -> unsat instance!
     * @return false, instance is unsatisfable
     */
    bool laHack(Riss::vec< Riss::Lit >& toEnqueue);
    /** concurrent clause strengthening, but interleaved instead of concurrent ...
    *  @return false, if the formula is proven to be unsatisfiable
    */
    bool interleavedClauseStrengthening();

    // Static helpers:
    //

    // Returns a random float 0 <= x < 1. Seed must never be 0.
    static inline double drand(double& seed)
    {
        seed *= 1389796;
        int q = (int)(seed / 2147483647);
        seed -= (double)q * 2147483647;
        return seed / 2147483647;
    }

    // Returns a random integer 0 <= x < size. Seed must never be 0.
  public: static inline int irand(double& seed, int size)
    {
        return (int)(drand(seed) * size);
    }

    /// build reduct wrt current unit clauses
    void buildReduct();

  protected:

    OnlineProofChecker* onlineDratChecker;

    uint64_t curr_restarts; // number of restarts for current call to solve_ method

    // UIP hack
    int l1conflicts; // number of conflicts at level 1
    int multiLearnt; // number of multiple learnt units at level 1
    int learntUnit;  // learnt a unit clause

    // restart interval
    unsigned conflictsSinceLastRestart; // number of conflicts since last restart
    unsigned currentRestartIntervalBound;      // max. nr. of conflicts until the next restart is triggered
    unsigned intervalRestart;  // number of restarts triggered by the interval

    // la hack
    // stats
    int laAssignments;
    int tabus;
    int las;
    int failedLAs;
    int maxBound;
    double laTime;
    int maxLaNumber;       // maximum number of LAs allowed
    int topLevelsSinceLastLa; // number of learned top level units since last LA
    int laEEvars, laEElits;   // number of equivalent literals
    std::vector< std::vector< Lit > > localLookAheadProofClauses;
    std::vector<Lit> localLookAheadTmpClause;

    // real data
    Lit hstry[6];
    vec<VarFlags> backupSolverState;  // vector to hold the solver state
    vec<LONG_INT> variablesPattern;   // vector for variable patterns
    vec<Var> relevantLAvariables;     // vector that stores the variables that are relevant for local LA
    int untilLa;      // count until  next LA is performed
    int laBound;      // current bound for l5-LA
    bool laStart;     // when reached the la level, perform la

    bool startedSolving;  // inidicate whether solving started already

    double useVSIDS;  // parameter for interpolating between VSIDS and VMTF

    int simplifyIterations; // number of visiting level 0 until simplification is to be performed
    int learnedDecisionClauses;

    /** all info neede to perform lazy on the fly self subsumption */
    class OTFSS
    {
      public:
        int otfsss, otfsssL1, otfssClss, otfssUnits, otfssBinaries, revealedClause, removedSat; // otfss stats

        /** store for each clause which literal can be removed */
        struct OtfssInfo {
            Riss::CRef cr;
            Lit shrinkLit;
            #ifdef PCASSO
            int dependencyLevel;
            #endif
        };

        vec<OtfssInfo> info;        // clauses that have to be processed
        vec<Lit> tmpPropagateLits;  // literals that should be propagated after otfss
        OTFSS() : otfsss(0), otfsssL1(0), otfssClss(0), otfssUnits(0), otfssBinaries(0), revealedClause(0), removedSat(0) {}

        /** to be used to keep a state clean, e.g. after preprocessing/inprocessing */
        void clearQueues() { info.clear(); tmpPropagateLits.clear(); }
    } otfss;

    /** run over the stored clauses and remove the indicated literal
     *  remove from watch list if necessary
     *  @return true, if a conflict was found (hence, the formula is unsatisfiable)
     */
    bool processOtfss(Riss::Solver::OTFSS& data);

    /** remove all clauses from temporary storage due to some global (untracable ) changes on the formula */
    void clearOtfss(Riss::Solver::OTFSS& data);


    bool doAddVariablesViaER; // indicator for allowing ER or not

    // stats for learning clauses
    double totalLearnedClauses, sumLearnedClauseSize, sumLearnedClauseLBD, maxLearnedClauseSize;
    int extendedLearnedClauses, extendedLearnedClausesCandidates, maxECLclause;
    int rerExtractedGates;
    int rerITEtries, rerITEsuccesses, rerITErejectS, rerITErejectT, rerITErejectF; // how often tried RER-ITE, and how often succeeded
    uint64_t maxResDepth;
    Clock rerITEcputime; // timer for RER-ITE

    int erRewriteRemovedLits, erRewriteClauses; // stats for ER rewriting

    vec<Lit> rerCommonLits, rerIteLits; // literals that are common in the clauses in the window
    int64_t rerCommonLitsSum; // sum of the current common literals - to Bloom-Filter common lits
    vec<Lit> rerLits; // literals that are replaced by the new variable
    vec<CRef> rerFuseClauses; // clauses that will be replaced by the new clause -
    int rerLearnedClause, rerLearnedSizeCandidates, rerSizeReject, rerPatternReject, rerPatternBloomReject, maxRERclause; // stat counters
    double rerOverheadTrailLits, totalRERlits; // stats

    MarkArray rerRewriteArray;          // markArray to rewrite learned clauses

    // interleaved clause strengthening (ics)
    int lastICSconflicts;     // number of conflicts for last ICS
    int icsCalls, icsCandidates, icsDroppedCandidates, icsShrinks, icsShrinkedLits; // stats

    // modified activity bumping
    vec<Var> varsToBump; // memorize the variables that need to be bumped in that order
    vec<CRef> clssToBump; // memorize the clauses that need to be bumped in that order, also used during propagate


    int rs_partialRestarts, rs_savedDecisions, rs_savedPropagations, rs_recursiveRefinements; // stats for partial restarts
    /** based no the current values of the solver attributes, return a decision level to jump to as restart
     * @return the decision level to jump to, or -1, if we have actually solved the problem already
     */
    int getRestartLevel();

    // for improved backbone finding
    Coprocessor::BIG* big;
    Clock bigBackboneTime;
    unsigned lastReshuffleRestart;
    unsigned L2units, L3units, L4units;
    /** if the new learned clause is binary, C = (a \lor b),
     *  then it is checked whether a literal l is implied by both a and b in the formula.
     *  Should be called after eventually enqueuing the asserting literal of the current learned clause
     *  @return true, if a contrdiction has been found, so that the result is UNSAT
     */
    bool analyzeNewLearnedClause(const CRef& newLearnedClause);

    // helper data structures
    std::vector< int > analyzePosition; // for full probing approximation
    std::vector< int > analyzeLimits; // all combination limits for full probing

    /// for generating bi-asserting clauses instead of first UIP clauses
    bool isBiAsserting;       // indicate whether the current learned clause is bi-asserting or not
    bool allowBiAsserting;    // conflict analysis is allowed to produce bi-asserting clauses
    uint32_t lastBiAsserting; // store number of conflicts when the last bi-asserting clause has been learnd
    uint64_t biAssertingPostCount, biAssertingPreCount;   // count number of biasserting clauses (after minimization, before minimization)

    // UHLE during search with learnt clauses:
    uint32_t searchUHLEs, searchUHLElits;

    /** reduce the literals inside the clause with the help of the information of the binary implication graph (UHLE only)
     * @param lbd current lbd value of the given clause
     * @return true, if the clause has been shrinked, false otherwise (then, the LBD also stays the same)
     */
    bool searchUHLE(vec<Lit>& learned_clause, unsigned int& lbd, unsigned& dependencyLevel);

    /// sort according to position of literal in trail
    struct TrailPosition_Gt {
        vec<VarData>& varData;  // data to use for sorting
        bool operator()(const Lit& x, const Lit& y) const
        {
            return varData[ var(x) ].position > varData[ var(y) ].position; // compare data of x and y instead of elements themselves
        }
        TrailPosition_Gt(vec<VarData>& _varData) : varData(_varData) {}
    };

    /** reduce the literals inside the clause by performing vivification in the opposite order the literals have been added to the trail
     * @param learned_clause pointer to the clause that should be minimized (can be vec<Lit> or Clause)
     * @param lbd current lbd value of the given clause
     * @param forced apply minimization without looking at measure limits
     * @return true, if the clause has been shrinked, false otherwise (then, the LBD also stays the same)
     */
    template <typename T>
    bool reverseLearntClause(T& learned_clause, unsigned int& lbd, unsigned int& dependencyLevel, bool force = false);

    /** setup reverse minimizatoin, if not done already */
    void initReverseMinimitaion();

    /** check whether an assumption literal that is left is already falsified
     * @param p literal that is chose based on the assumption list
     * @param mode (0=off,1=last,2=random,3=middle,4=all)
     * @return p, if no falsified literal is found, or an assumption literal that is now falsified
     */
    Lit prefetchAssumption(const Lit p, int mode);

    /** reduce the learned clause by replacing pairs of literals with their previously created extended resolution literal
     * @param lbd current lbd value of the given clause
     * @return true, if the clause has been shrinked, false otherwise (then, the LBD also stays the same)
     */
    bool erRewrite(Riss::vec< Riss::Lit >& learned_clause, unsigned int& lbd, unsigned int& dependencyLevel);
// contrasat hack

    bool      pq_order;           // If true, use a priority queue to decide the order in which literals are implied
    // and what antecedent is used.  The priority attempts to choose an antecedent
    // that permits further backtracking in case of a contradiction on this level.               (default false)

    struct ImplData {
        CRef reason;
        Lit impliedLit; // if not lit_Undef, use this literal as the implied literal
        int level;
        int dlev_pos;
        ImplData(CRef cr, Lit implied, int l, int p) : reason(cr), impliedLit(implied), level(l), dlev_pos(p) { }
        ImplData(CRef cr, int l, int p) : reason(cr), impliedLit(lit_Undef), level(l), dlev_pos(p) { }
        ImplData()                      : reason(0),  impliedLit(lit_Undef), level(0), dlev_pos(0) { }
    };

    struct HeapImpl {
        vec<ImplData> heap; // Heap of ImplData

        // Index "traversal" functions
        static inline int left(int i) { return i * 2 + 1; }
        static inline int right(int i) { return (i + 1) * 2; }
        static inline int parent(int i) { return (i - 1) >> 1; }

        // less than with respect to lexicographical order
        bool lt(ImplData& x, ImplData& y) const
        {
            return (x.level < y.level) ? true :
                   (y.level < x.level) ? false :
                   (x.dlev_pos < y.dlev_pos);
        }

        void percolateUp(int i)
        {
            ImplData x = heap[i];
            int p  = parent(i);

            while (i != 0 && lt(x, heap[p])) {
                heap[i] = heap[p];
                i       = p;
                p       = parent(p);
            }
            heap[i] = x;
        }

        void percolateDown(int i)
        {
            ImplData x = heap[i];
            while (left(i) < heap.size()) {
                int child =
                    (right(i) < heap.size() && lt(heap[right(i)], heap[left(i)])) ?
                    right(i) : left(i);
                if (!lt(heap[child], x)) { break; }
                heap[i]          = heap[child];
                i                = child;
            }
            heap   [i] = x;
        }

      public:
        HeapImpl()              : heap()    { heap.clear(); }
        HeapImpl(const int sz0) : heap(sz0) { heap.clear(); }

        int  size()  const                   { return heap.size(); }
        bool empty() const                   { return heap.size() == 0; }
        ImplData operator[](int index) const { assert(index < heap.size()); return heap[index]; }

        void insert(ImplData elem)
        {
            heap.push(elem);
            percolateUp(heap.size() - 1);
        }

        ImplData  removeMin()
        {
            ImplData x = heap[0];
            heap[0]    = heap.last();
            heap.pop();
            if (heap.size() > 1) { percolateDown(0); }
            return x;
        }

        void clear(bool dealloc = false) { heap.clear(dealloc); }
    };

    HeapImpl            impl_cl_heap;     // A priority queue of implication clauses wrapped as ImplData, ordered by level.

    // cir minisat hack
    void     counterImplicationRestart(); // to jump to as restart.

    /**
     * After how many steps the solver should perform failed literals and detection of necessary assignments. (default 32)
     * If set to '0', no inprocessing is performed.
     */
    int probing_step;     // Counter for probing. If zero, inprocessing (probing) will be performed.
    int probing_step_width;

    /**
     * Limit how many varialbes with highest activity should be probed during the inprocessing step.
     */
    int probing_limit;

    // MinitSAT parameters

    /**
     * If true, variable initialization is based on Jeroslow-Wang heuristic and the variable
     * activity is set to the value of the variable. Therefore, the last variables will be decided
     * first. This is helpful because the last variables are often auxiliary variables.
     *
     * This hack is useful for short timeouts.
     */
    // bool pol_act_init;

    // cir minisat Parameters
    int       cir_bump_ratio;     // bump ratio for VSIDS score after restart (if >0 cir is activated)
    int       cir_count;          // Counts calls of cir-function


    // MiPiSAT methods

    vec<Lit>      probing_uncheckedLits;            /// literals to be used in probing routine
    vec<VarFlags> probing_oldAssigns;  /// literals to be used in probing routine
    /**
     * Apply inprocessing on the variables with highest activity. The limit of
     * how many variables are probed is determined by the parameter "probing_limit".
     *
     * @return false if inconsistency was found. That means the formula is unsatisfiable
     */
    bool probingHighestActivity();

    /**
     * Call probingLiteral() for both - positive and negative - literal
     * of the passed variable.
     *
     * If a conflict is found, the formula is unsatisfiable.
     * Otherwise it collects common implied variables and perform
     * unit propagation.
     *
     * @param v Variable that will be checked as positive and negative literal
     * @return false if inconsistency was found, meaning the formula is UNSATs
     */
    bool probing(Var v);

    /**
     * Probing a single literal.
     *
     * Collects all literals that are inferred by unit propagation given a
     * single literal. It is called by the method Solver::probing() two times
     * for a literal "x" and its negation "not x".
     *
     * @param  v Literal that is used as unit to inferre other literals
     * @return 0 - no conflict found
     *         1 - conflict for literal v
     *         2 - contradiction (conflict for "v" and "not v") => UNSAT
     */
    int probingLiteral(Lit v);

    // 999 MS hack
    bool   activityBasedRemoval;     // use minisat or glucose style of clause removal and activities
    int    lbd_core_threshold;        // threadhold to move clause from learnt to formula (if LBD is low enough)
    double learnts_reduce_fraction;   // fraction of how many learned clauses should be removed


/// for coprocessor
  protected:  Coprocessor::Preprocessor* coprocessor;
  public:

    void setPreprocessor(Coprocessor::Preprocessor* cp);

    void setPreprocessor(Coprocessor::CP3Config* _config);

    /** replace the current instance of coprocessor with the new one
        @return pointer to the old coprocessor
     */
    Coprocessor::Preprocessor* swapPreprocessor(Coprocessor::Preprocessor* newPreprocessor);

    /** return the pointer to the currently used preprocessor
        @return pointer to coprocessor
     */
    Coprocessor::Preprocessor* getPreprocessor() const ;

    bool useCoprocessorPP;
    bool useCoprocessorIP;

    /** extend a given model (in case a preprocessor is present ) */
    void extendModel(Riss::vec<Riss::lbool>& model);

    // if (coprocessor != 0 && (useCoprocessorPP || useCoprocessorIP)) { coprocessor->extendModel(model); }

    /** temporarly enable or disable extended resolution, to ensure that the number of variables remains the same */
    void setExtendedResolution(bool enabled) { doAddVariablesViaER = enabled; }

    /** query whether extended resolution is enabled or not */
    bool getExtendedResolution() const { return doAddVariablesViaER; }

/// for qprocessor
  public:
//      void writeClauses( std::ostream& stream ) {
//
//      }


// [BEGIN] modifications for parallel assumption based solver
  public:
    /** setup the communication object
     * @param comm pointer to the communication object that should be used by this thread
     */
    void setCommunication(Communicator* comm);

    /** reset the state of an possibly interrupted solver
     * clear interrupt, jump to level 0
     */
    void resetLastSolve();

  private:

    int sharingTimePoint; // when to share a learned clause (0=when learned, 1=when first used for propagation, 2=when first used during conflict analysis)

    Communicator* communication; /// communication with the outside, and control of this solver

    bool isCommunicating() const { return communication != nullptr; }

    /** return dependency level we are currently working on */
    unsigned currentDependencyLevel() const ;

    /** goto sleep, wait until master did updates, wakeup, process the updates
     * @param toSend if not 0, send the (learned) clause, if 0, receive shared clauses
     * @param toSendSize number of literals in data to share
     * @param dependencyLevel (in Pcasso, dependencyLevel of information)
     * @param multiUnits send a set of unit clauses instead of a single clause
     * @param equivalences send an SCC of equivalent literals
     * note: can also interrupt search and incorporate new assumptions etc.
     * @return -1 = abort, 0=everythings nice, 1=conflict/false result
     */
    template<typename T> // can be either clause or vector
    #ifdef PCASSO
    int updateSleep(T* toSend, int toSendSize, int dependencyLevel, bool multiUnits = false, bool equivalences = false);
    #else
    int updateSleep(T* toSend, int toSendSize, bool multiUnits = false, bool equivalences = false);
    #endif

    /** add a learned clause to the solver
     * @param bump increases the activity of the current clause to the last seen value
     * note: behaves like the addClause structure
     */
    bool addLearnedClause(Riss::vec< Riss::Lit >& ps, bool bump);

    /** update the send limits based on whether a current clause could have been send or not
     * @param failed sending current clause failed because of limits
     * @param sizeOnly update only size information
     */
    void updateDynamicLimits(bool failed, bool sizeOnly = false);

    /** inits the protection environment for variables
     */
    void initVariableProtection();

    /** set send limits once!
     */
    void initLimits();

    /*
     * stats and limits for communication
     */
  public:
    /** necessary data structures for communication */
    struct CommunicationClient {
        vec<Lit> receiveClause;             /// temporary placeholder for receiving clause
        vec<Lit> receivedUnits;             /// temporary placeholder for receiving units
        vec<int> unitDependencies;          /// store the dependency of each unit
        vec<Lit> receivedEquivalences;      /// temporary placeholder for receiving equivalences, classes separated by lit_Undef
        vec<int> eeDependencies;            /// store the dependency of each equivalence class
        std::vector< CRef > receiveClauses; /// temporary placeholder indexes of the clauses that have been received by communication
        int currentTries;                          /// current number of waits
        int receiveEvery;                          /// do receive every n tries
        float currentSendSizeLimit;                /// dynamic limit to control send size
        float currentSendLbdLimit;                 /// dynamic limit to control send lbd

        bool receiveEE;                            /// indicate that EE is received by this client (turn off, if no inprocessing)
        bool refineReceived;                       /// apply vivification to received clauses
        bool resendRefined;                        /// send refined received clauses again
        bool doReceive;                            /// receive clauses

        int succesfullySend;                       /// number of clauses that have been sucessfully transmitted
        int succesfullyReceived;                   /// number of clauses that have been sucessfully transmitted

        float sendSize;                            /// Minimum Lbd of clauses to send  (also start value)
        float sendLbd;                             /// Minimum size of clauses to send (also start value)
        float sendMaxSize;                         /// Maximum size of clauses to send
        float sendMaxLbd;                          /// Maximum Lbd of clauses to send
        float sizeChange;                          /// How fast should size send limit be adopted?
        float lbdChange;                           /// How fast should lbd send limit be adopted?
        float sendRatio;                           /// How big should the ratio of send clauses be?

        bool checkLiterals;                        /// control allowing sending and receiving information based on literal instead of variables
        bool useDynamicLimits;                     /// update sharing limits dynamically
        bool sendAll;                              /// ignore sharing limits
        bool receiveAll;                           /// ignore limits for reception
        bool keepLonger;                           /// keep added clauses for at least one removal

        double lbdFactor;                          /// how to construct the LBD for a received clause (0 = set LBD of clause to 0, positive: relative to size of clause [0-1], negative: relative to average lbd/size ratio)

        CommunicationClient() : currentTries(0), receiveEvery(0), currentSendSizeLimit(0), receiveEE(false),
            refineReceived(false), resendRefined(false), doReceive(true), succesfullySend(0), succesfullyReceived(0),
            sendSize(0), sendLbd(0), sendMaxSize(0), sendMaxLbd(0), sizeChange(0), lbdChange(0), sendRatio(0),
            checkLiterals(true), useDynamicLimits(false), sendAll(false), receiveAll(false), keepLonger(false), lbdFactor(0) {}
    } communicationClient;

    class VariableInformation
    {
        vec<VarFlags>& varInfo;
        bool receiveUseLit;  // allow working with variables where the number of models increased
      public:
        VariableInformation(vec<VarFlags>& _varInfo, bool checkLits)
            : varInfo(_varInfo), receiveUseLit(checkLits) {}

        /** check based on the variable flags whether a certain literal is allowed to be received */
        bool canBeReceived(const Lit& l) const
        {
            const Var v = var(l);
            if (receiveUseLit) {
                if (sign(l)) { return varInfo[v].modifiedNegativeModels; }
                else { return varInfo[v].modifiedPositiveModels; }
            } else { return varInfo[v].modifiedNegativeModels || varInfo[v].modifiedPositiveModels; }
        }

        /** set dependency of a variable */
        void setDependency(const Var& v, const int& dep)
        {
            #ifdef PCASSO
            varInfo[v].varPT = dep > varInfo[v].varPT ? dep : varInfo[v].varPT;
            #endif
        }
    };

// [END] modifications for parallel assumption based solver

    /** store two pairs of literals */
    struct LitPairPair {
        LitPair p, q;
        void reset() { p.reset(); q.reset(); }
    };

    vec<LitPairPair> decisionLiteralPairs; /// data to be used to select search decision literals

    void recomputeLPDdata();   /// clears current info, and recomputes the info again based on the current clauses


    bool useNaiveBacktracking; /// use DPLL instead of CDCL for conflict analysis. breaks invariants of the CDCL implementation

    /** turn of clause learning and restarts*/
    void enableDPLL();

    /** perform naive backtracking (dpll like)
     @return l_Undef, if everything worked out nicely, l_False, if backtracking is not possible because the search space is exhaused
     */
    lbool dpllBacktracking();

// [BEGIN] modifications for model enumerating solver
    class EnumerationClient
    {
      public:
        enum MinimizeType {
            NONE = 0,
            ONLYFROMFULL = 1,
            ALSOFROMBLOCKED = 2
        };

        // how to enumerate under projection
        enum ProjectionType {
            NAIVE = 0,         // simply block the projection variables of the current solution
            BACKTRACKING = 1,  // use the sophisticated backtracking based algorithm from Gebser et al paper
        };

      private:

        int clientID;

        EnumerateMaster* master;

        Solver* solver; // handle to the solver class

        /** generate decision clause, and calculate the two highest levels in that clause */
        void createDecisionClause(vec< Lit >& clause, int& maxLevel, int& max2Level);

        /** colllect all assigned projection variables and add them as complement literal to the clause*/
        void createBlockingProjectionClause(vec< Lit >& clause, int& maxLevel, int& max2Level);

        /** try to add the given clause with as few changes as possible
         @param clause literals of the clause, can be modified during the method
         @param maxLevel maximum decision level in the clause (>= 0, otherwise, recomputed)
         @param max2Level second highest decision level in the clause (>= 0, otherwise, recomputed)
         @return reference to the added clause, CRef_Undef if the clause became unit, CRef_Error, if unsatisfiability was reached during adding the clause
         */
        CRef integrateClause(vec<Lit>& clause, int maxLevel, int max2Level);

        /** convert model into truth value representation
         @param nVars present variables in solver
         @param trail list of literals that have been satisfied (taken as input)
         @param model truth value representation of trail
         @return hash that represents the model
         */
        uint64_t convertModel(uint32_t nVars, vec< Lit >& trail, vec< lbool >& model);

        vec<Lit> blockingClause;  /// storage for the blocking clause
        vec<Lit> minimizedClause; /// storage for the minimize blocking clause

        uint64_t blockedModels; /// number of models that have been received from the master already
        uint64_t lastReceiveDecisions;  /// number of decisions when receiving the last blocking clause

        ProjectionType projectionType; /// how to perform enumeration under projection

        // for backtracking projection enumeration
        int projectionBacktrackingLevel;   // bl in the paper
        vec<Lit> projectionDecisionStack;  // memorize the decision variables used for projection
        vec<CRef> projectionReasonClauses; // memorize the reason clauses that are used for projection

      public:

        enum EnumerateState {
            stop = 0,
            goOn = 1,
            oneModel = 2,
        };

        MinimizeType mtype;            // minimization type before submitting blocking clause = master->minimizeBlocked();
        MinimizeType minimizeReceived; // apply reverse minimization after receiving blocking clause (0=none, 1=only from full, 2=ALSOFROMBLOCKED
        uint64_t foundModels;   // number of models found by this client
        uint64_t duplicateModels;   // number of models found by this client
        uint64_t receiveEveryDecisions; // try to receive new blocking clauses for models every X decisions

        EnumerationClient(Solver* _solver);

        ~EnumerationClient();

        /** tell client about master */
        void setMaster(EnumerateMaster* m) { assert(master == nullptr && "can set only one master"); master = m; }

        /** check whether all models have been encountered already
         * @param searchstatus status of the current search (l_True = currently found a model, l_Undef = currently found no model (intterup/restart) )
         */
        bool enoughModels(lbool searchstatus) const ;

        /** return the number of models that have been found by this thread */
        uint64_t getModels() const ;

        /** return the number of models that have been found too late (already submitted by another thread) */
        uint64_t getDupModels() const ;

        /** process the current model of the solver where the client is embedded
         @return goOn, if the search for a model should be continued, stop, if the search should be interrupted, and oneModel, if no enumeration is used
        */
        EnumerateState processCurrentModel(Lit& nextDecision);

        /** receive blocking clauses from enumeration master and add them to the formula of the current solver
        * Note: if the decision level has been changed, then clauses have been added lower than the decision level the solver has been working on
        @return false, if nothing has to be changed during search, true, if propoagation should be called next (instead of doing a decision)
        */
        bool receiveModelBlockingClauses();

        /** assign a id from the master to the client */
        void assignClientID();
        /** activate backtracking based enumeration */
        void enableBTbasedEnumeration();

        /** indicate whether enumeration on projection interferes with usual search */
        bool isBTenumerating() const;

    } enumerationClient;

    void setEnumnerationMaster(EnumerateMaster* master);
};


//=================================================================================================
// Implementation of inline methods:

inline Solver::ReasonStruct& Solver::reason(Var x)       { return vardata[x].reason; }
inline const Solver::ReasonStruct& Solver::reason(Var x) const { return vardata[x].reason; }

inline int  Solver::level(Var x) const { return vardata[x].level; }

inline void Solver::insertVarOrder(Var x)
{
    if (!order_heap.inHeap(x) && varFlags[x].decision) { order_heap.insert(x); }
}

inline void Solver::varSetActivity(Var v, double value) {activity[v] = value;}
inline double Solver::varGetActivity(Var v) const { return activity[v]; }
inline void Solver::varDecayActivity() { var_inc *= (1 / searchconfiguration.var_decay); }
inline void Solver::varBumpActivity(Var v, double inverseRatio) { varBumpActivityD(v, var_inc / (double) inverseRatio); }
inline void Solver::varBumpActivityD(Var v, double inc)
{
    DOUT(if (config.opt_printDecisions > 1) std::cerr << "c bump var activity for " << v + 1 << " with " << activity[v] << " by " << inc << std::endl;) ;
    activity[v] = (useVSIDS * activity[v]) + inc;   // interpolate between VSIDS and VMTF here!
    // NOTE this code is also used in extended clause learning, and restricted extended resolution
    if (activity[v] > 1e100) {
        DOUT(if (config.opt_printDecisions > 0) std::cerr << "c rescale decision heap" << std::endl;) ;
        // Rescale:
        for (int i = 0; i < nVars(); i++) {
            activity[i] *= 1e-100;
        }
        var_inc *= 1e-100;
    }

    // Update order_heap with respect to new activity:
    if (order_heap.inHeap(v)) {
        order_heap.decrease(v);
    }
}

inline void Solver::claDecayActivity() { cla_inc *= (1 / searchconfiguration.clause_decay); }
inline void Solver::claBumpActivity(Clause& c, double inverseRatio)
{
    DOUT(if (config.opt_removal_debug > 1) std::cerr << "c bump clause activity for " << c << " with " << c.activity() << " by " << inverseRatio << std::endl;) ;
    if ((c.activity() += cla_inc / inverseRatio) > 1e20) {
        // Rescale:
        for (int i = 0; i < learnts.size(); i++) {
            if (ca[learnts[i]].learnt()) { ca[learnts[i]].activity() *= 1e-20; }  // scale only if the clause is a learned clause
        }
        cla_inc *= 1e-20;
        DOUT(if (config.opt_removal_debug > 1) std::cerr << "c rescale clause activities" << std::endl;) ;
    }
}

inline void Solver::checkGarbage(void) { return checkGarbage(garbage_frac); }
inline void Solver::checkGarbage(double gf)
{
    if (ca.wasted() > ca.size() * gf) {
        garbageCollect();
    }
}

// NOTE: enqueue does not set the ok flag! (only public methods do)
inline bool     Solver::enqueue(Lit p, CRef from)      { return value(p) != l_Undef ? value(p) != l_False : (uncheckedEnqueue(p, from), true); }
inline bool     Solver::addClause(const vec<Lit>& ps)    { ps.copyTo(add_tmp); return addClause_(add_tmp); }
inline bool     Solver::addEmptyClause()                      { add_tmp.clear(); return addClause_(add_tmp); }
inline bool     Solver::addClause(Lit p)                 { add_tmp.clear(); add_tmp.push(p); return addClause_(add_tmp); }
inline bool     Solver::addClause(Lit p, Lit q)          { add_tmp.clear(); add_tmp.push(p); add_tmp.push(q); return addClause_(add_tmp); }
inline bool     Solver::addClause(Lit p, Lit q, Lit r)   { add_tmp.clear(); add_tmp.push(p); add_tmp.push(q); add_tmp.push(r); return addClause_(add_tmp); }
inline bool     Solver::locked(const Clause& c) const
{
    if (c.size() > 2) {
        return value(c[0]) == l_True
               && ! reason(var(c[0])).isBinaryClause()
               && reason(var(c[0])).getReasonC() != CRef_Undef
               && ca.lea(reason(var(c[0])).getReasonC()) == &c;
    }
    return
        (value(c[0]) == l_True
         && ! reason(var(c[0])).isBinaryClause()
         && reason(var(c[0])).getReasonC() != CRef_Undef
         && ca.lea(reason(var(c[0])).getReasonC()) == &c)
        ||
        (value(c[1]) == l_True
         && ! reason(var(c[1])).isBinaryClause()
         && reason(var(c[1])).getReasonC() != CRef_Undef
         && ca.lea(reason(var(c[1])).getReasonC()) == &c);
}
inline void     Solver::newDecisionLevel()                      { trail_lim.push(trail.size());}

inline int      Solver::decisionLevel()      const   { return trail_lim.size(); }
inline uint32_t Solver::abstractLevel(Var x) const   { return 1 << (level(x) & 31); }
inline lbool    Solver::value(Var x) const   { return varFlags[x].assigns; }
inline lbool    Solver::value(Lit p) const   { return varFlags[var(p)].assigns ^ sign(p); }
inline lbool    Solver::modelValue(Var x) const   { return model[x]; }
inline lbool    Solver::modelValue(Lit p) const   { return model[var(p)] ^ sign(p); }
inline int      Solver::nAssigns()      const   { return trail.size(); }
inline int      Solver::nClauses()      const   { return clauses.size(); }
inline int      Solver::nLearnts()      const   { return learnts.size(); }
inline int      Solver::nVars()      const   { return vardata.size(); }
inline int      Solver::nTotLits()      const   { return clauses_literals + learnts_literals; }
inline int      Solver::nFreeVars()      const   { return (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]); }
inline void     Solver::setPolarity(Var v, bool b) { varFlags[v].polarity = b; }
inline bool     Solver::getPolarity(Var v)         { return varFlags[v].polarity; }
inline void     Solver::setDecisionVar(Var v, bool b)
{
    if (b && !varFlags[v].decision) { dec_vars++; }
    else if (!b &&  varFlags[v].decision) { dec_vars--; }

    varFlags[v].decision = b;
    insertVarOrder(v);
}
inline void     Solver::setConfBudget(int64_t x) { conflict_budget    = conflicts    + x; }
inline void     Solver::setPropBudget(int64_t x) { propagation_budget = propagations + x; }
inline void     Solver::interrupt() { asynch_interrupt = true; }
inline void     Solver::clearInterrupt() { asynch_interrupt = false; }
inline void     Solver::budgetOff() { conflict_budget = propagation_budget = -1; }
inline bool     Solver::withinBudget() const
{
    return !asynch_interrupt &&
           (terminationCallbackMethod == 0 || 0 == terminationCallbackMethod(terminationCallbackState)) &&     // check callback to ask outside for termination, if the callback has been set
           (conflict_budget    < 0 || conflicts < (uint64_t)conflict_budget) &&
           (propagation_budget < 0 || propagations < (uint64_t)propagation_budget);
}

// FIXME: after the introduction of asynchronous interrruptions the solve-versions that return a
// pure bool do not give a safe interface. Either interrupts must be possible to turn off here, or
// all calls to solve must return an 'lbool'. I'm not yet sure which I prefer.
inline bool     Solver::solve()                    { budgetOff(); assumptions.clear(); return solve_(full) == l_True; }
inline bool     Solver::solve(Lit p)               { budgetOff(); assumptions.clear(); assumptions.push(p); return solve_(full) == l_True; }
inline bool     Solver::solve(Lit p, Lit q)        { budgetOff(); assumptions.clear(); assumptions.push(p); assumptions.push(q); return solve_(full) == l_True; }
inline bool     Solver::solve(Lit p, Lit q, Lit r) { budgetOff(); assumptions.clear(); assumptions.push(p); assumptions.push(q); assumptions.push(r); return solve_(full) == l_True; }
inline bool     Solver::solve(const vec<Lit>& assumps) { budgetOff(); assumps.copyTo(assumptions); return solve_(full) == l_True; }
inline lbool    Solver::solveLimited(const Riss::vec< Riss::Lit >& assumps, const Solver::SolveCallType preprocessCall) { assumps.copyTo(assumptions); return solve_(preprocessCall); }
inline void     Solver::setRandomSeed(double seed) { assert(seed != 0); random_seed = seed; }
inline bool     Solver::okay()      const   { return ok; }

inline void     Solver::toDimacs(const char* file) { vec<Lit> as; toDimacs(file, as); }
inline void     Solver::toDimacs(const char* file, Lit p) { vec<Lit> as; as.push(p); toDimacs(file, as); }
inline void     Solver::toDimacs(const char* file, Lit p, Lit q) { vec<Lit> as; as.push(p); as.push(q); toDimacs(file, as); }
inline void     Solver::toDimacs(const char* file, Lit p, Lit q, Lit r) { vec<Lit> as; as.push(p); as.push(q); as.push(r); toDimacs(file, as); }

inline void     Solver::setTerminationCallback(void* terminationState, int (*terminationCallback)(void*))
{
    terminationCallbackState  = terminationState;
    terminationCallbackMethod = terminationCallback;
}

inline void     Solver::setLearnCallback(void * state, int maxLength, void (*learn)(void * state, int * clause))
{
    learnCallbackState = state;
    learnCallbackLimit = maxLength;
    if (learnCallbackBuffer != 0) { delete [] learnCallbackBuffer; }
    learnCallbackBuffer = new int[maxLength + 2];
    learnCallback = learn;
}

inline
bool Solver::addUnitClauses(const vec< Lit >& other)
{
    assert(decisionLevel() == 0 && "init trail can only be done at level 0");
    for (int i = 0 ; i < other.size(); ++ i) {
        addUnitToProof(other[i]);   // add the unit clause to the proof
        if (value(other[i]) == l_Undef) {
            uncheckedEnqueue(other[i]);
        } else if (value(other[i]) == l_False) {
            ok = false; return true;
        }
    }
    return propagate() != CRef_Undef;
}



template<typename T>
inline bool Solver::reverseLearntClause(T& learned_clause, unsigned int& lbd, unsigned& dependencyLevel, bool force)
{
    #ifdef PCASSO
    // TODO implement dependencyLevel correctly!
    #endif
    if (!reverseMinimization.enabled || (!force && lbd > searchconfiguration.lbLBDReverseClause)) { return false; }

    if (learned_clause.size() == 0) { return false; }  // cannot shrink the empty clause

    // sort literal in the clause
    sort(learned_clause, TrailPosition_Gt(vardata));
    assert((force || communicationClient.refineReceived || level(var(learned_clause[0])) == decisionLevel()) && "first literal is conflict literal (or now assertion literal)");

    reverseMinimization.attempts ++;

    const bool localDebug = false; // for temporarly debugging this method

    DOUT(
    if (false) {
    for (int i = 0 ; i < nVars(); ++ i) {
            std::cerr << "c value var " << i + 1 << " sat: " << (reverseMinimization.value(i) == l_True)
                      << " unsat: " << (reverseMinimization.value(i) == l_False)
                      << " undef: " << (reverseMinimization.value(i) == l_Undef) << std::endl;
        }
    });

    // update minimization trail with top level units (if there have been new ones)
    const int levelZeroUnits = trail_lim.size() == 0 ? trail.size() : trail_lim[0];
    assert((trail_lim.size() > 0 || decisionLevel() == 0) && "if there is no trail lim, then we have not done any decision until now");
    int trailHead = reverseMinimization.trail.size(); // before reverse minimization propagate all other literals
    for (int i = reverseMinimization.trail.size() ; i < levelZeroUnits; ++i) {
        reverseMinimization.uncheckedEnqueue(trail[i]);
    }

    // perform vivification
    int keptLits = 0;
    if (localDebug) { std::cerr << "c rev minimize clause with " << learned_clause << " trail: " << reverseMinimization.trail << std::endl; }
    for (int i = 0; i < learned_clause.size(); ++ i) {
        // first propagate, then add literals. this way, the empty clause could be learned
        CRef    confl     = CRef_Undef;
        watches.cleanAll();
        while (trailHead < reverseMinimization.trail.size()) {
            const Lit p   = reverseMinimization.trail[trailHead++];     // 'p' is enqueued fact to propagate.
            vec<Watcher>&  ws  = watches[p];                // do not modify watch list!
            Watcher        *i, *end;
            // propagate longer clauses here!
            for (i = (Watcher*)ws, end = i + ws.size();  i != end; i++) {

                if (localDebug) { std::cerr << "c propagate " << p << " on clause " << ca[i->cref()] << std::endl; }

                if (i->isBinary()) {   // handle binary clauses as usual (no write access necessary!)
                    const Lit& imp = i->blocker();
                    if (reverseMinimization.value(imp) == l_False) {
                        confl = i->cref();              // store the conflict
                        trailHead = reverseMinimization.trail.size(); // to stop propagation (condition of the above while loop)
                        DOUT(if (localDebug) {
                        std::cerr << "reverse minimization hit a conflict during propagation" << std::endl;
                        std::cerr << "c qhead: " << qhead << " level 0: " << (trail_lim.size() == 0 ? trail.size() : trail_lim[0]) << " trail: " << trail << std::endl;
                            std::cerr << "c trailHead: " << trailHead << " rev-trail: " << reverseMinimization.trail << std::endl;
                        }
                            );
                        break;
                    } else if (reverseMinimization.value(imp) == l_Undef) {  // imply other literal
                        if (localDebug) { std::cerr << "c enqueue " << imp << " due to " << ca[ i->cref() ] << std::endl; }
                        reverseMinimization.uncheckedEnqueue(imp);
                    }
                    continue;
                }
                // Try to avoid inspecting the clause:
                const Lit blocker = i->blocker();
                if (reverseMinimization.value(blocker) == l_True) { // keep binary clauses, and clauses where the blocking literal is satisfied
                    continue;
                }

                // Make sure the false literal is data[1]:
                const CRef cr = i->cref();
                const Clause&  c = ca[cr];

                // Look for new watch:
                Lit impliedLit = lit_Undef;
                for (int k = 0; k < c.size(); k++) {
                    if (reverseMinimization.value(c[k]) == l_Undef) {
                        if (impliedLit == lit_Undef) {
                            impliedLit = c[k];  // if there is exactly one left
                        } else {
                            impliedLit = lit_Error;                   // there are multiple left
                            break;
                        }
                    } else if (reverseMinimization.value(c[k]) == l_True) {
                        impliedLit = lit_Error;
                        break;
                    }
                }

                if (impliedLit != lit_Error) {    // the clause is unit or conflict
                    if (impliedLit == lit_Undef) {  // conflict
                        confl = i->cref();
                        trailHead = reverseMinimization.trail.size(); // to stop propagation (condition of the above while loop)
                    } else {
                        if (localDebug) { std::cerr << "c enqueue " << impliedLit << " due to " << ca[ i->cref() ] << std::endl; }
                        reverseMinimization.uncheckedEnqueue(impliedLit);
                    }
                }
            }

        }

        // found a conflict during reverse propagation
        if (confl != CRef_Undef) {

            DOUT(if (localDebug) {
            std::cerr << "reverse minimization hit a conflict during propagation (" << i << ") with conflict " << ca[confl] << std::endl;
                std::cerr << "c qhead: " << qhead << " level 0: " << (trail_lim.size() == 0 ? trail.size() : trail_lim[0]) << " trail: " << trail << std::endl;
                std::cerr << "c trailHead: " << trailHead << " rev-trail: " << reverseMinimization.trail << std::endl;
            }
                );
            if (i + 1 < learned_clause.size()) { reverseMinimization.revMinConflicts ++; } // count cases when the technique was succesful
            break;
        }

        if (i == learned_clause.size()) { break; }

        const Lit l = learned_clause[i];
        assert((force || level(var(l)) != 0 || reverseMinimization.value(l) == l_False) && "there should not be any relevant literal of the top level in the clause");
// DOUT(   std::cerr << "c consider literal " << l << " sat: " << (reverseMinimization.value(l) == l_True) << " unsat: " << (reverseMinimization.value(l) == l_False)  << std::endl; );
        if (reverseMinimization.value(l) == l_Undef) {    // enqueue literal and perform propagation
            learned_clause[keptLits++ ] = l; // keep literal
        } else if (reverseMinimization.value(l) == l_True) {
// DOUT(   std::cerr << "c add implied lit " << l << " and abort" << std::endl; );
            learned_clause[keptLits++ ] = l; // keep literal, and terminate, as this clause is entailed by the formula already
            break;
        } else {
            assert(reverseMinimization.value(l) == l_False && "there only exists three values");
            // continue, as this clause is minimized
            reverseMinimization.revMindroppedLiterals ++;
            continue;
        }

        // propagate the current literal in next iteration of the loop
        if (localDebug) { std::cerr << "c enqueue next literal: " << ~l << " (pos: " << i << " / " << learned_clause.size() << ")" << std::endl; }
        reverseMinimization.uncheckedEnqueue(~l);


    } // end of looping over all literals of the clause

    // perform backtracking
    for (int i = levelZeroUnits ; i < reverseMinimization.trail.size(); ++ i) {
        reverseMinimization.assigns[ var(reverseMinimization.trail[i]) ] = l_Undef;
    }
    reverseMinimization.trail.shrink(reverseMinimization.trail.size() - levelZeroUnits);

    // remove all redundant literals
    if (keptLits < learned_clause.size()) {
        reverseMinimization.succesfulReverseMinimizations ++;
        reverseMinimization.revMincutOffLiterals += (learned_clause.size() - keptLits);
        assert((force || communicationClient.refineReceived || learned_clause.size() == 0 || level(var(learned_clause[0])) == decisionLevel()) && "first literal is conflict literal (or now assertion literal)");
        learned_clause.shrink(learned_clause.size() - keptLits);
        assert((force || communicationClient.refineReceived || learned_clause.size() == 0 || level(var(learned_clause[0])) == decisionLevel()) && "first literal is conflict literal (or now assertion literal)");
        return true;
    }
    return false; // clause was not changed, LBD should not be recomputed
}



/************************************************************
 * Compute LBD functions
 *************************************************************/
template<typename T>
inline int Solver::computeLBD(const T& lits, const int& litsSize)
{

    // Already discovered decision levels are stored in a mark array. We do
    // not want to allocate the mark array for each call of ths function.
    // Therefore a "gobal" mark array for the whole Solver instance will be
    // used.

    int distance = 0;

    // Generate a unique identifier (aka step) for this function call
    lbd_marker.nextStep();
    bool withLevelZero = false;
    const int minLevel = (config.opt_lbd_ignore_assumptions ? assumptions.size() : 0);
    for (int i = 0; i < litsSize; i++) {
        // decision level of the literal
        const int& dec_level = level(var(lits[i]));
        if (dec_level < minLevel) { continue; }  // ignore literals for assumptions
        withLevelZero = (dec_level == 0);

        // If the current decision level in the mark array is not associated
        // with the current step, that means that the decision level was
        // not discovered before.
        if (!lbd_marker.isCurrentStep(dec_level)) {
            // mark the current level as discovered
            lbd_marker.setCurrentStep(dec_level);
            // a new decision level was found
            distance++;
        }
    }

    // if the parameter says that level 0 should be ignored, ignore it (if it have been present)
    // Ignore all literals on level 0, as they are implied by the formula
    if (config.opt_lbd_ignore_l0 && withLevelZero) { return distance - 1; }
    return distance;
}

inline bool Solver::clauseAlreadyExists(const vec<CRef>& clauses, vec<Lit>& clause)
{
    char hit[ nVars() * 2 ];
    memset(hit, 0, sizeof(char)  * nVars() * 2);

    for (int i = 0 ; i < clause.size(); ++ i) {
        hit [ toInt(clause[i]) ] = 1;
    }

    bool found = false;
    for (int i = 0 ; i < clauses.size(); ++ i) {
        const Clause& c = ca[ clauses[i] ];
        if (clause.size() != c.size()) { continue; }  // size does not match

        bool failed = false;
        for (int j = 0 ; j < c.size(); ++ j) {
            if (hit[ toInt(c[j]) ] != 1) { failed = true; break; }
        }
        if (! failed) {
            std::cerr << "c found clause " << clause << " on position " << i << " as " << c << std::endl;
            found = true;
        }
    }

    return found;
}


//
// SECTION FOR DRAT PROOFS
//
// includes to avoid cyclic dependencies
//
}  // close namespace for include
// check generation of DRUP/DRAT proof on the fly
#include "proofcheck/OnlineProofChecker.h"

namespace Riss   // open namespace again!
{

//=================================================================================================
// Debug etc:


inline void Solver::printLit(Lit l)
{
    printf("%s%d:%c", sign(l) ? "-" : "", var(l) + 1, value(l) == l_True ? '1' : (value(l) == l_False ? '0' : 'X'));
}


inline void Solver::printClause(CRef cr)
{
    Clause& c = ca[cr];
    for (int i = 0; i < c.size(); i++) {
        printLit(c[i]);
        printf(" ");
    }
}

//=================================================================================================
}

//
// for parallel portfolio communication, have code in header, so that the code can be inlined!
//
#include "riss/core/Communication.h"
#include "riss/core/SolverCommunication.h"


namespace Riss   // open namespace again!
{
#ifdef DRATPROOF

inline bool Solver::outputsProof() const
{
    // either there is a local file, or there is a parallel build proof
    return proofFile != nullptr || (communication != 0 && communication->getPM() != 0);
}

template <class T>
inline bool Solver::checkClauseDRAT(const T& clause)
{
    // if we use the online checker, check the clauses!
    if (onlineDratChecker != 0) {

        const bool useExport = compression.isAvailable(); // indicate which data to be used
        if (useExport) {
            exportedClause.growTo(clause.size());
            exportedClause.clear();
            for (int i = 0 ; i < clause.size(); ++i) { exportedClause.push_(compression.exportLit(clause[i])); }
        }

        return useExport ? onlineDratChecker->addClause(exportedClause, lit_Undef, true) : onlineDratChecker->addClause(clause, lit_Undef, true);
    } else { return true; }
}

template <class T>
inline bool Solver::proofHasClause(const T& clause)
{
    if (onlineDratChecker != 0) {

        const bool useExport = compression.isAvailable(); // indicate which data to be used
        if (useExport) {
            exportedClause.growTo(clause.size());
            exportedClause.clear();
            for (int i = 0 ; i < clause.size(); ++i) { exportedClause.push_(compression.exportLit(clause[i])); }
        }

        return useExport ? onlineDratChecker->hasClause(exportedClause) : onlineDratChecker->hasClause(clause);
    } else { return true; }
}


template <class T>
inline void Solver::addToProof(const T& clause, const bool deleteFromProof, Lit remLit)
{
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly)) { return; }  // no proof, or delete and noDrup

    const bool useExport = compression.isAvailable(); // indicate which data to be used
    if (useExport) {
        exportedClause.growTo(clause.size());
        exportedClause.clear();
        for (int i = 0 ; i < clause.size(); ++i) { exportedClause.push_(compression.exportLit(clause[i])); }
        remLit = compression.exportLit(remLit);
    }

    // use an external tool to check the current addition?
    #ifndef NDEBUG
    if (!deleteFromProof && (const char*)config.opt_external_check != nullptr) {

        char formulaFileName[L_tmpnam];
        if (!tmpnam(formulaFileName)) {
            throw "cannot allocate temporary file for dump formula";
        }
        // write current CNF to this file
        dumpAndExit(formulaFileName, false, true); // do not exit, write full state
        char proofFileName[L_tmpnam];
        if (! tmpnam(proofFileName)) {
            throw "cannot allocate temporary file for dump proof";
        }

        FILE* f = fopen(proofFileName, "w");
        if (f == nullptr) {
            fprintf(stderr, "could not open proof file %s\n", proofFileName), exit(1);
        }
        stringstream s;
        s << clause;
        fprintf(f, "%s 0\n", s.str().c_str());

        std::cerr << "c to check adding current clause, call " << (const char*)config.opt_external_check << " " << formulaFileName << " " << proofFileName << std::endl;
        string toExecute = string((const char*)config.opt_external_check) + " "
                           + string(formulaFileName) + " "
                           + string(proofFileName);
        int returnCode = system(toExecute.c_str());
        std::cerr << "c finished with " << returnCode << std::endl;
        assert(returnCode == 0 && "checking new clause on current state has to go right");
        // exit(6); // for debugging stop here for now
    }
    #endif

    if (communication != 0) {  // if the solver is part of a portfolio, then produce a global proof!
//       if( deleteFromProof ) std::cerr << "c [" << communication->getID() << "] remove clause " << clause << " to proof" << std::endl;
//       else std::cerr << "c [" << communication->getID() << "] add clause " << clause << " to proof" << std::endl;
        if (deleteFromProof) {
            if (useExport) { communication->getPM()->delFromProof(exportedClause, remLit, communication->getID(), false); }    // first version: work on global proof only! TODO: change to local!
            else { communication->getPM()->delFromProof(clause, remLit, communication->getID(), false); }   // first version: work on global proof only! TODO: change to local!
        } else {
            if (useExport) { communication->getPM()->addToProof(exportedClause, remLit, communication->getID(), false); }   // first version: work on global proof only!
            else { communication->getPM()->addToProof(clause, remLit, communication->getID(), false); }  // first version: work on global proof only!
        }
        return;
    }

    // check before actually using the clause
    if (onlineDratChecker != 0) {
        if (deleteFromProof) {
            if (useExport) { onlineDratChecker->removeClause(exportedClause, remLit); }
            else { onlineDratChecker->removeClause(clause, remLit); }
        } else {
            bool addedClause = (useExport ? onlineDratChecker->addClause(exportedClause, remLit) : onlineDratChecker->addClause(clause, remLit));
            if (! addedClause) {
                cerr << "c WARNING: detected non DRAT clause, abort!" << endl;
                assert(false && "added clauses should be DRAT");
                exit(134);
            }
        }
    }
    // actually print the clause into the file
    if (deleteFromProof) { fprintf(proofFile, "d "); }
    if (remLit != lit_Undef) { fprintf(proofFile, "%i ", (var(remLit) + 1) * (-2 * sign(remLit) + 1)); }  // print this literal first (e.g. for DRAT clauses)
    const int csize = useExport ? exportedClause.size() : clause.size();
    if (useExport) {
        for (int i = 0; i < csize; i++) {
            if (exportedClause[i] == lit_Undef || exportedClause[i] == remLit) { continue; }   // print the remaining literal, if they have not been printed yet
            fprintf(proofFile, "%i ", (var(exportedClause[i]) + 1) * (-2 * sign(exportedClause[i]) + 1));
        }
    } else {
        for (int i = 0; i < csize; i++) {
            if (clause[i] == lit_Undef || clause[i] == remLit) { continue; }   // print the remaining literal, if they have not been printed yet
            fprintf(proofFile, "%i ", (var(clause[i]) + 1) * (-2 * sign(clause[i]) + 1));
        }
    }
    fprintf(proofFile, "0\n");

    if (config.opt_verboseProof == 2) {
        std::cerr << "c [PROOF] ";
        if (deleteFromProof) { std::cerr << " d "; }
        if (useExport) {
            for (int i = 0; i < csize; i++) {
                if (exportedClause[i] == lit_Undef) { continue; }
                std::cerr << exportedClause[i] << " ";
            }
        } else {
            for (int i = 0; i < csize; i++) {
                if (clause[i] == lit_Undef) { continue; }
                std::cerr << clause[i] << " ";
            }
        }
        if (deleteFromProof && remLit != lit_Undef) { std::cerr << remLit; }
        std::cerr << " 0" << std::endl;
    }
}

inline void Solver::addUnitToProof(Lit l, bool deleteFromProof)
{
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly)) { return; }  // no proof, or delete and noDrup

    if (compression.isAvailable()) { l = compression.exportLit(l); }

    if (communication != 0) {  // if the solver is part of a portfolio, then produce a global proof!
        if (deleteFromProof) { communication->getPM()->delFromProof(l, communication->getID(), false); }   // first version: work on global proof only! TODO: change to local!
        else { communication->getPM()->addUnitToProof(l, communication->getID(), false); }  // first version: work on global proof only!
        return;
    }

    // check before actually using the clause
    if (onlineDratChecker != 0) {
        if (deleteFromProof) { onlineDratChecker->removeClause(l); }
        else {
            onlineDratChecker->addClause(l);
        }
    }
    if (l == lit_Undef) { return; }  // no need to check this literal, however, routine can be used to check whether the empty clause is in the proof
    // actually print the clause into the file
    if (deleteFromProof) { fprintf(proofFile, "d "); }
    fprintf(proofFile, "%i 0\n", (var(l) + 1) * (-2 * sign(l) + 1));
    if (config.opt_verboseProof == 2) {
        if (deleteFromProof) { std::cerr << "c [PROOF] d " << l << std::endl; }
        else { std::cerr << "c [PROOF] " << l << std::endl; }
    }
}

inline void Solver::addCommentToProof(const char* text, bool deleteFromProof)
{
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly) || config.opt_verboseProof == 0) { return; } // no proof, no Drup, or no comments

    if (communication != 0 && communication->getPM() != 0) {
        communication->getPM()->addCommentToProof(text, communication->getID());
        return;
    }
    fprintf(proofFile, "c %s\n", text);
    if (config.opt_verboseProof == 2) { std::cerr << "c [PROOF] c " << text << std::endl; }
}

inline
lbool Solver::checkProof()
{
    if (onlineDratChecker != 0) {
        return onlineDratChecker->addClause(lit_Undef) ? l_True : l_False;
    } else {
        return l_Undef; // here, we simply do not know
    }
}

#endif

inline
void Solver::addInputClause_(vec< Lit >& ps)
{
    #ifdef DRATPROOF
    if (onlineDratChecker != 0) {
        // std::cerr << "c add parsed clause to DRAT-OTFC: " << ps << std::endl;
        onlineDratChecker->addParsedclause(ps);
    }
    #endif

}

inline
void Solver::updatePosNeg(bool somePosInClause, bool somNegInClause)
{
    posInAllClauses = posInAllClauses && somePosInClause; // memorize for the whole formula
    negInAllClauses = negInAllClauses && somNegInClause;
}


};

#endif
