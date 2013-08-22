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
 opt_K (_cr, "K", "The constant used to force restart", 0.8, DoubleRange(0, false, 1, false)), 
 opt_R (_cr, "R", "The constant used to block restart", 1.4, DoubleRange(1, false, 5, false)), 
 opt_size_lbd_queue (_cr, "szLBDQueue", "The size of moving average for LBD (restarts)", 50, IntRange(10, INT32_MAX)),
 opt_size_trail_queue (_cr, "szTrailQueue", "The size of moving average for trail (block restarts)", 5000, IntRange(10, INT32_MAX)),

 opt_first_reduce_db (_cred, "firstReduceDB", "The number of conflicts before the first reduce DB", 4000, IntRange(0, INT32_MAX)),
 opt_inc_reduce_db (_cred, "incReduceDB", "Increment for reduce DB", 300, IntRange(0, INT32_MAX)),
 opt_spec_inc_reduce_db (_cred, "specialIncReduceDB", "Special increment for reduce DB", 1000, IntRange(0, INT32_MAX)),
 opt_lb_lbd_frozen_clause (_cred, "minLBDFrozenClause", "Protect clauses if their LBD decrease and is lower than (for one turn)", 30, IntRange(0, INT32_MAX)),
 opt_lbd_ignore_l0 (_cred, "lbdIgnL0", "ignore top level literals for LBD calculation", false),
 
 opt_lb_size_minimzing_clause (_cm, "minSizeMinimizingClause", "The min size required to minimize clause", 30, IntRange(3, INT32_MAX)),
 opt_lb_lbd_minimzing_clause (_cm, "minLBDMinimizingClause", "The min LBD required to minimize clause", 6, IntRange(3, INT32_MAX)),

 opt_var_decay (_cat, "var-decay", "The variable activity decay factor", 0.95, DoubleRange(0, false, 1, false)),
 opt_clause_decay (_cat, "cla-decay", "The clause activity decay factor", 0.999, DoubleRange(0, false, 1, false)),
 opt_random_var_freq (_cat, "rnd-freq", "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true)),
 opt_random_seed (_cat, "rnd-seed", "Used by the random variable selection", 91648253, DoubleRange(0, false, HUGE_VAL, false)),
 opt_ccmin_mode (_cat, "ccmin-mode", "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2)),
 opt_phase_saving (_cat, "phase-saving","Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2)),
 opt_rnd_init_act (_cat, "rnd-init", "Randomize the initial activity", false),
 opt_init_act ("INIT", "init-act", "initialize activities (0=none,1=inc-lin,2=inc-geo,3=dec-lin,4=dec-geo,5=rnd,6=abs(jw))", 0, IntRange(0, 6)),
 opt_init_pol ("INIT", "init-pol", "initialize polarity (0=none,1=JW-pol,2=JW-neg,3=MOMS,4=MOMS-neg,5=rnd)", 0, IntRange(0, 5)),

 opt_restarts_type (_cat, "rtype", "Choose type of restart (0=dynamic,1=luby,2=geometric)", 00, IntRange(0, 2)),
 opt_restart_first (_cat, "rfirst", "The base restart interval", 100, IntRange(1, INT32_MAX)),
 opt_restart_inc (_cat, "rinc", "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false)),

 opt_garbage_frac (_cat, "gc-frac", "The fraction of wasted memory allowed before a garbage collection is triggered", 0.20, DoubleRange(0, false, HUGE_VAL, false)),

 opt_allUipHack ("MODS", "alluiphack", "learn all unit UIPs at any level", 0, IntRange(0, 2) ),
 opt_uipHack ("MODS", "uiphack", "learn more UIPs at decision level 1", false),
 opt_uips ("MODS", "uiphack-uips", "learn at most X UIPs at decision level 1 (0=all)", 0, IntRange(0, INT32_MAX)),
 opt_vmtf ("MODS", "vmtf", "use the vmtf heuristic", false),
 opt_LHBR ("MODS", "lhbr", "use lhbr (0=no,1=str,2=trans,str,3=new,4=trans,new)", 0, IntRange(0, 4)),
 opt_LHBR_max ("MODS", "lhbr-max", "max nr of newly created lhbr clauses", INT32_MAX, IntRange(0, INT32_MAX)),
 opt_LHBR_sub ("MODS", "lhbr-sub", "check whether new clause subsumes the old clause", false),
 opt_printLhbr ("MODS", "lhbr-print", "print info about lhbr", false),

 opt_updateLearnAct ("MODS", "updLearnAct", "UPDATEVARACTIVITY trick (see glucose competition'09 companion paper)", true ),

 opt_hack ("REASON", "hack", "use hack modifications", 0, IntRange(0, 3) ),
 opt_hack_cost ("REASON", "hack-cost", "use size cost", true ),
 opt_dbg ("REASON", "dbg", "debug hack", false ),

 opt_long_conflict ("REASON", "longConflict", "if a binary conflict is found, check for a longer one!", false),

// extra 
 opt_act ("INIT", "actIncMode", "how to inc 0=lin, 1=geo,2=reverse-lin,3=reverse-geo", 0, IntRange(0, 3) ),
 opt_actStart ("INIT", "actStart", "highest value for first variable", 1024, DoubleRange(0, false, HUGE_VAL, false)),
 pot_actDec ("INIT", "actDec", "decrease per element (sub, or divide)",1/0.95, DoubleRange(0, false, HUGE_VAL, true)),
 actFile ("INIT", "actFile", "increase activities of those variables"),
 opt_pol ("INIT", "polMode", "invert provided polarities", false ),
 polFile ("INIT", "polFile", "use these polarities"),
 opt_printDecisions ("INIT", "printDec", "print decisions", false ),

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
 debug_otfss ("MODS", "otfss-D", "print debug output", false),

 opt_learnDecPrecent ("MODS", "learnDecP", "if LBD of is > percent of decisionlevel, learn decision Clause (Knuth)", 100, IntRange(1, 100) ),

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
