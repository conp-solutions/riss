/**************************************************************************************[CPConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef CPConfig_h
#define CPConfig_h

#include "utils/Options.h"

using namespace Minisat;

namespace Coprocessor {

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CP3Config {
 
public:
 /** default constructor, which sets up all options in their standard format */
 CP3Config ();

 /** parse all options from the command line */
 void parseOptions (int& argc, char** argv, bool strict = false);
 
 /** checks all specified constraints */
 bool checkConfiguration();
 
 /** 
 * List of all used options, public members, can be changed and read directly
 */
 
// options
 BoolOption opt_unlimited  ;
 BoolOption opt_randomized  ;
 IntOption  opt_inprocessInt;
 BoolOption opt_enabled     ;
 BoolOption opt_inprocess   ;
 IntOption  opt_exit_pp     ;
 BoolOption opt_randInp     ;
 BoolOption opt_inc_inp     ;

#if defined CP3VERSION && CP3VERSION < 400
       const bool opt_printStats; // do not print stats, if restricted binary is produced
       const  int opt_verbose;        // do not talk during computation!
#else
       BoolOption opt_printStats;
       IntOption  opt_verbose;
#endif

// techniques
 BoolOption opt_up          ;
 BoolOption opt_subsimp     ;
 BoolOption opt_hte         ;
 BoolOption opt_bce         ;
 BoolOption opt_ent         ;
 BoolOption opt_cce         ;
 BoolOption opt_ee          ;
 BoolOption opt_bve         ;
 BoolOption opt_bva         ;
 BoolOption opt_unhide      ;
 BoolOption opt_probe       ;
 BoolOption opt_ternResolve ;
 BoolOption opt_addRedBins  ;
 BoolOption opt_dense       ;
 BoolOption opt_shuffle     ;
 BoolOption opt_simplify    ;
 BoolOption opt_symm        ;
 BoolOption opt_FM          ;


 StringOption opt_ptechs ;
 StringOption opt_itechs ;

// use 2sat and sls only for high versions!
#if defined CP3VERSION && CP3VERSION < 301
 const int opt_threads;
 const bool opt_sls;
 const bool opt_sls_phase;
 const int opt_sls_flips;
 const bool opt_xor;
 const bool opt_rew;    
 const bool opt_twosat;
 const bool opt_twosat_init;
 const bool  opt_ts_phase;
#else
 IntOption  opt_threads     ;
 BoolOption opt_sls         ;
 BoolOption opt_sls_phase   ;
 IntOption  opt_sls_flips   ;
 BoolOption opt_xor         ;
 BoolOption opt_rew         ;
 BoolOption opt_twosat      ;
 BoolOption opt_twosat_init ;
 BoolOption opt_ts_phase    ;
#endif

#if defined CP3VERSION // debug only, if no version is given!
 const bool opt_debug;       
 const bool opt_check;
 const int  opt_log;
 const char* printAfter;
#else
 BoolOption opt_debug    ;
 BoolOption opt_check    ;
 IntOption  opt_log      ;
 StringOption printAfter ;
#endif
 
//
// BVE
//

#if defined CP3VERSION && CP3VERSION < 302
  const bool opt_par_bve    ;
  const int  opt_bve_verbose;
#else
 IntOption opt_par_bve         ;
 IntOption  opt_bve_verbose     ;
#endif
 
 IntOption  opt_bve_limit       ;
 IntOption  opt_learnt_growth   ;
 IntOption  opt_resolve_learnts ;
 BoolOption opt_unlimited_bve   ;
 BoolOption opt_bve_strength    ;
 IntOption  opt_bve_lits        ;
 BoolOption opt_bve_findGate    ;
 BoolOption opt_force_gates     ;
 // pick order of eliminations
 IntOption  opt_bve_heap        ;
 // increasing eliminations
 IntOption  opt_bve_grow        ;
 IntOption  opt_bve_growTotal   ;
 BoolOption opt_totalGrow       ;
  
 BoolOption opt_bve_bc          ;
 IntOption heap_updates         ;
 BoolOption opt_bce_only        ;
 BoolOption opt_print_progress  ;
 IntOption  opt_bveInpStepInc   ;

#if defined CP3VERSION && CP3VERSION < 302
const int par_bve_threshold ;
const int postpone_locked_neighbors ;
const bool opt_minimal_updates ;
#else
IntOption  par_bve_threshold; 
IntOption  postpone_locked_neighbors;
BoolOption opt_minimal_updates;
#endif

//
// BVA
//
 IntOption  opt_bva_push             ;
 IntOption  opt_bva_VarLimit         ;
 IntOption  opt_bva_Alimit           ;
 BoolOption opt_Abva                 ;
 IntOption  opt_bvaInpStepInc        ;
 IntOption  opt_Abva_heap            ;
 BoolOption opt_bvaComplement        ;
 BoolOption opt_bvaRemoveDubplicates ;
 BoolOption opt_bvaSubstituteOr      ;
#if defined CP3VERSION  
 const int bva_debug;
#else
 IntOption  bva_debug                ;
#endif

#if defined CP3VERSION  && CP3VERSION < 350
 const bool opt_bvaAnalysis = false;
 const bool opt_Xbva = 0;
 const bool opt_Ibva = 0;
 const int opt_bvaAnalysisDebug = 0;
 const int opt_bva_Xlimit = 100000000;
 const int opt_bva_Ilimit = 100000000;
 const int opt_Xbva_heap = 1;
 const int opt_Ibva_heap = 1;
#else
 IntOption  opt_bvaAnalysisDebug     ;
 IntOption  opt_bva_Xlimit           ;
 IntOption  opt_bva_Ilimit           ;
 IntOption  opt_Xbva_heap            ;
 IntOption  opt_Ibva_heap            ;
 IntOption  opt_Xbva                 ;
 IntOption  opt_Ibva                 ;
#endif

//
// CCE
//
IntOption opt_cceSteps;
IntOption opt_ccelevel;
IntOption opt_ccePercent;
#if defined CP3VERSION  
const int cce_debug_out;
#else
IntOption cce_debug_out;
#endif
IntOption  opt_cceInpStepInc;

//
// Dense
//
#if defined CP3VERSION  
const bool dense_debug_out;
#else
BoolOption dense_debug_out;
#endif
IntOption  opt_dense_fragmentation;

};
 
}

#endif