/************************************************************************************[CP3Config.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "coprocessor-src/CP3Config.h"

using namespace Coprocessor;

const char* _cat = "COPROCESSOR 3";
const char* _cat2 = "PREPROCESSOR TECHNIQUES";
const char* _cat_bve = "COPROCESSOR 3 - BVE";
const char* _cat_bva = "COPROCESSOR 3 - BVA";
const char* _cat_cce = "COPROCESSOR 3 - CCE";
static const char* _cat_dense = "COPROCESSOR 3 - DENSE";

CP3Config::CP3Config() // add new options here!
:
// options
  opt_unlimited   (_cat, "cp3_limited",    "Limits for preprocessing techniques", true),
  opt_randomized  (_cat, "cp3_randomized", "Steps withing preprocessing techniques are executed in random order", false),
   opt_inprocessInt(_cat, "cp3_inp_cons",   "Perform Inprocessing after at least X conflicts", 20000, IntRange(0, INT32_MAX)),
  opt_enabled     (_cat, "enabled_cp3",    "Use CP3", false),
  opt_inprocess   (_cat, "inprocess",      "Use CP3 for inprocessing", false),
   opt_exit_pp     (_cat, "cp3-exit-pp",    "terminate after preprocessing (1=exit,2=print formula cerr+exit 3=cout+exit)", 0, IntRange(0, 3)),
  opt_randInp     (_cat, "randInp",        "Randomize Inprocessing", true),
  opt_inc_inp     (_cat, "inc-inp",        "increase technique limits per inprocess step", false),

#if defined CP3VERSION && CP3VERSION < 400
        opt_printStats ( false), // do not print stats, if restricted binary is produced
        opt_verbose ( 0),        // do not talk during computation!
#else
        opt_printStats  (_cat, "cp3_stats",      "Print Technique Statistics", false),
          opt_verbose     (_cat, "cp3_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 5)),
#endif

// techniques
  opt_up          (_cat2, "up",            "Use Unit Propagation during preprocessing", false),
  opt_subsimp     (_cat2, "subsimp",       "Use Subsumption during preprocessing", false),
  opt_hte         (_cat2, "hte",           "Use Hidden Tautology Elimination during preprocessing", false),
  opt_bce         (_cat2, "bce",           "Use Blocked Clause Elimination during preprocessing", false),
  opt_ent         (_cat2, "ent",           "Use checking for entailed redundancy during preprocessing", false),
  opt_cce         (_cat2, "cce",           "Use (covered) Clause Elimination during preprocessing", false),
  opt_ee          (_cat2, "ee",            "Use Equivalence Elimination during preprocessing", false),
  opt_bve         (_cat2, "bve",           "Use Bounded Variable Elimination during preprocessing", false),
  opt_bva         (_cat2, "bva",           "Use Bounded Variable Addition during preprocessing", false),
  opt_unhide      (_cat2, "unhide",        "Use Unhiding (UHTE, UHLE based on BIG sampling)", false),
  opt_probe       (_cat2, "probe",         "Use Probing/Clause Vivification", false),
  opt_ternResolve (_cat2, "3resolve",      "Use Ternary Clause Resolution", false),
  opt_addRedBins  (_cat2, "addRed2",       "Use Adding Redundant Binary Clauses", false),
  opt_dense       (_cat2, "dense",         "Remove gaps in variables of the formula", false),
  opt_shuffle     (_cat2, "shuffle",       "Shuffle the formula, before the preprocessor is initialized", false),
  opt_simplify    (_cat2, "simplify",      "Apply easy simplifications to the formula", true),
  opt_symm        (_cat2, "symm",          "Do local symmetry breaking", false),
  opt_FM          (_cat2, "fm",            "Use the Fourier Motzkin transformation", false),


  opt_ptechs (_cat2, "cp3_ptechs", "techniques for preprocessing"),
  opt_itechs (_cat2, "cp3_itechs", "techniques for inprocessing"),

// use 2sat and sls only for high versions!
#if defined CP3VERSION && CP3VERSION < 301
  opt_threads ( 0),
  opt_sls ( false),       
  opt_sls_phase ( false),    
  opt_sls_flips ( 8000000),
  opt_xor ( false),    
  opt_rew ( false),    
  opt_twosat ( false),
  opt_twosat_init(false),
   opt_ts_phase (false),    
#else
   opt_threads     (_cat, "cp3_threads",    "Number of extra threads that should be used for preprocessing", 0, IntRange(0, INT32_MAX)),
  opt_sls         (_cat2, "sls",           "Use Simple Walksat algorithm to test whether formula is satisfiable quickly", false),
  opt_sls_phase   (_cat2, "sls-phase",     "Use current interpretation of SLS as phase", false),
   opt_sls_flips   (_cat2, "sls-flips",     "Perform given number of SLS flips", 8000000, IntRange(-1, INT32_MAX)),
  opt_xor         (_cat2, "xor",           "Reason with XOR constraints", false),
  opt_rew         (_cat2, "rew",           "Rewrite AMO constraints", false),
  opt_twosat      (_cat2, "2sat",          "2SAT algorithm to check satisfiability of binary clauses", false),
  opt_twosat_init (_cat2, "2sat1",         "2SAT before all other algorithms to find units", false),
  opt_ts_phase    (_cat2, "2sat-phase",    "use 2SAT model as initial phase for SAT solver", false),
#endif

#if defined CP3VERSION // debug only, if no version is given!
  opt_debug ( false),       
  opt_check ( false),
   opt_log (0),
  printAfter ( 0),
#else
  opt_debug       (_cat, "cp3-debug",   "do more debugging", false),
  opt_check       (_cat, "cp3-check",   "check solver state before returning control to solver", false),
   opt_log         (_cat,  "cp3-log",    "Output log messages until given level", 0, IntRange(0, 3)),
  printAfter    (_cat,  "cp3-print",  "print intermediate formula after given technique"),
#endif
  
//
// parameters BVE
//

#if defined CP3VERSION && CP3VERSION < 302
 opt_par_bve(1),
 opt_bve_verbose(0),
#else
  opt_par_bve         (_cat_bve, "cp3_par_bve",    "Parallel BVE: 0 never, 1 heur., 2 always", 1,IntRange(0,2)),
   opt_bve_verbose     (_cat_bve, "cp3_bve_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3)),
#endif
 
   opt_bve_limit       (_cat_bve, "cp3_bve_limit", "perform at most this many clause derefferences", 25000000, IntRange(-1, INT32_MAX)),
   opt_learnt_growth   (_cat_bve, "cp3_bve_learnt_growth", "Keep C (x) D, where C or D is learnt, if |C (x) D| <= max(|C|,|D|) + N", 0, IntRange(-1, INT32_MAX)),
   opt_resolve_learnts (_cat_bve, "cp3_bve_resolve_learnts", "Resolve learnt clauses: 0: off, 1: original with learnts, 2: 1 and learnts with learnts", 0, IntRange(0,2)),
  opt_unlimited_bve   (_cat_bve, "bve_unlimited",  "perform bve test for Var v, if there are more than 10 + 10 or 15 + 5 Clauses containing v", false),
  opt_bve_strength    (_cat_bve, "bve_strength",  "do strengthening during bve", true),
   opt_bve_lits        (_cat_bve, "bve_red_lits",  "0=reduce number of literals, 1=reduce number of clauses,2=reduce any of the two,3 reduce both", 0, IntRange(0,3)),
  opt_bve_findGate    (_cat_bve, "bve_gates",  "try to find variable AND gate definition before elimination", true),
  opt_force_gates     (_cat_bve, "bve_force_gates", "Force gate search (slower, but probably more eliminations and blockeds are found)", false),
 // pick order of eliminations
   opt_bve_heap        (_cat_bve, "cp3_bve_heap"   ,  "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 0, IntRange(0,10)),
 // increasing eliminations
   opt_bve_grow        (_cat_bve, "bve_cgrow"  , "number of additional clauses per elimination", 0, IntRange(-INT32_MAX,INT32_MAX)),
   opt_bve_growTotal   (_cat_bve, "bve_cgrow_t", "total number of additional clauses", INT32_MAX, IntRange(0,INT32_MAX)),
  opt_totalGrow       (_cat_bve, "bve_totalG" , "Keep track of total size of formula when allowing increasing eliminations", false),
  
  opt_bve_bc          (_cat_bve, "bve_BCElim",    "Eliminate Blocked Clauses", true),
  heap_updates         (_cat_bve, "bve_heap_updates",    "Always update variable heap if clauses / literals are added or removed, 2 add variables, if not in heap", 1, IntRange(0,2)),
  opt_bce_only        (_cat_bve, "bce_only",    "Only remove blocked clauses but do not resolve variables.", false),
  opt_print_progress  (_cat_bve, "bve_progress", "Print bve progress stats.", false),
  opt_bveInpStepInc      (_cat_bve, "cp3_bve_inpInc","increase for steps per inprocess call", 5000000, IntRange(0, INT32_MAX)),

#if defined CP3VERSION && CP3VERSION < 302
par_bve_threshold(0),
postpone_locked_neighbors (1),
opt_minimal_updates (false),
#else
par_bve_threshold (_cat_bve, "par_bve_th", "Threshold for use of BVE-Worker", 10000, IntRange(0,INT32_MAX)),
postpone_locked_neighbors (_cat_bve, "postp_lockd_neighb", "Postpone Elimination-Check if more neighbors are locked", 3, IntRange(0,INT32_MAX)),
opt_minimal_updates       (_cat_bve, "par_bve_min_upd", "Omit LitOcc and Heap updates to reduce locking", false),
#endif

//
// BVA
//
opt_bva_push             (_cat_bva, "cp3_bva_push",    "push variables back to queue (0=none,1=original,2=all)", 2, IntRange(0, 2)),
opt_bva_VarLimit         (_cat_bva, "cp3_bva_Vlimit",  "use BVA only, if number of variables is below threshold", 3000000, IntRange(-1, INT32_MAX)),
opt_bva_Alimit           (_cat_bva, "cp3_bva_limit",   "number of steps allowed for AND-BVA", 1200000, IntRange(0, INT32_MAX)),
opt_Abva                 (_cat_bva, "cp3_Abva",        "perform AND-bva", true),
opt_bvaInpStepInc        (_cat_bva, "cp3_bva_incInp",  "increases of number of steps per inprocessing", 80000, IntRange(0, INT32_MAX)),
opt_Abva_heap            (_cat_bva, "cp3_Abva_heap",   "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 1, IntRange(0,10)),

opt_bvaComplement        (_cat_bva, "cp3_bva_compl",   "treat complementary literals special", true),
opt_bvaRemoveDubplicates (_cat_bva, "cp3_bva_dupli",   "remove duplicate clauses", true),
opt_bvaSubstituteOr      (_cat_bva, "cp3_bva_subOr",   "try to also substitus disjunctions", false),

#if defined CP3VERSION  
bva_debug (0),
#else
bva_debug (_cat_bva, "bva-debug",       "Debug Output of BVA", 0, IntRange(0, 4)),
#endif

#if defined CP3VERSION  && CP3VERSION < 350
opt_bvaAnalysis (false),
opt_Xbva (0),
opt_Ibva (0),
opt_bvaAnalysisDebug (0),
opt_bva_Xlimit (100000000),
opt_bva_Ilimit (100000000),
opt_Xbva_heap (1),
opt_Ibva_heap (1),
#else
opt_bvaAnalysisDebug     (_cat_bva, "cp3_bva_ad",      "experimental analysis", 0, IntRange(0, 4)),
opt_bva_Xlimit           (_cat_bva, "cp3_bva_Xlimit",   "number of steps allowed for XOR-BVA", 100000000, IntRange(0, INT32_MAX)),
opt_bva_Ilimit           (_cat_bva, "cp3_bva_Ilimit",   "number of steps allowed for ITE-BVA", 100000000, IntRange(0, INT32_MAX)),
opt_Xbva_heap            (_cat_bva, "cp3_Xbva_heap",   "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 1, IntRange(0,10)),
opt_Ibva_heap            (_cat_bva, "cp3_Ibva_heap",   "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 1, IntRange(0,10)),
opt_Xbva                 (_cat_bva, "cp3_Xbva",       "perform XOR-bva (1=half gates,2=full gates)", 0, IntRange(0, 2)),
opt_Ibva                 (_cat_bva, "cp3_Ibva",       "perform ITE-bva (1=half gates,2=full gates)", 0, IntRange(0, 2)),
#endif  
//
// CCE
//
  opt_cceSteps         (_cat_cce, "cp3_cce_steps", "Number of steps that are allowed per iteration", 2000000, IntRange(-1, INT32_MAX)),
  opt_ccelevel         (_cat_cce, "cp3_cce_level", "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE", 3, IntRange(0, 3)),
  opt_ccePercent    (_cat_cce, "cp3_cce_sizeP", "percent of max. clause size for clause elimination (excluding)", 40, IntRange(0,100)),
#if defined CP3VERSION  
 cce_debug_out (0),
#else
  cce_debug_out (_cat_cce, "cce-debug", "debug output for clause elimination",0, IntRange(0,4) ),
#endif
   opt_cceInpStepInc      (_cat_cce, "cp3_cce_inpInc","increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX)),
   
//
// Dense
//
#if defined CP3VERSION  
dense_debug_out (false),
#else
dense_debug_out (_cat_dense, "cp3_dense_debug", "print debug output to screen",false),
#endif
opt_dense_fragmentation  (_cat_dense, "cp3_dense_frag",   "Perform densing, if fragmentation is higher than (percent)", 0, IntRange(0, 100))
   
{}

void CP3Config::parseOptions(int& argc, char** argv, bool strict)
{
 ::parseOptions (argc, argv, strict ); // simply parse all options
}

bool CP3Config::checkConfiguration()
{
  return true;
}
