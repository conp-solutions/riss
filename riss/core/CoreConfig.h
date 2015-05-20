/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef CoreConfig_h
#define CoreConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"
#include "riss/utils/Debug.h"

namespace Riss {

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CoreConfig : public Config {
  /** pointer to all options in this object - used for parsing and printing the help! */
  vec<Option*> configOptions;

  
public:
 /** default constructor, which sets up all options in their standard format */
 CoreConfig (const std::string & presetOptions = "");
 
 
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
 IntOption opt_update_lbd; // update LBD during 0=propagation,1=learning,2=never (if during propagation, then during learning is not necessary!)
 BoolOption opt_lbd_inc;	// allow to increase LBD of clauses dynamically?
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

 DoubleOption opt_garbage_frac;
 DoubleOption opt_reduce_frac;        // When clause database is reduced, this fraction of learnt clauses are removed              (default 0.5)

 IntOption opt_allUipHack;
 DoubleOption opt_vsids_start; // interpolate between VSIDS and VMTF, start value
 DoubleOption opt_vsids_end;   // interpolate between VSIDS and VMTF, end value
 DoubleOption opt_vsids_inc;   // interpolate between VSIDS and VMTF, increase
 IntOption opt_vsids_distance; // interpolate between VSIDS and VMTF, update afte rthis number of conflict
 IntOption opt_var_act_bump_mode; // bump activity of a variable based on the size/LBD of the generated learned clause
 IntOption opt_cls_act_bump_mode; // bump activity of a learned clause based on the size/LBD of the generated learned clause
 
 BoolOption opt_pq_order;           // If true, use a priority queue to decide the order in which literals are implied
                                  // and what antecedent is used.  The priority attempts to choose an antecedent
                                  // that permits further backtracking in case of a contradiction on this level.               (default false)
 
 IntOption  opt_probing_step_width; // After how many steps the solver should perform failed literals and detection of necessary assignments. (default 32) If set to '0', no inprocessing is performed.
 IntOption  opt_probing_limit;       //Limit how many varialbes with highest activity should be probed during the inprocessing step.
 
 IntOption     opt_cir_bump;
 
 BoolOption   opt_act_based;
 IntOption    opt_lbd_core_thresh;
 DoubleOption opt_l_red_frac;
 
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
 BoolOption opt_learnDecRER;	// use decision learned clauses for restricted extended resolution?


IntOption opt_uhdProbe;  // non, linear, or quadratic analysis
BoolOption opt_uhdCleanRebuild; // rebuild BIG always before clause database is cleaned next
IntOption opt_uhdRestartReshuffle; // travers the BIG again during every i-th restart 0=off
IntOption uhle_minimizing_size;		// maximal clause size so that uhle minimization is applied
IntOption uhle_minimizing_lbd;		// maximal LBD so that uhle minimization is still applied

IntOption opt_maxSDcalls; // number of substitution calls
IntOption opt_sdLimit; // number of steps for substituteDisjunciton

IntOption opt_maxCBcalls; // number of cegarBVA iterations
IntOption opt_cbLimit; // number of steps for cegarBVA
BoolOption opt_cbLeast; // use least frequent lit, or most frequent lit
BoolOption opt_cbStrict; // cegar reduction has to be strict
 
IntOption opt_verboseProof; 
BoolOption opt_rupProofOnly; 
IntOption opt_checkProofOnline;
 
 IntOption opt_verb;
 
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