/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_CoreConfig_h
#define RISS_CoreConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"
#include "riss/utils/Debug.h"

namespace Riss
{

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CoreConfig : public Config
{
    /** pointer to all options in this object - used for parsing and printing the help! */
    vec<Option*> configOptions;


  public:
    /** default constructor, which sets up all options in their standard format */
    CoreConfig(const std::string& presetOptions = "");


    /**
    * List of all used options, public members, can be changed and read directly
    */
    BoolOption opt_solve_stats;
    BoolOption opt_fast_rem; // remove elements on watch list faster, but unsorted
    IntOption nanosleep; // nanoseconds to sleep for each conflict
    BoolOption ppOnly; // interrupt after preprocessing
    #ifndef NDEBUG
    BoolOption opt_learn_debug;
    IntOption opt_removal_debug;
    #endif
    BoolOption opt_refineConflict;
    BoolOption opt_refineConflictReverse;

    DoubleOption opt_K;
    DoubleOption opt_R;
    IntOption opt_size_lbd_queue;
    IntOption opt_size_trail_queue;
    IntOption opt_size_bounded_randomized; // Revisiting the Learned Clauses Database Reduction Strategies paper by Jabbour et al


    IntOption opt_first_reduce_db;
    IntOption opt_inc_reduce_db;
    IntOption opt_spec_inc_reduce_db;
    IntOption opt_lb_lbd_frozen_clause;
    BoolOption opt_lbd_ignore_l0; // do not consider literals that have toplevel assignments for LBD calculation
    BoolOption opt_lbd_ignore_assumptions; // do not consider assumption levels for LBD
    IntOption opt_update_lbd; // update LBD during 0=propagation,1=learning,2=never (if during propagation, then during learning is not necessary!)
    BoolOption opt_lbd_inc;    // allow to increase LBD of clauses dynamically?
    BoolOption opt_quick_reduce; // check clause for being satisfied based on the first two literals only!
    DoubleOption opt_keep_worst_ratio; // keep this (relative to all learnt clauses) number of worst learnt clauses

    IntOption opt_lb_size_minimzing_clause;
    IntOption opt_lb_lbd_minimzing_clause;

    DoubleOption opt_var_decay_start; // start value default: 0.95 glucose 2.3: 0.8
    DoubleOption opt_var_decay_stop;  // stop value  default: 0.95 glucose 2.3: 0.95
    DoubleOption opt_var_decay_inc;   // increase value default: 0 glucose 2.3: 0.01
    IntOption opt_var_decay_dist;     // increase after this number of conflicts: default: never, glucose 2.3: 5000

    DoubleOption opt_clause_decay;
    DoubleOption opt_random_var_freq;
    DoubleOption opt_random_seed;
    IntOption opt_ccmin_mode;
    IntOption opt_phase_saving;
    BoolOption opt_rnd_init_act;
    IntOption opt_init_act;
    IntOption opt_init_pol;

    IntOption opt_restart_level;
    IntOption opt_restarts_type;
    IntOption opt_restart_first;
    DoubleOption opt_restart_inc;
    IntOption opt_inc_restart_level;

    DoubleOption opt_garbage_frac;

    IntOption opt_allUipHack;
    DoubleOption opt_vsids_start; // interpolate between VSIDS and VMTF, start value
    DoubleOption opt_vsids_end;   // interpolate between VSIDS and VMTF, end value
    DoubleOption opt_vsids_inc;   // interpolate between VSIDS and VMTF, increase
    IntOption opt_vsids_distance; // interpolate between VSIDS and VMTF, update afte rthis number of conflict
    IntOption opt_var_act_bump_mode; // bump activity of a variable based on the size/LBD of the generated learned clause
    IntOption opt_cls_act_bump_mode; // bump activity of a learned clause based on the size/LBD of the generated learned clause

    BoolOption opt_receiveEquivalences;   // receive equivalenced (is turned automatically on after first succesful inprocessing)
    BoolOption opt_refineReceivedClauses; // apply viviification to received clauses
    BoolOption opt_resendRefinedClauses;  // resend refined clauses
    
    BoolOption opt_pq_order;           // If true, use a priority queue to decide the order in which literals are implied
    // and what antecedent is used.  The priority attempts to choose an antecedent
    // that permits further backtracking in case of a contradiction on this level.               (default false)

    IntOption  opt_probing_step_width; // After how many steps the solver should perform failed literals and detection of necessary assignments. (default 32) If set to '0', no inprocessing is performed.
    IntOption  opt_probing_limit;       //Limit how many varialbes with highest activity should be probed during the inprocessing step.

    IntOption     opt_cir_bump;

    BoolOption   opt_act_based;
    IntOption    opt_lbd_core_thresh;
    DoubleOption opt_l_red_frac;
    IntOption    opt_keep_permanent_size;

    BoolOption opt_updateLearnAct;

    #ifndef NDEBUG
    BoolOption opt_dbg;
    #endif

    BoolOption opt_long_conflict;


// extra
    IntOption opt_act;
    DoubleOption opt_actStart;
    DoubleOption pot_actDec;
    StringOption actFile;
    BoolOption opt_pol;
    StringOption polFile;
    #ifndef NDEBUG
    IntOption opt_printDecisions;
    #endif

    IntOption opt_rMax;
    DoubleOption opt_rMaxInc;

    StringOption printOnSolveTo; // print formula of the solver once ::solve_ is called, and exit afterwards
    
    StringOption search_schedule;
    IntOption    scheduleConflicts;
    IntOption    scheduleDefaultConflicts;
    DoubleOption sscheduleGrowFactor;

    IntOption    sharingType;  // determine when learned clauses are shared

    #ifndef NDEBUG
    BoolOption localLookaheadDebug;
    #endif
    BoolOption localLookAhead;
    BoolOption tb;
    BoolOption opt_laDyn;
    BoolOption opt_laEEl;
    IntOption opt_laEEp;
    IntOption opt_laMaxEvery;
    IntOption opt_laLevel;
    IntOption opt_laEvery;
    IntOption opt_laBound;
    IntOption opt_laTopUnit;

    BoolOption opt_hpushUnit;
    IntOption opt_simplifyInterval;

    IntOption opt_learnDecPrecent; // learn decision clauses instead of others
    IntOption opt_learnDecMinSize; // min size of a learned clause so that its turned into an decision clause
    BoolOption opt_learnDecRER;    // use decision learned clauses for restricted extended resolution?

    BoolOption opt_restrictedExtendedResolution; // perform restricted extended resolution
    BoolOption opt_rer_as_learned;     // add rer clauses as learned clauses?
    IntOption opt_rer_as_replaceAll;   // run through formula/learned clauses and replace all the disjunctions (if not reason/watched ... )
    BoolOption opt_rer_rewriteNew;     // rewrite upcoming learned clauses (only, if full extension, and not as learned clauses!)
    BoolOption opt_rer_full;       // add full rer extension?
    IntOption  opt_rer_minSize;        // minimum size of learned clause to perform rer
    IntOption  opt_rer_maxSize;        // minimum size of learned clause to perform rer
    IntOption  opt_rer_minLBD;         // minimum LBD to perform rer
    IntOption  opt_rer_maxLBD;         // maximum LBD to perform rer
    IntOption  opt_rer_windowSize;     // number of clauses needed, to perform rer
    IntOption  opt_rer_newAct;         // how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean
    BoolOption opt_rer_ite;        // test for ITE pattern?
    #ifndef NDEBUG
    BoolOption opt_rer_debug;      // enable debug output
    #endif
    DoubleOption opt_rer_every;        // perform rer at most every n conflicts
    BoolOption opt_rer_each;       // when a pair is rejected, initialize with the latter clause
    BoolOption opt_rer_extractGates;   // extract binary AND gates from the formula to rewrite the new learned clauses
    DoubleOption opt_rer_addInputAct;      // increase activity of input variables of found gates

    IntOption erRewrite_size;  // size of clauses, so that it is tested whether they can be rewritten with ER
    IntOption erRewrite_lbd;   // LBD of clauses, so that it is tested whether they can be rewritten with ER

    BoolOption opt_interleavedClauseStrengthening; // enable interleaved clause strengthening
    IntOption opt_ics_interval; // run ICS after another N conflicts
    IntOption opt_ics_processLast; // process this number of learned clauses (analyse, reject if quality too bad!)
    BoolOption opt_ics_keepLearnts; // keep the learned clauses that have been produced during the ICS
    BoolOption opt_ics_dynUpdate; // change activity of variables during ICS?
    BoolOption opt_ics_shrinkNew; // shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)
    DoubleOption opt_ics_LBDpercent;  // only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at
    DoubleOption opt_ics_SIZEpercent; // only look at a clause if its size is less than this percent of the average size of the clauses that are looked at
    #ifndef NDEBUG
    BoolOption opt_ics_debug; // enable interleaved clause strengthening debug output
    #endif

    BoolOption opt_use_reverse_minimization; // indicate that reverse minimization is used
    IntOption reverse_minimizing_size;       // size to perform reverse minimization
    IntOption lbLBDreverseClause;            // lbd to perform reverse minimization

    IntOption opt_uhdProbe;  // non, linear, or quadratic analysis
    IntOption opt_uhdRestartReshuffle; // travers the BIG again during every i-th restart 0=off
    IntOption uhle_minimizing_size;     // maximal clause size so that uhle minimization is applied
    IntOption uhle_minimizing_lbd;      // maximal LBD so that uhle minimization is still applied

    IntOption opt_verboseProof;
    BoolOption opt_rupProofOnly;
    IntOption opt_checkProofOnline;

    IntOption opt_verb;
    IntOption opt_inc_verb;

    BoolOption opt_usePPpp;
    BoolOption opt_usePPip;

//
// for incremental solving
//
    IntOption resetActEvery;
    IntOption resetPolEvery;
    IntOption intenseCleaningEvery;
    IntOption incKeepSize;
    IntOption incKeepLBD;
    IntOption opt_reset_counters;
};

}

#endif