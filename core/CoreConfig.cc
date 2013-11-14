/***********************************************************************************[CoreConfig.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "core/CoreConfig.h"

using namespace Minisat;

static const char* _cat = "CORE";
static const char* _cr = "CORE -- RESTART";
static const char* _cred = "CORE -- REDUCE";
static const char* _cm = "CORE -- MINIMIZE";

CoreConfig::CoreConfig() // add new options here!
:
 opt_solve_stats (_cat, "solve_stats", "print stats about solving process", false),
#if defined TOOLVERSION
 opt_learn_debug(false),
#else
 opt_learn_debug (_cat, "learn-debug", "print debug information during learning", false),
#endif
 

 opt_K (_cr, "K", "The constant used to force restart", 0.8, DoubleRange(0, false, 1, false)), 
 opt_R (_cr, "R", "The constant used to block restart", 1.4, DoubleRange(1, false, 5, false)), 
 opt_size_lbd_queue (_cr, "szLBDQueue", "The size of moving average for LBD (restarts)", 50, IntRange(10, INT32_MAX)),
 opt_size_trail_queue (_cr, "szTrailQueue", "The size of moving average for trail (block restarts)", 5000, IntRange(10, INT32_MAX)),

 opt_first_reduce_db (_cred, "firstReduceDB", "The number of conflicts before the first reduce DB", 4000, IntRange(0, INT32_MAX)),
 opt_inc_reduce_db (_cred, "incReduceDB", "Increment for reduce DB", 300, IntRange(0, INT32_MAX)),
 opt_spec_inc_reduce_db (_cred, "specialIncReduceDB", "Special increment for reduce DB", 1000, IntRange(0, INT32_MAX)),
 opt_lb_lbd_frozen_clause (_cred, "minLBDFrozenClause", "Protect clauses if their LBD decrease and is lower than (for one turn)", 30, IntRange(0, INT32_MAX)),
 opt_lbd_ignore_l0 (_cred, "lbdIgnL0", "ignore top level literals for LBD calculation", false),
 opt_quick_reduce(_cred,"quickRed","check only first two literals for being satisfied",false),
 opt_keep_worst_ratio(_cred,"keeWorst","keep this (relative to all learned) number of worst learned clauses during removal", 0, DoubleRange(0, true, 1, true)), 
 
 opt_lb_size_minimzing_clause (_cm, "minSizeMinimizingClause", "The min size required to minimize clause", 30, IntRange(3, INT32_MAX)),
 opt_lb_lbd_minimzing_clause (_cm, "minLBDMinimizingClause", "The min LBD required to minimize clause", 6, IntRange(3, INT32_MAX)),

 opt_var_decay_start(_cat, "var-decay-b", "The variable activity decay factor start value", 0.95, DoubleRange(0, false, 1, false)),
 opt_var_decay_stop(_cat, "var-decay-e", "The variable activity decay factor stop value", 0.95, DoubleRange(0, false, 1, false)),
 opt_var_decay_inc(_cat, "var-decay-i",  "The variable activity decay factor increase ", 0.01, DoubleRange(0, false, 1, false)),
 opt_var_decay_dist(_cat, "var-decay-d", "Nr. of conflicts for activity decay increase", 5000, IntRange(1, INT32_MAX)),
 
 opt_agility_restart_reject("MODS", "agil-r", "reject restarts based on agility", false),
 opt_agility_rejectLimit("MODS", "agil-limit", "agility above this limit rejects restarts", 0.22, DoubleRange(0, true, 1, true)),
 opt_agility_decay("MODS", "agil-decay", "search agility decay", 0.9999, DoubleRange(0, true, 1, true)),
 opt_agility_init("MODS", "agil-init",   "initial agility", 0.11, DoubleRange(0, true, 1, true)),
 
 opt_clause_decay (_cat, "cla-decay", "The clause activity decay factor", 0.999, DoubleRange(0, false, 1, false)),
 opt_random_var_freq (_cat, "rnd-freq", "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true)),
 opt_random_seed (_cat, "rnd-seed", "Used by the random variable selection", 91648253, DoubleRange(0, false, HUGE_VAL, false)),
 opt_ccmin_mode (_cat, "ccmin-mode", "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2)),
 opt_phase_saving (_cat, "phase-saving","Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2)),
 opt_rnd_init_act (_cat, "rnd-init", "Randomize the initial activity", false),
 opt_init_act ("INIT", "init-act", "initialize activities (0=none,1=inc-lin,2=inc-geo,3=dec-lin,4=dec-geo,5=rnd,6=abs(jw))", 0, IntRange(0, 6)),
 opt_init_pol ("INIT", "init-pol", "initialize polarity (0=none,1=JW-pol,2=JW-neg,3=MOMS,4=MOMS-neg,5=rnd)", 0, IntRange(0, 5)),

 opt_restarts_type (_cat, "rtype", "Choose type of restart (0=dynamic,1=luby,2=geometric)", 0, IntRange(0, 2)),
 opt_restart_first (_cat, "rfirst", "The base restart interval", 100, IntRange(1, INT32_MAX)),
 opt_restart_inc (_cat, "rinc", "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false)),

 opt_garbage_frac (_cat, "gc-frac", "The fraction of wasted memory allowed before a garbage collection is triggered", 0.20, DoubleRange(0, false, HUGE_VAL, false)),

 opt_allUipHack ("MODS", "alluiphack", "learn all unit UIPs at any level", 0, IntRange(0, 2) ),
 opt_uipHack ("MODS", "uiphack", "learn more UIPs at decision level 1", false),
 opt_uips ("MODS", "uiphack-uips", "learn at most X UIPs at decision level 1 (0=all)", 0, IntRange(0, INT32_MAX)),
 opt_vsids_start ("MODS", "vsids-s", "interpolate between VSIDS and VMTF,start value", 1, DoubleRange(0, true, 1, true)),
 opt_vsids_end("MODS", "vsids-e", "interpolate between VSIDS and VMTF, end value", 1, DoubleRange(0, true, 1, true)),
 opt_vsids_inc("MODS", "vsids-i", "interpolate between VSIDS and VMTF, inc during update", 1, DoubleRange(0, true, 1, true)),
 opt_vsids_distance("MODS", "vsids-d", "interpolate between VSIDS and VMTF, numer of conflits until next update", INT32_MAX, IntRange(0, INT32_MAX)),
 opt_var_act_bump_mode("MODS", "varActB", "bump activity of a variable (0 as usual, 1 relativ to cls size, 2 relative to LBD)", 0, IntRange(0, 2)),
 opt_cls_act_bump_mode("MODS", "clsActB", "bump activity of a clause (0 as usual, 1 relativ to cls size, 2 relative to LBD)", 0, IntRange(0, 2)),
 
 opt_LHBR ("MODS", "lhbr", "use lhbr (0=no,1=str,2=trans,str,3=new,4=trans,new)", 0, IntRange(0, 4)),
 opt_LHBR_max ("MODS", "lhbr-max", "max nr of newly created lhbr clauses", INT32_MAX, IntRange(0, INT32_MAX)),
 opt_LHBR_sub ("MODS", "lhbr-sub", "check whether new clause subsumes the old clause", false),
 opt_printLhbr ("MODS", "lhbr-print", "print info about lhbr", false),

 opt_updateLearnAct ("MODS", "updLearnAct", "UPDATEVARACTIVITY trick (see glucose competition'09 companion paper)", true ),

 opt_hack ("REASON", "hack", "use hack modifications", 0, IntRange(0, 3) ),
 opt_hack_cost ("REASON", "hack-cost", "use size cost", true ),
#if defined TOOLVERSION
 opt_dbg(false),
#else
 opt_dbg ("REASON", "dbg", "debug hack", false ),
#endif

 

 opt_long_conflict ("REASON", "longConflict", "if a binary conflict is found, check for a longer one!", false),

// extra 
 opt_act ("INIT", "actIncMode", "how to inc 0=lin, 1=geo,2=reverse-lin,3=reverse-geo", 0, IntRange(0, 3) ),
 opt_actStart ("INIT", "actStart", "highest value for first variable", 1024, DoubleRange(0, false, HUGE_VAL, false)),
 pot_actDec ("INIT", "actDec", "decrease per element (sub, or divide)",1/0.95, DoubleRange(0, false, HUGE_VAL, true)),
 actFile ("INIT", "actFile", "increase activities of those variables"),
 opt_pol ("INIT", "polMode", "invert provided polarities", false ),
 polFile ("INIT", "polFile", "use these polarities"),
 opt_printDecisions ("INIT", "printDec", "1=print decisions, 2=print all enqueues, 3=show clauses", 0, IntRange(0, 3)  ),

 opt_rMax ("MODS", "rMax", "initial max. interval between two restarts (-1 = off)", -1, IntRange(-1, INT32_MAX) ),
 opt_rMaxInc ("MODS", "rMaxInc", "increase of the max. restart interval per restart", 1.1, DoubleRange(1, true, HUGE_VAL, false)),

 dx ("MODS", "laHackOutput","output info about LA", false),
 hk ("MODS", "laHack", "enable lookahead on level 0", false),
 tb ("MODS", "tabu", "do not perform LA, if all considered LA variables are as before", false),
 opt_laDyn ("MODS", "dyn", "dynamically set the frequency based on success", false),
 opt_laEEl ("MODS", "laEEl", "add EE clauses as learnt clauses", true),
 opt_laEEp ("MODS", "laEEp", "add EE clauses, if less than p percent tests failed", 0, IntRange(0, 100)),
 opt_laMaxEvery ("MODS", "hlaMax", "maximum bound for frequency", 50, IntRange(0, INT32_MAX) ),
 opt_laLevel ("MODS", "hlaLevel", "level of look ahead", 5, IntRange(0, 5) ),
 opt_laEvery ("MODS", "hlaevery", "initial frequency of LA", 1, IntRange(0, INT32_MAX) ),
 opt_laBound ("MODS", "hlabound", "max. nr of LAs (-1 == inf)", -1, IntRange(-1, INT32_MAX) ),
 opt_laTopUnit ("MODS", "hlaTop", "allow another LA after learning another nr of top level units (-1 = never)", -1, IntRange(-1, INT32_MAX)),

 opt_prefetch ("MODS", "prefetch", "prefetch watch list, when literal is enqueued", false),
 opt_hpushUnit ("MODS", "delay-units", "does not propagate unit clauses until solving is initialized", false),
 opt_simplifyInterval ("MODS", "sInterval", "how often to perform simplifications on level 0", 0, IntRange(0, INT32_MAX) ),

 opt_otfss ("MODS", "otfss", "perform otfss during conflict analysis", false),
 opt_otfssL ("MODS", "otfssL", "otfss for learnt clauses", false),
 opt_otfssMaxLBD ("MODS", "otfssMLDB", "max. LBD of learnt clauses that are candidates for otfss", 30, IntRange(2, INT32_MAX) ),
#if defined TOOLVERSION

#else
 debug_otfss ("MODS", "otfss-d", "print debug output", false),
#endif

 opt_learnDecPrecent ("MODS", "learnDecP", "if LBD of is > percent of decisionlevel, learn decision Clause (Knuth)", 100, IntRange(1, 100) ),

#if defined TOOLVERSION && TOOLVERSION < 400
  const bool opt_extendedClauseLearning(false),
 const bool opt_ecl_as_learned(false),
 const int opt_ecl_as_replaceAll(0),
 const bool opt_ecl_full(false),
 const int opt_ecl_minSize(0),
 const int opt_ecl_maxLBD(0),
 const int opt_ecl_newAct(0),
 const bool opt_ecl_debug(false),
 const double opt_ecl_smallLevel(0),
 const double opt_ecl_every(0),
 
 const bool opt_restrictedExtendedResolution(false),
 const bool opt_rer_as_learned(false),
 const int opt_rer_as_replaceAll(0),
 const bool opt_rer_full(false),
 const int  opt_rer_minSize(0),
 const int  opt_rer_maxSize(0),
 const int  opt_rer_minLBD(0),
 const int  opt_rer_maxLBD(0),
 const int  opt_rer_windowSize(0),
 const int  opt_rer_newAct(0),
 const bool opt_rer_debug(false),
 const double opt_rer_every(0),
 
 const bool opt_interleavedClauseStrengthening(false),
 const int opt_ics_interval(0),
 const int opt_ics_processLast(0),
 const bool opt_ics_keepLearnts(false),
 const bool opt_ics_shrinkNew(false),
 const double opt_ics_LBDpercent(0),
 const double opt_ics_SIZEpercent(0),
 const bool opt_ics_debug(false),

#else // version > 400 
 opt_extendedClauseLearning("EXTENDED RESOLUTION ECL", "ecl", "perform extended clause learning (along Huang 2010)", false), 
 opt_ecl_as_learned("EXTENDED RESOLUTION ECL", "ecl-l", "add ecl clauses as learned clauses", true),
 opt_ecl_as_replaceAll("EXTENDED RESOLUTION ECL", "ecl-r", "run through formula and replace all disjunctions in the ECL (only if not added as learned) 0=no,1=formula,2=formula+learned", 0, IntRange(0, 2)),
 opt_ecl_full("EXTENDED RESOLUTION ECL", "ecl-f", "add full ecl extension?", true), 
 opt_ecl_minSize ("EXTENDED RESOLUTION ECL", "ecl-min-size", "minimum size of learned clause to perform ecl", 3, IntRange(3, INT32_MAX) ),
 opt_ecl_maxLBD("EXTENDED RESOLUTION ECL", "ecl-maxLBD", "maximum LBD to perform ecl", 4, IntRange(2, INT32_MAX) ), 
 opt_ecl_newAct("EXTENDED RESOLUTION ECL", "ecl-new-act", "how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean", 0, IntRange(0, 4) ),
#if defined TOOLVERSION
 opt_ecl_debug(false);
#else
 opt_ecl_debug("EXTENDED RESOLUTION ECL", "ecl-d", "debug output for ECL", false),
#endif
 opt_ecl_smallLevel("EXTENDED RESOLUTION ECL", "ecl-smL", "ecl only, if smallest lit level is below the given (negative=neg.ratio from bj. level,positive=absolute)", -1, DoubleRange(-1, true, HUGE_VAL, true) ),
 opt_ecl_every("EXTENDED RESOLUTION ECL", "ecl-freq", "how often ecl compared to usual learning", 1, DoubleRange(0, true, 1, true) ),
 
 opt_restrictedExtendedResolution("EXTENDED RESOLUTION RER", "rer", "perform restricted extended resolution (along Audemard ea 2010)", false), 
 opt_rer_as_learned("EXTENDED RESOLUTION RER", "rer-l", "store extensions as learned clauses", true), 
 opt_rer_as_replaceAll("EXTENDED RESOLUTION RER", "rer-r", "replace all disjunctions of the RER extension (only, if not added as learned, and if full - RER adds a conjunction), 0=no,1=formula,2=formula+learned", 0, IntRange(0, 2)), 
 opt_rer_full("EXTENDED RESOLUTION RER", "rer-f", "add full rer extension?", true), 
 opt_rer_minSize ("EXTENDED RESOLUTION RER", "rer-min-size", "minimum size of learned clause to perform rer", 2, IntRange(2, INT32_MAX) ),
 opt_rer_maxSize("EXTENDED RESOLUTION RER", "rer-max-size", "maximum size of learned clause to perform rer", INT32_MAX, IntRange(2, INT32_MAX) ),
 opt_rer_minLBD("EXTENDED RESOLUTION RER", "rer-minLBD", "minimum LBD to perform rer", 1, IntRange(1, INT32_MAX) ), 
 opt_rer_maxLBD("EXTENDED RESOLUTION RER", "rer-maxLBD", "maximum LBD to perform rer", INT32_MAX, IntRange(1, INT32_MAX) ), 
 opt_rer_windowSize("EXTENDED RESOLUTION RER", "rer-window", "number of clauses to collect before fuse", 2, IntRange(2, INT32_MAX) ), 
 opt_rer_newAct("EXTENDED RESOLUTION RER", "rer-new-act", "how to set the new activity: 0=avg, 1=max, 2=min, 3=sum, 4=geo-mean", 0, IntRange(0, 4) ),
#if defined TOOLVERSION
 opt_ecl_debug(false);
#else
 opt_rer_debug("EXTENDED RESOLUTION RER", "rer-d", "debug output for RER", false),
#endif
 opt_rer_every("EXTENDED RESOLUTION RER", "rer-freq", "how often rer compared to usual learning", 1, DoubleRange(0, true, 1, true) ),
 
 opt_interleavedClauseStrengthening("INTERLEAVED CLAUSE STRENGTHENING", "ics", "perform interleaved clause strengthening (along Wieringa ea 2013)", false), 
 opt_ics_interval("INTERLEAVED CLAUSE STRENGTHENING", "ics_window" ,"run ICS after another N conflicts", 5000, IntRange(0, INT32_MAX) ),
 opt_ics_processLast("INTERLEAVED CLAUSE STRENGTHENING", "ics_processLast" ,"process this number of learned clauses (analyse, reject if quality too bad!)", 5050, IntRange(0, INT32_MAX) ),
 opt_ics_keepLearnts("INTERLEAVED CLAUSE STRENGTHENING", "ics_keepNew" ,"keep the learned clauses that have been produced during the ICS", false ),
 opt_ics_shrinkNew("INTERLEAVED CLAUSE STRENGTHENING", "ics_shrinkNew" ,"shrink the kept learned clauses in the very same run?! (makes only sense if the other clauses are kept!)", false ),
 opt_ics_LBDpercent("INTERLEAVED CLAUSE STRENGTHENING", "ics_relLBD" ,"only look at a clause if its LBD is less than this percent of the average of the clauses that are looked at, 1=100%",1, DoubleRange(0, true, HUGE_VAL, true) ), 
 opt_ics_SIZEpercent("INTERLEAVED CLAUSE STRENGTHENING", "ics_relSIZE" ,"only look at a clause if its size is less than this percent of the average size of the clauses that are looked at, 1=100%",1, DoubleRange(0, true, HUGE_VAL, true) ),
#if defined TOOLVERSION
 opt_ics_debug(false);
#else
 opt_ics_debug("INTERLEAVED CLAUSE STRENGTHENING", "ics-debug","debug output for ICS",false),
#endif
#endif
 
 
 opt_verboseProof ("PROOF", "verb-proof", "also print comments into the proof, 2=print proof also to stderr", 1, IntRange(0, 2) ),
 opt_rupProofOnly ("PROOF", "rup-only", "do not print delete lines into proof", false), 
 
 opt_verb ("CORE", "solververb",   "Verbosity level (0=silent, 1=some, 2=more).", 0, IntRange(0, 2)),
 
 opt_usePPpp ("CORE", "usePP", "use preprocessor for preprocessing", true),
 opt_usePPip ("CORE", "useIP", "use preprocessor for inprocessing", true),
//
// for incremental solving
//
 resetActEvery        ("INCREMENTAL", "incResAct", "when incrementally called, reset activity every X calls (0=off)", 0, IntRange(0, INT32_MAX) ),
 resetPolEvery        ("INCREMENTAL", "incResPol", "when incrementally called, reset polarities every X calls (0=off)", 0, IntRange(0, INT32_MAX) ),
 intenseCleaningEvery ("INCREMENTAL", "incClean",  "when incrementally called, extra clean learnt data base every X calls (0=off)", 0, IntRange(0, INT32_MAX) ),
 incKeepSize          ("INCREMENTAL", "incClSize", "keep size for extra cleaning (any higher is dropped)", 5, IntRange(1, INT32_MAX) ),
 incKeepLBD           ("INCREMENTAL", "incClLBD",  "keep lbd for extra cleaning (any higher is dropped)", 10, IntRange(1, INT32_MAX) ),
 opt_reset_counters   ("INCREMENTAL", "incResCnt", "reset solving counters every X start (0=off)", 100000, IntRange(0, INT32_MAX) )
{}

void CoreConfig::parseOptions(int& argc, char** argv, bool strict)
{
 ::parseOptions (argc, argv, strict ); // simply parse all options
}

bool CoreConfig::checkConfiguration()
{
  return true;
}
