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
const char* _cat_bce = "COPROCESSOR 3 - BCE";
const char* _cat_cce = "COPROCESSOR 3 - CCE";
const char* _cat_dense = "COPROCESSOR 3 - DENSE";
const char* _cat_entailed = "COPROCESSOR 3 - ENTAILED";
const char* _cat_ee = "COPROCESSOR 3 - EQUIVALENCE ELIMINATION";
const char* _cat_fm = "COPROCESSOR 3 - FOURIERMOTZKIN";
const char* _cat_hte = "COPROCESSOR 3 - HTE";
const char* _cat_pr = "COPROCESSOR 3 - PROBING";
const char* _cat_up = "COPROCESSOR 3 - UP";
const char* _cat_res = "COPROCESSOR 3 - RES";
const char* _cat_rew = "COPROCESSOR 3 - REWRITE";
const char* _cat_shuffle = "COPROCESSOR 3 - SHUFFLE";
const char* _cat_sls = "COPROCESSOR 3 - SLS";
const char* _cat_sub = "COPROCESSOR 3 - SUBSUMPTION";
const char* _cat_sym = "COPROCESSOR 3 - SYMMETRY";
const char* _cat_twosat = "COPROCESSOR 3 - TWOSAT";
const char* _cat_uhd = "COPROCESSOR 3 - UNHIDE";
const char* _cat_xor = "COPROCESSOR 3 - XOR";

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

#if defined TOOLVERSION && TOOLVERSION < 400
        opt_printStats ( true ), // do not print stats, if restricted binary is produced
        opt_verbose ( 0),        // do not talk during computation!
#else
        opt_printStats  (_cat, "cp3_stats",      "Print Technique Statistics", false),
          opt_verbose     (_cat, "cp3_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 5)),
#endif

// techniques
  opt_up          (_cat2, "up",            "Use Unit Propagation during preprocessing", false),
  opt_subsimp     (_cat2, "subsimp",       "Use Subsumption during preprocessing", false),
  opt_hte         (_cat2, "hte",           "Use Hidden Tautology Elimination during preprocessing", false),
#if defined TOOLVERSION && TOOLVERSION < 355
  opt_bce ( false ),
#else
  opt_bce         (_cat2, "bce",           "Use Blocked Clause Elimination during preprocessing", false),
#endif
#if defined TOOLVERSION && TOOLVERSION < 360
  opt_ent ( false ),
#else
  opt_ent         (_cat2, "ent",           "Use checking for entailed redundancy during preprocessing", false),
#endif
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
#if defined TOOLVERSION && TOOLVERSION < 301
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

#if defined TOOLVERSION // debug only, if no version is given!
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

#if defined TOOLVERSION && TOOLVERSION < 302
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

#if defined TOOLVERSION && TOOLVERSION < 302
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

#if defined TOOLVERSION  
bva_debug (0),
#else
bva_debug (_cat_bva, "bva-debug",       "Debug Output of BVA", 0, IntRange(0, 4)),
#endif

#if defined TOOLVERSION  && TOOLVERSION < 350
opt_bvaAnalysis (false),
opt_Xbva (false),
opt_Ibva (false),
opt_bvaAnalysisDebug (0),
opt_bva_Xlimit (100000000),
opt_bva_Ilimit (100000000),
opt_Xbva_heap (1),
opt_Ibva_heap (1),
#else
 #if defined TOOLVERSION
 opt_bvaAnalysisDebug (0),
#else
 opt_bvaAnalysisDebug     (_cat_bva, "cp3_bva_ad",      "experimental analysis", 0, IntRange(0, 4)),
#endif
opt_bva_Xlimit           (_cat_bva, "cp3_bva_Xlimit",   "number of steps allowed for XOR-BVA", 100000000, IntRange(0, INT32_MAX)),
opt_bva_Ilimit           (_cat_bva, "cp3_bva_Ilimit",   "number of steps allowed for ITE-BVA", 100000000, IntRange(0, INT32_MAX)),
opt_Xbva_heap            (_cat_bva, "cp3_Xbva_heap",   "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 1, IntRange(0,10)),
opt_Ibva_heap            (_cat_bva, "cp3_Ibva_heap",   "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 1, IntRange(0,10)),
opt_Xbva                 (_cat_bva, "cp3_Xbva",       "perform XOR-bva (1=half gates,2=full gates)", 0, IntRange(0, 2)),
opt_Ibva                 (_cat_bva, "cp3_Ibva",       "perform ITE-bva (1=half gates,2=full gates)", 0, IntRange(0, 2)),
#endif  
//
// BCE
//
orderComplements (_cat_bce,"bce-compl", "test literals for BCE based on the number of occurrences of the complementary literal", true) ,
bceBinary (_cat_bce,"bce-bin", "allow to remove binary clauses during BCE", false) ,
bceLimit (_cat_bce,"bce-limit", "number of pairwise clause comparisons before interrupting BCE", 100000000, IntRange(0, INT32_MAX) ),
opt_bce_bce(_cat_bce,"bce-bce", "actually perform BCE", true) ,
opt_bce_cle(_cat_bce,"bce-cle", "perform covered literal elimination (CLE)", false),
opt_bce_cla(_cat_bce,"bce-cla", "perform covered literal addition (CLA)", false),
opt_bce_cle_conservative(_cat_bce,"bce-cle-cons", "conservative cle if taut. resolvents are present", false),
opt_bceInpStepInc (_cat_bce,"bce-incInp", "number of steps given to BCE for another inprocessign round", 10000, IntRange(0, INT32_MAX) ),
claStepSize(_cat_bce,"bce-claStep", "number of extension literals per step so that literals are removed randomly", 4, IntRange(1, INT32_MAX) ), 
claStepMax(_cat_bce,"bce-claMax", "number of extension literals per step so that literals are removed randomly", 2, IntRange(1, INT32_MAX) ), 
#if defined TOOLVERSION
opt_bce_verbose (0),
opt_cle_debug(false),
#else
opt_bce_verbose (_cat_bce, "bce-verbose", "be verbose during BCE", 0, IntRange(0, 3)) ,
opt_bce_debug (_cat_bce, "bce-debug", "output debug info during BCE", false) ,
#endif

//
// CCE
//
  opt_cceSteps         (_cat_cce, "cp3_cce_steps", "Number of steps that are allowed per iteration", 2000000, IntRange(-1, INT32_MAX)),
  opt_ccelevel         (_cat_cce, "cp3_cce_level", "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE", 3, IntRange(0, 3)),
  opt_ccePercent    (_cat_cce, "cp3_cce_sizeP", "percent of max. clause size for clause elimination (excluding)", 40, IntRange(0,100)),
#if defined TOOLVERSION  
 cce_debug_out (0),
#else
  cce_debug_out (_cat_cce, "cce-debug", "debug output for clause elimination",0, IntRange(0,4) ),
#endif
   opt_cceInpStepInc      (_cat_cce, "cp3_cce_inpInc","increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX)),
   
//
// Dense
//
#if defined TOOLVERSION  
dense_debug_out (false),
#else
dense_debug_out (_cat_dense, "cp3_dense_debug", "print debug output to screen",false),
#endif
opt_dense_fragmentation  (_cat_dense, "cp3_dense_frag", "Perform densing, if fragmentation is higher than (percent)", 0, IntRange(0, 100)),
opt_dense_store_forward  (_cat_dense, "cp3_dense_forw", "store forward mapping",false),
opt_dense_keep_assigned  (_cat_dense, "cp3_keep_set",   "keep already assigned literals",false),
//
// Entailed
//
#if defined TOOLVERSION && TOOLVERSION < 360
opt_entailed_minClsSize (INT32_MAX),
#else
opt_entailed_minClsSize  (_cat_entailed, "ent-min",    "minimum clause size that is tested", 2, IntRange(2, INT32_MAX)),
#endif
#if defined TOOLVERSION 
entailed_debug(0),
#else
entailed_debug(_cat_entailed, "ent-debug",       "Debug Output for ENT reasoning", 0, IntRange(0, 5)),
#endif

//
// Equivalence
//
#if defined TOOLVERSION  && TOOLVERSION < 350
opt_ee_level            ( 0),
opt_ee_gate_limit       ( 0),
opt_ee_circuit_iters    ( 2),
opt_ee_eagerEquivalence (false),
opt_eeGateBigFirst   (false),
opt_ee_aagFile (0),
#else
opt_ee_level            (_cat_ee, "cp3_ee_level",    "EE on BIG, gate probing, structural hashing", 0, IntRange(0, 3)),
opt_ee_gate_limit       (_cat_ee, "cp3_ee_glimit",   "step limit for structural hashing", INT32_MAX, IntRange(0, INT32_MAX)),
opt_ee_circuit_iters    (_cat_ee, "cp3_ee_cIter",    "max. EE iterations for circuit (-1 == inf)", 2, IntRange(-1, INT32_MAX)),
opt_ee_eagerEquivalence (_cat_ee, "cp3_eagerGates",  "do handle gates eagerly", true),
opt_eeGateBigFirst   (_cat_ee, "cp3_BigThenGate", "detect binary equivalences before going for gates", true),
opt_ee_aagFile            (_cat_ee, "ee_aag", "write final circuit to this file"),
#endif
#if defined TOOLVERSION  
ee_debug_out (0),
#else
ee_debug_out            (_cat_ee, "ee_debug", "print debug output to screen", 0, IntRange(0, 3)),
#endif
opt_eeSub            (_cat_ee, "ee_sub",          "do subsumption/strengthening during applying equivalent literals?", false),
opt_eeFullReset      (_cat_ee, "ee_reset",        "after Subs or Up, do full reset?", false),
opt_ee_limit         (_cat_ee, "cp3_ee_limit",    "step limit for detecting equivalent literals", 1000000, IntRange(0, INT32_MAX)),
opt_ee_inpStepInc       (_cat_ee, "cp3_ee_inpInc",   "increase for steps per inprocess call", 200000, IntRange(0, INT32_MAX)),
opt_ee_bigIters         (_cat_ee, "cp3_ee_bIter",    "max. iteration to perform EE search on BIG", 3, IntRange(0, INT32_MAX)),
opt_ee_iterative        (_cat_ee, "cp3_ee_it",       "use the iterative BIG-EE algorithm", false),
opt_EE_checkNewSub   (_cat_ee, "cp3_ee_subNew",   "check for new subsumptions immediately when adding new clauses", false),
opt_ee_eager_frozen     (_cat_ee, "ee_freeze_eager", "exclude frozen variables eagerly from found equivalences", false),

//
// Fourier Motzkin
//
opt_fmLimit        (_cat_fm, "cp3_fm_limit"  ,"number of steps allowed for FM", 12000000, IntRange(0, INT32_MAX)),
opt_fmGrow         (_cat_fm, "cp3_fm_grow"   ,"max. grow of number of constraints per step", 40, IntRange(0, INT32_MAX)),
opt_fmGrowT        (_cat_fm, "cp3_fm_growT"  ,"total grow of number of constraints", 100000, IntRange(0, INT32_MAX)),
opt_atMostTwo      (_cat_fm, "cp3_fm_amt"     ,"extract at-most-two", false),
opt_findUnit       (_cat_fm, "cp3_fm_unit"    ,"check for units first", true),
opt_merge          (_cat_fm, "cp3_fm_merge"   ,"perform AMO merge", true),
opt_duplicates     (_cat_fm, "cp3_fm_dups"    ,"avoid finding the same AMO multiple times", true),
opt_cutOff         (_cat_fm, "cp3_fm_cut"     ,"avoid eliminating too expensive variables (>10,10 or >5,15)", true),
opt_newAmo          (_cat_fm, "cp3_fm_newAmo"  ,"encode the newly produced AMOs (with pairwise encoding) 0=no,1=yes,2=try to avoid redundant clauses",  2, IntRange(0, 2)),
opt_keepAllNew     (_cat_fm, "cp3_fm_keepM"   ,"keep all new AMOs (also rejected ones)", true),
opt_newAlo          (_cat_fm, "cp3_fm_newAlo"  ,"create clauses from deduced ALO constraints 0=no,1=from kept,2=keep all ",  2, IntRange(0, 2)),
opt_newAlk          (_cat_fm, "cp3_fm_newAlk"  ,"create clauses from deduced ALK constraints 0=no,1=from kept,2=keep all (possibly redundant!)",  2, IntRange(0, 2)),
opt_checkSub       (_cat_fm, "cp3_fm_newSub"  ,"check whether new ALO and ALK subsume other clauses (only if newALO or newALK)", true),
opt_rem_first      (_cat_fm, "cp3_fm_1st"     ,"extract first AMO candidate, or last AMO candidate", false),
#if defined TOOLVERSION 
fm_debug_out (0),
#else
fm_debug_out (_cat_fm, "fm-debug",       "Debug Output of Fourier Motzkin", 0, IntRange(0, 4)),
#endif

//
// Hidden Tautology Elimination
//
opt_hte_steps    (_cat_hte, "cp3_hte_steps",  "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX)),
#if defined TOOLVERSION && TOOLVERSION < 302
opt_par_hte        (false),
#else
opt_par_hte         (_cat_hte, "cp3_par_hte",    "Forcing Parallel HTE", false),
#endif
#if defined TOOLVERSION  
hte_debug_out (0),
opt_hteTalk (false),
#else
hte_debug_out    (_cat_hte, "cp3_hte_debug",  "print debug output to screen", 0, IntRange(0, 4)),
opt_hteTalk (_cat_hte, "cp3_hteTalk",    "talk about algorithm execution", false),
#endif
opt_hte_inpStepInc      (_cat_hte, "cp3_hte_inpInc","increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX)),

//
// Probing
//
pr_uip            (_cat_pr, "pr-uips",   "perform learning if a conflict occurs up to x-th UIP (-1 = all )", -1, IntRange(-1, INT32_MAX)),
pr_double        (_cat_pr, "pr-double", "perform double look-ahead",true),
pr_probe         (_cat_pr, "pr-probe",  "perform probing",true),
pr_rootsOnly     (_cat_pr, "pr-roots",  "probe only on root literals",true),
pr_repeat        (_cat_pr, "pr-repeat", "repeat probing if changes have been applied",false),
pr_clsSize        (_cat_pr, "pr-csize",  "size of clauses that are considered for probing/vivification (propagation)", INT32_MAX,  IntRange(0, INT32_MAX)),
pr_prLimit        (_cat_pr, "pr-probeL", "step limit for probing", 5000000,  IntRange(0, INT32_MAX)),
pr_EE            (_cat_pr, "pr-EE",     "run equivalent literal detection",true),
pr_vivi          (_cat_pr, "pr-vivi",   "perform clause vivification",true),
pr_keepLearnts    (_cat_pr, "pr-keepL",  "keep conflict clauses in solver (0=no,1=learnt,2=original)", 2, IntRange(0,2)),
pr_keepImplied    (_cat_pr, "pr-keepI",  "keep clauses that imply on level 1 (0=no,1=learnt,2=original)", 2, IntRange(0,2)),
pr_viviPercent    (_cat_pr, "pr-viviP",  "percent of max. clause size for clause vivification", 80, IntRange(0,100)),
pr_viviLimit      (_cat_pr, "pr-viviL",  "step limit for clause vivification", 5000000,  IntRange(0, INT32_MAX)),
pr_opt_inpStepInc1      (_cat_pr, "cp3_pr_inpInc","increase for steps per inprocess call", 1000000, IntRange(0, INT32_MAX)),
pr_opt_inpStepInc2      (_cat_pr, "cp3_viv_inpInc","increase for steps per inprocess call", 1000000, IntRange(0, INT32_MAX)),
pr_keepLHBRs    (_cat_pr, "pr-keepLHBR",  "keep clauses that have been created during LHBR during probing/vivification (0=no,1=learnt)", 0, IntRange(0,1)),
#if defined TOOLVERSION  
pr_debug_out (0),
#else
pr_debug_out        (_cat_pr, "pr-debug", "debug output for probing",0, IntRange(0,4) ),
#endif

//
// Unit Propagation
//
#if defined TOOLVERSION  
up_debug_out (0),
#else
up_debug_out (_cat_up, "up-debug", "debug output for propagation",0, IntRange(0,4) ),
#endif

//
// Resolution and Redundancy Addition
//
opt_res3_use_binaries  (_cat_res, "cp3_res_bin",      "resolve with binary clauses", false),
opt_res3_steps    (_cat_res, "cp3_res3_steps",   "Number of resolution-attempts that are allowed per iteration", 1000000, IntRange(0, INT32_MAX-1)),
opt_res3_newCls   (_cat_res, "cp3_res3_ncls",    "Max. Number of newly created clauses", 100000, IntRange(0, INT32_MAX-1)),
opt_res3_reAdd    (_cat_res, "cp3_res3_reAdd",   "Add variables of newly created resolvents back to working queues", false),
opt_res3_use_subs      (_cat_res, "cp3_res_eagerSub", "perform eager subsumption", true),
opt_add2_percent   (_cat_res, "cp3_res_percent",  "produce this percent many new clauses out of the total", 0.01, DoubleRange(0, true, 1, true)),
opt_add2_red       (_cat_res, "cp3_res_add_red",  "add redundant binary clauses", false),
opt_add2_red_level     (_cat_res, "cp3_res_add_lev",  "calculate added percent based on level", true),
opt_add2_red_lea   (_cat_res, "cp3_res_add_lea",  "add redundants based on learneds as well?", false),
opt_add2_red_start (_cat_res, "cp3_res_ars",      "also before preprocessing?", false),
opt_res3_inpStepInc      (_cat_res, "cp3_res_inpInc","increase for steps per inprocess call", 200000, IntRange(0, INT32_MAX)),
opt_add2_inpStepInc      (_cat_res, "cp3_add_inpInc","increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX)),
/// enable this parameter only during debug!
#if defined TOOLVERSION  
res3_debug_out (false),
#else
res3_debug_out         (_cat_res, "cp3_res_debug",   "print debug output to screen",false),
#endif


//
// Rewriter
//
opt_rew_min             (_cat_rew, "cp3_rew_min"  ,"min occurrence to be considered", 3, IntRange(0, INT32_MAX)),
opt_rew_iter            (_cat_rew, "cp3_rew_iter" ,"number of iterations", 1, IntRange(0, INT32_MAX)),
opt_rew_minAMO          (_cat_rew, "cp3_rew_minA" ,"min size of altered AMOs", 3, IntRange(0, INT32_MAX)),
opt_rew_limit        (_cat_rew, "cp3_rew_limit","number of steps allowed for REW", 1200000, IntRange(0, INT32_MAX)),
opt_rew_Varlimit        (_cat_rew, "cp3_rew_Vlimit","max number of variables to still perform REW", 1000000, IntRange(0, INT32_MAX)),
opt_rew_Addlimit        (_cat_rew, "cp3_rew_Addlimit","number of new variables being allowed", 100000, IntRange(0, INT32_MAX)),
opt_rew_amo        (_cat_rew, "cp3_rew_amo"   ,"rewrite amos", true),
opt_rew_imp        (_cat_rew, "cp3_rew_imp"   ,"rewrite implication chains", false),
opt_rew_scan_exo        (_cat_rew, "cp3_rew_exo"   ,"scan for encoded exactly once constraints first", false),
opt_rew_merge_amo       (_cat_rew, "cp3_rew_merge" ,"merge AMO constraints to create larger AMOs (fourier motzkin)", false),
opt_rew_rem_first       (_cat_rew, "cp3_rew_1st"   ,"how to find AMOs", false),
opt_rew_avg         (_cat_rew, "cp3_rew_avg"   ,"use AMOs above equal average only?", true),
opt_rew_ratio       (_cat_rew, "cp3_rew_ratio" ,"allow literals in AMO only, if their complement is not more frequent", true),
opt_rew_once        (_cat_rew, "cp3_rew_once"  ,"rewrite each variable at most once! (currently: yes only!)", true),
opt_rew_stat_only       (_cat_rew, "cp3_rew_stats" ,"analyze formula, but do not apply rewriting", false ),
opt_rew_min_imp_size        (_cat_rew, "cp3_rewI_min"   ,"min size of an inplication chain to be rewritten", 4, IntRange(0, INT32_MAX)),
opt_rew_impl_pref_small     (_cat_rew, "cp3_rewI_small" ,"prefer little imply variables", true),
opt_rew_inpStepInc      (_cat_rew, "cp3_rew_inpInc","increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX)),
#if defined TOOLVERSION 
rew_debug_out (0),
#else
rew_debug_out                 (_cat_rew, "rew-debug",       "Debug Output of Rewriter", 0, IntRange(0, 4)),
#endif

//
// Shuffle
//
opt_shuffle_seed          (_cat_shuffle, "shuffle-seed",  "seed for shuffling",  0, IntRange(0, INT32_MAX)),
opt_shuffle_order        (_cat_shuffle, "shuffle-order", "shuffle the order of the clauses", true),
#if defined TOOLVERSION  
shuffle_debug_out (0),
#else
shuffle_debug_out                 (_cat_shuffle, "shuffle-debug", "Debug Output of Shuffler", 0, IntRange(0, 4)),
#endif

//
// Sls
//
#if defined TOOLVERSION 
opt_sls_debug (false),
#else
opt_sls_debug (_cat_sls, "sls-debug", "Print SLS debug output", false),
#endif
opt_sls_ksat_flips (_cat_sls, "sls-ksat-flips",   "how many flips should be performed, if k-sat is detected (-1 = infinite)", 20000000, IntRange(-1, INT32_MAX)),
opt_sls_rand_walk  (_cat_sls, "sls-rnd-walk",     "probability of random walk (0-10000)", 2000, IntRange(0,10000)),
opt_sls_adopt      (_cat_sls, "sls-adopt-cls",    "reduce nr of flips for large instances", false),

//
// Subsumption
//
opt_sub_naivStrength    (_cat_sub, "naive_strength",   "use naive strengthening", false),
opt_sub_allStrengthRes  (_cat_sub, "all_strength_res", "Create all self-subsuming resolvents of clauses less equal given size (prob. slow & blowup, only seq)", 0, IntRange(0,INT32_MAX)), 
opt_sub_strength        (_cat_sub, "cp3_strength",     "Perform clause strengthening", true), 
opt_sub_preferLearned   (_cat_sub, "cp3_inpPrefL",    "During inprocessing, check learned clauses first!", true), 
opt_sub_subLimit        (_cat_sub, "cp3_sub_limit", "limit of subsumption steps",   300000000, IntRange(0,INT32_MAX)), 
opt_sub_strLimit        (_cat_sub, "cp3_str_limit", "limit of strengthening steps", 300000000, IntRange(0,INT32_MAX)), 
opt_sub_callIncrease    (_cat_sub, "cp3_call_inc",  "max. limit increase per process call (subsimp is frequently called from other techniques)", 100, IntRange(0,INT32_MAX)), 
opt_sub_inpStepInc      (_cat_sub, "cp3_sub_inpInc","increase for steps per inprocess call", 40000000, IntRange(0, INT32_MAX)),
#if defined TOOLVERSION && TOOLVERSION < 302
opt_sub_par_strength    (1),
opt_sub_lock_stats      (false),
opt_sub_par_subs        (1),
opt_sub_par_subs_counts (false),
opt_sub_chunk_size      (100000),
opt_sub_par_str_minCls  (250000),
#else
opt_sub_par_strength    (_cat_sub, "cp3_par_strength", "par strengthening: 0 never, 1 heuristic, 2 always", 1, IntRange(0,2)),
opt_sub_lock_stats      (_cat_sub, "cp3_lock_stats", "measure time waiting in spin locks", false),
opt_sub_par_subs        (_cat_sub, "cp3_par_subs", "par subsumption: 0 never, 1 heuristic, 2 always", 1, IntRange(0,2)),
opt_sub_par_subs_counts (_cat_sub, "par_subs_counts" ,  "Updates of counts in par-subs 0: compare_xchange, 1: CRef-vector", 1, IntRange(0,1)),
opt_sub_chunk_size      (_cat_sub, "susi_chunk_size" ,  "Size of Par SuSi Chunks", 100000, IntRange(1,INT32_MAX)),
opt_sub_par_str_minCls  (_cat_sub, "par_str_minCls"  ,  "number of clauses to start parallel strengthening", 250000, IntRange(1,INT32_MAX)),
#endif
#if defined TOOLVERSION
opt_sub_debug (0),
#else
opt_sub_debug   (_cat_sub, "susi_debug" , "Debug Output for Subsumption", 0, IntRange(0,3)),
#endif

//
// Symmetry Breaker
//
sym_opt_hsize             (_cat_sym, "sym-size",    "scale with the size of the clause", false),
sym_opt_hpol              (_cat_sym, "sym-pol",     "consider the polarity of the occurrences", false),
sym_opt_hpushUnit         (_cat_sym, "sym-unit",    "ignore unit clauses", false), // there should be a parameter delay-units already!
sym_opt_hmin              (_cat_sym, "sym-min",     "minimum symmtry to be exploited", 2, IntRange(1, INT32_MAX) ),
sym_opt_hratio            (_cat_sym, "sym-ratio",   "only consider a variable if it appears close to the average of variable occurrences", 0.4, DoubleRange(0, true, HUGE_VAL, true)),
sym_opt_iter              (_cat_sym, "sym-iter",    "number of symmetry approximation iterations", 3, IntRange(0, INT32_MAX) ),
sym_opt_pairs             (_cat_sym, "sym-show",    "show symmetry pairs", false),
sym_opt_print             (_cat_sym, "sym-print",   "show the data for each variable", false),
sym_opt_exit              (_cat_sym, "sym-exit",    "exit after analysis", false),
sym_opt_hprop             (_cat_sym, "sym-prop",    "try to generate symmetry breaking clauses with propagation", false),
sym_opt_hpropF            (_cat_sym, "sym-propF",    "generate full clauses", false),
sym_opt_hpropA            (_cat_sym, "sym-propA",    "test all four casese instead of two", false),
sym_opt_cleanLearn        (_cat_sym, "sym-clLearn",  "clean the learned clauses that have been created during symmetry search", false),
sym_opt_conflicts         (_cat_sym, "sym-cons",     "number of conflicts for looking for being implied", 0, IntRange(0, INT32_MAX) ),
sym_opt_total_conflicts   (_cat_sym, "sym-consT",    "number of total conflicts for looking for being implied", 10000, IntRange(0, INT32_MAX) ),
#if defined TOOLVERSION  
sym_debug_out (0),
#else
sym_debug_out        (_cat_sym, "sym-debug", "debug output for probing",0, IntRange(0,4) ),
#endif

//
// Twosat
//
#if defined TOOLVERSION 
twosat_debug_out (0),
twosat_useUnits (false),
twosat_clearQueue( true),
#else
twosat_debug_out                 (_cat_twosat, "2sat-debug",  "Debug Output of 2sat", 0, IntRange(0, 4)),
twosat_useUnits                 (_cat_twosat, "2sat-units",  "If 2SAT finds units, use them!", false),
twosat_clearQueue               (_cat_twosat, "2sat-cq",     "do a decision after a unit has been found", true),
#endif

 
//
// Unhide
//
opt_uhd_Iters     (_cat_uhd, "cp3_uhdIters",     "Number of iterations for unhiding", 3, IntRange(0, INT32_MAX)),
opt_uhd_Trans     (_cat_uhd, "cp3_uhdTrans",     "Use Transitive Graph Reduction (buggy)", false),
opt_uhd_UHLE      (_cat_uhd, "cp3_uhdUHLE",      "Use Unhiding+Hidden Literal Elimination",  3, IntRange(0, 3)),
opt_uhd_UHTE      (_cat_uhd, "cp3_uhdUHTE",      "Use Unhiding+Hidden Tautology Elimination", true),
opt_uhd_NoShuffle (_cat_uhd, "cp3_uhdNoShuffle", "Do not perform randomized graph traversation", false),
opt_uhd_EE        (_cat_uhd, "cp3_uhdEE",        "Use equivalent literal elimination (buggy)", false),
opt_uhd_TestDbl   (_cat_uhd, "cp3_uhdTstDbl",    "Test for duplicate binary clauses", false),
#if defined TOOLVERSION  
opt_uhd_Debug(0),
#else
opt_uhd_Debug     (_cat_uhd, "cp3_uhdDebug",     "Debug Level of Unhiding", 0, IntRange(0, 6)),
#endif


//
// Xor
//
opt_xorMatchLimit (_cat_xor, "xorMaxSize",  "Maximum Clause Size for detecting XOrs (high number consume much memory!)", 12, IntRange(3, 63)),
opt_xorFindLimit  (_cat_xor, "xorLimit",    "number of checks for finding xors", 1200000, IntRange(0, INT32_MAX)),
opt_xor_selectX       (_cat_xor, "xorSelect",    "how to select next xor 0=first,1=smallest", 0, IntRange(0, 1)),
opt_xor_keepUsed      (_cat_xor, "xorKeepUsed",  "continue to simplify kept xors", true),
opt_xor_findSubsumed  (_cat_xor, "xorFindSubs",  "try to recover XORs that are partially subsumed", true),
opt_xor_findResolved  (_cat_xor, "xorFindRes",   "try to recover XORs including resolution steps", false),
#if defined TOOLVERSION 
opt_xor_debug(0),
#else
opt_xor_debug             (_cat_xor, "xor-debug",       "Debug Output of XOR reasoning", 0, IntRange(0, 5)),
#endif
 dummy (0)
{}

void CP3Config::parseOptions(int& argc, char** argv, bool strict)
{
 ::parseOptions (argc, argv, strict ); // simply parse all options
}

bool CP3Config::checkConfiguration()
{
  return true;
}
