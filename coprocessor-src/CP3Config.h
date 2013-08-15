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
BoolOption opt_dense_store_forward;

//
// Entailed
//
IntOption  opt_entailed_minClsSize;
#if defined CP3VERSION 
const int entailed_debug;
#else
IntOption  entailed_debug;
#endif


//
// Equivalence
//
#if defined CP3VERSION  && CP3VERSION < 350
const int opt_ee_level            ;
const int opt_ee_gate_limit       ;
const int opt_ee_circuit_iters    ;
const bool opt_ee_eagerEquivalence;
const bool opt_eeGateBigFirst     ;
const char* opt_ee_aagFile        ;
#else
IntOption  opt_ee_level           ;
IntOption  opt_ee_gate_limit      ;
IntOption  opt_ee_circuit_iters   ;
BoolOption opt_ee_eagerEquivalence;
BoolOption opt_eeGateBigFirst     ;
StringOption opt_ee_aagFile       ;
#endif
#if defined CP3VERSION  
const int ee_debug_out;
#else
IntOption  ee_debug_out           ;
#endif
BoolOption opt_eeSub            ;
BoolOption opt_eeFullReset      ;
IntOption  opt_ee_limit         ;
IntOption  opt_ee_inpStepInc    ;
IntOption  opt_ee_bigIters      ;
BoolOption opt_ee_iterative     ;
BoolOption opt_EE_checkNewSub   ;
BoolOption opt_ee_eager_frozen  ;

//
// Fourier Motzkin
//
IntOption  opt_fmLimit    ;
IntOption  opt_fmGrow     ;
IntOption  opt_fmGrowT    ;
BoolOption opt_atMostTwo  ;
BoolOption opt_findUnit   ;
BoolOption opt_merge      ;
BoolOption opt_duplicates ;
BoolOption opt_cutOff     ;
IntOption opt_newAmo      ;
BoolOption opt_keepAllNew ;
IntOption opt_newAlo      ;
IntOption opt_newAlk      ;
BoolOption opt_checkSub   ;
BoolOption opt_rem_first  ;
#if defined CP3VERSION 
const int fm_debug_out;
#else
IntOption fm_debug_out       ;
#endif

//
// Hidden Tautology Elimination
//
IntOption opt_hte_steps;
#if defined CP3VERSION && CP3VERSION < 302
const bool opt_par_hte;
#else
BoolOption opt_par_hte;
#endif
#if defined CP3VERSION  
const int hte_debug_out;
const bool opt_hteTalk;
#else
IntOption hte_debug_out;
BoolOption opt_hteTalk ;
#endif
IntOption  opt_hte_inpStepInc;

//
// Probing
//
IntOption pr_uip;
BoolOption pr_double     ;
BoolOption pr_probe      ;
BoolOption pr_rootsOnly  ;
BoolOption pr_repeat     ;
IntOption pr_clsSize     ;
IntOption pr_prLimit     ;
BoolOption pr_EE         ;
BoolOption pr_vivi       ;
IntOption pr_keepLearnts ;
IntOption pr_keepImplied ;
IntOption pr_viviPercent ;
IntOption pr_viviLimit   ;
IntOption  pr_opt_inpStepInc1      ;
IntOption  pr_opt_inpStepInc2      ;
#if defined CP3VERSION  
const int pr_debug_out;
#else
IntOption pr_debug_out;
#endif

//
// Unit Propagation
//
#if defined CP3VERSION  
const int up_debug_out;
#else
IntOption up_debug_out;
#endif

//
// Resolution and Redundancy Addition
//
BoolOption   opt_res3_use_binaries ;
IntOption    opt_res3_steps    ;
IntOption    opt_res3_newCls   ;
BoolOption   opt_res3_reAdd    ;
BoolOption   opt_res3_use_subs ;
DoubleOption opt_add2_percent  ;
BoolOption   opt_add2_red      ;
BoolOption   opt_add2_red_level;
BoolOption   opt_add2_red_lea  ;
BoolOption   opt_add2_red_start;
IntOption  opt_res3_inpStepInc ;
IntOption  opt_add2_inpStepInc ;
/// enable this parameter only during debug!
#if defined CP3VERSION  
const bool res3_debug_out (false);
#else
BoolOption res3_debug_out      ;
#endif

//
// Rewriter
//
 IntOption  opt_rew_min   ;   
 IntOption  opt_rew_iter   ;  
 IntOption  opt_rew_minAMO ;  
 IntOption  opt_rew_limit  ;  
 IntOption  opt_rew_Varlimit ;
 IntOption  opt_rew_Addlimit ;
 BoolOption opt_rew_amo    ;  
 BoolOption opt_rew_imp    ;  
 BoolOption opt_rew_scan_exo ;
 BoolOption opt_rew_merge_amo;
 BoolOption opt_rew_rem_first;
 BoolOption opt_rew_avg     ; 
 BoolOption opt_rew_ratio  ;  
 BoolOption opt_rew_once     ;
 BoolOption opt_rew_stat_only;
 IntOption  opt_rew_min_imp_size     ;   
 BoolOption opt_rew_impl_pref_small   ;  
 IntOption  opt_rew_inpStepInc     ;
#if defined CP3VERSION 
 const int rew_debug_out;
#else
 IntOption rew_debug_out;           
#endif
 
//
// Shuffle
//
IntOption opt_shuffle_seed;
BoolOption opt_shuffle_order;
#if defined CP3VERSION  
const int shuffle_debug_out;
#else
IntOption shuffle_debug_out;
#endif

//
// Sls
//
#if defined CP3VERSION 
const bool opt_sls_debug;
#else
BoolOption opt_sls_debug ;
#endif
IntOption  opt_sls_ksat_flips ;
IntOption  opt_sls_rand_walk  ;
BoolOption opt_sls_adopt      ;

//
// Subsumption
//
 BoolOption  opt_sub_naivStrength;
 IntOption   opt_sub_allStrengthRes; 
 BoolOption  opt_sub_strength     ;
 BoolOption  opt_sub_preferLearned; 
 IntOption   opt_sub_subLimit     ; 
 IntOption   opt_sub_strLimit     ; 
 IntOption   opt_sub_callIncrease ; 
 IntOption  opt_sub_inpStepInc    ;
#if defined CP3VERSION && CP3VERSION < 302
 const int   opt_sub_par_strength ;
 const bool  opt_sub_lock_stats   ;
 const int   opt_sub_par_subs     ;
 const int   opt_sub_par_subs_counts;
 const int   opt_sub_chunk_size     ;
 const int   opt_sub_par_str_minCls ;
#else
 IntOption   opt_sub_par_strength   ;
 BoolOption  opt_sub_lock_stats     ;
 IntOption   opt_sub_par_subs       ;
 IntOption   opt_sub_par_subs_counts;
 IntOption   opt_sub_chunk_size     ;
 IntOption   opt_sub_par_str_minCls ;
#endif
#if defined CP3VERSION
 const int opt_sub_debug;
#else
 IntOption   opt_sub_debug  ;
#endif

//
// Symmetry Breaker
//
 BoolOption    sym_opt_hsize          ;
 BoolOption    sym_opt_hpol           ;
 BoolOption    sym_opt_hpushUnit      ; // there should be a parameter delay-units already!
 IntOption     sym_opt_hmin           ;
 DoubleOption  sym_opt_hratio         ;
 IntOption     sym_opt_iter           ;
 BoolOption    sym_opt_pairs          ;
 BoolOption    sym_opt_print          ;
 BoolOption    sym_opt_exit           ;
 BoolOption    sym_opt_hprop          ;
 BoolOption    sym_opt_hpropF         ;
 BoolOption    sym_opt_hpropA         ;
 BoolOption    sym_opt_cleanLearn     ;
 IntOption     sym_opt_conflicts      ;
 IntOption     sym_opt_total_conflicts;
#if defined CP3VERSION  
 const int sym_debug_out;
#else
 IntOption sym_debug_out;
#endif
 
//
// Twosat
//
#if defined CP3VERSION 
 const int twosat_debug_out;
 const bool twosat_useUnits;
 const bool twosat_clearQueue;
#else
 IntOption twosat_debug_out  ;
 BoolOption twosat_useUnits  ;
 BoolOption twosat_clearQueue;
#endif
 
//
// Unhide
//
 IntOption  opt_uhd_Iters     ;
 BoolOption opt_uhd_Trans     ;
 IntOption  opt_uhd_UHLE      ;
 BoolOption opt_uhd_UHTE      ;
 BoolOption opt_uhd_NoShuffle ;
 BoolOption opt_uhd_EE        ;
 BoolOption opt_uhd_TestDbl   ;
#if defined CP3VERSION  
 const int opt_uhd_Debug;
#else
 IntOption  opt_uhd_Debug;
#endif
 
//
// Xor
//
 IntOption  opt_xorMatchLimit ;
 IntOption  opt_xorFindLimit  ;
 IntOption  opt_xor_selectX     ;
 BoolOption opt_xor_keepUsed    ;
 BoolOption opt_xor_findSubsumed;
 BoolOption opt_xor_findResolved;
#if defined CP3VERSION 
 const int opt_xor_debug;
#else
 IntOption  opt_xor_debug;
#endif
};
 
}

#endif