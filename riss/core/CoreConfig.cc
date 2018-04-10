/***********************************************************************************[CoreConfig.cc]

Copyright (c) 2012-2013, Norbert Manthey, LGPL v2, see LICENSE

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

// Disable astyle formatting
// *INDENT-OFF*

static const char* _cat  = "CORE";
static const char* _cr   = "CORE -- RESTART";
static const char* _crsw   = "CORE -- RESTART SWITCHING";
static const char* _cs   = "CORE -- SEARCH";
static const char* _cred = "CORE -- REDUCE";
static const char* _cm   = "CORE -- MINIMIZE";
static const char* _misc = "MISC";
static const char* _init = "INIT";

CoreConfig::CoreConfig(const std::string& presetOptions)  // add new options here!
    :
    Config(&configOptions, presetOptions),

    //
    // all the options for the object
    //
    opt_solve_stats  (_cat, "solve_stats", "print stats about solving process #NoAutoT", false,                     optionListPtr),
    opt_fast_rem     (_cat, "rmf", "use fast remove", false,                                                        optionListPtr),

    nanosleep(_misc, "nanosleep", "For each conflict sleep this amount of nano seconds #NoAutoT", 0, IntRange(0, INT32_MAX), optionListPtr),
    ppOnly   (_misc, "ppOnly",    "interrupts search after preprocessing #NoAutoT", false,                                   optionListPtr),

    #ifndef NDEBUG
    opt_learn_debug          (_cat, "learn-debug", "print debug information during learning", false,                        optionListPtr),
    opt_removal_debug        (_cat, "rem-debug",   "print debug information about removal", 0, IntRange(0, 5),              optionListPtr),
    #endif

    opt_refineConflict          (_cm, "refConflict",             "refine conflict clause after solving with assumptions", true,        optionListPtr),
    opt_refineConflictReverse   (_cm, "revRevC",                 "reverse new conflict clause after reverse minimization", false,       optionListPtr),
    opt_prefetch_assumption     (_cm, "prefA",                   "check whether a later assumption is already falsified at each assumption decision (0=off,1=last,2=random,3=middle,4=remaining)", 0, IntRange(0, 4), optionListPtr),
    opt_earlyAssumptionConflict (_cm, "eac",                     "abort search as soon as assumtions conflict", false,       optionListPtr),

    opt_K                       (_cr, "K",                       "The constant used to force restart",                          0.8, DoubleRange(0, false, 1, false), optionListPtr),
    opt_R                       (_cr, "R",                       "The constant used to block restart",                          1.4, DoubleRange(1, false, 5, false), optionListPtr),
    opt_size_lbd_queue          (_cr, "szLBDQueue",              "The size of moving average for LBD (restarts)",                50, IntRange(10, 100000),         optionListPtr),
    opt_size_trail_queue        (_cr, "szTrailQueue",            "The size of moving average for trail (block restarts)",      5000, IntRange(10, 100000),         optionListPtr),
    opt_size_bounded_randomized (_cr, "sbr",                     "use removal with clause activity based on sbr (randomized)",   12, IntRange(0, INT32_MAX),       optionListPtr),

    opt_litPairDecisions        (_cr, "lpd",                     "decisions to be performed based on previous decisions (0=off)",   0, IntRange(0, 4096),     optionListPtr),

    opt_first_reduce_db         (_cred, "firstReduceDB",         "The number of conflicts before the first reduce DB", 4000, IntRange(0, INT32_MAX),                                      optionListPtr),
    opt_inc_reduce_db           (_cred, "incReduceDB",           "Increment for reduce DB", 300, IntRange(0, INT32_MAX),                                                                  optionListPtr),
    opt_spec_inc_reduce_db      (_cred, "specialIncReduceDB",    "Special increment for reduce DB", 1000, IntRange(0, INT32_MAX),                                                         optionListPtr),
    opt_lb_lbd_frozen_clause    (_cred, "minLBDFrozenClause",    "Protect clauses if their LBD decrease and is lower than (for one turn)", 30, IntRange(0, INT32_MAX),                    optionListPtr),
    opt_lbd_ignore_l0           (_cred, "lbdIgnL0",              "ignore top level literals for LBD calculation", false,                                                                  optionListPtr),
    opt_lbd_ignore_assumptions  (_cred, "lbdIgnLA",              "ignore top level literals for LBD calculation", false,                                                                  optionListPtr),
    opt_update_lbd              (_cred, "lbdupd",                "update LBD during (0=propagation,1=learning,2=never),", 1, IntRange(0, 2),                                              optionListPtr),
    opt_lbd_inc                 (_cred, "incLBD",                "allow to increment lbd of clauses dynamically", false,                                                                  optionListPtr),
    opt_rem_inc_lbd             (_cred, "remIncLBD",             "reset delete flag if LBD of a learned clause increases", false,                                                         optionListPtr),
    opt_quick_reduce            (_cred, "quickRed",              "check only first two literals for being satisfied", false,                                                              optionListPtr),
    opt_keep_worst_ratio        (_cred, "keepWorst",             "keep this (relative to all learned) number of worst learned clauses during removal", 0, DoubleRange(0, true, 1, true),  optionListPtr),

    opt_reduceType              (_cred, "remtype",               "remove clauses (0=glucose/dynamic,1=minisat/geometric,2=fixed limit)", 0, IntRange(0, 2),                                                  optionListPtr),
    opt_learnt_size_factor      (_cred, "rem-lsf",               "factor of learnts compared to original formula", (double)1/(double)3,  DoubleRange(0, false, HUGE_VAL, false),          optionListPtr),
    opt_learntsize_inc          (_cred, "rem-lsi",               "learnt size increase", 1.1,  DoubleRange(0, false, HUGE_VAL, false),                                                    optionListPtr),
    opt_learntsize_adjust_start_confl(_cred, "rem-asc",          "first number of conflicts to adjust learnt factors", 100,  IntRange(0, INT32_MAX),                                      optionListPtr),
    opt_learntsize_adjust_inc   (_cred, "rem-asi",               "learnt size increase", 1.1,  DoubleRange(0, false, HUGE_VAL, false),                                                    optionListPtr),
    opt_max_learnts             (_cred, "maxlearnts",            "number of learnt clauses to initialize geometric/static removal", 0, IntRange(0, INT32_MAX),                                                                optionListPtr),

    opt_dpll                    (_cm, "dpll",                    "Perform DPLL instead of CDCL (no restarts, no learning) #NoAutoT", false,                                                        optionListPtr),

    opt_biAsserting             (_cm, "biAsserting",             "Learn bi-asserting clauses, if possible (do not learn asserting clause!)", false,                                       optionListPtr),
    opt_biAssiMaxEvery          (_cm, "biAsFreq",                "The min nr. of clauses between two learned bi-asserting clauses", 4, IntRange(1, INT32_MAX),                            optionListPtr, &opt_biAsserting ),
    opt_lb_size_minimzing_clause(_cm, "minSizeMinimizingClause", "The min size required to minimize clause", 30, IntRange(0, INT32_MAX),                                                  optionListPtr),
    opt_lb_lbd_minimzing_clause (_cm, "minLBDMinimizingClause",  "The min LBD required to minimize clause", 6, IntRange(0, INT32_MAX),                                                    optionListPtr),

    opt_lcm                     (_cm, "lcm",                     "Perform LCM during search (Chu-Min Li, 2017)", true,                                       optionListPtr),
    opt_lcm_style               (_cm, "lcm-style",               "Use vivifaction for learned clauses (1=plain vivi, 2=vivi+analyze, 3=vivi+unionanalyze, 4+reverse, >4 X-4, and-reverse)", 18, IntRange(0, 24), optionListPtr, &opt_lcm),
    opt_lcm_freq                (_cm, "lcm-freq",                "Use LCM after every X reduceDB calls", 2, IntRange(1, INT32_MAX), optionListPtr, &opt_lcm),
    opt_lcm_min_size            (_cm, "lcm-min-size",            "Apply LCM only to clauses that have at least X literals", 1, IntRange(1, INT32_MAX), optionListPtr, &opt_lcm),
    opt_lcm_full                (_cm, "lcm-full",                "at all restarts, always on all learned clauses #NoAutoT", false , optionListPtr, &opt_lcm),
#ifndef NDEBUG
    opt_lcm_dbg                 (_cm, "lcm-dbg",                 "debug LCM computation #NoAutoT", 0, IntRange(0, 5), optionListPtr),
#endif

    opt_var_decay_start         (_cs,   "var-decay-b",           "The variable activity decay factor start value", 0.95, DoubleRange(0, false, 1, false),                                 optionListPtr),
    opt_var_decay_stop          (_cs,   "var-decay-e",           "The variable activity decay factor stop value", 0.95, DoubleRange(0, false, 1, false),                                  optionListPtr),
    opt_var_decay_inc           (_cs,   "var-decay-i",           "The variable activity decay factor increase ", 0.01, DoubleRange(0, false, 1, false),                                   optionListPtr),
    opt_var_decay_dist          (_cs,   "var-decay-d",           "Nr. of conflicts for activity decay increase", 5000, IntRange(1, INT32_MAX),                                            optionListPtr),

    opt_clause_decay            (_cred, "cla-decay",             "The clause activity decay factor", 0.999, DoubleRange(0, false, 1, false),                                              optionListPtr),
    opt_random_var_freq         (_cs,   "rnd-freq",              "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true),   optionListPtr),
    opt_random_seed             (_cs,   "rnd-seed",              "Used by the random variable selection", 91648253, DoubleRange(0, false, HUGE_VAL, false),                               optionListPtr),
    opt_ccmin_mode              (_cm,   "ccmin-mode",            "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2),                                    optionListPtr),
    opt_phase_saving            (_cs,   "phase-saving",          "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2),                                     optionListPtr),
    opt_phase_bit_level         (_cs,   "phase-bit",             "decision level until which the bit phase is used", 0, IntRange(0, INT32_MAX),                                           optionListPtr),
    opt_phase_bit_number        (_cs,   "phase-bitmod",          "mod of bits of the counter to be used to select bits for bit phase", 4, IntRange(1, 64),                                optionListPtr, &opt_phase_bit_level),
    opt_phase_bit_invert        (_cs,   "phase-bitinv",          "invert value of polarity assigned by bit-strategy", false,                                                              optionListPtr, &opt_phase_bit_level),
    opt_rnd_init_act            (_init, "rnd-init",              "Randomize the initial activity", false,                                                                                 optionListPtr),
    opt_init_act                (_init, "init-act",              "initialize activities (0=none,1=inc-lin,2=inc-geo,3=dec-lin,4=dec-geo,5=rnd,6=abs(jw))", 0, IntRange(0, 6),             optionListPtr),
    opt_init_pol                (_init, "init-pol",              "initialize polarity (0=none,1=JW-pol,2=JW-neg,3=MOMS,4=MOMS-neg,5=rnd,6=pos)", 0, IntRange(0, 6),                       optionListPtr),

    opt_restart_level           (_cr,     "rlevel",              "Choose to which level to jump to: 0=0, 1=ReusedTrail, 2=recursive reused trail", 0, IntRange(0, 2),                     optionListPtr),
    opt_restarts_type           (_cr,      "rtype",              "Choose type of restart (0=dynamic,1=luby,2=geometric,3=static,4=none)", 0, IntRange(0, 4),                              optionListPtr),
    opt_allow_restart_blocking  (_cr,   "r-dyn-bl",              "Perform dynamic restarts blocking", true,                                                                               optionListPtr),
    opt_restarts_dyn_ema        (_cr,  "r-dyn-ema",              "Perform dynamic restarts based on EMA", false,                                                                          optionListPtr),
    opt_restart_ema_lbdfast     (_cr,"r-ema-lfast",              "Alpha for fast evolving EMA for interpretation size", 0.03125, DoubleRange(0, true, 1, true),                           optionListPtr, &opt_restarts_dyn_ema),
    opt_restart_ema_lbdslow     (_cr,"r-ema-lslow",              "Alpha for slow evolving EMA for interpretation size", 0.000061035156, DoubleRange(0, true, 1, true),                    optionListPtr, &opt_restarts_dyn_ema),
    opt_restart_ema_trailslow   (_cr,"r-ema-tslow",              "Alpha for slow evolving EMA for clause LBDs", 0.000244140625, DoubleRange(0, true, 1, true),                            optionListPtr, &opt_restarts_dyn_ema),
    opt_restart_first           (_cr,     "rfirst",              "The base restart interval", 100, IntRange(1, INT32_MAX),                                                                optionListPtr, &opt_restarts_type),
    opt_restart_min_noBlock     (_cr, "r-min-noBlock",           "Do not allow restart blocking before this number of conflicts", 10000, IntRange(1, INT32_MAX),                          optionListPtr),
    opt_restart_inc             (_cr,       "rinc",              "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false),                                           optionListPtr, &opt_restarts_type),
    opt_inc_restart_level       (_cr,    "irlevel",              "Choose how often restarts beyond assumptions shoud be performed (every X)", 1, IntRange(1, INT32_MAX),                  optionListPtr, &opt_restarts_type),

    opt_rswitch_isize           (_crsw,    "rsw-int",            "First interval for restart heuristic switching (>0 to activate)",   0, IntRange(0, INT32_MAX),                          optionListPtr), // restart type used when switching restart heuristics
    opt_alternative_rtype       (_crsw,   "rsw-type",            "Type of restart for switching(1=luby,2=geometric,3=static,4=none)", 4, IntRange(1, 4),                                  optionListPtr, &opt_rswitch_isize), // restart type used when switching restart heuristics
    opt_rswitch_interval_inc    (_crsw,   "rsw-iinc",            "Increase of the interval after finishing an interval", 1.1, DoubleRange(1, true, 10, true),                             optionListPtr, &opt_rswitch_isize),
    opt_dynamic_rtype_ratio     (_crsw, "rsw-iratio",            "Percentage of dynamic restarts in switch intervals", 0.6666, DoubleRange(0, true, 1, true),                             optionListPtr, &opt_rswitch_isize),

    opt_garbage_frac            (_cat,   "gc-frac",              "The fraction of wasted memory allowed before a garbage collection is triggered", 0.20, DoubleRange(0, false, 1, false), optionListPtr),

    opt_allUipHack              (_cs, "alluiphack",              "learn all unit UIPs at any level", 0, IntRange(0, 2),                                                                   optionListPtr),
    opt_vsids_start             (_cs,    "vsids-s",              "interpolate between VSIDS and VMTF,start value", 1, DoubleRange(0, true, 1, true),                                      optionListPtr),
    opt_vsids_end               (_cs,    "vsids-e",              "interpolate between VSIDS and VMTF, end value", 1, DoubleRange(0, true, 1, true),                                       optionListPtr),
    opt_vsids_inc               (_cs,    "vsids-i",              "interpolate between VSIDS and VMTF, inc during update", 1, DoubleRange(0, true, 1, true),                               optionListPtr),
    opt_vsids_distance          (_cs,    "vsids-d",              "interpolate between VSIDS and VMTF, numer of conflits until next update", INT32_MAX, IntRange(1, INT32_MAX),            optionListPtr),
    opt_var_act_bump_mode       (_cs,    "varActB",              "bump activity of a variable (0 as usual, 1 relativ to cls size, 2 relative to LBD)", 0, IntRange(0, 2),                 optionListPtr),
    opt_cls_act_bump_mode       (_cs,    "clsActB",              "bump activity of a clause (0 as usual, 1 relativ to cls size, 2 relative to LBD, 3 SBR)", 0, IntRange(0, 3),            optionListPtr),

    opt_receiveData             ("CLAUSE SHARING", "receive",    "receive shared clauses/equivalences #NoAutoT", true,                                                                             optionListPtr),
    sharingType                 ("CLAUSE SHARING", "shareTime",  "when to share clause (0=new,1=prop,2=analyse) #NoAutoT", 1, IntRange(0, 2) ,                                                     optionListPtr),
    opt_receiveEquivalences     ("CLAUSE SHARING", "recEE",      "receive equivalent literal classes #NoAutoT", false,                                                                             optionListPtr),
    opt_refineReceivedClauses   ("CLAUSE SHARING", "refRec",     "refine received clauses (vivification) #NoAutoT", false,                                                                         optionListPtr),
    opt_resendRefinedClauses    ("CLAUSE SHARING", "resRefRec",  "share refined clauses again #NoAutoT", false,                                                                                    optionListPtr),
    opt_sendAll                 ("CLAUSE SHARING", "sendAll",    "ignore sharing limits and sends clause right away #NoAutoT", false,                                                              optionListPtr),
    opt_dynLimit                ("CLAUSE SHARING", "dynLimits",  "use dynamic sharing limits #NoAutoT", false,                                                                                     optionListPtr),
    opt_keepLonger              ("CLAUSE SHARING", "keepLonger", "keep clauses for at least one remove round #NoAutoT", false,                                                                     optionListPtr),
    opt_recLBDfactor            ("CLAUSE SHARING", "recLBDf",    "how to construct LBD of received clause (0=0, pos: relative to size, neg: relative to avg LBD/size ratio #NoAutoT", 0, DoubleRange(-10, true, 1, true), optionListPtr),
    opt_useOriginal             ("CLAUSE SHARING", "independent",  "work on parsed formula (ignore global simplification, sharing currently unsound) #NoAutoT", false,                             optionListPtr),

    opt_pq_order            ("Contrasat",   "pq-order",          "Use priority queue to decide the order in which literals are implied  #NoAutoT", false,                                           optionListPtr),

    opt_probing_step_width  ("MiPiSAT",     "prob-step-width",   "Perform failed literals and detection of necessary assignments each n times",   0, IntRange(0, INT32_MAX),              optionListPtr),
    opt_probing_limit       ("MiPiSAT",     "prob-limit",        "Limit how many variables with highest activity should be probed", 32, IntRange(1, INT32_MAX),                           optionListPtr),

    opt_cir_bump            ("cir-minisat", "cir-bump",          "Activates CIR with bump ratio for VSIDS score (choose large: 9973)", 0, IntRange(0, INT32_MAX),                         optionListPtr),

    opt_act_based           ("999HACK",     "act-based",          "use activity for learned clauses", false,                                                                               optionListPtr),
    opt_avg_size_lbd_ratio  ("999HACK",     "act-lbd-size-ratio", "still use LBD for removal, if stddevLBD * X < stddevSize (if X is negative, invert comparison)", 0, DoubleRange(-10, true, 10, true), optionListPtr, &opt_act_based),
    opt_lbd_core_thresh     ("999HACK",     "lbd-core-th",        "Saving learnt clause forever if LBD deceeds this threshold", 0, IntRange(0, INT32_MAX),                                 optionListPtr),
    opt_l_red_frac          ("999HACK",     "reduce-frac",        "Remove this quota of learnt clauses when database is reduced", 0.50, DoubleRange(0, false, 1, false),                   optionListPtr),
    opt_keep_permanent_size ("999HACK",     "size-core",          "Saving learnt clause forever if size deceeds this threshold", 0, IntRange(0, INT32_MAX),                                optionListPtr),

    opt_updateLearnAct      (_cm,           "updLearnAct",       "UPDATEVARACTIVITY trick (see glucose competition'09 companion paper)", true,                                             optionListPtr),

    #ifndef NDEBUG
    opt_dbg                 ("REASON", "dbg",          "debug hack", false , optionListPtr),
    #endif

    opt_long_conflict       ("REASON", "longConflict", "if a binary conflict is found, check for a longer one!", false, optionListPtr),

    // extra
    opt_act            (_init, "actIncMode", "how to inc 0=lin, 1=geo,2=reverse-lin,3=reverse-geo", 0, IntRange(0, 3),           optionListPtr),
    opt_actStart       (_init, "actStart",   "highest value for first variable", 1024, DoubleRange(0, false, HUGE_VAL, false),   optionListPtr),
    pot_actDec         (_init, "actDec",     "decrease per element (sub, or divide)", 1 / 0.95, DoubleRange(0, false, 10, true), optionListPtr),
    actFile            (_init, "actFile",    "increase activities of those variables", 0,                                        optionListPtr),
    opt_pol            (_init, "polMode",    "invert provided polarities #NoAutoT", false,                                       optionListPtr),
    polFile            (_init, "polFile",    "use these polarities", 0,                                                          optionListPtr),
    #ifndef NDEBUG
    opt_printDecisions (_init, "printDec",   "1=print decisions, 2=print all enqueues, 3=show clauses #NoAutoT", 0, IntRange(0, 3),      optionListPtr),
    opt_ordered_branch (_init, "orderDec",   "always pick decisions bottom up #NoAutoT", false,                                  optionListPtr),
    opt_external_check (_init, "extChecker",  "point to a proofchecking tool that runs X CNF PROOF with $? = 0", 0, optionListPtr),
    #endif

    opt_rMax   (_cr, "rMax",    "initial max. interval between two restarts (-1 = off)", -1, IntRange(-1, INT32_MAX),            optionListPtr),
    opt_rMaxInc(_cr, "rMaxInc", "increase of the max. restart interval per restart", 1.1, DoubleRange(1, true, HUGE_VAL, false), optionListPtr, &opt_rMax),

    printOnSolveTo          ("DEBUG",    "printOnSolve",    "print formula present at call solve to given filename and exit #NoAutoT", 0, optionListPtr),

    search_schedule         ("SCHEDULE", "sschedule",       "specify configs to be schedules", 0,                                                     optionListPtr),
    scheduleConflicts       ("SCHEDULE", "sscheConflicts",  "initial conflicts for schedule", 10000000, IntRange(1, INT32_MAX),                       optionListPtr, &search_schedule),
    scheduleDefaultConflicts("SCHEDULE", "sscheDConflicts", "initial conflicts for default", 3000000, IntRange(1, INT32_MAX),                         optionListPtr, &search_schedule),
    sscheduleGrowFactor     ("SCHEDULE", "sscheInc",        "increment for conflicts per schedule round", 1.3, DoubleRange(1, true, HUGE_VAL, false), optionListPtr, &search_schedule),

    #ifndef NDEBUG
    localLookaheadDebug("CORE -- LOCAL LOOK AHEAD", "laHackOutput", "output info about LA #NoAutoT", false,                                                    optionListPtr),
    #endif
    localLookAhead     ("CORE -- LOCAL LOOK AHEAD", "laHack",       "enable lookahead on level 0", false,                                             optionListPtr),
    tb                 ("CORE -- LOCAL LOOK AHEAD", "tabu",         "do not perform LA, if all considered LA variables are as before", true,          optionListPtr, &localLookAhead),
    opt_laDyn          ("CORE -- LOCAL LOOK AHEAD", "dyn",          "dynamically set the frequency based on success", false,                          optionListPtr, &localLookAhead),
    opt_laEEl          ("CORE -- LOCAL LOOK AHEAD", "laEEl",        "add EE clauses as learnt clauses", false,                                        optionListPtr, &localLookAhead),
    opt_laEEp          ("CORE -- LOCAL LOOK AHEAD", "laEEp",        "add EE clauses, if less than p percent tests failed", 0, IntRange(0, 100),       optionListPtr, &localLookAhead),
    opt_laMaxEvery     ("CORE -- LOCAL LOOK AHEAD", "hlaMax",       "maximum bound for frequency", 50, IntRange(0, INT32_MAX),                        optionListPtr, &localLookAhead),
    #ifdef DONT_USE_128_BIT
    opt_laLevel        ("CORE -- LOCAL LOOK AHEAD", "hlaLevel",     "level of look ahead", 5, IntRange(1, 5),                                         optionListPtr, &localLookAhead),
    #else
    opt_laLevel        ("CORE -- LOCAL LOOK AHEAD", "hlaLevel",     "level of look ahead", 5, IntRange(1, 5),                                         optionListPtr, &localLookAhead),
    #endif
    opt_laEvery        ("CORE -- LOCAL LOOK AHEAD", "hlaevery",     "initial frequency of LA", 1, IntRange(0, INT32_MAX),                             optionListPtr, &localLookAhead),
    opt_laBound        ("CORE -- LOCAL LOOK AHEAD", "hlabound",     "max. nr of LAs (-1 == inf)", 4096, IntRange(-1, INT32_MAX),                      optionListPtr, &localLookAhead),
    opt_laTopUnit      ("CORE -- LOCAL LOOK AHEAD", "hlaTop",       "allow another LA after learning another nr of top level units (-1 = never)", -1, IntRange(-1, INT32_MAX), optionListPtr, &localLookAhead),

    opt_hpushUnit       (_misc, "delay-units", "does not propagate unit clauses until solving is initialized  #NoAutoT", false,        optionListPtr),
    opt_simplifyInterval(_misc, "sInterval",  "how often to perform simplifications on level 0", 0, IntRange(0, INT32_MAX) , optionListPtr),

 opt_otfss ("SEARCH -- OTFSS", "otfss", "perform otfss during conflict analysis", false, optionListPtr ),
 opt_otfssL ("SEARCH -- OTFSS", "otfssL", "otfss for learnt clauses", false, optionListPtr ),
 opt_otfssMaxLBD ("SEARCH -- OTFSS", "otfssMLDB", "max. LBD of learnt clauses that are candidates for otfss", 30, IntRange(2, INT32_MAX) , optionListPtr ),
#ifndef NDEBUG
 debug_otfss ("SEARCH -- OTFSS", "otfss-d", "print debug output", false, optionListPtr ),
#endif

    opt_learnDecPrecent("CORE -- CONFLICT ANALYSIS", "learnDecP",   "if LBD of is > percent of decisionlevel, learn decision Clause (Knuth), -1 = off", -1, IntRange(-1, 100) ,    optionListPtr),
    opt_learnDecMinSize("CORE -- CONFLICT ANALYSIS", "learnDecMS",  "min size so that decision clauses are learned, -1 = off", 2, IntRange(2, INT32_MAX) ,                         optionListPtr),
    opt_learnDecRER    ("CORE -- CONFLICT ANALYSIS", "learnDecRER", "consider decision clauses for RER?", false ,                                                                  optionListPtr),

    opt_restrictedExtendedResolution("CORE -- EXTENDED RESOLUTION RER", "rer",          "perform restricted extended resolution (along Audemard ea 2010)", false,                  optionListPtr),
    opt_rer_as_learned     ("CORE -- EXTENDED RESOLUTION RER", "rer-l",        "store extensions as learned clauses", true,                                               optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_as_replaceAll  ("CORE -- EXTENDED RESOLUTION RER", "rer-r",        "replace all disjunctions of the RER extension (only, if not added as learned, and if full - RER adds a conjunction, optionListPtr ), 0=no,1=formula,2=formula+learned", 0, IntRange(0, 2), optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_rewriteNew     ("CORE -- EXTENDED RESOLUTION RER", "rer-rn",       "rewrite new learned clauses, only if full and not added as learned", false,               optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_full           ("CORE -- EXTENDED RESOLUTION RER", "rer-f",        "add full rer extension?", true,                                                           optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_minSize        ("CORE -- EXTENDED RESOLUTION RER", "rer-min-size", "minimum size of learned clause to perform rer", 2, IntRange(2, INT32_MAX) ,               optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_maxSize        ("CORE -- EXTENDED RESOLUTION RER", "rer-max-size", "maximum size of learned clause to perform rer", INT32_MAX, IntRange(2, INT32_MAX) ,       optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_minLBD         ("CORE -- EXTENDED RESOLUTION RER", "rer-minLBD",   "minimum LBD to perform rer", 1, IntRange(1, INT32_MAX) ,                                  optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_maxLBD         ("CORE -- EXTENDED RESOLUTION RER", "rer-maxLBD",   "maximum LBD to perform rer", INT32_MAX, IntRange(1, INT32_MAX) ,                          optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_windowSize     ("CORE -- EXTENDED RESOLUTION RER", "rer-window",   "number of clauses to collect before fuse", 2, IntRange(2, INT32_MAX) ,                    optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_newAct         ("CORE -- EXTENDED RESOLUTION RER", "rer-new-act",  "how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean", 0, IntRange(0, 4) , optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_ite            ("CORE -- EXTENDED RESOLUTION RER", "rer-ite",      "check for ITE pattern, if AND is not found?", false ,                                     optionListPtr, &opt_restrictedExtendedResolution),
    #ifndef NDEBUG
    opt_rer_debug          ("CORE -- EXTENDED RESOLUTION RER", "rer-d",    "debug output for RER", false,                                                                 optionListPtr, &opt_restrictedExtendedResolution),
    #endif
    opt_rer_every          ("CORE -- EXTENDED RESOLUTION RER", "rer-freq", "how often rer compared to usual learning", 1, DoubleRange(0, true, 1, true) ,                 optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_each           ("CORE -- EXTENDED RESOLUTION RER", "rer-e",    "when a pair is rejected, initialize with the new clause", false,                              optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_extractGates   ("CORE -- EXTENDED RESOLUTION RER", "rer-g",    "extract binary and gates from the formula for RER rewriting", false,                          optionListPtr, &opt_restrictedExtendedResolution),
    opt_rer_addInputAct    ("CORE -- EXTENDED RESOLUTION RER", "rer-ga",   "increase activity for input variables",  0, DoubleRange(0, true, HUGE_VAL, true) ,            optionListPtr, &opt_rer_extractGates),
    erRewrite_size         ("CORE -- EXTENDED RESOLUTION RER", "er-size",  "rewrite new learned clauses with ER, if size is small enough", 30, IntRange(0, INT32_MAX),    optionListPtr, &opt_rer_rewriteNew),
    erRewrite_lbd          ("CORE -- EXTENDED RESOLUTION RER", "er-lbd",   "rewrite new learned clauses with ER, if lbd is small enough",  6,  IntRange(0, INT32_MAX),    optionListPtr, &opt_rer_rewriteNew),

    opt_interleavedClauseStrengthening("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics", "perform interleaved clause strengthening (along Wieringa ea 2013)", false,                                         optionListPtr),
    opt_ics_interval       ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_window" , "run ICS after another N conflicts", 5000, IntRange(0, INT32_MAX) ,                                                    optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_processLast    ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_processLast" , "process this number of learned clauses (analyse, reject if quality too bad!)", 5050, IntRange(0, INT32_MAX) ,    optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_keepLearnts    ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_keepNew" , "keep the learned clauses that have been produced during the ICS", false ,                                            optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_dynUpdate      ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_dyn" , "update variable/clause activities during ICS", false ,                                                                   optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_shrinkNew      ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_shrinkNew" , "shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)", false , optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_LBDpercent     ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_relLBD" , "only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at, 1=100%", 1, DoubleRange(0, true, HUGE_VAL, true) ,        optionListPtr, &opt_interleavedClauseStrengthening),
    opt_ics_SIZEpercent    ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics_relSIZE" , "only look at a clause if its size is less than this percent of the average size of the clauses that are looked at, 1=100%", 1, DoubleRange(0, true, HUGE_VAL, true) , optionListPtr, &opt_interleavedClauseStrengthening),
    #ifndef NDEBUG
    opt_ics_debug          ("CORE -- INTERLEAVED CLAUSE STRENGTHENING", "ics-debug", "debug output for ICS", false, optionListPtr, &opt_interleavedClauseStrengthening),
    #endif

    // MINIMIZATION BY REVERSING AND VIVIFICATION
    opt_use_reverse_minimization(_cm, "revMin",     "minimize learned clause by using reverse vivification", false,                              optionListPtr),
    reverse_minimizing_size     (_cm, "revMinSize", "max clause size for revMin" , 12, IntRange(2, INT32_MAX),                                   optionListPtr, &opt_use_reverse_minimization),
    lbLBDreverseClause          (_cm, "revMinLBD",  "max clause LBD for revMin", 6, IntRange(1, INT32_MAX),                                      optionListPtr, &opt_use_reverse_minimization),
    opt_minimize_max_size       (_cm, "minmaxsize", "maximal learned clause size to apply minimization", 0, IntRange(0, INT32_MAX),              optionListPtr),

    // USING BIG information during search
    opt_uhdProbe                (_cm, "sUhdProbe",  "perform probing based on learned clauses (off,linear,quadratic,larger)", 0, IntRange(0, 3), optionListPtr),
    opt_uhdRestartReshuffle     (_cm, "sUhdPrSh",   "travers the BIG again during every i-th restart 0=off" , 0, IntRange(0, INT32_MAX),         optionListPtr, &opt_uhdProbe),
    uhle_minimizing_size        (_cm, "sUHLEsize",  "maximal clause size for UHLE for learnt clauses (0=off)" , 0, IntRange(0, INT32_MAX),       optionListPtr, &opt_uhdProbe),
    uhle_minimizing_lbd         (_cm, "sUHLElbd",   "maximal LBD for UHLE for learnt clauses (0=off)", 6, IntRange(0, INT32_MAX),                optionListPtr, &opt_uhdProbe),

    // DRUP
    opt_verboseProof    ("CORE -- PROOF", "verb-proof",      "also print comments into the proof, 2=print proof also to stderr #NoAutoT", 0, IntRange(0, 2) ,                  optionListPtr),
    opt_rupProofOnly    ("CORE -- PROOF", "rup-only",        "do not print delete lines into proof #NoAutoT", false,                                                           optionListPtr),
    opt_checkProofOnline("CORE -- PROOF", "proof-oft-check", "check proof construction during execution (1=on, higher => more verbose checking, only if -proof is specified) #NoAutoT", 0, IntRange(0, 10), optionListPtr),

    opt_verb    (_misc, "solververb",   "Verbosity level (0=silent, 1=some, 2=more). #NoAutoT", 0, IntRange(0, 2),                                                             optionListPtr),
    opt_inc_verb(_misc, "incsverb",     "Verbosity level for MaxSAT (0=silent, 1=some, 2=more). #NoAutoT", 0, IntRange(0, 2),                                                  optionListPtr),

    opt_usePPpp(_misc, "usePP",         "use preprocessor for preprocessing #NoAutoT", true,                                                                                   optionListPtr),
    opt_usePPip(_misc, "useIP",         "use preprocessor for inprocessing #NoAutoT", true,                                                                                    optionListPtr),

    //
    // for incremental solving
    //
    opt_savesearch      ("CORE -- INCREMENTAL", "incSaveState", "do not jump back to level 0 after search finished #NoAutoT", false                                                    , optionListPtr),
    opt_assumprestart   ("CORE -- INCREMENTAL", "incRestartA", "do not jump back over assumptions during restart #NoAutoT", true                                                       , optionListPtr),
    resetActEvery       ("CORE -- INCREMENTAL", "incResAct", "when incrementally called, reset activity every X calls (0=off)  #NoAutoT",                    0, IntRange(0, INT32_MAX) , optionListPtr),
    resetPolEvery       ("CORE -- INCREMENTAL", "incResPol", "when incrementally called, reset polarities every X calls (0=off)  #NoAutoT",                  0, IntRange(0, INT32_MAX) , optionListPtr),
    intenseCleaningEvery("CORE -- INCREMENTAL", "incClean",  "when incrementally called, extra clean learnt data base every X calls (0=off)  #NoAutoT",      0, IntRange(0, INT32_MAX) , optionListPtr),
    incKeepSize         ("CORE -- INCREMENTAL", "incClSize", "keep size for extra cleaning (any higher is dropped)  #NoAutoT",                               5, IntRange(1, INT32_MAX) , optionListPtr),
    incKeepLBD          ("CORE -- INCREMENTAL", "incClLBD",  "keep lbd for extra cleaning (any higher is dropped)  #NoAutoT",                               10, IntRange(1, INT32_MAX) , optionListPtr),
    opt_reset_counters  ("CORE -- INCREMENTAL", "incResCnt", "reset solving counters every X start (0=off)  #NoAutoT",                                  100000, IntRange(0, INT32_MAX) , optionListPtr)
{
    if (defaultPreset.size() != 0) { setPreset(defaultPreset); }    // set configuration options immediately
}

// *INDENT-ON*

} // namespace Riss
