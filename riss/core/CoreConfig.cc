/***********************************************************************************[CoreConfig.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "riss/core/CoreConfig.h"
#include "riss/mtl/Sort.h"

namespace Riss
{

static const char* _cat  = "CORE";
static const char* _cr   = "CORE -- RESTART";
static const char* _cred = "CORE -- REDUCE";
static const char* _cm   = "CORE -- MINIMIZE";

CoreConfig::CoreConfig(const std::string& presetOptions)  // add new options here!
    :
    Config(&configOptions, presetOptions),

//
// all the options for the object
//
    opt_solve_stats(_cat, "solve_stats", "print stats about solving process", false, optionListPtr),
    opt_fast_rem(_cat, "rmf", "use fast remove", false, optionListPtr),

    nanosleep("MISC", "nanosleep", "For each conflict sleep this amount of nano seconds", 0, IntRange(0, INT32_MAX), optionListPtr),


    ppOnly("MISC", "ppOnly", "interrupts search after preprocessing", false, optionListPtr),

    #ifndef NDEBUG
    opt_learn_debug(_cat, "learn-debug", "print debug information during learning", false, optionListPtr),
    opt_removal_debug(_cat, "rem-debug",   "print debug information about removal", 0, IntRange(0, 5), optionListPtr),
    #endif

    opt_refineConflict(_cm, "refConflict", "refine conflict clause after solving with assumptions", true, optionListPtr),
    opt_refineConflictReverse(_cm, "revRevC", "refine conflict clause after solving with assumptions", false, optionListPtr),

    opt_K(_cr, "K", "The constant used to force restart", 0.8, DoubleRange(0, false, 1, false), optionListPtr),
    opt_R(_cr, "R", "The constant used to block restart", 1.4, DoubleRange(1, false, 5, false), optionListPtr),
    opt_size_lbd_queue(_cr, "szLBDQueue", "The size of moving average for LBD (restarts)", 50, IntRange(10, INT32_MAX), optionListPtr),
    opt_size_trail_queue(_cr, "szTrailQueue", "The size of moving average for trail (block restarts)", 5000, IntRange(10, INT32_MAX), optionListPtr),
    opt_size_bounded_randomized(_cr, "sbr", "use removal with clause activity based on sbr (randomized)", 12, IntRange(0, INT32_MAX), optionListPtr),

    opt_first_reduce_db(_cred, "firstReduceDB", "The number of conflicts before the first reduce DB", 4000, IntRange(0, INT32_MAX), optionListPtr),
    opt_inc_reduce_db(_cred, "incReduceDB", "Increment for reduce DB", 300, IntRange(0, INT32_MAX), optionListPtr),
    opt_spec_inc_reduce_db(_cred, "specialIncReduceDB", "Special increment for reduce DB", 1000, IntRange(0, INT32_MAX), optionListPtr),
    opt_lb_lbd_frozen_clause(_cred, "minLBDFrozenClause", "Protect clauses if their LBD decrease and is lower than (for one turn)", 30, IntRange(0, INT32_MAX), optionListPtr),
    opt_lbd_ignore_l0(_cred, "lbdIgnL0", "ignore top level literals for LBD calculation", false, optionListPtr),
    opt_lbd_ignore_assumptions(_cred, "lbdIgnLA", "ignore top level literals for LBD calculation", false, optionListPtr),
    opt_update_lbd(_cred, "lbdupd", "update LBD during (0=propagation,1=learning,2=never),", 1, IntRange(0, 2), optionListPtr),
    opt_lbd_inc(_cred, "incLBD", "allow to increment lbd of clauses dynamically", false, optionListPtr),
    opt_quick_reduce(_cred, "quickRed", "check only first two literals for being satisfied", false, optionListPtr),
    opt_keep_worst_ratio(_cred, "keepWorst", "keep this (relative to all learned) number of worst learned clauses during removal", 0, DoubleRange(0, true, 1, true), optionListPtr),

    opt_lb_size_minimzing_clause(_cm, "minSizeMinimizingClause", "The min size required to minimize clause", 30, IntRange(0, INT32_MAX), optionListPtr),
    opt_lb_lbd_minimzing_clause(_cm, "minLBDMinimizingClause", "The min LBD required to minimize clause", 6, IntRange(0, INT32_MAX), optionListPtr),

    opt_var_decay_start("CORE -- SEARCH", "var-decay-b", "The variable activity decay factor start value", 0.95, DoubleRange(0, false, 1, false), optionListPtr),
    opt_var_decay_stop("CORE -- SEARCH", "var-decay-e", "The variable activity decay factor stop value", 0.95, DoubleRange(0, false, 1, false), optionListPtr),
    opt_var_decay_inc("CORE -- SEARCH", "var-decay-i",  "The variable activity decay factor increase ", 0.01, DoubleRange(0, false, 1, false), optionListPtr),
    opt_var_decay_dist("CORE -- SEARCH", "var-decay-d", "Nr. of conflicts for activity decay increase", 5000, IntRange(1, INT32_MAX), optionListPtr),

    opt_clause_decay(_cred, "cla-decay", "The clause activity decay factor", 0.999, DoubleRange(0, false, 1, false), optionListPtr),
    opt_random_var_freq("CORE -- SEARCH", "rnd-freq", "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true), optionListPtr),
    opt_random_seed("CORE -- SEARCH", "rnd-seed", "Used by the random variable selection", 91648253, DoubleRange(0, false, HUGE_VAL, false), optionListPtr),
    opt_ccmin_mode(_cm, "ccmin-mode", "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2), optionListPtr),
    opt_phase_saving("CORE -- SEARCH", "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2), optionListPtr),
    opt_rnd_init_act("INIT", "rnd-init", "Randomize the initial activity", false, optionListPtr),
    opt_init_act("INIT", "init-act", "initialize activities (0=none,1=inc-lin,2=inc-geo,3=dec-lin,4=dec-geo,5=rnd,6=abs(jw))", 0, IntRange(0, 6), optionListPtr),
    opt_init_pol("INIT", "init-pol", "initialize polarity (0=none,1=JW-pol,2=JW-neg,3=MOMS,4=MOMS-neg,5=rnd,6=pos)", 0, IntRange(0, 6), optionListPtr),

    opt_restart_level(_cr, "rlevel", "Choose to which level to jump to: 0=0, 1=ReusedTrail, 2=recursive reused trail", 0, IntRange(0, 2), optionListPtr),
    opt_restarts_type(_cr, "rtype", "Choose type of restart (0=dynamic,1=luby,2=geometric)", 0, IntRange(0, 2), optionListPtr),
    opt_restart_first(_cr, "rfirst", "The base restart interval", 100, IntRange(1, INT32_MAX), optionListPtr, &opt_restarts_type),
    opt_restart_inc(_cr, "rinc", "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false), optionListPtr, &opt_restarts_type),
    opt_inc_restart_level(_cr, "irlevel", "Choose how often restarts beyond assumptions shoud be performed (every X)", 1, IntRange(1, INT32_MAX), optionListPtr, &opt_restarts_type),

    opt_garbage_frac(_cat, "gc-frac", "The fraction of wasted memory allowed before a garbage collection is triggered", 0.20, DoubleRange(0, false, 1, false), optionListPtr),

    opt_allUipHack("CORE -- CONFLIG ANALYSIS", "alluiphack", "learn all unit UIPs at any level", 0, IntRange(0, 2) , optionListPtr),
    opt_vsids_start("CORE -- SEARCH", "vsids-s", "interpolate between VSIDS and VMTF,start value", 1, DoubleRange(0, true, 1, true), optionListPtr),
    opt_vsids_end("CORE -- SEARCH", "vsids-e", "interpolate between VSIDS and VMTF, end value", 1, DoubleRange(0, true, 1, true), optionListPtr),
    opt_vsids_inc("CORE -- SEARCH", "vsids-i", "interpolate between VSIDS and VMTF, inc during update", 1, DoubleRange(0, true, 1, true), optionListPtr),
    opt_vsids_distance("CORE -- SEARCH", "vsids-d", "interpolate between VSIDS and VMTF, numer of conflits until next update", INT32_MAX, IntRange(1, INT32_MAX), optionListPtr),
    opt_var_act_bump_mode("CORE -- SEARCH", "varActB", "bump activity of a variable (0 as usual, 1 relativ to cls size, 2 relative to LBD)", 0, IntRange(0, 2), optionListPtr),
    opt_cls_act_bump_mode("CORE -- SEARCH", "clsActB", "bump activity of a clause (0 as usual, 1 relativ to cls size, 2 relative to LBD, 3 SBR)", 0, IntRange(0, 3), optionListPtr),

    opt_pq_order("Contrasat", "pq-order",    "Use priority queue to decide the order in which literals are implied", false, optionListPtr),

    opt_probing_step_width("MiPiSAT", "prob-step-width", "Perform failed literals and detection of necessary assignments each n times",   0, IntRange(0, INT32_MAX), optionListPtr),
    opt_probing_limit("MiPiSAT", "prob-limit",      "Limit how many variables with highest activity should be probed", 32, IntRange(1, INT32_MAX), optionListPtr),

    opt_cir_bump("cir-minisat", "cir-bump", "Activates CIR with bump ratio for VSIDS score (choose large: 9973)", 0, IntRange(0, INT32_MAX), optionListPtr),

    opt_act_based("999HACK", "act-based",    "use activity for learned clauses", false, optionListPtr),
    opt_lbd_core_thresh("999HACK", "lbd-core-th",  "Saving learnt clause forever if LBD deceeds this threshold", 0, IntRange(0, INT32_MAX), optionListPtr),
    opt_l_red_frac("999HACK", "reduce-frac",  "Remove this quota of learnt clauses when database is reduced", 0.50, DoubleRange(0, false, 1, false), optionListPtr),
    opt_keep_permanent_size("999HACK", "size-core", "Saving learnt clause forever if size deceeds this threshold", 0, IntRange(0, INT32_MAX), optionListPtr),

    opt_updateLearnAct(_cm, "updLearnAct", "UPDATEVARACTIVITY trick (see glucose competition'09 companion paper)", true , optionListPtr),

    #ifndef NDEBUG
    opt_dbg("REASON", "dbg", "debug hack", false , optionListPtr),
    #endif

    opt_long_conflict("REASON", "longConflict", "if a binary conflict is found, check for a longer one!", false, optionListPtr),

// extra
    opt_act("INIT", "actIncMode", "how to inc 0=lin, 1=geo,2=reverse-lin,3=reverse-geo", 0, IntRange(0, 3) , optionListPtr),
    opt_actStart("INIT", "actStart", "highest value for first variable", 1024, DoubleRange(0, false, HUGE_VAL, false), optionListPtr),
    pot_actDec("INIT", "actDec", "decrease per element (sub, or divide)", 1 / 0.95, DoubleRange(0, false, 1, true), optionListPtr),
    actFile("INIT", "actFile", "increase activities of those variables", 0, optionListPtr),
    opt_pol("INIT", "polMode", "invert provided polarities", false , optionListPtr),
    polFile("INIT", "polFile", "use these polarities", 0, optionListPtr),
    #ifndef NDEBUG
    opt_printDecisions("INIT", "printDec", "1=print decisions, 2=print all enqueues, 3=show clauses", 0, IntRange(0, 3)  , optionListPtr),
    #endif

    opt_rMax(_cr, "rMax", "initial max. interval between two restarts (-1 = off)", -1, IntRange(-1, INT32_MAX) , optionListPtr),
    opt_rMaxInc(_cr, "rMaxInc", "increase of the max. restart interval per restart", 1.1, DoubleRange(1, true, HUGE_VAL, false), optionListPtr, &opt_rMax),

    search_schedule("SCHEDULE", "sschedule", "specify configs to be schedules", 0, optionListPtr),
    scheduleConflicts("SCHEDULE", "sscheConflicts", "initial conflicts for schedule", 10000000, IntRange(1, INT32_MAX) , optionListPtr, &search_schedule),
    scheduleDefaultConflicts("SCHEDULE", "sscheDConflicts", "initial conflicts for default", 3000000, IntRange(1, INT32_MAX) , optionListPtr, &search_schedule),
    sscheduleGrowFactor("SCHEDULE", "sscheInc", "increment for conflicts per schedule round", 1.3, DoubleRange(1, true, HUGE_VAL, false), optionListPtr, &search_schedule),
    
    sharingType("COMMUNICATION", "shareTime", "when to share clause (0=new,1=prop,2=analyse)", 0, IntRange(0, 2) , optionListPtr),

    #ifndef NDEBUG
    localLookaheadDebug("CORE -- LOCAL LOOK AHEAD", "laHackOutput", "output info about LA", false, optionListPtr),
    #endif
    localLookAhead("CORE -- LOCAL LOOK AHEAD", "laHack", "enable lookahead on level 0", false, optionListPtr),
    tb("CORE -- LOCAL LOOK AHEAD", "tabu", "do not perform LA, if all considered LA variables are as before", true, optionListPtr, &localLookAhead),
    opt_laDyn("CORE -- LOCAL LOOK AHEAD", "dyn", "dynamically set the frequency based on success", false, optionListPtr, &localLookAhead),
    opt_laEEl("CORE -- LOCAL LOOK AHEAD", "laEEl", "add EE clauses as learnt clauses", false, optionListPtr, &localLookAhead),
    opt_laEEp("CORE -- LOCAL LOOK AHEAD", "laEEp", "add EE clauses, if less than p percent tests failed", 0, IntRange(0, 100), optionListPtr, &localLookAhead),
    opt_laMaxEvery("CORE -- LOCAL LOOK AHEAD", "hlaMax", "maximum bound for frequency", 50, IntRange(0, INT32_MAX) , optionListPtr, &localLookAhead),
    #ifdef DONT_USE_128_BIT
    opt_laLevel("CORE -- LOCAL LOOK AHEAD", "hlaLevel", "level of look ahead", 5, IntRange(1, 5) , optionListPtr, &localLookAhead),
    #else
    opt_laLevel("CORE -- LOCAL LOOK AHEAD", "hlaLevel", "level of look ahead", 5, IntRange(1, 5) , optionListPtr, &localLookAhead),
    #endif
    opt_laEvery("CORE -- LOCAL LOOK AHEAD", "hlaevery", "initial frequency of LA", 1, IntRange(0, INT32_MAX) , optionListPtr, &localLookAhead),
    opt_laBound("CORE -- LOCAL LOOK AHEAD", "hlabound", "max. nr of LAs (-1 == inf)", 4096, IntRange(-1, INT32_MAX) , optionListPtr, &localLookAhead),
    opt_laTopUnit("CORE -- LOCAL LOOK AHEAD", "hlaTop", "allow another LA after learning another nr of top level units (-1 = never)", -1, IntRange(-1, INT32_MAX), optionListPtr, &localLookAhead),

    opt_hpushUnit("MISC", "delay-units", "does not propagate unit clauses until solving is initialized", false, optionListPtr),
    opt_simplifyInterval("MISC", "sInterval", "how often to perform simplifications on level 0", 0, IntRange(0, INT32_MAX) , optionListPtr),

    opt_learnDecPrecent("CORE -- CONFLICT ANALYSIS", "learnDecP",   "if LBD of is > percent of decisionlevel, learn decision Clause (Knuth), -1 = off", -1, IntRange(-1, 100) , optionListPtr),
    opt_learnDecMinSize("CORE -- CONFLICT ANALYSIS", "learnDecMS",  "min size so that decision clauses are learned, -1 = off", 2, IntRange(2, INT32_MAX) , optionListPtr),
    opt_learnDecRER("CORE -- CONFLICT ANALYSIS", "learnDecRER", "consider decision clauses for RER?", false , optionListPtr),

    opt_restrictedExtendedResolution("CORE -- EXTENDED RESOLUTION RER", "rer", "perform restricted extended resolution (along Audemard ea 2010)", false, optionListPtr),
    opt_rer_as_learned("CORE -- EXTENDED RESOLUTION RER", "rer-l", "store extensions as learned clauses", true, optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_as_replaceAll("CORE -- EXTENDED RESOLUTION RER", "rer-r", "replace all disjunctions of the RER extension (only, if not added as learned, and if full - RER adds a conjunction, optionListPtr ), 0=no,1=formula,2=formula+learned", 0, IntRange(0, 2), optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_rewriteNew("CORE -- EXTENDED RESOLUTION RER", "rer-rn", "rewrite new learned clauses, only if full and not added as learned", false, optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_full("CORE -- EXTENDED RESOLUTION RER", "rer-f", "add full rer extension?", true, optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_minSize("CORE -- EXTENDED RESOLUTION RER", "rer-min-size", "minimum size of learned clause to perform rer", 2, IntRange(2, INT32_MAX) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_maxSize("CORE -- EXTENDED RESOLUTION RER", "rer-max-size", "maximum size of learned clause to perform rer", INT32_MAX, IntRange(2, INT32_MAX) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_minLBD("CORE -- EXTENDED RESOLUTION RER", "rer-minLBD", "minimum LBD to perform rer", 1, IntRange(1, INT32_MAX) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_maxLBD("CORE -- EXTENDED RESOLUTION RER", "rer-maxLBD", "maximum LBD to perform rer", INT32_MAX, IntRange(1, INT32_MAX) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_windowSize("CORE -- EXTENDED RESOLUTION RER", "rer-window", "number of clauses to collect before fuse", 2, IntRange(2, INT32_MAX) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_newAct("CORE -- EXTENDED RESOLUTION RER", "rer-new-act", "how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean", 0, IntRange(0, 4) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_ite("CORE -- EXTENDED RESOLUTION RER", "rer-ite", "check for ITE pattern, if AND is not found?", false , optionListPtr, &opt_restrictedExtendedResolution),
    #ifndef NDEBUG
    opt_rer_debug("CORE -- EXTENDED RESOLUTION RER", "rer-d", "debug output for RER", false, optionListPtr, &opt_restrictedExtendedResolution),
    #endif
    opt_rer_every("CORE -- EXTENDED RESOLUTION RER", "rer-freq", "how often rer compared to usual learning", 1, DoubleRange(0, true, 1, true) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_each("CORE -- EXTENDED RESOLUTION RER", "rer-e", "when a pair is rejected, initialize with the new clause", false, optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_extractGates("CORE -- EXTENDED RESOLUTION RER", "rer-g", "extract binary and gates from the formula for RER rewriting", false, optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_addInputAct("CORE -- EXTENDED RESOLUTION RER", "rer-ga", "increase activity for input variables",  0, DoubleRange(0, true, HUGE_VAL, true) , optionListPtr, &opt_rer_extractGates),
    erRewrite_size("CORE -- EXTENDED RESOLUTION RER", "er-size", "rewrite new learned clauses with ER, if size is small enough", 30, IntRange(0, INT32_MAX), optionListPtr, &opt_rer_rewriteNew),
    erRewrite_lbd("CORE -- EXTENDED RESOLUTION RER", "er-lbd" , "rewrite new learned clauses with ER, if lbd is small enough",  6,  IntRange(0, INT32_MAX), optionListPtr, &opt_rer_rewriteNew),

    opt_interleavedClauseStrengthening("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics", "perform interleaved clause strengthening (along Wieringa ea 2013)", false, optionListPtr),
    opt_ics_interval("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_window" , "run ICS after another N conflicts", 5000, IntRange(0, INT32_MAX) , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_processLast("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_processLast" , "process this number of learned clauses (analyse, reject if quality too bad!)", 5050, IntRange(0, INT32_MAX) , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_keepLearnts("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_keepNew" , "keep the learned clauses that have been produced during the ICS", false , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_dynUpdate("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_dyn" , "update variable/clause activities during ICS", false , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_shrinkNew("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_shrinkNew" , "shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)", false , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_LBDpercent("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_relLBD" , "only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at, 1=100%", 1, DoubleRange(0, true, HUGE_VAL, true) , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_SIZEpercent("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_relSIZE" , "only look at a clause if its size is less than this percent of the average size of the clauses that are looked at, 1=100%", 1, DoubleRange(0, true, HUGE_VAL, true) , optionListPtr, &opt_interleavedClauseStrengthening),
    #ifndef NDEBUG
    opt_ics_debug("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics-debug", "debug output for ICS", false, optionListPtr, &opt_interleavedClauseStrengthening),
    #endif

// MINIMIZATION BY REVERSING AND VIVIFICATION
    opt_use_reverse_minimization(_cm, "revMin",  "minimize learned clause by using reverse vivification", false, optionListPtr),
    reverse_minimizing_size(_cm, "revMinSize", "max clause size for revMin" , 12, IntRange(2, INT32_MAX), optionListPtr, &opt_use_reverse_minimization),
    lbLBDreverseClause(_cm, "revMinLBD", "max clause LBD for revMin", 6, IntRange(1, INT32_MAX), optionListPtr, &opt_use_reverse_minimization),

// USING BIG information during search
    opt_uhdProbe(_cm, "sUhdProbe", "perform probing based on learned clauses (off,linear,quadratic,larger)", 0, IntRange(0, 3), optionListPtr),
    opt_uhdRestartReshuffle(_cm, "sUhdPrSh",  "travers the BIG again during every i-th restart 0=off" , 0, IntRange(0, INT32_MAX), optionListPtr, &opt_uhdProbe),
    uhle_minimizing_size(_cm, "sUHLEsize", "maximal clause size for UHLE for learnt clauses (0=off)" , 0, IntRange(0, INT32_MAX), optionListPtr, &opt_uhdProbe),
    uhle_minimizing_lbd(_cm, "sUHLElbd",  "maximal LBD for UHLE for learnt clauses (0=off)", 6, IntRange(0, INT32_MAX), optionListPtr, &opt_uhdProbe),

// DRUP
    opt_verboseProof("CORE -- PROOF", "verb-proof", "also print comments into the proof, 2=print proof also to stderr", 0, IntRange(0, 2) , optionListPtr),
    opt_rupProofOnly("CORE -- PROOF", "rup-only", "do not print delete lines into proof", false, optionListPtr),
    opt_checkProofOnline("CORE -- PROOF", "proof-oft-check", "check proof construction during execution (1=on, higher => more verbose checking)", 0, IntRange(0, 10), optionListPtr),

    opt_verb("MISC", "solververb",   "Verbosity level (0=silent, 1=some, 2=more).", 0, IntRange(0, 2), optionListPtr),
    opt_inc_verb("MISC", "incsverb",     "Verbosity level for MaxSAT (0=silent, 1=some, 2=more).", 0, IntRange(0, 2), optionListPtr),

    opt_usePPpp("MISC", "usePP", "use preprocessor for preprocessing", true, optionListPtr),
    opt_usePPip("MISC", "useIP", "use preprocessor for inprocessing", true, optionListPtr),
//
// for incremental solving
//
    resetActEvery("CORE -- INCREMENTAL", "incResAct", "when incrementally called, reset activity every X calls (0=off)", 0, IntRange(0, INT32_MAX) , optionListPtr),
    resetPolEvery("CORE -- INCREMENTAL", "incResPol", "when incrementally called, reset polarities every X calls (0=off)", 0, IntRange(0, INT32_MAX) , optionListPtr),
    intenseCleaningEvery("CORE -- INCREMENTAL", "incClean",  "when incrementally called, extra clean learnt data base every X calls (0=off)", 0, IntRange(0, INT32_MAX) , optionListPtr),
    incKeepSize("CORE -- INCREMENTAL", "incClSize", "keep size for extra cleaning (any higher is dropped)", 5, IntRange(1, INT32_MAX) , optionListPtr),
    incKeepLBD("CORE -- INCREMENTAL", "incClLBD",  "keep lbd for extra cleaning (any higher is dropped)", 10, IntRange(1, INT32_MAX) , optionListPtr),
    opt_reset_counters("CORE -- INCREMENTAL", "incResCnt", "reset solving counters every X start (0=off)", 100000, IntRange(0, INT32_MAX) , optionListPtr)
{
    if (defaultPreset.size() != 0) { setPreset(defaultPreset); }    // set configuration options immediately
}

} // namespace Riss
