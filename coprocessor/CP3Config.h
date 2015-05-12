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

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"
#include "riss/utils/Debug.h"

// using namespace Riss;

namespace Coprocessor {

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CP3Config : public Config {
 
  /** pointer to all options in this object - used for parsing and printing the help! */
  vec<Option*> configOptions;
  
public:
 /** default constructor, which sets up all options in their standard format */
 CP3Config (const std::string & presetOptions = "");
 
 /** 
 * List of all used options, public members, can be changed and read directly
 */
 
// options
 IntOption opt_cp3_vars; 	// variable limit to enable CP3
 IntOption opt_cp3_cls;  	// clause limit to enable CP3
 IntOption opt_cp3_lits;
 IntOption opt_cp3_ipvars;  	// variable limit to enable CP3 inprocessing
 IntOption opt_cp3_ipcls;  	// clause limit to enable CP3 inprocessing
 IntOption opt_cp3_iplits;
 BoolOption opt_unlimited  ;
 BoolOption opt_randomized  ;
 IntOption  opt_inprocessInt;
 IntOption  opt_simplifyRounds;
 BoolOption opt_enabled     ;
 BoolOption opt_inprocess   ;
 IntOption  opt_exit_pp     ;
 BoolOption opt_randInp     ;
 BoolOption opt_inc_inp     ;

 StringOption opt_whiteList ;
 
#if defined TOOLVERSION && TOOLVERSION < 400
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
#if defined TOOLVERSION && TOOLVERSION < 355
 const bool opt_bce;
#else
 BoolOption opt_bce         ;
#endif
#if defined TOOLVERSION && TOOLVERSION < 360
  const bool opt_ent;
#else
  BoolOption opt_ent        ;
#endif
 BoolOption opt_exp         ;
 BoolOption opt_la          ;
 BoolOption opt_cce         ;
 BoolOption opt_rate        ;
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
#if defined TOOLVERSION && TOOLVERSION < 301
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

 

 IntOption opt_subsimp_vars; 	// variable limit to enable 
 IntOption opt_subsimp_cls;  	// clause limit to enable 
 IntOption opt_subsimp_lits;	// total literals limit to enable 
 IntOption opt_hte_vars; 	// variable limit to enable 
 IntOption opt_hte_cls;  	// clause limit to enable 
 IntOption opt_hte_lits;	// total literals limit to enable 
 IntOption opt_bce_vars; 	// variable limit to enable 
 IntOption opt_bce_cls;  	// clause limit to enable 
 IntOption opt_bce_lits;	// total literals limit to enable 
 IntOption opt_ent_vars; 	// variable limit to enable 
 IntOption opt_ent_cls;  	// clause limit to enable 
 IntOption opt_ent_lits;	// total literals limit to enable 
 IntOption opt_la_vars; 	// variable limit to enable 
 IntOption opt_la_cls;  	// clause limit to enable 
 IntOption opt_la_lits;	// total literals limit to enable 
 IntOption opt_cce_vars; 	// variable limit to enable 
 IntOption opt_cce_cls;  	// clause limit to enable 
 IntOption opt_cce_lits;	// total literals limit to enable 
 IntOption opt_rate_vars; 	// variable limit to enable 
 IntOption opt_rate_cls;  	// clause limit to enable 
 IntOption opt_rate_lits;	// total literals limit to enable 
 IntOption opt_ee_vars; 	// variable limit to enable 
 IntOption opt_ee_cls;  	// clause limit to enable 
 IntOption opt_ee_lits;	// total literals limit to enable 
 IntOption opt_bve_vars; 	// variable limit to enable 
 IntOption opt_bve_cls;  	// clause limit to enable 
 IntOption opt_bve_lits;	// total literals limit to enable 
 IntOption opt_bva_vars; 	// variable limit to enable 
 IntOption opt_bva_cls;  	// clause limit to enable 
 IntOption opt_bva_lits;	// total literals limit to enable 
 IntOption opt_Ibva_vars; 	// variable limit to enable 
 IntOption opt_Ibva_cls;  	// clause limit to enable 
 IntOption opt_Ibva_lits;	// total literals limit to enable 
 IntOption opt_Xbva_vars; 	// variable limit to enable 
 IntOption opt_Xbva_cls;  	// clause limit to enable 
 IntOption opt_Xbva_lits;	// total literals limit to enable 
 IntOption opt_unhide_vars; 	// variable limit to enable 
 IntOption opt_unhide_cls;  	// clause limit to enable 
 IntOption opt_unhide_lits;	// total literals limit to enable 
 IntOption opt_probe_vars; 	// variable limit to enable 
 IntOption opt_probe_cls;  	// clause limit to enable 
 IntOption opt_probe_lits;	// total literals limit to enable 
 IntOption opt_viv_vars; 	// variable limit to enable 
 IntOption opt_viv_cls;  	// clause limit to enable 
 IntOption opt_viv_lits;	// total literals limit to enable 
 IntOption opt_ternResolve_vars; 	// variable limit to enable 
 IntOption opt_ternResolve_cls;  	// clause limit to enable 
 IntOption opt_ternResolve_lits;	// total literals limit to enable 
 IntOption opt_addRedBins_vars; 	// variable limit to enable 
 IntOption opt_addRedBins_cls;  	// clause limit to enable 
 IntOption opt_addRedBins_lits;	// total literals limit to enable 
 IntOption opt_symm_vars; 	// variable limit to enable 
 IntOption opt_symm_cls;  	// clause limit to enable 
 IntOption opt_symm_lits;	// total literals limit to enable 
 IntOption opt_fm_vars; 	// variable limit to enable 
 IntOption opt_fm_cls;  	// clause limit to enable  
 IntOption opt_fm_lits;	// total literals limit to enable 
 
 IntOption opt_xor_vars; 	// variable limit to enable 
 IntOption opt_xor_cls;  	// clause limit to enable  
 IntOption opt_xor_lits;	// total literals limit to enable 
 IntOption opt_sls_vars; 	// variable limit to enable 
 IntOption opt_sls_cls;  	// clause limit to enable  
 IntOption opt_sls_lits;	// total literals limit to enable 
 IntOption opt_rew_vars; 	// variable limit to enable 
 IntOption opt_rew_cls;  	// clause limit to enable  
 IntOption opt_rew_lits;	// total literals limit to enable 
 
#ifndef NDEBUG
 BoolOption opt_debug    ;
 IntOption opt_check    ;
 IntOption  opt_log      ;
 StringOption printAfter ;
#endif
 
//
// BVE
//

 IntOption opt_par_bve         ;
 IntOption  opt_bve_verbose     ;

 IntOption  opt_bve_limit       ;
 IntOption  opt_learnt_growth   ;
 IntOption  opt_resolve_learnts ;
 BoolOption opt_unlimited_bve   ;
 BoolOption opt_bve_strength    ;
 BoolOption opt_bve_findGate    ;
 BoolOption opt_force_gates     ;
 BoolOption bve_funcDepOnly     ;
 // pick order of eliminations
 IntOption  opt_bve_heap        ;
 // increasing eliminations
 IntOption  opt_bve_grow        ;
 IntOption  opt_bve_growTotal   ;
 BoolOption opt_totalGrow       ;
  
 BoolOption opt_bve_bc          ;
 IntOption heap_updates         ;
 BoolOption opt_bve_earlyAbort  ;
 BoolOption opt_bce_only        ;
 BoolOption opt_print_progress  ;
 IntOption  opt_bveInpStepInc   ;

#if defined TOOLVERSION && TOOLVERSION < 302
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
 IntOption  opt_Abva_maxRed          ;
 IntOption  opt_bvaInpStepInc        ;
 IntOption  opt_Abva_heap            ;
 BoolOption opt_bvaComplement        ;
 BoolOption opt_bvaRemoveDubplicates ;
 BoolOption opt_bvaSubstituteOr      ;
#ifndef NDEBUG
 IntOption  bva_debug                ;
 IntOption  opt_bvaAnalysisDebug     ;
#endif

 IntOption  opt_bva_Xlimit           ;
 IntOption  opt_bva_Ilimit           ;
 IntOption  opt_Xbva_maxRed          ;
 IntOption  opt_Ibva_maxRed          ;
 IntOption  opt_Xbva_heap            ;
 IntOption  opt_Ibva_heap            ;
 IntOption  opt_Xbva                 ;
 IntOption  opt_Ibva                 ;


//
// BCE
//
BoolOption orderComplements; // sort the heap based on the occurrence of complementary literals
BoolOption bceBinary; // remove binary clauses during BCE
IntOption bceLimit;
BoolOption opt_bce_bce; // actually remove blocked clauses
BoolOption opt_bce_bcm; // minimize blocked clauses instead of eliminating them (keep the literals that are required for being blocked)
BoolOption opt_bce_cle; // perform covered literal elimination
BoolOption opt_bce_cla; // perform covered literal addition
BoolOption opt_bce_cle_conservative; // perform CLE conservative and cheap, if tautological resolvents occur
IntOption opt_bceInpStepInc; // add to limit for inprocessing
IntOption opt_bce_verbose; // output operation steps
#ifndef NDEBUG
BoolOption opt_bce_debug; // debug output
#endif

//
// LiteralAddition
//
BoolOption opt_la_cla; // perform covered literal addition
BoolOption opt_la_ala; // perform asymmetric literal addition

IntOption claLimit; // number of steps before aborting LA
IntOption claStepSize; // number of extension literals so that literals are removed randomly
IntOption claStepMax; // number of first extension literals that are considered (should be smaller then size!)
IntOption claIterations; // number of iterations to do for CLA

IntOption alaLimit; // number of steps for limits
IntOption alaIterations; // number of iterations to do for ALA
BoolOption ala_binary; // perform ALA with binary clauses
#ifndef NDEBUG
BoolOption opt_la_debug; // debug output
#endif

 
//
// CCE
//
IntOption opt_cceSteps;
IntOption opt_ccelevel;
IntOption opt_ccePercent;
#ifndef NDEBUG
IntOption cce_debug_out;
#endif
IntOption  opt_cceInpStepInc;

//
// Options for rat elimination
//
BoolOption rate_orderComplements;
Int64Option rate_Limit;
Int64Option ratm_Limit;
#ifndef NDEBUG
IntOption opt_rate_debug;
#endif
BoolOption opt_rate_brat; // test resolvent not only for AT, but also for being blocked
IntOption rate_minSize;
BoolOption opt_rate_rate;
BoolOption opt_rate_bcs; // perform blocked clause substitution
BoolOption opt_rate_ratm;
BoolOption opt_rate_ratm_extended;
BoolOption opt_rate_ratm_rounds;
 
//
// Dense
//
#ifndef NDEBUG
IntOption dense_debug_out;
#endif
IntOption  opt_dense_fragmentation;
BoolOption opt_dense_store_forward;
BoolOption opt_dense_keep_assigned;

//
// Entailed
//
#if defined TOOLVERSION && TOOLVERSION < 360
const int opt_entailed_minClsSize;
#else
IntOption opt_entailed_minClsSize;
#endif

#ifndef NDEBUG
IntOption  entailed_debug;
#endif


//
// Equivalence
//
#if defined TOOLVERSION  && TOOLVERSION < 350
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
#ifndef NDEBUG
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
// Structural hashing options
//
#if defined TOOLVERSION  && TOOLVERSION < 350
const bool circ_AND       ;
const bool circ_ITE       ;
const bool circ_XOR       ;
const bool circ_ExO       ;
const bool circ_genAND    ;
const bool circ_FASUM     ;
const bool circ_BLOCKED   ;
const bool circ_AddBlocked;
const bool circ_NegatedI  ;
const bool circ_Implied   ;
#else
BoolOption circ_AND;
BoolOption circ_ITE;
BoolOption circ_XOR;
BoolOption circ_ExO;
BoolOption circ_genAND;
BoolOption circ_FASUM;

BoolOption circ_BLOCKED;
BoolOption circ_AddBlocked;
BoolOption circ_NegatedI;
BoolOption circ_Implied;
#endif
/// temporary Boolean flag to quickly enable debug output for the whole file
#ifndef NDEBUG
  BoolOption circ_debug_out;
#endif


//
// Fourier Motzkin
//
IntOption  opt_fm_max_constraints;
Int64Option opt_fmLimit    ;
Int64Option opt_fmSearchLimit    ;
IntOption  opt_fmMaxAMO   ;
IntOption  opt_fmGrow     ;
IntOption  opt_fmGrowT    ;
BoolOption opt_atMostTwo  ;
BoolOption opt_fm_twoPr   ;
BoolOption opt_fm_sem     ;
BoolOption opt_findUnit   ;
BoolOption opt_merge      ;
BoolOption opt_fm_avoid_duplicates ;
BoolOption opt_fm_multiVarAMO ;
BoolOption opt_multiVarAMT;
BoolOption opt_cutOff     ;
IntOption opt_newAmo      ;
BoolOption opt_keepAllNew ;
IntOption opt_newAlo      ;
IntOption opt_newAlk      ;
BoolOption opt_checkSub   ;
BoolOption opt_rem_first  ;
IntOption opt_minCardClauseSize;
IntOption opt_maxCardClauseSize; 
IntOption opt_maxCardSize      ;
Int64Option opt_semSearchLimit ;
#ifndef NDEBUG
BoolOption opt_semDebug        ;
#endif
BoolOption opt_noReduct        ;

#ifndef NDEBUG
IntOption fm_debug_out       ;
#endif

//
// Hidden Tautology Elimination
//
IntOption opt_hte_steps;
#if defined TOOLVERSION && TOOLVERSION < 302
const bool opt_par_hte;
#else
BoolOption opt_par_hte;
#endif
#ifndef NDEBUG
IntOption hte_debug_out;
#endif
BoolOption opt_hteTalk ;
IntOption  opt_hte_inpStepInc;

//
// Probing
//
IntOption pr_uip;
BoolOption opt_pr_probeBinary;
BoolOption pr_double     ;
BoolOption pr_probe      ;
BoolOption pr_rootsOnly  ;
BoolOption pr_repeat     ;
IntOption pr_clsSize     ;
BoolOption pr_LHBR       ; // LHBR during probing
IntOption pr_prLimit     ;
BoolOption pr_EE         ;
BoolOption pr_vivi       ;
IntOption pr_keepLearnts ;
IntOption pr_keepImplied ;
IntOption pr_viviPercent ;
IntOption pr_viviLimit   ;
IntOption  pr_opt_inpStepInc1      ;
IntOption  pr_opt_inpStepInc2      ;
IntOption  pr_keepLHBRs  ;
BoolOption pr_necBinaries  ;
#ifndef NDEBUG
IntOption pr_debug_out;
#endif

//
// Unit Propagation
//
#ifndef NDEBUG
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
#ifndef NDEBUG
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
#ifndef NDEBUG
 IntOption rew_debug_out;           
#endif
 
//
// Shuffle
//
IntOption opt_shuffle_seed;
BoolOption opt_shuffle_order;
#ifndef NDEBUG
IntOption shuffle_debug_out;
#endif

//
// Sls
//
#ifndef NDEBUG
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
#if defined TOOLVERSION && TOOLVERSION < 302
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
#ifndef NDEBUG
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
#ifndef NDEBUG
 IntOption sym_debug_out;
#endif
 
//
// Twosat
//
#ifndef NDEBUG
 IntOption twosat_debug_out  ;
#endif
 BoolOption twosat_useUnits  ;
 BoolOption twosat_clearQueue;
 
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
 IntOption  opt_uhd_probe     ;
 IntOption  opt_uhd_fullProbe ;
 BoolOption opt_uhd_probeEE   ;
 BoolOption opt_uhd_fullBorder;
#ifndef NDEBUG
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
 
#ifndef NDEBUG
 IntOption  opt_xor_debug;
#endif
private:
 int dummy;
};
 
}

#endif