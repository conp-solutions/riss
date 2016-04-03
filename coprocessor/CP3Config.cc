/************************************************************************************[CP3Config.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "coprocessor/CP3Config.h"

#include "riss/mtl/Sort.h"

using namespace Riss;

namespace Coprocessor
{

//
// to exclude options from being printed in the automatically generated parameter file, add #NoAutoT in the description!
//

// Disable astyle formatting
// *INDENT-OFF*

// parameter categories
const char* _cat          = "COPROCESSOR";
const char* _cat2         = "COPROCESSOR  TECHNIQUES";
const char* _cat_bve      = "COPROCESSOR - BVE";
const char* _cat_bva      = "COPROCESSOR - BVA";
const char* _cat_bce      = "COPROCESSOR - BCE";
const char* _cat_la       = "COPROCESSOR - LA #NoAutoT";
const char* _cat_cce      = "COPROCESSOR - CCE";
const char* _cat_dense    = "COPROCESSOR - DENSE";
const char* _cat_entailed = "COPROCESSOR - ENTAILED #NoAutoT";
const char* _cat_ee       = "COPROCESSOR - EQUIVALENCE ELIMINATION";
const char* _cat_ee_hash  = "COPROCESSOR - EQUIVALENCE ELIMINATION - HASHING";
const char* _cat_fm       = "COPROCESSOR - FOURIERMOTZKIN";
const char* _cat_hte      = "COPROCESSOR - HTE";
const char* _cat_pr       = "COPROCESSOR - PROBING";
const char* _cat_up       = "COPROCESSOR - UP";
const char* _cat_res      = "COPROCESSOR - RES";
const char* _cat_rew      = "COPROCESSOR - REWRITE #NoAutoT";
const char* _cat_shuffle  = "COPROCESSOR - SHUFFLE #NoAutoT";
const char* _cat_sls      = "COPROCESSOR - SLS";
const char* _cat_sub      = "COPROCESSOR - SUBSUMPTION";
const char* _cat_sym      = "COPROCESSOR - SYMMETRY";
const char* _cat_twosat   = "COPROCESSOR - TWOSAT";
const char* _cat_uhd      = "COPROCESSOR - UNHIDE";
const char* _cat_xor      = "COPROCESSOR - XOR";
const char* _cat_rat      = "COPROCESSOR - RAT Elimination";
const char* _cat_hbr      = "COPROCESSOR - Hyper Binary Resolution";

CP3Config::CP3Config(const std::string& presetOptions) // add new options here!
    :
    Config(&configOptions, presetOptions),

    //
    // all the options for the object
    //
    opt_enabled       (_cat, "enabled_cp3",     "Use CP3", false, optionListPtr),
    opt_inprocess     (_cat, "inprocess",       "Use CP3 for inprocessing", false, optionListPtr, &opt_enabled),
    opt_cp3_vars      (_cat, "cp3_vars",        "variable limit to enable CP3",                    5000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_enabled),
    opt_cp3_cls       (_cat, "cp3_cls",         "clause limit to enable CP3",                     30000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_enabled),
    opt_cp3_lits      (_cat, "cp3_lits",        "total literal limit to enable CP3",              50000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_enabled),
    opt_cp3_ipvars    (_cat, "cp3_ipvars",      "variable limit to enable CP3 inprocessing",       5000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_inprocess),
    opt_cp3_ipcls     (_cat, "cp3_ipcls",       "clause limit to enable CP3 inprocessing",        30000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_inprocess),
    opt_cp3_iplits    (_cat, "cp3_iplits",      "total literal limit to enable CP3 inprocessing", 50000000, IntRange(0, INT32_MAX),                optionListPtr, &opt_inprocess),

    opt_unlimited     (_cat, "cp3_limited",     "Limits for preprocessing techniques", true,                                                       optionListPtr, &opt_enabled),
    opt_randomized    (_cat, "cp3_randomized",  "Steps withing preprocessing techniques are executed in random order", false,                      optionListPtr, &opt_enabled),
    opt_inprocessInt  (_cat, "cp3_inp_cons",    "Perform Inprocessing after at least X conflicts", 20000, IntRange(0, INT32_MAX),                  optionListPtr, &opt_inprocess),
    opt_inpIntInc     (_cat, "cp3_iinp_cons",   "Increase inprocessing interval in each iteration", 0, IntRange(0, INT32_MAX),                     optionListPtr, &opt_inprocess),
    opt_simplifyRounds(_cat, "cp3_iters",       "simplification rounds in preprocessing", 1, IntRange(0, INT32_MAX),                               optionListPtr, &opt_enabled),

    opt_exit_pp       (_cat, "cp3-exit-pp",     "terminate after preprocessing (1=exit,2=print formula cerr+exit 3=cout+exit) #NoAutoT", 0, IntRange(0, 3), optionListPtr, &opt_enabled),
    opt_randInp       (_cat, "randInp",         "Randomize Inprocessing", true,                                                                    optionListPtr, &opt_inprocess),
    opt_inc_inp       (_cat, "inc-inp",         "increase technique limits per inprocess step", false,                                             optionListPtr, &opt_inprocess),
    opt_remL_inp      (_cat, "inp-remL",        "remove all learned clauses for first inprocessing", false,                                        optionListPtr, &opt_inprocess),

    opt_whiteList     (_cat2, "whiteList",      "variables whose set of solution is not touched", 0,                                               optionListPtr, &opt_enabled),

    opt_printStats    (_cat, "cp3_stats",       "Print Technique Statistics #NoAutoT", false,                                                      optionListPtr, &opt_enabled),
    opt_verbose       (_cat, "cp3_verbose",     "Verbosity of preprocessor #NoAutoT", 0, IntRange(0, 5),                                                    optionListPtr, &opt_enabled),

    // techniques
    opt_up             (_cat2, "up",            "Use Unit Propagation during preprocessing", false,                            optionListPtr, &opt_enabled),
    opt_subsimp        (_cat2, "subsimp",       "Use Subsumption during preprocessing", false,                                 optionListPtr, &opt_enabled),
    opt_hte            (_cat2, "hte",           "Use Hidden Tautology Elimination during preprocessing", false,                optionListPtr, &opt_enabled),

    opt_bce            (_cat2, "bce",           "Use Blocked Clause Elimination during preprocessing", false,                     optionListPtr, &opt_enabled),
    opt_modprep        (_cat2, "modprep",       "Use Modularity Based Preprocessing", false,                                      optionListPtr, &opt_enabled),
    opt_ent            (_cat2, "ent",           "Use checking for entailed redundancy during preprocessing #NoAutoT", false,      optionListPtr, &opt_enabled),
    opt_exp            (_cat2, "exp",           "Use experimental simplification techniques #NoAutoT", false,                     optionListPtr, &opt_enabled),
    opt_la             (_cat2, "la",            "Use (covered/asymmetric) Literal Addition during preprocessing #NoAutoT", false, optionListPtr, &opt_enabled),
    opt_cce            (_cat2, "cce",           "Use (covered) Clause Elimination during preprocessing", false,                   optionListPtr, &opt_enabled),
    opt_rate           (_cat2, "rate",          "Use resolution asymmetric tautologye limination during preprocessing", false,    optionListPtr, &opt_enabled),
    opt_ee             (_cat2, "ee",            "Use Equivalence Elimination during preprocessing", false,                        optionListPtr, &opt_enabled),
    opt_bve            (_cat2, "bve",           "Use Bounded Variable Elimination during preprocessing", false,                   optionListPtr, &opt_enabled),
    opt_bva            (_cat2, "bva",           "Use Bounded Variable Addition during preprocessing", false,                      optionListPtr, &opt_enabled),
    opt_unhide         (_cat2, "unhide",        "Use Unhiding (UHTE, UHLE based on BIG sampling)", false,                         optionListPtr, &opt_enabled),
    opt_probe          (_cat2, "probe",         "Use Probing/Clause Vivification", false,                                         optionListPtr, &opt_enabled),
    opt_ternResolve    (_cat2, "3resolve",      "Use Ternary Clause Resolution", false,                                           optionListPtr, &opt_enabled),
    opt_addRedBins     (_cat2, "addRed2",       "Use Adding Redundant Binary Clauses", false,                                     optionListPtr, &opt_enabled),
    opt_dense          (_cat2, "dense",         "Remove gaps in variables of the formula", false,                                 optionListPtr, &opt_enabled),
    opt_shuffle        (_cat2, "shuffle",       "Shuffle the formula, before the preprocessor is initialized #NoAutoT", false,    optionListPtr, &opt_enabled),
    opt_simplify       (_cat2, "simplify",      "Apply easy simplifications to the formula", true,                                optionListPtr, &opt_enabled),
    opt_symm           (_cat2, "symm",          "Do local symmetry breaking", false,                                              optionListPtr, &opt_enabled),
    opt_FM             (_cat2, "fm",            "Use the Fourier Motzkin transformation", false,                                  optionListPtr, &opt_enabled),
    opt_hbr            (_cat2, "hbr",           "Use hyper binary resolution", false,                                             optionListPtr, &opt_enabled),

    stepbystepoutput   (_cat2, "debugCNFbase",  "CNF filename prefix for step by step formulas", 0,                            optionListPtr, &opt_enabled),

    opt_ptechs         (_cat2, "cp3_ptechs",    "techniques for preprocessing", 0,                                             optionListPtr, &opt_enabled),
    opt_itechs         (_cat2, "cp3_itechs",    "techniques for inprocessing",  0,                                             optionListPtr, &opt_inprocess),

    opt_threads     (_cat,        "cp3_threads",   "Number of extra threads that should be used for preprocessing #NoAutoT", 0, IntRange(0, 64), optionListPtr, &opt_enabled),
    opt_sls         (_cat2,       "sls",           "Use Simple Walksat algorithm to test whether formula is satisfiable quickly", false,         optionListPtr, &opt_enabled),
    opt_sls_phase   (_cat_sls,    "sls-phase",     "Use current interpretation of SLS as phase", false,                                          optionListPtr, &opt_sls),
    opt_sls_flips   (_cat_sls,    "sls-flips",     "Perform given number of SLS flips", 8000000, IntRange(-1, INT32_MAX),                        optionListPtr, &opt_sls),
    opt_xor         (_cat2,       "xor",           "Reason with XOR constraints", false,                                                         optionListPtr, &opt_enabled),
    opt_rew         (_cat_rew,    "rew",           "Rewrite AMO constraints #NoAutoT", false,                                                    optionListPtr, &opt_enabled),

    //
    // Var and Cls LIMTS
    //

    opt_subsimp_vars    (_cat, "cp3_susi_vars",   "variable limit to enable subsimp",            5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_subsimp),
    opt_subsimp_cls     (_cat, "cp3_susi_cls",    "clause limit to enable subsimp",              5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_subsimp),
    opt_subsimp_lits    (_cat, "cp3_susi_lits",   "total literal limit to enable subsimp",       10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_subsimp),
    opt_hte_vars        (_cat, "cp3_hte_vars",    "variable limit to enable HTE",                1000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_hte),
    opt_hte_cls         (_cat, "cp3_hte_cls",     "clause limit to enable HTE",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_hte),
    opt_hte_lits        (_cat, "cp3_hte_lits",    "total literal limit to enable HTE",           20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_hte),
    opt_bce_vars        (_cat, "cp3_bce_vars",    "variable limit to enable BCE,CLE,CLA",        3000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_bce),
    opt_bce_cls         (_cat, "cp3_bce_cls",     "clause limit to enable BCE,CLE,CLA",          10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bce),
    opt_bce_lits        (_cat, "cp3_bce_lits",    "total literal limit to enable BCE,CLE,CLA",   30000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bce),
    opt_ent_vars        (_cat, "cp3_ent_vars",    "variable limit to enable ENT",                INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_ent),
    opt_ent_cls         (_cat, "cp3_ent_cls",     "clause limit to enable ENT",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_ent),
    opt_ent_lits        (_cat, "cp3_ent_lits",    "total literal limit to enable ENT",           INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_ent),
    opt_la_vars         (_cat, "cp3_la_vars",     "clause limit to enable CLA",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_la),
    opt_la_cls          (_cat, "cp3_la_cls",      "clause limit to enable CLA",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_la),
    opt_la_lits         (_cat, "cp3_la_lits",     "total literal limit to enable CLA",           INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_la),
    opt_cce_vars        (_cat, "cp3_cce_vars",    "variable limit to enable CCE",                5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_cce),
    opt_cce_cls         (_cat, "cp3_cce_cls",     "clause limit to enable CCE",                  30000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_cce),
    opt_cce_lits        (_cat, "cp3_cce_lits",    "total literal limit to enable CCE",           50000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_cce),
    opt_rate_vars       (_cat, "cp3_rate_vars",   "variable limit to enable RATE",               1000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_rate),
    opt_rate_cls        (_cat, "cp3_rate_cls",    "clause limit to enable RATE",                 2000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_rate),
    opt_rate_lits       (_cat, "cp3_rate_lits",   "total literal limit to enable RATE",          10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_rate),
    opt_ee_vars         (_cat, "cp3_ee_vars",     "variable limit to enable EE",                 5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_ee),
    opt_ee_cls          (_cat, "cp3_ee_cls",      "clause limit to enable EE",                   20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_ee),
    opt_ee_lits         (_cat, "cp3_ee_lits",     "total literal limit to enable EE",            40000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_ee),
    opt_bve_vars        (_cat, "cp3_bve_vars",    "variable limit to enable BVE",                5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_bve),
    opt_bve_cls         (_cat, "cp3_bve_cls",     "clause limit to enable BVE",                  20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bve),
    opt_bve_lits        (_cat, "cp3_bve_lits",    "total literal limit to enable BVE",           50000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bve),
    opt_bva_vars        (_cat, "cp3_bva_vars",    "variable limit to enable BVA",                3000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_bva),
    opt_bva_cls         (_cat, "cp3_bva_cls",     "clause limit to enable BVA",                  20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bva),
    opt_bva_lits        (_cat, "cp3_bva_lits",    "total literal limit to enable BVA",           40000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_bva),

    opt_unhide_vars     (_cat, "cp3_unhide_vars", "variable limit to enable UNHIDE",             3000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_unhide),
    opt_unhide_cls      (_cat, "cp3_unhide_cls",  "clause limit to enable UNHIDE",               10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_unhide),
    opt_unhide_lits     (_cat, "cp3_unhide_lits", "total literal limit to enable UNHIDE",        7000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_unhide),

    opt_ternResolve_vars(_cat, "cp3_tRes_vars",   "variable limit to enable 3RES",               1000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_ternResolve),
    opt_ternResolve_cls (_cat, "cp3_tRes_cls",    "clause limit to enable 3RES",                 20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_ternResolve),
    opt_ternResolve_lits(_cat, "cp3_tRes_lits",   "total literal limit to enable 3RES",          50000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_ternResolve),
    opt_addRedBins_vars (_cat, "cp3_aBin_vars",   "variable limit to enable ADD2",               INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_add2_red),
    opt_addRedBins_cls  (_cat, "cp3_aBin_cls",    "clause limit to enable ADD2",                 INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_add2_red),
    opt_addRedBins_lits (_cat, "cp3_aBin_lits",   "total literal limit to enable ADD2",          INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_add2_red),
    opt_symm_vars       (_cat, "cp3_symm_vars",   "variable limit to enable SYMM",               3000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_symm),
    opt_symm_cls        (_cat, "cp3_symm_cls",    "clause limit to enable SYMM",                 20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_symm),
    opt_symm_lits       (_cat, "cp3_symm_lits",   "total literal limit to enable SYMM",          15000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_symm),
    opt_fm_vars         (_cat, "cp3_fm_vars",     "variable limit to enable FM",                 2000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_FM),
    opt_fm_cls          (_cat, "cp3_fm_cls",      "clause limit to enable FM",                   10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_FM),
    opt_fm_lits         (_cat, "cp3_fm_lits",     "total literal limit to enable FM",            20000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_FM),
    opt_xor_vars        (_cat, "cp3_xor_vars",    "variable limit to enable XOR",                700000,    IntRange(0, INT32_MAX), optionListPtr, &opt_xor),
    opt_xor_cls         (_cat, "cp3_xor_cls",     "clause limit to enable XOR",                  3000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_xor),
    opt_xor_lits        (_cat, "cp3_xor_lits",    "total literal limit to enable XOR",           5000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_xor),
    opt_sls_vars        (_cat, "cp3_sls_vars",    "variable limit to enable SLS",                500000,    IntRange(0, INT32_MAX), optionListPtr, &opt_sls),
    opt_sls_cls         (_cat, "cp3_sls_cls",     "clause limit to enable SLS",                  500000,    IntRange(0, INT32_MAX), optionListPtr, &opt_sls),
    opt_sls_lits        (_cat, "cp3_sls_lits",    "total literal limit to enable SLS",           4000000,   IntRange(0, INT32_MAX), optionListPtr, &opt_sls),
    opt_rew_vars        (_cat, "cp3_rew_vars",    "variable limit to enable REW",                INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_rew),
    opt_rew_cls         (_cat, "cp3_rew_cls",     "clause limit to enable REW",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_rew),
    opt_rew_lits        (_cat, "cp3_rew_lits",    "total literal limit to enable REW",           INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_rew),

    opt_hbr_vars        (_cat, "cp3_hbr_vars",    "variable limit to enable HBR",                INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_hbr),
    opt_hbr_cls         (_cat, "cp3_hbr_cls",     "clause limit to enable HBR",                  INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_hbr),
    opt_hbr_lits        (_cat, "cp3_hbr_lits",    "total literal limit to enable HBR",           INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_hbr),



    #ifndef NDEBUG
    opt_debug (_cat, "cp3-debug", "do more debugging #NoAutoT", false,                                                                            optionListPtr, &opt_enabled),
    opt_check (_cat, "cp3-check", "check solver state during simplification and before returning control to solver #NoAutoT",  0, IntRange(0, 4), optionListPtr, &opt_enabled),
    opt_log   (_cat, "cp3-log",   "Output log messages until given level #NoAutoT", 0, IntRange(0, 3),                                   optionListPtr, &opt_enabled),
    printAfter(_cat, "cp3-print", "print intermediate formula after given technique #NoAutoT", 0,                                        optionListPtr, &opt_debug),
    #endif

    //
    // parameters BVE
    //
    opt_par_bve              (_cat_bve, "cp3_par_bve",            "Parallel BVE: 0 never, 1 heur., 2 always #NoAutoT", 1, IntRange(0, 2),                                                       optionListPtr, &opt_bve),
    opt_bve_verbose          (_cat_bve, "cp3_bve_verbose",        "Verbosity of preprocessor #NoAutoT", 0, IntRange(0, 4),                                                                      optionListPtr, &opt_bve),

    opt_bve_limit            (_cat_bve, "cp3_bve_limit",          "perform at most this many clause derefferences", 25000000, IntRange(-1, INT32_MAX),                                          optionListPtr, &opt_bve),
    opt_learnt_growth        (_cat_bve, "cp3_bve_learnt_growth",  "Keep C (x) D, where C or D is learnt, if |C (x) D| <= max(|C|,|D|) + N", 0, IntRange(-1, INT32_MAX),                         optionListPtr, &opt_bve),
    opt_resolve_learnts      (_cat_bve, "cp3_bve_resolve_learnts","Resolve learnt clauses: 0: off, 1: original with learnts, 2: 1 and learnts with learnts", 0, IntRange(0, 2),                 optionListPtr, &opt_bve),
    opt_unlimited_bve        (_cat_bve, "bve_unlimited",          "perform bve test for Var v, if there are more than 10 + 10 or 15 + 5 Clauses containing v", false,                           optionListPtr, &opt_bve),
    opt_bve_strength         (_cat_bve, "bve_strength",           "do strengthening during bve", true,                                                                                          optionListPtr, &opt_bve),
    opt_bve_findGate         (_cat_bve, "bve_gates",              "try to find variable AND gate definition before elimination", true,                                                          optionListPtr, &opt_bve),
    opt_force_gates          (_cat_bve, "bve_force_gates",        "Force gate search (slower, but probably more eliminations and blockeds are found)", false,                                   optionListPtr, &opt_bve),
    bve_funcDepOnly          (_cat_bve, "bve_fdepOnly",           "eliminate only variables that are func.dep (based on gates)", false,                                                         optionListPtr, &opt_bve),
    // pick order of eliminations
    opt_bve_heap             (_cat_bve, "cp3_bve_heap",           "0: minimum heap, 1: maximum heap, 2: random, 3: ratio pos/neg smaller+less, 4: ratio pos/neg smaller+greater, 5: ratio pos/neg greater+less, 6: ratio pos/neg greater + greater, 7-10: same as 3-6, but inverse measure order", 0, IntRange(0, 10), optionListPtr, &opt_bve),
    // increasing eliminations
    opt_bve_grow             (_cat_bve, "bve_cgrow",              "number of additional clauses per elimination", 0, IntRange(-2000, 2000),                                           optionListPtr, &opt_bve),
    opt_bve_growTotal        (_cat_bve, "bve_cgrow_t",            "total number of additional clauses", 1000, IntRange(0, 50000),                                                              optionListPtr, &opt_bve),
    opt_totalGrow            (_cat_bve, "bve_totalG",             "Keep track of total size of formula when allowing increasing eliminations", false,                                           optionListPtr, &opt_bve),

    opt_bve_bc               (_cat_bve, "bve_BCElim",             "Eliminate Blocked Clauses", false,                                                                                           optionListPtr, &opt_bve),
    heap_updates             (_cat_bve, "bve_heap_updates",       "Always update variable heap if clauses / literals are added or removed, 2 add variables, if not in heap", 1, IntRange(0, 2), optionListPtr, &opt_bve),
    opt_bve_earlyAbort       (_cat_bve, "bve_early",              "Interupt anticipate eagerly", false,                                                                                         optionListPtr, &opt_bve),
    opt_bce_only             (_cat_bve, "bce_only",               "Only remove blocked clauses but do not resolve variables.", false,                                                           optionListPtr, &opt_bve),
    opt_print_progress       (_cat_bve, "bve_progress",           "Print bve progress stats. #NoAutoT", false,                                                                                  optionListPtr, &opt_bve),
    opt_bveInpStepInc        (_cat_bve, "cp3_bve_inpInc",         "increase for steps per inprocess call", 5000000, IntRange(0, INT32_MAX),                                                     optionListPtr, &opt_bve),


    par_bve_threshold        (_cat_bve, "par_bve_th",             "Threshold for use of BVE-Worker #NoAutoT", 10000, IntRange(0, INT32_MAX),                                                             optionListPtr, &opt_par_bve),
    postpone_locked_neighbors(_cat_bve, "postp_lockd_neighb",     "Postpone Elimination-Check if more neighbors are locked #NoAutoT", 3, IntRange(0, INT32_MAX),                                         optionListPtr, &opt_par_bve),
    opt_minimal_updates      (_cat_bve, "par_bve_min_upd",        "Omit LitOcc and Heap updates to reduce locking #NoAutoT", false,                                                                      optionListPtr, &opt_par_bve),

    //
    // BVA
    //
    opt_bva_push            (_cat_bva, "cp3_bva_push",    "push variables back to queue (0=none,1=original,2=all)", 2, IntRange(0, 2), optionListPtr, &opt_bva),
    opt_bva_VarLimit        (_cat_bva, "cp3_bva_Vlimit",  "use BVA only, if number of variables is below threshold", 3000000, IntRange(-1, INT32_MAX), optionListPtr, &opt_bva),
    opt_Abva                (_cat_bva, "cp3_Abva",        "perform AND-bva", false, optionListPtr, &opt_bva),
    opt_bva_Alimit          (_cat_bva, "cp3_bva_limit",   "number of steps allowed for AND-BVA", 1200000, IntRange(0, INT32_MAX), optionListPtr, &opt_Abva),
    opt_Abva_maxRed         (_cat_bva, "cp3_bva_Amax",    "maximum reduction for one additional variable", INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_Abva),
    opt_bvaInpStepInc       (_cat_bva, "cp3_bva_incInp",  "increases of number of steps per inprocessing", 80000, IntRange(0, INT32_MAX), optionListPtr, &opt_Abva),
    opt_Abva_heap           (_cat_bva, "cp3_Abva_heap",   "0: minimum heap, 1: maximum heap, 2: ratio pos/neg smaller+less, 3: ratio pos/neg smaller+greater, 4: ratio pos/neg greater+less, 5: ratio pos/neg greater + greater, 6-9: same as 3-6, but inverse measure order", 1, IntRange(0, 9), optionListPtr, &opt_Abva),

    opt_bvaComplement       (_cat_bva, "cp3_bva_compl",   "treat complementary literals special", true, optionListPtr, &opt_Abva),
    opt_bvaRemoveDubplicates(_cat_bva, "cp3_bva_dupli",   "remove duplicate clauses", true, optionListPtr, &opt_Abva),
    opt_bvaSubstituteOr     (_cat_bva, "cp3_bva_subOr",   "try to also substitus disjunctions", false, optionListPtr, &opt_Abva),

    #ifndef NDEBUG
    bva_debug               (_cat_bva, "bva-debug",       "Debug Output of BVA", 0, IntRange(0, 4), optionListPtr, &opt_Abva),
    #endif

    #ifndef NDEBUG
    opt_bvaAnalysisDebug    (_cat_bva, "cp3_bva_ad",     "experimental analysis",                                 0, IntRange(0, 4),         optionListPtr, &opt_Abva),
    #endif
    opt_Xbva                (_cat_bva, "cp3_Xbva",       "perform XOR-bva (1=half gates,2=full gates)",           0, IntRange(0, 2),         optionListPtr, &opt_bva),
    opt_Ibva                (_cat_bva, "cp3_Ibva",       "perform ITE-bva (1=half gates,2=full gates)",           0, IntRange(0, 2),         optionListPtr, &opt_bva),
    opt_bva_Xlimit          (_cat_bva, "cp3_bva_Xlimit", "number of steps allowed for XOR-BVA",           100000000, IntRange(0, INT32_MAX), optionListPtr, &opt_Xbva),
    opt_bva_Ilimit          (_cat_bva, "cp3_bva_Ilimit", "number of steps allowed for ITE-BVA",           100000000, IntRange(0, INT32_MAX), optionListPtr, &opt_Ibva),
    opt_Xbva_maxRed         (_cat_bva, "cp3_bva_Xmax",   "maximum reduction for one additional variable", INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_Xbva),
    opt_Ibva_maxRed         (_cat_bva, "cp3_bva_Imax",   "maximum reduction for one additional variable", INT32_MAX, IntRange(0, INT32_MAX), optionListPtr, &opt_Ibva),
    opt_Xbva_heap           (_cat_bva, "cp3_Xbva_heap",  "0: minimum heap, 1: maximum heap, 2: ratio pos/neg smaller+less, 3: ratio pos/neg smaller+greater, 4: ratio pos/neg greater+less, 5: ratio pos/neg greater + greater, 6-9: same as 3-6, but inverse measure order", 1, IntRange(0, 9), optionListPtr, &opt_Xbva),
    opt_Ibva_heap           (_cat_bva, "cp3_Ibva_heap",  "0: minimum heap, 1: maximum heap, 2: ratio pos/neg smaller+less, 3: ratio pos/neg smaller+greater, 4: ratio pos/neg greater+less, 5: ratio pos/neg greater + greater, 6-9: same as 3-6, but inverse measure order", 1, IntRange(0, 9), optionListPtr, &opt_Ibva),
    opt_Ibva_vars           (_cat,     "cp3_Ibva_vars",  "variable limit to enable IBVA",                  1000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Ibva),
    opt_Ibva_cls            (_cat,     "cp3_Ibva_cls",   "clause limit to enable IBVA",                   10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Ibva),
    opt_Ibva_lits           (_cat,     "cp3_Ibva_lits",  "total literal limit to enable IBVA",            40000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Ibva),
    opt_Xbva_vars           (_cat,     "cp3_Xbva_vars",  "variable limit to enable XBVA",                  1000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Xbva),
    opt_Xbva_cls            (_cat,     "cp3_Xbva_cls",   "clause limit to enable XBVA",                    5000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Xbva),
    opt_Xbva_lits           (_cat,     "cp3_Xbva_lits",  "total literal limit to enable XBVA",            10000000,  IntRange(0, INT32_MAX), optionListPtr, &opt_Xbva),

    //
    // BCE
    //
    orderComplements        (_cat_bce, "bce-compl",    "test literals for BCE based on the number of occurrences of the complementary literal", true ,      optionListPtr, &opt_bce),
    bceBinary               (_cat_bce, "bce-bin",      "allow to remove binary clauses during BCE", false ,                                                 optionListPtr, &opt_bce),
    bceLimit                (_cat_bce, "bce-limit",    "number of pairwise clause comparisons before interrupting BCE", 100000000, IntRange(0, INT32_MAX),  optionListPtr, &opt_bce),
    opt_bce_bce             (_cat_bce, "bce-bce",      "actually perform BCE", true,                                                                        optionListPtr, &opt_bce),
    opt_bce_bcm             (_cat_bce, "bce-bcm",      "actually perform BCM (instead of BCE)", false,                                                      optionListPtr, &opt_bce),
    opt_bce_cle             (_cat_bce, "bce-cle",      "perform covered literal elimination (CLE)", false,                                                  optionListPtr, &opt_bce),
    opt_bce_cla             (_cat_bce, "bce-cla",      "perform covered literal addition (CLA)", false,                                                     optionListPtr, &opt_bce),
    opt_bce_cle_conservative(_cat_bce, "bce-cle-cons", "conservative cle if taut. resolvents are present", false,                                           optionListPtr, &opt_bce_cle),
    opt_bceInpStepInc       (_cat_bce, "bce-incInp",   "number of steps given to BCE for another inprocessign round", 10000, IntRange(0, INT32_MAX),        optionListPtr, &opt_bce),
    opt_bce_verbose         (_cat_bce, "bce-verbose",  "be verbose during BCE #NoAutoT", 0, IntRange(0, 3),                                                          optionListPtr, &opt_bce),
    #ifndef NDEBUG
    opt_bce_debug           (_cat_bce, "bce-debug",    "output debug info during BCE", false,                                                               optionListPtr, &opt_bce),
    #endif

    //
    // HBR
    //
    hbrLimit                (_cat_hbr, "hbr-limit",    "number of pairwise clause comparisons before interrupting HBR", 100000000, IntRange(0, INT32_MAX),  optionListPtr, &opt_hbr),
    opt_hbr_maxCsize        (_cat_hbr, "hbr-csize",    "max. clause size to be considered for HBR", 3, IntRange(3, INT32_MAX),                              optionListPtr, &opt_hbr),
    opt_hbr_addBinaries     (_cat_hbr, "hbr-addBin",   "when add binary clauses (0=always,1=1st iteration,2=never", 0, IntRange(0, 2),                      optionListPtr, &opt_hbr),
    opt_hbrInpStepInc       (_cat_hbr, "hbr-incInp",   "number of steps given to HBR for another inprocessign round", 10000, IntRange(0, INT32_MAX),        optionListPtr, &opt_hbr),
    opt_hbr_verbose         (_cat_hbr, "hbr-verbose",  "be verbose during HBR #NoAutoT", 0, IntRange(0, 3),                                                          optionListPtr, &opt_hbr),
    #ifndef NDEBUG
    opt_hbr_debug           (_cat_hbr, "hbr-debug",    "output debug info during HBR", false,                                                               optionListPtr, &opt_hbr),
    #endif

    //
    // Literal Addition
    //
    opt_la_cla   (_cat_la, "la-cla",     "perform covered literal addition (CLA)", true,                                                                    optionListPtr, &opt_la),
    opt_la_ala   (_cat_la, "la-ala",     "perform asymmetric literal addition (ALA)", false,                                                                optionListPtr, &opt_la),
    claLimit     (_cat_la, "cla-limit",  "number of pairwise clause comparisons before interrupting LA",                100000000, IntRange(0, INT32_MAX),  optionListPtr, &opt_la),
    claStepSize  (_cat_la, "la-claStep", "number of extension literals per step so that literals are removed randomly",         4, IntRange(1, INT32_MAX),  optionListPtr, &opt_la),
    claStepMax   (_cat_la, "la-claMax",  "number of extension literals per step so that literals are removed randomly",         2, IntRange(1, INT32_MAX),  optionListPtr, &opt_la),
    claIterations(_cat_la, "la-claIter", "number of extension literals per step so that literals are removed randomly",         1, IntRange(1, INT32_MAX),  optionListPtr, &opt_la),

    alaLimit     (_cat_la, "ala-limit",  "number of pairwise clause comparisons before interrupting LA",                100000000, IntRange(0, INT32_MAX),  optionListPtr, &opt_la_ala),
    alaIterations(_cat_la, "la-alaIter", "number of extension literals per step so that literals are removed randomly",         1, IntRange(1, INT32_MAX),  optionListPtr, &opt_la_ala),
    ala_binary   (_cat_la, "la-alaBin",  "use binary clauses for ALA", false,                                                                               optionListPtr, &opt_la_ala),
    #ifndef NDEBUG
    opt_la_debug (_cat_la, "la-debug",   "output debug info during LA", false,                                                                              optionListPtr, &opt_la_ala),
    #endif

    //
    // CCE
    //
    opt_cceSteps     (_cat_cce, "cp3_cce_steps",  "Number of steps that are allowed per iteration",                 2000000, IntRange(-1, INT32_MAX), optionListPtr, &opt_cce),
    opt_ccelevel     (_cat_cce, "cp3_cce_level",  "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE",                                  3, IntRange(0, 3),          optionListPtr, &opt_cce),
    opt_ccePercent   (_cat_cce, "cp3_cce_sizeP",  "percent of max. clause size for clause elimination (excluding)",      40, IntRange(0, 100),        optionListPtr, &opt_cce),
    #ifndef NDEBUG
    cce_debug_out    (_cat_cce, "cce-debug",      "debug output for clause elimination",                                  0, IntRange(0, 4) ,         optionListPtr, &opt_cce),
    #endif
    opt_cceInpStepInc(_cat_cce, "cp3_cce_inpInc", "increase for steps per inprocess call",                            60000, IntRange(0, INT32_MAX),  optionListPtr, &opt_cce),

    //
    // RAT Elimination
    //
    rate_orderComplements (_cat_rat, "rat-compl",        "sort according to nr. of complements", true,                                                                   optionListPtr, &opt_rate),
    rate_Limit            (_cat_rat, "rate-limit",       "number of pairwise clause comparisons before interrupting RATE and up", 9000000000, Int64Range(0, INT64_MAX) , optionListPtr, &opt_rate),
    ratm_Limit            (_cat_rat, "ratm-limit",       "number of pairwise clause comparisons before interrupting RATM and up", 9000000000, Int64Range(0, INT64_MAX) , optionListPtr, &opt_rate),
    #ifndef NDEBUG
    opt_rate_debug        (_cat_rat, "rate-debug",       "debug output for RAT elimination", 0, IntRange(0, 4) ,                                                         optionListPtr, &opt_rate),
    #endif
    opt_rate_brat         (_cat_rat, "rate-brat",        "test resolvents for being blocked if not AT", false,                                                           optionListPtr, &opt_rate),
    rate_minSize          (_cat_rat, "rate-min",         "minimal clause size for RAT elimination", 3, IntRange(2, INT32_MAX) ,                                          optionListPtr, &opt_rate),
    opt_rate_rate         (_cat_rat, "rate-rate",        "perform RAT elimination (rate)", false,                                                                        optionListPtr, &opt_rate),
    opt_rate_bcs          (_cat_rat, "rate-bcs",         "perform blocked substitution (bcs)", false,                                                                    optionListPtr, &opt_rate),
    opt_rate_ratm         (_cat_rat, "rate-ratm",        "perform RAT minimization (ratm)", false,                                                                       optionListPtr, &opt_rate),
    opt_rate_ratm_extended(_cat_rat, "rate-ratm_ext",    "perform extended RAT minimization (ratm)", false,                                                              optionListPtr, &opt_rate_ratm),
    opt_rate_ratm_rounds  (_cat_rat, "rate-ratm_rounds", "perform more than one RATM iteration", false,                                                                  optionListPtr, &opt_rate_ratm),

    //
    // Dense
    //
    #ifndef NDEBUG
    dense_debug_out        (_cat_dense, "cp3_dense_debug", "print debug output to screen", 0, IntRange(0, 2) ,                                optionListPtr, &opt_dense),
    #endif
    opt_dense_inprocess    (_cat_dense, "dense_inp",       "use dense during inprocessing #NoAutoT", false,                                   optionListPtr, &opt_dense),
    opt_dense_fragmentation(_cat_dense, "cp3_dense_frag",  "Perform densing, if fragmentation is higher than (percent)", 0, IntRange(0, 100), optionListPtr, &opt_dense),
    opt_dense_keep_assigned(_cat_dense, "cp3_keep_set",    "keep already assigned literals #NoAutoT", false,                                  optionListPtr, &opt_dense),

    //
    // Entailed
    //
    opt_entailed_minClsSize(_cat_entailed, "ent-min",    "minimum clause size that is tested", 2, IntRange(2, INT32_MAX), optionListPtr, &opt_ent),
    #ifndef NDEBUG
    entailed_debug         (_cat_entailed, "ent-debug",  "Debug Output for ENT reasoning",     0, IntRange(0, 5),         optionListPtr, &opt_ent),
    #endif

    //
    // ModPrep
    //
    #ifndef NDEBUG
    modprep_debug          (_cat_entailed, "modprep-debug",  "Debug Output for ModPrep",     0, IntRange(0, 5),         optionListPtr, &opt_modprep),
    #endif
    
    //
    // Equivalence
    //
    opt_ee_level           (_cat_ee, "cp3_ee_level",    "EE on BIG, gate probing, structural hashing", 0, IntRange(0, 3),                    optionListPtr, &opt_ee),
    opt_ee_gate_limit      (_cat_ee, "cp3_ee_glimit",   "step limit for structural hashing", INT32_MAX, IntRange(0, INT32_MAX),              optionListPtr, &opt_ee_level),
    opt_ee_circuit_iters   (_cat_ee, "cp3_ee_cIter",    "max. EE iterations for circuit (-1 == inf)", 2, IntRange(-1, INT32_MAX),            optionListPtr, &opt_ee_level),
    opt_ee_eagerEquivalence(_cat_ee, "cp3_eagerGates",  "do handle gates eagerly", true,                                                     optionListPtr, &opt_ee),
    opt_eeGateBigFirst     (_cat_ee, "cp3_BigThenGate", "detect binary equivalences before going for gates [should not be disabled!]", true, optionListPtr, &opt_ee),
    opt_ee_aagFile         (_cat_ee, "ee_aag", "write final circuit to this file", 0,                                                        optionListPtr, &opt_ee),
    #ifndef NDEBUG
    ee_debug_out           (_cat_ee, "ee-debug", "print debug output to screen", 0, IntRange(0, 3),                                          optionListPtr, &opt_ee),
    #endif
    opt_eeSub              (_cat_ee, "ee_sub",          "do subsumption/strengthening during applying equivalent literals?", false,          optionListPtr, &opt_ee),
    opt_eeFullReset        (_cat_ee, "ee_reset",        "after Subs or Up, do full reset?", false,                                           optionListPtr, &opt_ee),
    opt_ee_limit           (_cat_ee, "cp3_ee_limit",    "step limit for detecting equivalent literals", 1000000, IntRange(0, INT32_MAX),     optionListPtr, &opt_ee),
    opt_ee_inpStepInc      (_cat_ee, "cp3_ee_inpInc",   "increase for steps per inprocess call", 200000, IntRange(0, INT32_MAX),             optionListPtr, &opt_ee),
    opt_ee_bigIters        (_cat_ee, "cp3_ee_bIter",    "max. iteration to perform EE search on BIG", 3, IntRange(0, INT32_MAX),             optionListPtr, &opt_ee),
    opt_ee_iterative       (_cat_ee, "cp3_ee_it",       "use the iterative BIG-EE algorithm", false,                                         optionListPtr, &opt_ee),
    opt_EE_checkNewSub     (_cat_ee, "cp3_ee_subNew",   "check for new subsumptions immediately when adding new clauses", false,             optionListPtr, &opt_ee),
    opt_ee_eager_frozen    (_cat_ee, "ee_freeze_eager", "exclude frozen variables eagerly from found equivalences", true,                    optionListPtr, &opt_ee),

    //
    // Structural hashing options
    //
    circ_AND       (_cat_ee_hash, "cp3_extAND",      "extract AND gates", true,                                                                 optionListPtr, &opt_ee_level),
    circ_ITE       (_cat_ee_hash, "cp3_extITE",      "extract ITE gates", false,                                                                optionListPtr, &opt_ee_level),
    circ_XOR       (_cat_ee_hash, "cp3_extXOR",      "extract XOR gates", false,                                                                optionListPtr, &opt_ee_level),
    circ_ExO       (_cat_ee_hash, "cp3_extExO",      "extract ExO gates", false,                                                                optionListPtr, &opt_ee_level),
    circ_genAND    (_cat_ee_hash, "cp3_genAND",      "extract generic AND gates", false,                                                        optionListPtr, &opt_ee_level),
    circ_FASUM     (_cat_ee_hash, "cp3_extHASUM",    "extract full adder sum bit gates", false,                                                 optionListPtr, &opt_ee_level),
    circ_BLOCKED   (_cat_ee_hash, "cp3_extBlocked",  "extract gates, that can be found by blocked clause addition", false,                      optionListPtr, &opt_ee_level),
    circ_AddBlocked(_cat_ee_hash, "cp3_addBlocked",  "clauses that are used to extract blocked gates will be added eagerly (soundness)", false, optionListPtr, &opt_ee_level),
    circ_NegatedI  (_cat_ee_hash, "cp3_extNgtInput", "extract gates, where inputs come from the same variable", true,                           optionListPtr, &opt_ee_level),
    circ_Implied   (_cat_ee_hash, "cp3_extImplied",  "do search binary clause also in BIG with dfs", true,                                      optionListPtr, &opt_ee_level),

    // temporary Boolean flag to quickly enable debug output for the whole file
    #ifndef NDEBUG
    circ_debug_out (_cat_ee_hash, "cp3_circ_debug",  "print debug output for circuitextraction", false,                                         optionListPtr, &opt_ee_level),
    #endif

    //
    // Fourier Motzkin
    //
    opt_fm_max_constraints (_cat_fm, "cp3_fm_maxConstraints", "number of constraints that are allows", 200000, IntRange(0, INT32_MAX),                                                   optionListPtr, &opt_FM),
    opt_fmLimit            (_cat_fm, "cp3_fm_limit",          "number of steps allowed for FM", 6000000, Int64Range(0, INT64_MAX),                                                       optionListPtr, &opt_FM),
    opt_fmSearchLimit      (_cat_fm, "cp3_fm_Slimit",         "number of steps allowed for searching AMOs for FM", 12000000, Int64Range(0, INT64_MAX),                                   optionListPtr, &opt_FM),
    opt_fmMaxAMO           (_cat_fm, "cp3_fm_maxA",           "largest AMO that will be found during search", 200, IntRange(3, INT32_MAX),                                               optionListPtr, &opt_FM),
    opt_fmGrow             (_cat_fm, "cp3_fm_grow",           "max. grow of number of constraints per step", 40, IntRange(0, INT32_MAX),                                                 optionListPtr, &opt_FM),
    opt_fmGrowT            (_cat_fm, "cp3_fm_growT",          "total grow of number of constraints", 100000, IntRange(0, INT32_MAX),                                                     optionListPtr, &opt_FM),
    opt_atMostTwo          (_cat_fm, "cp3_fm_amt",            "extract at-most-two", true,                                                                                               optionListPtr, &opt_FM),
    opt_fm_twoPr           (_cat_fm, "cp3_fm_twoPr",          "extract AMO using two product structures", true,                                                                          optionListPtr, &opt_FM),
    opt_fm_sem             (_cat_fm, "cp3_fm_sem",            "extract Card constraints using UP", true,                                                                                 optionListPtr, &opt_FM),
    opt_findUnit           (_cat_fm, "cp3_fm_unit",           "check for units first", true,                                                                                             optionListPtr, &opt_FM),
    opt_merge              (_cat_fm, "cp3_fm_merge",          "perform AMO merge", true,                                                                                                 optionListPtr, &opt_FM),
    opt_fm_avoid_duplicates(_cat_fm, "cp3_fm_dups",           "avoid finding the same AMO multiple times", true,                                                                         optionListPtr, &opt_FM),
    opt_fm_multiVarAMO     (_cat_fm, "cp3_fm_vMulAMO",        "try to find multiple AMOs per variable", true,                                                                            optionListPtr, &opt_FM),
    opt_multiVarAMT        (_cat_fm, "cp3_fm_vMulAMT",        "try to find multiple AMTs per variable", false,                                                                           optionListPtr, &opt_FM),
    opt_cutOff             (_cat_fm, "cp3_fm_cut",            "avoid eliminating too expensive variables (>10,10 or >5,15)", true,                                                       optionListPtr, &opt_FM),
    opt_newAmo             (_cat_fm, "cp3_fm_newAmo",         "encode the newly produced AMOs (with pairwise encoding) 0=no,1=yes,2=try to avoid redundant clauses",  2, IntRange(0, 2), optionListPtr, &opt_FM),
    opt_keepAllNew         (_cat_fm, "cp3_fm_keepM",          "keep all new AMOs (also rejected ones)", true,                                                                            optionListPtr, &opt_FM),
    opt_newAlo             (_cat_fm, "cp3_fm_newAlo",         "create clauses from deduced ALO constraints 0=no,1=from kept,2=keep all ",  2, IntRange(0, 2),                            optionListPtr, &opt_FM),
    opt_newAlk             (_cat_fm, "cp3_fm_newAlk",         "create clauses from deduced ALK constraints 0=no,1=from kept,2=keep all (possibly redundant!)",  2, IntRange(0, 2),       optionListPtr, &opt_FM),
    opt_checkSub           (_cat_fm, "cp3_fm_newSub",         "check whether new ALO and ALK subsume other clauses (only if newALO or newALK)", true,                                    optionListPtr, &opt_FM),
    opt_rem_first          (_cat_fm, "cp3_fm_1st",            "extract first AMO candidate, or last AMO candidate", false,                                                               optionListPtr, &opt_FM),
    opt_fm_garbageColelct  (_cat_fm, "cp3_fm_gc",             "perform garbage collection during FM", true,                                                                              optionListPtr, &opt_FM),
    opt_fm_prooftrace      (_cat_fm, "cp3_fm_proof",          "prints FM proof steps", false,                                                                                            optionListPtr, &opt_FM),
    opt_fm_printtrace      (_cat_fm, "cp3_fm_printproof",     "print FM proof steps (0=off,1=ids,2=formula constraints,3=all constraints) use -no-cp3_fm_gc #NoAutoT", 0, IntRange(0, 3),         optionListPtr, &opt_FM),
    opt_minCardClauseSize  (_cat_fm, "card_minC",             "min clause size to find cards", 3, IntRange(2, INT32_MAX),                                      optionListPtr, &opt_FM),
    opt_maxCardClauseSize  (_cat_fm, "card_maxC",             "max clause size to find cards", 6, IntRange(2, INT32_MAX),                                      optionListPtr, &opt_FM),
    opt_maxCardSize        (_cat_fm, "card_max",              "max card size that will be looked for", 12, IntRange(2, INT32_MAX),                             optionListPtr, &opt_FM),
    opt_semSearchLimit     (_cat_fm, "card_Elimit",           "number of steps allowed for searching AMOs semantically", 1200000, Int64Range(0, INT64_MAX),    optionListPtr, &opt_FM),
    #ifndef NDEBUG
    opt_semDebug           (_cat_fm, "card_debug",            "print info during running semantic card find", false,                                           optionListPtr, &opt_FM),
    #endif
    opt_noReduct           (_cat_fm, "card_noUnits",          "assume there are no unit clauses inside the formula (otherwise, more expensive)", false,        optionListPtr, &opt_FM),

    #ifndef NDEBUG
    fm_debug_out           (_cat_fm, "fm-debug",              "Debug Output of Fourier Motzkin", 0, IntRange(0, 4),                                            optionListPtr, &opt_FM),
    #endif

    //
    // Hidden Tautology Elimination
    //
    opt_hte_steps     (_cat_hte, "cp3_hte_steps",  "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX), optionListPtr, &opt_hte),

    opt_par_hte       (_cat_hte, "cp3_par_hte",    "Forcing Parallel HTE #NoAutoT", false,                                                        optionListPtr, &opt_hte),
    #ifndef NDEBUG
    hte_debug_out     (_cat_hte, "cp3_hte_debug",  "print debug output to screen", 0, IntRange(0, 4),                                    optionListPtr, &opt_hte),
    #endif
    opt_hteTalk       (_cat_hte, "cp3_hteTalk",    "talk about algorithm execution #NoAutoT", false,                                              optionListPtr, &opt_hte),
    opt_hte_inpStepInc(_cat_hte, "cp3_hte_inpInc", "increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX),               optionListPtr, &opt_hte),

    //
    // Probing
    //
    pr_probe          (_cat_pr, "pr-probe",       "perform probing", false,                                                                                         optionListPtr, &opt_probe),
    pr_uip            (_cat_pr, "pr-uips",        "perform learning if a conflict occurs up to x-th UIP (-1 = all )", -1, IntRange(-1, INT32_MAX),                  optionListPtr, &pr_probe),
    opt_pr_probeBinary(_cat_pr, "pr-bins",        "use binary clauses for probing", true,                                                                           optionListPtr, &pr_probe),
    pr_double         (_cat_pr, "pr-double",      "perform double look-ahead", true,                                                                                optionListPtr, &pr_probe),
    pr_rootsOnly      (_cat_pr, "pr-roots",       "probe only on root literals", true,                                                                              optionListPtr, &pr_probe),
    pr_repeat         (_cat_pr, "pr-repeat",      "repeat probing if changes have been applied", false,                                                             optionListPtr, &pr_probe),
    pr_clsSize        (_cat_pr, "pr-csize",       "size of clauses that are considered for probing/vivification (propagation)", INT32_MAX,  IntRange(0, INT32_MAX), optionListPtr, &pr_probe),
    pr_LHBR           (_cat_pr, "pr-lhbr",        "perform lhbr during probing", true,                                                                              optionListPtr, &pr_probe),
    pr_prLimit        (_cat_pr, "pr-probeL",      "step limit for probing", 200000,  IntRange(0, INT32_MAX),                                                        optionListPtr, &pr_probe),
    pr_EE             (_cat_pr, "pr-EE",          "run equivalent literal detection", true,                                                                         optionListPtr, &pr_probe),
    pr_vivi           (_cat_pr, "pr-vivi",        "perform clause vivification", false,                                                                             optionListPtr, &opt_probe),
    pr_keepLearnts    (_cat_pr, "pr-keepL",       "keep conflict clauses in solver (0=no,1=learnt,2=original)", 2, IntRange(0, 2),                                  optionListPtr, &opt_probe),
    pr_keepImplied    (_cat_pr, "pr-keepI",       "keep clauses that imply on level 1 (0=no,1=learnt,2=original)", 2, IntRange(0, 2),                               optionListPtr, &opt_probe),
    pr_viviPercent    (_cat_pr, "pr-viviP",       "percent of max. clause size for clause vivification", 80, IntRange(0, 100),                                      optionListPtr, &pr_vivi),
    pr_viviLimit      (_cat_pr, "pr-viviL",       "step limit for clause vivification", 5000000,  IntRange(0, INT32_MAX),                                           optionListPtr, &pr_vivi),
    pr_opt_inpStepInc1(_cat_pr, "cp3_pr_inpInc",  "increase for steps per inprocess call", 1000000, IntRange(0, INT32_MAX),                                         optionListPtr, &pr_probe),
    pr_opt_inpStepInc2(_cat_pr, "cp3_viv_inpInc", "increase for steps per inprocess call", 1000000, IntRange(0, INT32_MAX),                                         optionListPtr, &pr_vivi),
    pr_keepLHBRs      (_cat_pr, "pr-keepLHBR",    "keep clauses that have been created during LHBR during probing/vivification (0=no,1=learnt)", 0, IntRange(0, 1), optionListPtr, &opt_probe),
    pr_necBinaries    (_cat_pr, "pr-nce",         "generate L2 necessary assignments as binary clauses", true,                                                      optionListPtr, &opt_probe),
    opt_probe_vars    (_cat,    "cp3_probe_vars", "variable limit to enable PROBING", 3000000, IntRange(0, INT32_MAX),                                              optionListPtr, &pr_probe),
    opt_probe_cls     (_cat,    "cp3_probe_cls",  "clause limit to enable PROBING",   3000000, IntRange(0, INT32_MAX),                                              optionListPtr, &pr_probe),
    opt_probe_lits    (_cat,    "cp3_probe_lits", "total literal limit to enable PROBING",   30000000, IntRange(0, INT32_MAX),                                      optionListPtr, &pr_probe),
    opt_viv_vars      (_cat,    "cp3_viv_vars",   "variable limit to enable VIVIFICATION", 5000000, IntRange(0, INT32_MAX),                                         optionListPtr, &pr_vivi),
    opt_viv_cls       (_cat,    "cp3_viv_cls",    "clause limit to enable VIVIFICATION",   10000000, IntRange(0, INT32_MAX),                                        optionListPtr, &pr_vivi),
    opt_viv_lits      (_cat,    "cp3_viv_lits",   "total literal limit to enable VIVIFICATION",   15000000, IntRange(0, INT32_MAX),                                 optionListPtr, &pr_vivi),
    #ifndef NDEBUG
    pr_debug_out      (_cat_pr, "pr-debug",       "debug output for probing", 0, IntRange(0, 4) ,                                                                   optionListPtr, &opt_probe),
    #endif

    //
    // Unit Propagation
    //
    #ifndef NDEBUG
    up_debug_out(_cat_up, "up-debug", "debug output for propagation", 0, IntRange(0, 4) , optionListPtr, &opt_up),
    #endif

    //
    // Resolution and Redundancy Addition
    //
    opt_res3_use_binaries(_cat_res, "cp3_res_bin",      "resolve with binary clauses", false,                                                                optionListPtr, &opt_ternResolve),
    opt_res3_steps       (_cat_res, "cp3_res3_steps",   "Number of resolution-attempts that are allowed per iteration", 1000000, IntRange(0, INT32_MAX - 1), optionListPtr, &opt_ternResolve),
    opt_res3_newCls      (_cat_res, "cp3_res3_ncls",    "Max. Number of newly created clauses", 100000, IntRange(0, INT32_MAX - 1),                          optionListPtr, &opt_ternResolve),
    opt_res3_reAdd       (_cat_res, "cp3_res3_reAdd",   "Add variables of newly created resolvents back to working queues", false,                           optionListPtr, &opt_ternResolve),
    opt_res3_use_subs    (_cat_res, "cp3_res_eagerSub", "perform eager subsumption", true,                                                                   optionListPtr, &opt_ternResolve),
    opt_add2_percent     (_cat_res, "cp3_res_percent",  "produce this percent many new clauses out of the total", 0.01, DoubleRange(0, true, 1, true),       optionListPtr, &opt_ternResolve),
    opt_add2_red         (_cat_res, "cp3_res_add_red",  "add redundant binary clauses", false,                                                               optionListPtr, &opt_ternResolve),
    opt_add2_red_level   (_cat_res, "cp3_res_add_lev",  "calculate added percent based on level", true,                                                      optionListPtr, &opt_ternResolve),
    opt_add2_red_lea     (_cat_res, "cp3_res_add_lea",  "add redundants based on learneds as well?", false,                                                  optionListPtr, &opt_ternResolve),
    opt_add2_red_start   (_cat_res, "cp3_res_ars",      "also before preprocessing?", false,                                                                 optionListPtr, &opt_ternResolve),
    opt_res3_inpStepInc  (_cat_res, "cp3_res_inpInc",   "increase for steps per inprocess call", 200000, IntRange(0, INT32_MAX),                             optionListPtr, &opt_ternResolve),
    opt_add2_inpStepInc  (_cat_res, "cp3_add_inpInc",   "increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX),                              optionListPtr, &opt_add2_red),
    // enable this parameter only during debug!
    #ifndef NDEBUG
    res3_debug_out       (_cat_res, "cp3_res_debug",    "print debug output to screen", false,                                                               optionListPtr, &opt_ternResolve),
    #endif

    //
    // Rewriter
    //
    opt_rew_min            (_cat_rew, "cp3_rew_min",      "min occurrence to be considered", 3, IntRange(0, INT32_MAX),                    optionListPtr, &opt_rew),
    opt_rew_iter           (_cat_rew, "cp3_rew_iter",     "number of iterations", 1, IntRange(0, 64),                                      optionListPtr, &opt_rew),
    opt_rew_minAMO         (_cat_rew, "cp3_rew_minA",     "min size of altered AMOs", 3, IntRange(0, INT32_MAX),                           optionListPtr, &opt_rew),
    opt_rew_limit          (_cat_rew, "cp3_rew_limit",    "number of steps allowed for REW", 1200000, IntRange(0, INT32_MAX),              optionListPtr, &opt_rew),
    opt_rew_Varlimit       (_cat_rew, "cp3_rew_Vlimit",   "max number of variables to still perform REW", 1000000, IntRange(0, INT32_MAX), optionListPtr, &opt_rew),
    opt_rew_Addlimit       (_cat_rew, "cp3_rew_Addlimit", "number of new variables being allowed", 100000, IntRange(0, INT32_MAX),         optionListPtr, &opt_rew),
    opt_rew_amo            (_cat_rew, "cp3_rew_amo",      "rewrite amos", true,                                                            optionListPtr, &opt_rew),
    opt_rew_imp            (_cat_rew, "cp3_rew_imp",      "rewrite implication chains", false,                                             optionListPtr, &opt_rew),
    opt_rew_scan_exo       (_cat_rew, "cp3_rew_exo",      "scan for encoded exactly once constraints first", true,                         optionListPtr, &opt_rew),
    opt_rew_merge_amo      (_cat_rew, "cp3_rew_merge",    "merge AMO constraints to create larger AMOs (fourier motzkin)", false,          optionListPtr, &opt_rew),
    opt_rew_rem_first      (_cat_rew, "cp3_rew_1st",      "how to find AMOs", false,                                                       optionListPtr, &opt_rew),
    opt_rew_avg            (_cat_rew, "cp3_rew_avg",      "use AMOs above equal average only?", true,                                      optionListPtr, &opt_rew),
    opt_rew_ratio          (_cat_rew, "cp3_rew_ratio",    "allow literals in AMO only, if their complement is not more frequent", true,    optionListPtr, &opt_rew),
    opt_rew_once           (_cat_rew, "cp3_rew_once",     "rewrite each variable at most once! (currently: yes only!) #NoAutoT", true,              optionListPtr, &opt_rew),
    opt_rew_stat_only      (_cat_rew, "cp3_rew_stats",    "analyze formula, but do not apply rewriting", false ,                           optionListPtr, &opt_rew),
    opt_rew_min_imp_size   (_cat_rew, "cp3_rewI_min",     "min size of an inplication chain to be rewritten", 4, IntRange(0, INT32_MAX),   optionListPtr, &opt_rew),
    opt_rew_impl_pref_small(_cat_rew, "cp3_rewI_small",   "prefer little imply variables", true,                                           optionListPtr, &opt_rew),
    opt_rew_inpStepInc     (_cat_rew, "cp3_rew_inpInc",   "increase for steps per inprocess call", 60000, IntRange(0, INT32_MAX),          optionListPtr, &opt_rew),
    #ifndef NDEBUG
    rew_debug_out          (_cat_rew, "rew-debug",        "Debug Output of Rewriter", 0, IntRange(0, 4),                                   optionListPtr, &opt_rew),
    #endif

    //
    // Shuffle
    //
    opt_shuffle_seed(_cat_shuffle,  "shuffle-seed",  "seed for shuffling",  0, IntRange(0, INT32_MAX), optionListPtr, &opt_shuffle),
    opt_shuffle_order(_cat_shuffle, "shuffle-order", "shuffle the order of the clauses", true,         optionListPtr, &opt_shuffle),
    #ifndef NDEBUG
    shuffle_debug_out(_cat_shuffle, "shuffle-debug", "Debug Output of Shuffler", 0, IntRange(0, 4),    optionListPtr, &opt_shuffle),
    #endif

    //
    // Sls
    //
    #ifndef NDEBUG
    opt_sls_debug     (_cat_sls, "sls-debug",      "Print SLS debug output", false,                                                                               optionListPtr, &opt_sls),
    #endif
    opt_sls_ksat_flips(_cat_sls, "sls-ksat-flips", "how many flips should be performed, if k-sat is detected (-1 = infinite)", 20000000, IntRange(-1, INT32_MAX), optionListPtr, &opt_sls),
    opt_sls_rand_walk (_cat_sls, "sls-rnd-walk",   "probability of random walk (0-10000)", 2000, IntRange(0, 10000),                                              optionListPtr, &opt_sls),
    opt_sls_adopt     (_cat_sls, "sls-adopt-cls",  "reduce nr of flips for large instances", false,                                                               optionListPtr, &opt_sls),

    //
    // Subsumption
    //
    opt_sub_naivStrength   (_cat_sub, "naive_strength",   "use naive strengthening", false,                                                                                                   optionListPtr, &opt_subsimp),
    opt_sub_allStrengthRes (_cat_sub, "all_strength_res", "Create all self-subsuming resolvents of clauses less equal given size (prob. slow & blowup, only seq)", 0, IntRange(0, INT32_MAX), optionListPtr, &opt_subsimp),
    opt_sub_strength       (_cat_sub, "cp3_strength",     "Perform clause strengthening", true,                                                                                               optionListPtr, &opt_subsimp),
    opt_sub_preferLearned  (_cat_sub, "cp3_inpPrefL",     "During inprocessing, check learned clauses first!", true,                                                                          optionListPtr, &opt_subsimp),
    opt_sub_subLimit       (_cat_sub, "cp3_sub_limit",    "limit of subsumption steps",   300000000, IntRange(0, INT32_MAX),                                                                  optionListPtr, &opt_subsimp),
    opt_sub_strLimit       (_cat_sub, "cp3_str_limit",    "limit of strengthening steps", 300000000, IntRange(0, INT32_MAX),                                                                  optionListPtr, &opt_subsimp),
    opt_sub_callIncrease   (_cat_sub, "cp3_call_inc",     "max. limit increase per process call (subsimp is frequently called from other techniques)", 200, IntRange(0, INT32_MAX),           optionListPtr, &opt_subsimp),
    opt_sub_inpStepInc     (_cat_sub, "cp3_sub_inpInc",   "increase for steps per inprocess call", 40000000, IntRange(0, INT32_MAX),                                                          optionListPtr, &opt_subsimp),

    opt_sub_par_strength   (_cat_sub, "cp3_par_strength", "par strengthening: 0 never, 1 heuristic, 2 always #NoAutoT", 1, IntRange(0, 2),                                                             optionListPtr, &opt_subsimp),
    opt_sub_lock_stats     (_cat_sub, "cp3_lock_stats",   "measure time waiting in spin locks #NoAutoT", false,                                                                               optionListPtr, &opt_subsimp),
    opt_sub_par_subs       (_cat_sub, "cp3_par_subs",     "par subsumption: 0 never, 1 heuristic, 2 always #NoAutoT", 1, IntRange(0, 2),                                                               optionListPtr, &opt_subsimp),
    opt_sub_par_subs_counts(_cat_sub, "par_subs_counts",  "Updates of counts in par-subs 0: compare_xchange, 1: CRef-vector #NoAutoT", 1, IntRange(0, 1),                                              optionListPtr, &opt_subsimp),
    opt_sub_chunk_size     (_cat_sub, "susi_chunk_size",  "Size of Par SuSi Chunks #NoAutoT", 100000, IntRange(1, INT32_MAX),                                                                          optionListPtr, &opt_subsimp),
    opt_sub_par_str_minCls (_cat_sub, "par_str_minCls",   "number of clauses to start parallel strengthening #NoAutoT", 250000, IntRange(1, INT32_MAX),                                                optionListPtr, &opt_subsimp),

    #ifndef NDEBUG
    opt_sub_debug          (_cat_sub, "susi_debug",       "Debug Output for Subsumption", 0, IntRange(0, 3),                                                                                  optionListPtr, &opt_subsimp),
    #endif

    //
    // Symmetry Breaker
    //
    sym_opt_hsize          (_cat_sym, "sym-size",    "scale with the size of the clause", false,                                                                                       optionListPtr, &opt_symm),
    sym_opt_hpol           (_cat_sym, "sym-pol",     "consider the polarity of the occurrences", false,                                                                                optionListPtr, &opt_symm),
    // there should be a parameter delay-units already!
    sym_opt_hpushUnit      (_cat_sym, "sym-unit",    "ignore unit clauses", false,                                                                                                     optionListPtr, &opt_symm), 
    sym_opt_hmin           (_cat_sym, "sym-min",     "minimum symmtry to be exploited", 2, IntRange(1, INT32_MAX) ,                                                                    optionListPtr, &opt_symm),
    sym_opt_hratio         (_cat_sym, "sym-ratio",   "only consider a variable if it appears close to the average of variable occurrences", 0.4, DoubleRange(0, true, HUGE_VAL, true), optionListPtr, &opt_symm),
    sym_opt_iter           (_cat_sym, "sym-iter",    "number of symmetry approximation iterations", 3, IntRange(0, INT32_MAX) ,                                                        optionListPtr, &opt_symm),
    sym_opt_pairs          (_cat_sym, "sym-show",    "show symmetry pairs", false,                                                                                                     optionListPtr, &opt_symm),
    sym_opt_print          (_cat_sym, "sym-print",   "show the data for each variable", false,                                                                                         optionListPtr, &opt_symm),
    sym_opt_exit           (_cat_sym, "sym-exit",    "exit after analysis #NoAutoT", false,                                                                                            optionListPtr, &opt_symm),
    sym_opt_hprop          (_cat_sym, "sym-prop",    "try to generate symmetry breaking clauses with propagation", false,                                                              optionListPtr, &opt_symm),
    sym_opt_hpropF         (_cat_sym, "sym-propF",   "generate full clauses", false,                                                                                                   optionListPtr, &opt_symm),
    sym_opt_hpropA         (_cat_sym, "sym-propA",   "test all four casese instead of two", false,                                                                                     optionListPtr, &opt_symm),
    sym_opt_cleanLearn     (_cat_sym, "sym-clLearn", "clean the learned clauses that have been created during symmetry search", false,                                                 optionListPtr, &opt_symm),
    sym_opt_conflicts      (_cat_sym, "sym-cons",    "number of conflicts for looking for being implied", 0, IntRange(0, INT32_MAX) ,                                                  optionListPtr, &opt_symm),
    sym_opt_total_conflicts(_cat_sym, "sym-consT",   "number of total conflicts for looking for being implied", 10000, IntRange(0, INT32_MAX) ,                                        optionListPtr, &opt_symm),
    #ifndef NDEBUG
    sym_debug_out          (_cat_sym, "sym-debug",   "debug output for probing", 0, IntRange(0, 4) ,                                                                                   optionListPtr, &opt_symm),
    #endif   //
    // Unhide
    //
    opt_uhd_Iters     (_cat_uhd, "cp3_uhdIters",     "Number of iterations for unhiding", 3, IntRange(0, INT32_MAX),                                                 optionListPtr, &opt_unhide),
    opt_uhd_Trans     (_cat_uhd, "cp3_uhdTrans",     "Use Transitive Graph Reduction (buggy)", false,                                                                optionListPtr, &opt_unhide),
    opt_uhd_UHLE      (_cat_uhd, "cp3_uhdUHLE",      "Use Unhiding+Hidden Literal Elimination",  3, IntRange(0, 3),                                                  optionListPtr, &opt_unhide),
    opt_uhd_UHTE      (_cat_uhd, "cp3_uhdUHTE",      "Use Unhiding+Hidden Tautology Elimination", true,                                                              optionListPtr, &opt_unhide),
    opt_uhd_NoShuffle (_cat_uhd, "cp3_uhdNoShuffle", "Do not perform randomized graph traversation", false,                                                          optionListPtr, &opt_unhide),
    opt_uhd_EE        (_cat_uhd, "cp3_uhdEE",        "Use equivalent literal elimination", false,                                                                    optionListPtr, &opt_unhide),
    opt_uhd_TestDbl   (_cat_uhd, "cp3_uhdTstDbl",    "Test for duplicate binary clauses", false,                                                                     optionListPtr, &opt_unhide),
    opt_uhd_probe     (_cat_uhd, "cp3_uhdProbe",     "Approximate probing (bin cls) with stamp info (off,constant,linear,quadratic,exponential)", 0, IntRange(0, 4), optionListPtr, &opt_unhide),
    opt_uhd_fullProbe (_cat_uhd, "cp3_uhdPrSize",    "Enable unhide probing for larger clauses, size <= given parameter", 2, IntRange(2, INT32_MAX),                 optionListPtr, &opt_uhd_probe),
    opt_uhd_probeEE   (_cat_uhd, "cp3_uhdPrEE",      "Find Equivalences during uhd probing (requ. uhdProbe > 1)", false,                                             optionListPtr, &opt_uhd_probe),
    opt_uhd_fullBorder(_cat_uhd, "cp3_uhdPrSiBo",    "Check larger clauses only in first and last iteration", true,                                                  optionListPtr, &opt_uhd_probe),
    #ifndef NDEBUG
    opt_uhd_Debug     (_cat_uhd, "cp3_uhdDebug",     "debug level of unhiding", 0, IntRange(0, 6),                                                                   optionListPtr, &opt_unhide),
    #endif


    //
    // Xor
    //
    opt_xorMatchLimit      (_cat_xor, "xorMaxSize",   "Maximum Clause Size for detecting XOrs (high number consume much memory!)", 12, IntRange(3, 63), optionListPtr, &opt_xor),
    opt_xorFindLimit       (_cat_xor, "xorLimit",     "number of checks for finding xors", 1200000, IntRange(0, INT32_MAX),                             optionListPtr, &opt_xor),
    opt_xor_selectX        (_cat_xor, "xorSelect",    "how to select next xor 0=first,1=smallest", 0, IntRange(0, 1),                                   optionListPtr, &opt_xor),
    opt_xor_keepUsed       (_cat_xor, "xorKeepUsed",  "continue to simplify kept xors", true,                                                           optionListPtr, &opt_xor),
    opt_xor_findSubsumed   (_cat_xor, "xorFindSubs",  "try to recover XORs that are partially subsumed", true,                                          optionListPtr, &opt_xor),
    opt_xor_findResolved   (_cat_xor, "xorFindRes",   "try to recover XORs including resolution steps", false,                                          optionListPtr, &opt_xor),
    opt_xor_backdoor       (_cat_xor, "xorBackdoor",  "work on XOR backdoor, is size is smaller equal", 0, IntRange(0, INT32_MAX),                      optionListPtr, &opt_xor),
    opt_xor_dropPure       (_cat_xor, "xorDropPure",  "drop XORs with a literal that occurs only once", false,                                          optionListPtr, &opt_xor),
    opt_xor_encodeSize     (_cat_xor, "xorEncSize",   "size of xors that are encoded back (<=2 ^= none)", 2, IntRange(1, 4),                            optionListPtr, &opt_xor),
    opt_xor_checkNewSubsume(_cat_xor, "xorEncSubs",   "perform subsumption checks with newly added XOR clauses", false,                                 optionListPtr, &opt_xor),
    opt_xor_addAsLearnt    (_cat_xor, "xorEncL",      "add clause to encode XOR as learnt clause", false,                                               optionListPtr, &opt_xor),
    opt_xor_setPolarity    (_cat_xor, "xorSetPol",    "set default polarities based on XOR elimination order and UP(-1=neg,1=pos)", 0, IntRange(-1, 1), optionListPtr, &opt_xor),
    opt_xor_addOnNewlyAdded(_cat_xor, "xorAddNew",    "add simplified XORs to list of variables that have been added during add #NoAutoT", false,       optionListPtr, &opt_xor),

    #ifndef NDEBUG
    opt_xor_debug          (_cat_xor, "xor-debug",       "Debug Output of XOR reasoning", 0, IntRange(0, 5),                                            optionListPtr, &opt_xor),
    #endif
    dummy(0)
{
    if (defaultPreset.size() != 0) { setPreset(defaultPreset); }    // set configuration options immediately
}

// *INDENT-ON*

} // namespace Coprocessor
