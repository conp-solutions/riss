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

#include "utils/Options.h"

namespace Minisat {

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CoreConfig {
 
public:
 /** default constructor, which sets up all options in their standard format */
 CoreConfig ();

 /** parse all options from the command line */
 void parseOptions (int& argc, char** argv, bool strict = false);
 
 /** checks all specified constraints */
 bool checkConfiguration();
 
 /** 
 * List of all used options, public members, can be changed and read directly
 */
 BoolOption opt_solve_stats;
#if defined TOOLVERSION
 const opt_learn_debug = false;
#else
 BoolOption opt_learn_debug;
#endif
 
 DoubleOption opt_K; 
 DoubleOption opt_R; 
 IntOption opt_size_lbd_queue;
 IntOption opt_size_trail_queue;

 IntOption opt_first_reduce_db;
 IntOption opt_inc_reduce_db;
 IntOption opt_spec_inc_reduce_db;
 IntOption opt_lb_lbd_frozen_clause;
 BoolOption opt_lbd_ignore_l0; // do not consider literals that have toplevel assignments for LBD calculation

 IntOption opt_lb_size_minimzing_clause;
 IntOption opt_lb_lbd_minimzing_clause;


 DoubleOption opt_var_decay_start; // start value default: 0.95 glucose 2.3: 0.8
 DoubleOption opt_var_decay_stop;  // stop value  default: 0.95 glucose 2.3: 0.95
 DoubleOption opt_var_decay_inc;   // increase value default: 0 glucose 2.3: 0.01
 IntOption opt_var_decay_dist;     // increase after this number of conflicts: default: never, glucose 2.3: 5000
 
 BoolOption opt_agility_restart_reject; // reject restarts based on search agility?
 DoubleOption opt_agility_rejectLimit; // agility above this value rejects restarts
 DoubleOption opt_agility_decay;   // decay that controls the agility of the search (stuck: ag = ag * decay; moving: ag = ag*decay + (1-decay) Biere08)
 DoubleOption opt_agility_init;	    // initilize agility with this value
 
 DoubleOption opt_clause_decay;
 DoubleOption opt_random_var_freq;
 DoubleOption opt_random_seed;
 IntOption opt_ccmin_mode;
 IntOption opt_phase_saving;
 BoolOption opt_rnd_init_act;
 IntOption opt_init_act;
 IntOption opt_init_pol;


 IntOption opt_restarts_type;
 IntOption opt_restart_first;
 DoubleOption opt_restart_inc;

 DoubleOption opt_garbage_frac;

 IntOption opt_allUipHack;
 BoolOption opt_uipHack;
 IntOption opt_uips;
 DoubleOption opt_vsids_start; // interpolate between VSIDS and VMTF, start value
 DoubleOption opt_vsids_end;   // interpolate between VSIDS and VMTF, end value
 DoubleOption opt_vsids_inc;   // interpolate between VSIDS and VMTF, increase
 IntOption opt_vsids_distance; // interpolate between VSIDS and VMTF, update afte rthis number of conflict
 
 IntOption opt_LHBR;
 IntOption opt_LHBR_max;
 BoolOption opt_LHBR_sub;
 BoolOption opt_printLhbr;

 BoolOption opt_updateLearnAct;

 IntOption opt_hack;
 BoolOption opt_hack_cost;
#if defined TOOLVERSION
 const bool opt_dbg;
#else
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
 IntOption opt_printDecisions;

 IntOption opt_rMax;
 DoubleOption opt_rMaxInc;

 BoolOption dx; 
 BoolOption hk; 
 BoolOption tb; 
 BoolOption opt_laDyn;
 BoolOption opt_laEEl;
 IntOption opt_laEEp;
 IntOption opt_laMaxEvery;
 IntOption opt_laLevel;
 IntOption opt_laEvery;
 IntOption opt_laBound;
 IntOption opt_laTopUnit;

 BoolOption opt_prefetch; 
 BoolOption opt_hpushUnit; 
 IntOption opt_simplifyInterval; 

 BoolOption opt_otfss;
 BoolOption opt_otfssL;
 IntOption opt_otfssMaxLBD;
#if defined TOOLVERSION
 const bool debug_otfss = false;
#else
 BoolOption debug_otfss;
#endif

 IntOption opt_learnDecPrecent;

 // these features become available with version 4
#if defined TOOLVERSION && TOOLVERSION < 400
  const bool opt_extendedClauseLearning; // perform extended clause learning
 const bool opt_ecl_as_learned; // add ecl clauses as learned clauses?
 const int opt_ecl_as_replaceAll; // run through formula/learned clauses and replace all the disjunctions (if not reason/watched ... )
 const bool opt_ecl_full; // add full ecl extension?
 const int opt_ecl_minSize; // minimum size of learned clause to perform ecl
 const int opt_ecl_maxLBD;  // maximum LBD to perform ecl
 const int opt_ecl_newAct;  // how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean
 const bool opt_ecl_debug;
 const double opt_ecl_smallLevel; // add ecl clauses only, if smallest two literals are from this level or below ( negative -> relativ, positive -> absolute! )
 const double opt_ecl_every;  // perform ecl at most every n conflicts
 
 const bool opt_restrictedExtendedResolution; // perform restricted extended resolution
 const bool opt_rer_as_learned; // add rer clauses as learned clauses?
 const int opt_rer_as_replaceAll; // run through formula/learned clauses and replace all the disjunctions (if not reason/watched ... )
 const bool opt_rer_full; // add full rer extension?
 const int  opt_rer_minSize; // minimum size of learned clause to perform rer
 const int  opt_rer_maxSize; // minimum size of learned clause to perform rer
 const int  opt_rer_minLBD;  // minimum LBD to perform rer
 const int  opt_rer_maxLBD;  // maximum LBD to perform rer
 const int  opt_rer_windowSize;  // number of clauses needed, to perform rer
 const int  opt_rer_newAct;  // how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean
 const bool opt_rer_debug; // enable debug output
 const double opt_rer_every;  // perform rer at most every n conflicts
 
 const bool opt_interleavedClauseStrengthening; // enable interleaved clause strengthening
 const int opt_ics_interval; // run ICS after another N conflicts
 const int opt_ics_processLast; // process this number of learned clauses (analyse, reject if quality too bad!)
 const bool opt_ics_keepLearnts; // keep the learned clauses that have been produced during the ICS
 const bool opt_ics_shrinkNew; // shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)
 const double opt_ics_LBDpercent;  // only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at
 const double opt_ics_SIZEpercent; // only look at a clause if its size is less than this percent of the average size of the clauses that are looked at
 const bool opt_ics_debug = false;

#else // version > 400
 BoolOption opt_extendedClauseLearning; // perform extended clause learning
 BoolOption opt_ecl_as_learned; // add ecl clauses as learned clauses?
 IntOption opt_ecl_as_replaceAll; // run through formula/learned clauses and replace all the disjunctions (if not reason/watched ... )
 BoolOption opt_ecl_full; // add full ecl extension?
 IntOption opt_ecl_minSize; // minimum size of learned clause to perform ecl
 IntOption opt_ecl_maxLBD;  // maximum LBD to perform ecl
 IntOption opt_ecl_newAct;  // how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean
#if defined TOOLVERSION
 const bool opt_ecl_debug = false;
#else
 BoolOption opt_ecl_debug; // enable debug output
#endif
 DoubleOption opt_ecl_smallLevel; // add ecl clauses only, if smallest two literals are from this level or below ( negative -> relativ, positive -> absolute! )
 DoubleOption opt_ecl_every;  // perform ecl at most every n conflicts
 
 BoolOption opt_restrictedExtendedResolution; // perform restricted extended resolution
 BoolOption opt_rer_as_learned; // add rer clauses as learned clauses?
 IntOption opt_rer_as_replaceAll; // run through formula/learned clauses and replace all the disjunctions (if not reason/watched ... )
 BoolOption opt_rer_full; // add full rer extension?
 IntOption  opt_rer_minSize; // minimum size of learned clause to perform rer
 IntOption  opt_rer_maxSize; // minimum size of learned clause to perform rer
 IntOption  opt_rer_minLBD;  // minimum LBD to perform rer
 IntOption  opt_rer_maxLBD;  // maximum LBD to perform rer
 IntOption  opt_rer_windowSize;  // number of clauses needed, to perform rer
 IntOption  opt_rer_newAct;  // how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean
#if defined TOOLVERSION
 const bool opt_rer_debug = false;
#else
 BoolOption opt_rer_debug; // enable debug output
#endif
 DoubleOption opt_rer_every;  // perform rer at most every n conflicts
 
 BoolOption opt_interleavedClauseStrengthening; // enable interleaved clause strengthening
 IntOption opt_ics_interval; // run ICS after another N conflicts
 IntOption opt_ics_processLast; // process this number of learned clauses (analyse, reject if quality too bad!)
 BoolOption opt_ics_keepLearnts; // keep the learned clauses that have been produced during the ICS
 BoolOption opt_ics_shrinkNew; // shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)
 DoubleOption opt_ics_LBDpercent;  // only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at
 DoubleOption opt_ics_SIZEpercent; // only look at a clause if its size is less than this percent of the average size of the clauses that are looked at
#if defined TOOLVERSION
 const bool opt_ics_debug = false;
#else
 BoolOption opt_ics_debug; // enable interleaved clause strengthening debug output
#endif
#endif // version < 400
 
IntOption opt_verboseProof; 
BoolOption opt_rupProofOnly; 
 
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