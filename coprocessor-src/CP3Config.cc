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
  printAfter ( 0)
#else
  opt_debug       (_cat, "cp3-debug",   "do more debugging", false),
  opt_check       (_cat, "cp3-check",   "check solver state before returning control to solver", false),
   opt_log         (_cat,  "cp3-log",    "Output log messages until given level", 0, IntRange(0, 3)),
  printAfter    (_cat,  "cp3-print",  "print intermediate formula after given technique")
#endif
{}

void CP3Config::parseOptions(int& argc, char** argv, bool strict)
{
 ::parseOptions (argc, argv, strict ); // simply parse all options
}

bool CP3Config::checkConfiguration()
{
  return true;
}
