/****************************************************************************************[Solver.h]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
				CRIL - Univ. Artois, France
				LRI  - Univ. Paris Sud, France
 
Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2013, Norbert Manthey, All rights reserved.

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

#ifndef Minisat_Solver_h
#define Minisat_Solver_h

#include "riss/mtl/Vec.h"
#include "riss/mtl/Heap.h"
#include "riss/mtl/Alg.h"
#include "riss/utils/Options.h"
#include "riss/utils/System.h"
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

//
// if PCASSO is compiled, use virtual methods
//
#ifdef PCASSO
#define PCASSOVIRTUAL virtual
#else
#define PCASSOVIRTUAL 
#endif

//
// forward declaration
//
namespace Coprocessor {
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
  class BIG;
}

#ifdef PCASSO 
namespace Pcasso {
  class PcassoClient;
}
#endif

class Communicator;

// since template methods need to be in headers ...
extern Riss::IntOption opt_verboseProof;
extern Riss::BoolOption opt_rupProofOnly;

namespace Riss {

class OnlineProofChecker; 
class IncSolver;
 
//=================================================================================================
// Solver -- the main class:

class Solver {
  
    friend class Coprocessor::Preprocessor;
    friend class Coprocessor::Propagation;
    friend class Coprocessor::BoundedVariableElimination;
    friend class Coprocessor::CoprocessorData;
    friend class Coprocessor::Probing;
    friend class Coprocessor::Symmetry;
    friend class Coprocessor::RATElimination;
    friend class Coprocessor::FourierMotzkin;
    friend class Coprocessor::ExperimentalTechniques;
    friend class Riss::IncSolver; // for bmc

#ifdef PCASSO 
    friend class Pcasso::PcassoClient; // PcassoClient is allowed to access all the solver data structures
#endif
    
    CoreConfig& config;
public:

    // Constructor/Destructor:
    //
    Solver(CoreConfig& _config);

    PCASSOVIRTUAL
    ~Solver();

    // Problem specification:
    //
    PCASSOVIRTUAL
    Var     newVar    (bool polarity = true, bool dvar = true, char type = 'o'); // Add a new variable with parameters specifying variable mode.
    void    reserveVars( Var v );

    PCASSOVIRTUAL
    bool    addClause (const vec<Lit>& ps);                     /// Add a clause to the solver. 
    
    bool    addClause (const Clause& ps);                       /// Add a clause to the solver (all clause invariants do not need to be checked) 
    bool    addEmptyClause();                                   /// Add the empty clause, making the solver contradictory.
    bool    addClause (Lit p);                                  /// Add a unit clause to the solver. 
    bool    addClause (Lit p, Lit q);                           /// Add a binary clause to the solver. 
    bool    addClause (Lit p, Lit q, Lit r);                    /// Add a ternary clause to the solver. 
    
    PCASSOVIRTUAL
    bool    addClause_(      vec<Lit>& ps);                     /// Add a clause to the solver without making superflous internal copy. Will
                                                                /// change the passed vector 'ps'.
    void    addInputClause_( vec<Lit>& ps);                     /// Add a clause to the online proof checker
    
    // Solving:
    //
    bool    simplify     ();                        /// Removes already satisfied clauses.
    bool    solve        (const vec<Lit>& assumps); /// Search for a model that respects a given set of assumptions.
    lbool   solveLimited (const vec<Lit>& assumps); /// Search for a model that respects a given set of assumptions (With resource constraints).
    bool    solve        ();                        /// Search without assumptions.
    bool    solve        (Lit p);                   /// Search for a model that respects a single assumption.
    bool    solve        (Lit p, Lit q);            /// Search for a model that respects two assumptions.
    bool    solve        (Lit p, Lit q, Lit r);     /// Search for a model that respects three assumptions.
    bool    okay         () const;                  /// FALSE means solver is in a conflicting state

    void    toDimacs     (FILE* f, const vec<Lit>& assumps);            // Write CNF to file in DIMACS-format.
    void    toDimacs     (const char *file, const vec<Lit>& assumps);
    void    toDimacs     (FILE* f, Clause& c, vec<Var>& map, Var& max);
    void printLit(Lit l);
    void printClause(CRef c);
    // Convenience versions of 'toDimacs()':
    void    toDimacs     (const char* file);
    void    toDimacs     (const char* file, Lit p);
    void    toDimacs     (const char* file, Lit p, Lit q);
    void    toDimacs     (const char* file, Lit p, Lit q, Lit r);
    
    // Variable mode:
    // 
    void    setPolarity    (Var v, bool b); /// Declare which polarity the decision heuristic should use for a variable. Requires mode 'polarity_user'.
    void    setDecisionVar (Var v, bool b); /// Declare if a variable should be eligible for selection in the decision heuristic.
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
    lbool   value      (Var x) const;       /// The current value of a variable.
    lbool   value      (Lit p) const;       /// The current value of a literal.
    lbool   modelValue (Var x) const;       /// The value of a variable in the last model. The last call to solve must have been satisfiable.
    lbool   modelValue (Lit p) const;       /// The value of a literal in the last model. The last call to solve must have been satisfiable.
    int     nAssigns   ()      const;       /// The current number of assigned literals.
    int     nClauses   ()      const;       /// The current number of original clauses.
    int     nLearnts   ()      const;       /// The current number of learnt clauses.
    int     nVars      ()      const;       /// The current number of variables.
    int     nTotLits   ()      const;       /// The current number of total literals in the formula.
    int     nFreeVars  ()      const;

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
    FILE*               drupProofFile;

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
    // Constants For restarts
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

    double    var_decay;
    double    clause_decay;
    double    random_var_freq;
    double    random_seed;
    int       ccmin_mode;         // Controls conflict clause minimization (0=none, 1=basic, 2=deep).
    int       phase_saving;       // Controls the level of phase saving (0=none, 1=limited, 2=full).
    bool      rnd_pol;            // Use random polarities for branching heuristics.
    bool      rnd_init_act;       // Initialize variable activities with a small random value.
    double    garbage_frac;       // The fraction of wasted memory allowed before a garbage collection is triggered.


    
    // Statistics: (read-only member variable)
    //
    uint64_t nbRemovedClauses,nbReducedClauses,nbDL2,nbBin,nbUn,nbReduceDB,solves, starts, decisions, rnd_decisions, propagations, conflicts,nbstopsrestarts,nbstopsrestartssame,lastblockatrestart;
    uint64_t dec_vars, clauses_literals, learnts_literals, max_literals, tot_literals;

protected:

    long curRestart;
    // Helper structures:
    //

    struct VarData { CRef reason; int level; 
      Lit dom;      /// for lhbr
      int32_t cost; /// for hack
#ifdef CLS_EXTRA_INFO
      uint64_t extraInfo;
#endif
    };
    static inline VarData mkVarData(CRef cr, int l){ VarData d = {cr, l, lit_Undef, -1
#ifdef CLS_EXTRA_INFO
      , 0
#endif
    }; return d; }
    static inline VarData mkVarData(CRef cr, int l, int _cost){ VarData d = {cr, l,lit_Undef,_cost
#ifdef CLS_EXTRA_INFO
      , 0
#endif
    }; return d; }  
    
public:

    // movd watcher data structure to solver types!
  

    struct VarOrderLt {
        const vec<double>&  activity;
        bool operator () (Var x, Var y) const { return activity[x] > activity[y]; }
        VarOrderLt(const vec<double>&  act) : activity(act) { }
    };

protected:
    // Solver state:
    //
    int lastIndexRed;
    bool                ok;               // If FALSE, the constraints are already unsatisfiable. No part of the solver state may be used!
    double              cla_inc;          // Amount to bump next clause with.
    vec<double>         activity;         // A heuristic measurement of the activity of a variable.
    double              var_inc;          // Amount to bump next variable with.
public: // TODO FIXME undo after debugging!
    OccLists<Lit, vec<Watcher>, WatcherDeleted>
                        watches;          // 'watches[lit]' is a list of constraints watching 'lit' (will go there if literal becomes true).
    // no watchesBin, incorporated into watches
public: // TODO: set more nicely, or write method!
    vec<CRef>           clauses;          // List of problem clauses.
    vec<CRef>           learnts;          // List of learnt clauses.

    struct VarFlags {
      lbool assigns;
      unsigned polarity:1;
      unsigned decision:1;
      unsigned seen:1;
      unsigned extra:4; // TODO: use for special variable (not in LBD) and do not touch!
      unsigned frozen:1; // indicate that this variable cannot be used for simplification techniques that do not preserve equivalence
#ifdef PCASSO
      unsigned varPT:16; // partition tree level for this variable
#endif
      VarFlags( char _polarity ) : assigns(l_Undef), polarity(_polarity), decision(0), seen(0), extra(0), frozen(0) 
#ifdef PCASSO
      , varPT(0)
#endif
      {}
      VarFlags () : assigns(l_Undef), polarity(1), decision(0), seen(0), extra(0), frozen(0) 
#ifdef PCASSO
      , varPT(0)
#endif
      {}
    };
    
protected:
    vec<VarFlags> varFlags;
    
//     vec<lbool>          assigns;          // The current assignments.
//     vec<char>           polarity;         // The preferred polarity of each variable.
//     vec<char>           decision;         // Declares if a variable is eligible for selection in the decision heuristic.
//      vec<char>           seen;
    

public:
    /// set whether a variable can be used for simplification techniques that do not preserve equivalence
    void freezeVariable( const Var&v, const bool& frozen ) { varFlags[v].frozen = frozen; }
    /// indicates that this variable cannot be used for simplification techniques that do not preserve equivalence
    bool isFrozen( const Var&v ) const { return varFlags[v].frozen; }
    
    vec<Lit>            trail;            // Assignment stack; stores all assigments made in the order they were made.
protected:
    vec<int>            nbpos;
    vec<int>            trail_lim;        // Separator indices for different decision levels in 'trail'.
    vec<VarData>        vardata;          // Stores reason and level for each variable.
    int                 qhead;            // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
    int                 realHead;         // indicate last literal that has been analyzed for unit propagation
    int                 simpDB_assigns;   // Number of top-level assignments since last execution of 'simplify()'.
    int64_t             simpDB_props;     // Remaining number of propagations that must be made before next execution of 'simplify()'.
    vec<Lit>            assumptions;      // Current set of assumptions provided to solve by the user.
    Heap<VarOrderLt>    order_heap;       // A priority queue of variables ordered with respect to the variable activity.
    double              progress_estimate;// Set by 'search()'.
    bool                remove_satisfied; // Indicates whether possibly inefficient linear scan for satisfied clauses should be performed in 'simplify'.
    MarkArray permDiff;
    //vec<unsigned long>  permDiff;         // permDiff[var] contains the current conflict number... Used to count the number of  LBD
    
#ifdef UPDATEVARACTIVITY
    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
    vec<Lit> lastDecisionLevel; 
#endif

public: // TODO: set more nicely!
    ClauseAllocator     ca;
protected:

    int nbclausesbeforereduce;            // To know when it is time to reduce clause database
    
    bqueue<unsigned int> trailQueue,lbdQueue; // Bounded queues for restarts.
    float sumLBD; // used to compute the global average of LBD. Restarts...


    // Temporaries (to reduce allocation overhead). Each variable is prefixed by the method in which it is
    // used, exept 'seen' wich is used in several places.
    //
    vec<Lit>            analyze_stack;
    vec<Lit>            analyze_toclear;
    vec<Lit>            add_tmp;
    unsigned long  MYFLAG;

    vec<int> trailPos;			/// store the position where the variable is located in the trail exactly (for hack)
    
    double              max_learnts;
    double              learntsize_adjust_confl;
    int                 learntsize_adjust_cnt;

    Clock totalTime, propagationTime, analysisTime, preprocessTime, inprocessTime, extResTime, reduceDBTime, icsTime; // times for methods during search
    int preprocessCalls, inprocessCalls;	// stats
    
    // Resource contraints:
    //
    int64_t             conflict_budget;    // -1 means no budget.
    int64_t             propagation_budget; // -1 means no budget.
    bool                asynch_interrupt;

    // Main internal methods:
    //
    void     insertVarOrder   (Var x);                                                 // Insert a variable in the decision order priority queue.
    PCASSOVIRTUAL
    Lit      pickBranchLit    ();                                                      // Return the next decision variable.
    void     newDecisionLevel ();                                                      // Begins a new decision level.
    PCASSOVIRTUAL
    void     uncheckedEnqueue (Lit p, CRef from = CRef_Undef,                          // Enqueue a literal. Assumes value of literal is undefined.
			       bool addToProof = false, const uint64_t extraInfo = 0);     // decide whether the method should furthermore add the literal to the proof, and whether the literal has an extra information (interegsting for decision level 0)
    bool     enqueue          (Lit p, CRef from = CRef_Undef);                         // Test if fact 'p' contradicts current state, enqueue otherwise.
    PCASSOVIRTUAL
    CRef     propagate        (bool duringAddingClauses = false );                     // Perform unit propagation. Returns possibly conflicting clause (during adding clauses, to add proof infos, if necessary)
    void     cancelUntil      (int level);                                             // Backtrack until a certain level.
    PCASSOVIRTUAL
    int      analyze          (CRef confl, vec< Lit >& out_learnt, int& out_btlevel, unsigned int& lbd, uint64_t& extraInfo );    // // (bt = backtrack, return is number of unit clauses in out_learnt. if 0, treat as usual!)
    void     analyzeFinal     (Lit p, vec<Lit>& out_conflict);                         // COULD THIS BE IMPLEMENTED BY THE ORDINARIY "analyze" BY SOME REASONABLE GENERALIZATION?
    bool     litRedundant     (Lit p, uint32_t abstract_levels,uint64_t& extraInfo);                       // (helper method for 'analyze()')
    PCASSOVIRTUAL
    lbool    search           (int nof_conflicts);                                     // Search for a given number of conflicts.
    PCASSOVIRTUAL
    lbool    solve_           ();                                                      // Main solve method (assumptions given in 'assumptions').
    PCASSOVIRTUAL
    void     reduceDB         ();                                                      // Reduce the set of learnt clauses.
    void     removeSatisfied  (vec<CRef>& cs);                                         // Shrink 'cs' to contain only non-satisfied clauses.
    void     rebuildOrderHeap ();

    // Maintaining Variable/Clause activity:
    //
    void     varDecayActivity ();                      // Decay all variables with the specified factor. Implemented by increasing the 'bump' value instead.
    void     varBumpActivityD (Var v, double inc);     // Increase a variable with the current 'bump' value.
    void     varBumpActivity  (Var v, double inverseRatio = 1) ;      // Increase a variable with the current 'bump' value, multiplied by 1 / (inverseRatio).
    void     claDecayActivity ();                      // Decay all clauses with the specified factor. Implemented by increasing the 'bump' value instead.
    void     claBumpActivity  (Clause& c, double inverseRatio = 1);   // Increase a clause with the current 'bump' value, multiplied by 1 / (inverseRatio).

    // Operations on clauses:
    //
    void     attachClause     (CRef cr);               // Attach a clause to watcher lists.
    void     detachClause     (CRef cr, bool strict = false); // Detach a clause to watcher lists.
    PCASSOVIRTUAL
    void     removeClause     (CRef cr, bool strict = false); // Detach and free a clause.
    bool     locked           (const Clause& c) const; // Returns TRUE if a clause is a reason for some implication in the current state.
    bool     satisfied        (const Clause& c) const; // Returns TRUE if a clause is satisfied in the current state.
public:
    bool addUnitClauses( const vec< Lit >& other );		// use the given trail as starting point, return true, if fails!
protected:
    unsigned int computeLBD(const vec<Lit> & lits);
    unsigned int computeLBD(const Clause &c);
    
    /** perform minimization with binary clauses of the formula
     *  @param lbd the current calculated LBD score of the clause
     *  @return true, if the clause has been shrinked
     */
    bool minimisationWithBinaryResolution(vec<Lit> &out_learnt, unsigned int& lbd);

    PCASSOVIRTUAL
    void     relocAll         (ClauseAllocator& to);

    // Misc:
    //
    int      decisionLevel    ()      const; // Gives the current decisionlevel.
    uint32_t abstractLevel    (Var x) const; // Used to represent an abstraction of sets of decision levels.
    CRef     reason           (Var x) const;
    int      level            (Var x) const;
    double   progressEstimate ()      const; // DELETE THIS ?? IT'S NOT VERY USEFUL ...
    bool     withinBudget     ()      const;
    
    /** to handle termination from the outside by a callback function */
    void* terminationCallbackState;                            // state that should be passed to the calling method
    int (*terminationCallbackMethod)(void* terminationState);  // pointer to the callback method

    
public:
    /** set a callback to a function that should be frequently tested by the solver to be noticed that the current search should be interrupted
     * Note: the state has to be used as argument when calling the callback
     * @param terminationState pointer to an external state object that is used in the termination callback
     * @param terminationCallbackMethod pointer to an external callback method that indicates termination (return value is != 0 to terminate)
     */
    void setTerminationCallback(void* terminationState, int (*terminationCallback)(void*));

    /// use the set preprocessor (if present) to simplify the current formula
    lbool preprocess();
protected:
    lbool inprocess(lbool status); // inprocessing code
    lbool initSolve( int solves ); // set up the next call to solve
    
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
    lbool handleLearntClause(Riss::vec< Riss::Lit >& learnt_clause, bool backtrackedBeyond, unsigned int nblevels, uint64_t extraInfo);
    
    /** check whether a restart should be performed (return true, if restart) 
     * @param nof_conflicts limit can be increased by the method, if an agility reject has been applied
     */
    bool restartSearch(int& nof_conflicts, const int conflictC);
    
    /** remove learned clauses during search */
    void clauseRemoval();
    
#ifdef DRATPROOF
    // DRUP proof
    bool outputsProof() const ;
    template <class T>
    void addToProof(   const T& clause, bool deleteFromProof = false, const Lit remLit = lit_Undef); // write the given clause to the output, if the output is enabled
    void addUnitToProof( const Lit& l, bool deleteFromProof=false);    // write a single unit clause to the proof
    void addCommentToProof( const char* text, bool deleteFromProof=false); // write the text as comment into the proof!
public:
    bool checkProof(); // if online checker is used, return whether the current proof is valid
protected:
#else // have empty dummy functions
    bool outputsProof() const { return false; }
    template <class T>
    void addToProof(   const T& clause, bool deleteFromProof = false, const Lit remLit = lit_Undef) const {};
    void addUnitToProof( const Lit& l, bool deleteFromProof=false) const {};    
    void addCommentToProof( const char* text, bool deleteFromProof=false) const {}; 
public:
    bool checkProof() const { return true; } // if online checker is used, return whether the current proof is valid
protected:
#endif

    
    /** fill the current variable assignment into the given vector */
    void fillLAmodel(vec<LONG_INT>& pattern, const int steps, vec<Var>& relevantVariables ,const bool moveOnly = false); // fills current model into variable vector
    
    /** perform la hack, return false -> unsat instance!
     * @return false, instance is unsatisfable
     */
    bool laHack(Riss::vec< Riss::Lit >& toEnqueue);
 
    // Static helpers:
    //

    // Returns a random float 0 <= x < 1. Seed must never be 0.
    static inline double drand(double& seed) {
        seed *= 1389796;
        int q = (int)(seed / 2147483647);
        seed -= (double)q * 2147483647;
        return seed / 2147483647; }

    // Returns a random integer 0 <= x < size. Seed must never be 0.
public: static inline int irand(double& seed, int size) {
        return (int)(drand(seed) * size); }

        /// build reduct wrt current unit clauses
	void buildReduct(); 
        
protected:
  
  OnlineProofChecker* onlineDratChecker;
  
  
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
  int maxLaNumber;	     // maximum number of LAs allowed
  int topLevelsSinceLastLa; // number of learned top level units since last LA
  int laEEvars,laEElits;    // number of equivalent literals
  vector< vector< Lit > > localLookAheadProofClauses;
  vector<Lit> localLookAheadTmpClause;
  
  // real data
  Lit hstry[6];
  vec<VarFlags> backupSolverState;	// vector to hold the solver state
  vec<LONG_INT> variablesPattern;	// vector for variable patterns
  vec<Var> relevantLAvariables;		// vector that stores the variables that are relevant for local LA
  int untilLa;		// count until  next LA is performed
  int laBound;		// current bound for l5-LA
  bool laStart;		// when reached the la level, perform la
        
  bool startedSolving;	// inidicate whether solving started already
  
  double useVSIDS;	// parameter for interpolating between VSIDS and VMTF
 
  int simplifyIterations; // number of visiting level 0 until simplification is to be performed
  int learnedDecisionClauses;

  // stats for learning clauses
  double totalLearnedClauses, sumLearnedClauseSize, sumLearnedClauseLBD, maxLearnedClauseSize;

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
  unsigned L2units,L3units,L4units;
  /** if the new learned clause is binary, C = (a \lor b), 
   *  then it is checked whether a literal l is implied by both a and b in the formula. 
   *  Should be called after eventually enqueuing the asserting literal of the current learned clause
   *  @return true, if a contrdiction has been found, so that the result is UNSAT
   */
  bool analyzeNewLearnedClause( const CRef newLearnedClause );

  // helper data structures
  vector< int > analyzePosition; // for full probing approximation
  vector< int > analyzeLimits; // all combination limits for full probing
  
 
  // UHLE during search with learnt clauses:
  uint32_t searchUHLEs, searchUHLElits;
  
  /** reduce the literals inside the clause with the help of the information of the binary implication graph (UHLE only) 
   * @param lbd current lbd value of the given clause
   * @return true, if the clause has been shrinked, false otherwise (then, the LBD also stays the same)
   */
  bool searchUHLE(vec<Lit>& learned_clause, unsigned int& lbd );
  
  /** reduce the learned clause by replacing pairs of literals with their previously created extended resolution literal
   * @param lbd current lbd value of the given clause
   * @return true, if the clause has been shrinked, false otherwise (then, the LBD also stays the same)
   */
  bool erRewrite(vec<Lit>& learned_clause, unsigned int& lbd );


/// for coprocessor
protected:  Coprocessor::Preprocessor* coprocessor;
public:     
  
  void setPreprocessor( Coprocessor::Preprocessor* cp );
  
  void setPreprocessor( Coprocessor::CP3Config* _config );
  
  bool useCoprocessorPP;
  bool useCoprocessorIP;

  /** if extra info should be used, this method needs to return true! */
  bool usesExtraInfo() const { 
#ifdef CLS_EXTRA_INFO
  return true;
#else
  return false;
#endif
  } 

  /** for solver extensions, which rely on extra informations per clause (including unit clauses), e.g. the level of the solver in a partition tree*/
  uint64_t defaultExtraInfo() const ;

  /** return extra variable information (should be called for top level units only!) */
  uint64_t variableExtraInfo( const Var& v ) const ;
  
/// for qprocessor
public:
// 	    void writeClauses( std::ostream& stream ) {
// 	       
// 	    }


// [BEGIN] modifications for parallel assumption based solver
public: 
    /** setup the communication object
     * @param comm pointer to the communication object that should be used by this thread
     */
    void setCommunication( Communicator* comm );
    
    /** reset the state of an possibly interrupted solver 
     * clear interrupt, jump to level 0
     */
    void resetLastSolve();
    
private: 
    Communicator* communication; /// communication with the outside, and control of this solver
    
    /** goto sleep, wait until master did updates, wakeup, process the updates
     * @param toSend if not 0, send the (learned) clause, if 0, receive shared clauses
     * note: can also interrupt search and incorporate new assumptions etc.
     * @return -1 = abort, 0=everythings nice, 1=conflict/false result
     */
    int updateSleep(vec<Lit>* toSend, bool multiUnits = false);
    
    /** add a learned clause to the solver
     * @param bump increases the activity of the current clause to the last seen value
     * note: behaves like the addClause structure
     */
    bool addLearnedClause(Riss::vec< Riss::Lit >& ps, bool bump);
    
    /** update the send limits based on whether a current clause could have been send or not
     * @param failed sending current clause failed because of limits
     * @param sizeOnly update only size information
     */
    void updateDynamicLimits( bool failed, bool sizeOnly = false );
    
    /** inits the protection environment for variables
     */
    void initVariableProtection();
    
    /** set send limits once!
     */
    void initLimits();
    
    /*
     * stats and limits for communication
     */
    vec<Lit> receiveClause;			/// temporary placeholder for receiving clause
    std::vector< CRef > receiveClauses;	/// temporary placeholder indexes of the clauses that have been received by communication
    int currentTries;                          /// current number of waits
    int receiveEvery;                          /// do receive every n tries
    float currentSendSizeLimit;                /// dynamic limit to control send size
    float currentSendLbdLimit;                 /// dynamic limit to control send lbd
public:
    int succesfullySend;                       /// number of clauses that have been sucessfully transmitted
    int succesfullyReceived;                   /// number of clauses that have been sucessfully transmitted
private:
    float sendSize;                            /// Minimum Lbd of clauses to send  (also start value)
    float sendLbd;                             /// Minimum size of clauses to send (also start value)
    float sendMaxSize;                         /// Maximum size of clauses to send
    float sendMaxLbd;                          /// Maximum Lbd of clauses to send
    float sizeChange;                          /// How fast should size send limit be adopted?
    float lbdChange;                           /// How fast should lbd send limit be adopted?
    float sendRatio;                           /// How big should the ratio of send clauses be?
    
// [END] modifications for parallel assumption based solver

// Modifications for Pcasso
#ifdef PCASSO
    Pcasso::PcassoClient* pcassoClient;

#endif
// END modifications for Pcasso


};


//=================================================================================================
// Implementation of inline methods:

inline CRef Solver::reason(Var x) const { return vardata[x].reason; }
inline int  Solver::level (Var x) const { return vardata[x].level; }

inline void Solver::insertVarOrder(Var x) {
    if (!order_heap.inHeap(x) && varFlags[x].decision) order_heap.insert(x); }

inline void Solver::varDecayActivity() { var_inc *= (1 / var_decay); }
inline void Solver::varBumpActivity(Var v, double inverseRatio) { varBumpActivityD(v, var_inc / (double) inverseRatio); }
inline void Solver::varBumpActivityD(Var v, double inc) {
    activity[v] = ( useVSIDS * activity[v] ) + inc; // interpolate between VSIDS and VMTF here!
    // NOTE this code is also used in extended clause learning, and restricted extended resolution
    if ( activity[v] > 1e100 ) {
        // Rescale:
        for (int i = 0; i < nVars(); i++)
            activity[i] *= 1e-100;
        var_inc *= 1e-100; }

    // Update order_heap with respect to new activity:
    if (order_heap.inHeap(v))
        order_heap.decrease(v); }

inline void Solver::claDecayActivity() { cla_inc *= (1 / clause_decay); }
inline void Solver::claBumpActivity (Clause& c, double inverseRatio) {
        if ( (c.activity() += cla_inc / inverseRatio) > 1e20 ) {
            // Rescale:
            for (int i = 0; i < learnts.size(); i++)
                ca[learnts[i]].activity() *= 1e-20;
            cla_inc *= 1e-20; } }

inline void Solver::checkGarbage(void){ return checkGarbage(garbage_frac); }
inline void Solver::checkGarbage(double gf){
    if (ca.wasted() > ca.size() * gf)
        garbageCollect(); }

// NOTE: enqueue does not set the ok flag! (only public methods do)
inline bool     Solver::enqueue         (Lit p, CRef from)      { return value(p) != l_Undef ? value(p) != l_False : (uncheckedEnqueue(p, from), true); }
inline bool     Solver::addClause       (const vec<Lit>& ps)    { ps.copyTo(add_tmp); return addClause_(add_tmp); }
inline bool     Solver::addEmptyClause  ()                      { add_tmp.clear(); return addClause_(add_tmp); }
inline bool     Solver::addClause       (Lit p)                 { add_tmp.clear(); add_tmp.push(p); return addClause_(add_tmp); }
inline bool     Solver::addClause       (Lit p, Lit q)          { add_tmp.clear(); add_tmp.push(p); add_tmp.push(q); return addClause_(add_tmp); }
inline bool     Solver::addClause       (Lit p, Lit q, Lit r)   { add_tmp.clear(); add_tmp.push(p); add_tmp.push(q); add_tmp.push(r); return addClause_(add_tmp); }
 inline bool     Solver::locked          (const Clause& c) const { 
   if(c.size()>2) 
     return value(c[0]) == l_True && reason(var(c[0])) != CRef_Undef && ca.lea(reason(var(c[0]))) == &c; 
   return 
     (value(c[0]) == l_True && reason(var(c[0])) != CRef_Undef && ca.lea(reason(var(c[0]))) == &c)
     || 
     (value(c[1]) == l_True && reason(var(c[1])) != CRef_Undef && ca.lea(reason(var(c[1]))) == &c);
 }
inline void     Solver::newDecisionLevel()                      { trail_lim.push(trail.size());}

inline int      Solver::decisionLevel ()      const   { return trail_lim.size(); }
inline uint32_t Solver::abstractLevel (Var x) const   { return 1 << (level(x) & 31); }
inline lbool    Solver::value         (Var x) const   { return varFlags[x].assigns; }
inline lbool    Solver::value         (Lit p) const   { return varFlags[var(p)].assigns ^ sign(p); }
inline lbool    Solver::modelValue    (Var x) const   { return model[x]; }
inline lbool    Solver::modelValue    (Lit p) const   { return model[var(p)] ^ sign(p); }
inline int      Solver::nAssigns      ()      const   { return trail.size(); }
inline int      Solver::nClauses      ()      const   { return clauses.size(); }
inline int      Solver::nLearnts      ()      const   { return learnts.size(); }
inline int      Solver::nVars         ()      const   { return vardata.size(); }
inline int      Solver::nTotLits      ()      const   { return clauses_literals + learnts_literals; }
inline int      Solver::nFreeVars     ()      const   { return (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]); }
inline void     Solver::setPolarity   (Var v, bool b) { varFlags[v].polarity = b; }
inline void     Solver::setDecisionVar(Var v, bool b) 
{ 
    if      ( b && !varFlags[v].decision) dec_vars++;
    else if (!b &&  varFlags[v].decision) dec_vars--;

    varFlags[v].decision = b;
    insertVarOrder(v);
}
inline void     Solver::setConfBudget(int64_t x){ conflict_budget    = conflicts    + x; }
inline void     Solver::setPropBudget(int64_t x){ propagation_budget = propagations + x; }
inline void     Solver::interrupt(){ asynch_interrupt = true; }
inline void     Solver::clearInterrupt(){ asynch_interrupt = false; }
inline void     Solver::budgetOff(){ conflict_budget = propagation_budget = -1; }
inline bool     Solver::withinBudget() const {
    return !asynch_interrupt && 
	   (terminationCallbackMethod == 0 || 0 == terminationCallbackMethod ( terminationCallbackState ) ) && // check callback to ask outside for termination, if the callback has been set
           (conflict_budget    < 0 || conflicts < (uint64_t)conflict_budget) &&
           (propagation_budget < 0 || propagations < (uint64_t)propagation_budget); }

// FIXME: after the introduction of asynchronous interrruptions the solve-versions that return a
// pure bool do not give a safe interface. Either interrupts must be possible to turn off here, or
// all calls to solve must return an 'lbool'. I'm not yet sure which I prefer.
inline bool     Solver::solve         ()                    { budgetOff(); assumptions.clear(); return solve_() == l_True; }
inline bool     Solver::solve         (Lit p)               { budgetOff(); assumptions.clear(); assumptions.push(p); return solve_() == l_True; }
inline bool     Solver::solve         (Lit p, Lit q)        { budgetOff(); assumptions.clear(); assumptions.push(p); assumptions.push(q); return solve_() == l_True; }
inline bool     Solver::solve         (Lit p, Lit q, Lit r) { budgetOff(); assumptions.clear(); assumptions.push(p); assumptions.push(q); assumptions.push(r); return solve_() == l_True; }
inline bool     Solver::solve         (const vec<Lit>& assumps){ budgetOff(); assumps.copyTo(assumptions); return solve_() == l_True; }
inline lbool    Solver::solveLimited  (const vec<Lit>& assumps){ assumps.copyTo(assumptions); return solve_(); }
inline void     Solver::setRandomSeed (double seed) { assert(seed != 0); random_seed = seed; }
inline bool     Solver::okay          ()      const   { return ok; }

inline void     Solver::toDimacs     (const char* file){ vec<Lit> as; toDimacs(file, as); }
inline void     Solver::toDimacs     (const char* file, Lit p){ vec<Lit> as; as.push(p); toDimacs(file, as); }
inline void     Solver::toDimacs     (const char* file, Lit p, Lit q){ vec<Lit> as; as.push(p); as.push(q); toDimacs(file, as); }
inline void     Solver::toDimacs     (const char* file, Lit p, Lit q, Lit r){ vec<Lit> as; as.push(p); as.push(q); as.push(r); toDimacs(file, as); }

inline void     Solver::setTerminationCallback(void* terminationState, int (*terminationCallback)(void*)) {
  terminationCallbackState  = terminationState;
  terminationCallbackMethod = terminationCallback;
}

inline
bool Solver::addUnitClauses(const vec< Lit >& other)
{
  assert( decisionLevel() == 0 && "init trail can only be done at level 0" );
  for( int i = 0 ; i < other.size(); ++ i ) {
    addUnitToProof( other[i] ); // add the unit clause to the proof
    if( value( other[i] ) == l_Undef ) {
      uncheckedEnqueue( other[i] );
    } else if ( value( other[i] ) == l_False ) {
      ok = false; return true;
    }
  }
  return propagate() != CRef_Undef;
}



//
// SECTION FOR DRAT PROOFS
//
// includes to avoid cyclic dependencies
//
}  // close namespace for include
// check generation of DRUP/DRAT proof on the fly
#include "proofcheck/OnlineProofChecker.h"

namespace Riss { // open namespace again!

//=================================================================================================
// Debug etc:


inline void Solver::printLit(Lit l)
{
    printf("%s%d:%c", sign(l) ? "-" : "", var(l)+1, value(l) == l_True ? '1' : (value(l) == l_False ? '0' : 'X'));
}


inline void Solver::printClause(CRef cr)
{
  Clause &c = ca[cr];
    for (int i = 0; i < c.size(); i++){
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


namespace Riss { // open namespace again!
  #ifdef DRATPROOF

  inline bool Solver::outputsProof () const { 
    // either there is a local file, or there is a parallel build proof
    return drupProofFile != NULL || (communication != 0 && communication->getPM() != 0 );
  }

  template <class T>
  inline void Solver::addToProof( const T& clause, bool deleteFromProof, const Lit remLit)
  {
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly) ) return; // no proof, or delete and noDrup

    if( communication != 0 ) { // if the solver is part of a portfolio, then produce a global proof!
//       if( deleteFromProof ) cerr << "c [" << communication->getID() << "] remove clause " << clause << " to proof" << endl;
//       else cerr << "c [" << communication->getID() << "] add clause " << clause << " to proof" << endl;
      if( deleteFromProof ) communication->getPM()->delFromProof(clause, remLit, communication->getID(), false ); // first version: work on global proof only! TODO: change to local!
      else communication->getPM()->addToProof(clause, remLit, communication->getID(), false ); // first version: work on global proof only!
      return;
    }

    // check before actually using the clause 
    if( onlineDratChecker != 0 ) {
      if( deleteFromProof ) onlineDratChecker->removeClause( clause, remLit );
      else {
	onlineDratChecker->addClause( clause, remLit );
      }
    }
    // actually print the clause into the file
    if( deleteFromProof ) fprintf(drupProofFile, "d ");
    if( remLit != lit_Undef ) fprintf(drupProofFile, "%i ", (var(remLit) + 1) * (-2 * sign(remLit) + 1)); // print this literal first (e.g. for DRAT clauses)
    for (int i = 0; i < clause.size(); i++) {
      if( clause[i] == lit_Undef || clause[i] == remLit ) continue;	// print the remaining literal, if they have not been printed yet
      fprintf(drupProofFile, "%i ", (var(clause[i]) + 1) * (-2 * sign(clause[i]) + 1));
    }
    fprintf(drupProofFile, "0\n");
    
    if( config.opt_verboseProof == 2 ) {
      cerr << "c [PROOF] ";
      if( deleteFromProof ) cerr << " d ";
      for (int i = 0; i < clause.size(); i++) {
	if( clause[i] == lit_Undef ) continue;
	cerr << clause[i] << " ";
      }    
      if( deleteFromProof && remLit != lit_Undef ) cerr << remLit;
      cerr << " 0" << endl;
    }
  }

  inline void Solver::addUnitToProof(const Lit& l, bool deleteFromProof)
  {
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly) ) return; // no proof, or delete and noDrup

    if( communication != 0 ) { // if the solver is part of a portfolio, then produce a global proof!
      if( deleteFromProof ) communication->getPM()->delFromProof(l, communication->getID(), false ); // first version: work on global proof only! TODO: change to local!
      else communication->getPM()->addUnitToProof(l, communication->getID(), false ); // first version: work on global proof only!
      return;
    }

    // check before actually using the clause 
    if( onlineDratChecker != 0 ) {
      if( deleteFromProof ) onlineDratChecker->removeClause(l);
      else {
	onlineDratChecker->addClause(l);
      }
    }
    if( l == lit_Undef ) return; // no need to check this literal, however, routine can be used to check whether the empty clause is in the proof
    // actually print the clause into the file
    if( deleteFromProof ) fprintf(drupProofFile, "d ");
    fprintf(drupProofFile, "%i 0\n", (var(l) + 1) * (-2 * sign(l) + 1));  
    if( config.opt_verboseProof == 2 ) {
      if( deleteFromProof ) cerr << "c [PROOF] d " << l << endl;
      else cerr << "c [PROOF] " << l << endl;
    }
  }

  inline void Solver::addCommentToProof(const char* text, bool deleteFromProof)
  {
    if (!outputsProof() || (deleteFromProof && config.opt_rupProofOnly) || config.opt_verboseProof == 0) return; // no proof, no Drup, or no comments

    if( communication != 0 && communication->getPM() != 0) {
      communication->getPM()->addCommentToProof(text, communication->getID() );
      return;
    }
    fprintf(drupProofFile, "c %s\n", text);  
    if( config.opt_verboseProof == 2 ) cerr << "c [PROOF] c " << text << endl;
  }

  inline
  bool Solver::checkProof () 
  {
    if( onlineDratChecker != 0 ) {
      return onlineDratChecker->addClause(lit_Undef);
    } else {
      return true; // here, we simply do not know
    }
  }

  #endif

  inline
  void Solver::addInputClause_(vec< Lit >& ps)
  {
  #ifdef DRATPROOF
    if( onlineDratChecker != 0 ) {
      // cerr << "c add parsed clause to DRAT-OTFC: " << ps << endl;
      onlineDratChecker->addParsedclause( ps );
    }
  #endif
  }

};

#endif