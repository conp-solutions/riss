/**************************************************************************************[CPConfig.h]

Copyright (c) 2013, Norbert Manthey, LGPL v2, see LICENSE

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_CPConfig_h
#define RISS_CPConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"
#include "riss/utils/Debug.h"

// using namespace Riss;

namespace Coprocessor
{

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CP3Config : public Riss::Config
{

    /** pointer to all options in this object - used for parsing and printing the help! */
    Riss::vec<Riss::Option*> configOptions;

  public:
    /** default constructor, which sets up all options in their standard format */
    CP3Config(const std::string& presetOptions = "");

    /**
    * List of all used options, public members, can be changed and read directly
    */

// options
    Riss::BoolOption opt_enabled     ;
    Riss::BoolOption opt_inprocess   ;
    Riss::IntOption opt_cp3_vars;  // variable limit to enable CP3
    Riss::IntOption opt_cp3_cls;   // clause limit to enable CP3
    Riss::IntOption opt_cp3_lits;
    Riss::IntOption opt_cp3_ipvars;    // variable limit to enable CP3 inprocessing
    Riss::IntOption opt_cp3_ipcls;     // clause limit to enable CP3 inprocessing
    Riss::IntOption opt_cp3_iplits;
    Riss::BoolOption opt_unlimited  ;
    Riss::BoolOption opt_randomized  ;
    Riss::IntOption  opt_inprocessInt;
    Riss::IntOption  opt_inpIntInc;
    Riss::IntOption  opt_simplifyRounds;

    Riss::IntOption  opt_exit_pp     ;
    Riss::BoolOption opt_randInp     ;
    Riss::BoolOption opt_inc_inp     ;
    Riss::BoolOption opt_remL_inp    ;

    Riss::StringOption opt_whiteList ;

    #if defined TOOLVERSION && TOOLVERSION < 400
    const bool opt_printStats; // do not print stats, if restricted binary is produced
    const  int opt_verbose;        // do not talk during computation!
    #else
    Riss::BoolOption opt_printStats;
    Riss::IntOption  opt_verbose;
    #endif

// techniques
    Riss::BoolOption opt_up          ;
    Riss::BoolOption opt_subsimp     ;
    Riss::BoolOption opt_hte         ;
    Riss::BoolOption opt_bce         ;
    Riss::BoolOption opt_modprep     ;
    Riss::BoolOption opt_ent         ;
    Riss::BoolOption opt_exp         ;
    Riss::BoolOption opt_la          ;
    Riss::BoolOption opt_cce         ;
    Riss::BoolOption opt_rate        ;
    Riss::BoolOption opt_ee          ;
    Riss::BoolOption opt_bve         ;
    Riss::BoolOption opt_bva         ;
    Riss::BoolOption opt_unhide      ;
    Riss::BoolOption opt_probe       ;
    Riss::BoolOption opt_ternResolve ;
    Riss::BoolOption opt_addRedBins  ;
    Riss::BoolOption opt_dense       ;
    Riss::BoolOption opt_shuffle     ;
    Riss::BoolOption opt_simplify    ;
    Riss::BoolOption opt_symm        ;
    Riss::BoolOption opt_FM          ;
    Riss::BoolOption opt_hbr         ;


    Riss::StringOption stepbystepoutput; // prefix of CNF filename to be printed after executing a given technique (adds technique to name, but not iteration)

    Riss::StringOption opt_ptechs ;
    Riss::StringOption opt_itechs ;

    Riss::IntOption  opt_threads     ;
    Riss::BoolOption opt_sls         ;
    Riss::BoolOption opt_sls_phase   ;
    Riss::IntOption  opt_sls_flips   ;
    Riss::BoolOption opt_xor         ;
    Riss::BoolOption opt_rew         ;

    Riss::IntOption opt_subsimp_vars;  // variable limit to enable
    Riss::IntOption opt_subsimp_cls;   // clause limit to enable
    Riss::IntOption opt_subsimp_lits;  // total literals limit to enable
    Riss::IntOption opt_hte_vars;  // variable limit to enable
    Riss::IntOption opt_hte_cls;   // clause limit to enable
    Riss::IntOption opt_hte_lits;  // total literals limit to enable
    Riss::IntOption opt_bce_vars;  // variable limit to enable
    Riss::IntOption opt_bce_cls;   // clause limit to enable
    Riss::IntOption opt_bce_lits;  // total literals limit to enable
    Riss::IntOption opt_ent_vars;  // variable limit to enable
    Riss::IntOption opt_ent_cls;   // clause limit to enable
    Riss::IntOption opt_ent_lits;  // total literals limit to enable
    Riss::IntOption opt_la_vars;   // variable limit to enable
    Riss::IntOption opt_la_cls;    // clause limit to enable
    Riss::IntOption opt_la_lits;   // total literals limit to enable
    Riss::IntOption opt_cce_vars;  // variable limit to enable
    Riss::IntOption opt_cce_cls;   // clause limit to enable
    Riss::IntOption opt_cce_lits;  // total literals limit to enable
    Riss::IntOption opt_rate_vars;     // variable limit to enable
    Riss::IntOption opt_rate_cls;      // clause limit to enable
    Riss::IntOption opt_rate_lits; // total literals limit to enable
    Riss::IntOption opt_ee_vars;   // variable limit to enable
    Riss::IntOption opt_ee_cls;    // clause limit to enable
    Riss::IntOption opt_ee_lits;   // total literals limit to enable
    Riss::IntOption opt_bve_vars;  // variable limit to enable
    Riss::IntOption opt_bve_cls;   // clause limit to enable
    Riss::IntOption opt_bve_lits;  // total literals limit to enable
    Riss::IntOption opt_bva_vars;  // variable limit to enable
    Riss::IntOption opt_bva_cls;   // clause limit to enable
    Riss::IntOption opt_bva_lits;  // total literals limit to enable
    Riss::IntOption opt_unhide_vars;   // variable limit to enable
    Riss::IntOption opt_unhide_cls;    // clause limit to enable
    Riss::IntOption opt_unhide_lits;   // total literals limit to enable
    Riss::IntOption opt_ternResolve_vars;  // variable limit to enable
    Riss::IntOption opt_ternResolve_cls;   // clause limit to enable
    Riss::IntOption opt_ternResolve_lits;  // total literals limit to enable
    Riss::IntOption opt_addRedBins_vars;   // variable limit to enable
    Riss::IntOption opt_addRedBins_cls;    // clause limit to enable
    Riss::IntOption opt_addRedBins_lits;   // total literals limit to enable
    Riss::IntOption opt_symm_vars;     // variable limit to enable
    Riss::IntOption opt_symm_cls;      // clause limit to enable
    Riss::IntOption opt_symm_lits; // total literals limit to enable
    Riss::IntOption opt_fm_vars;   // variable limit to enable
    Riss::IntOption opt_fm_cls;    // clause limit to enable
    Riss::IntOption opt_fm_lits;   // total literals limit to enable

    Riss::IntOption opt_xor_vars;  // variable limit to enable
    Riss::IntOption opt_xor_cls;   // clause limit to enable
    Riss::IntOption opt_xor_lits;  // total literals limit to enable
    Riss::IntOption opt_sls_vars;  // variable limit to enable
    Riss::IntOption opt_sls_cls;   // clause limit to enable
    Riss::IntOption opt_sls_lits;  // total literals limit to enable
    Riss::IntOption opt_rew_vars;  // variable limit to enable
    Riss::IntOption opt_rew_cls;   // clause limit to enable
    Riss::IntOption opt_rew_lits;  // total literals limit to enable
    Riss::IntOption opt_hbr_vars;  // variable limit to enable
    Riss::IntOption opt_hbr_cls;   // clause limit to enable
    Riss::IntOption opt_hbr_lits;  // total literals limit to enable

    #ifndef NDEBUG
    Riss::BoolOption opt_debug    ;
    Riss::IntOption opt_check     ;
    Riss::IntOption  opt_log      ;
    Riss::StringOption printAfter ;
    #endif

//
// BVE
//

    Riss::IntOption opt_par_bve         ;
    Riss::IntOption  opt_bve_verbose    ;

    Riss::IntOption  opt_bve_limit       ;
    Riss::IntOption  opt_learnt_growth   ;
    Riss::IntOption  opt_resolve_learnts ;
    Riss::BoolOption opt_unlimited_bve   ;
    Riss::BoolOption opt_bve_strength    ;
    Riss::BoolOption opt_bve_findGate    ;
    Riss::BoolOption opt_force_gates     ;
    Riss::BoolOption bve_funcDepOnly     ;
// pick order of eliminations
    Riss::IntOption  opt_bve_heap        ;
// increasing eliminations
    Riss::IntOption  opt_bve_grow        ;
    Riss::IntOption  opt_bve_growTotal   ;
    Riss::BoolOption opt_totalGrow       ;

    Riss::BoolOption opt_bve_bc          ;
    Riss::IntOption  heap_updates        ;
    Riss::BoolOption opt_bve_earlyAbort  ;
    Riss::BoolOption opt_bce_only        ;
    Riss::BoolOption opt_print_progress  ;
    Riss::IntOption  opt_bveInpStepInc   ;

    #if defined TOOLVERSION && TOOLVERSION < 302
    const int par_bve_threshold ;
    const int postpone_locked_neighbors ;
    const bool opt_minimal_updates ;
    #else
    Riss::IntOption  par_bve_threshold;
    Riss::IntOption  postpone_locked_neighbors;
    Riss::BoolOption opt_minimal_updates;
    #endif

//
// BVA
//
    Riss::IntOption  opt_bva_push             ;
    Riss::IntOption  opt_bva_VarLimit         ;
    Riss::BoolOption opt_Abva                 ;
    Riss::IntOption  opt_bva_Alimit           ;
    Riss::IntOption  opt_Abva_maxRed          ;
    Riss::IntOption  opt_bvaInpStepInc        ;
    Riss::IntOption  opt_Abva_heap            ;
    Riss::BoolOption opt_bvaComplement        ;
    Riss::BoolOption opt_bvaRemoveDubplicates ;
    Riss::BoolOption opt_bvaSubstituteOr      ;
    #ifndef NDEBUG
    Riss::IntOption  bva_debug                ;
    Riss::IntOption  opt_bvaAnalysisDebug     ;
    #endif

    Riss::IntOption  opt_Xbva                 ;
    Riss::IntOption  opt_Ibva                 ;
    Riss::IntOption  opt_bva_Xlimit           ;
    Riss::IntOption  opt_bva_Ilimit           ;
    Riss::IntOption  opt_Xbva_maxRed          ;
    Riss::IntOption  opt_Ibva_maxRed          ;
    Riss::IntOption  opt_Xbva_heap            ;
    Riss::IntOption  opt_Ibva_heap            ;
    Riss::IntOption opt_Ibva_vars;     // variable limit to enable
    Riss::IntOption opt_Ibva_cls;      // clause limit to enable
    Riss::IntOption opt_Ibva_lits; // total literals limit to enable
    Riss::IntOption opt_Xbva_vars;     // variable limit to enable
    Riss::IntOption opt_Xbva_cls;      // clause limit to enable
    Riss::IntOption opt_Xbva_lits; // total literals limit to enable

//
// BCE
//
    Riss::BoolOption orderComplements; // sort the heap based on the occurrence of complementary literals
    Riss::BoolOption bceBinary; // remove binary clauses during BCE
    Riss::IntOption bceLimit;
    Riss::BoolOption opt_bce_bce; // actually remove blocked clauses
    Riss::BoolOption opt_bce_bcm; // minimize blocked clauses instead of eliminating them (keep the literals that are required for being blocked)
    Riss::BoolOption opt_bce_cle; // perform covered literal elimination
    Riss::BoolOption opt_bce_cla; // perform covered literal addition
    Riss::BoolOption opt_bce_cle_conservative; // perform CLE conservative and cheap, if tautological resolvents occur
    Riss::IntOption opt_bceInpStepInc; // add to limit for inprocessing
    Riss::IntOption opt_bce_verbose; // output operation steps
    #ifndef NDEBUG
    Riss::BoolOption opt_bce_debug; // debug output
    #endif


//
// HBR
//
    Riss::IntOption hbrLimit;
    Riss::IntOption opt_hbr_maxCsize;   // min size = 3
    Riss::IntOption opt_hbr_addBinaries; // add new binary clauses (always, only in first iteration, never)
    Riss::IntOption opt_hbrInpStepInc; // add to limit for inprocessing
    Riss::IntOption opt_hbr_verbose; // output operation steps
    #ifndef NDEBUG
    Riss::BoolOption opt_hbr_debug; // debug output
    #endif

//
// LiteralAddition
//
    Riss::BoolOption opt_la_cla; // perform covered literal addition
    Riss::BoolOption opt_la_ala; // perform asymmetric literal addition

    Riss::IntOption claLimit; // number of steps before aborting LA
    Riss::IntOption claStepSize; // number of extension literals so that literals are removed randomly
    Riss::IntOption claStepMax; // number of first extension literals that are considered (should be smaller then size!)
    Riss::IntOption claIterations; // number of iterations to do for CLA

    Riss::IntOption alaLimit; // number of steps for limits
    Riss::IntOption alaIterations; // number of iterations to do for ALA
    Riss::BoolOption ala_binary; // perform ALA with binary clauses
    #ifndef NDEBUG
    Riss::BoolOption opt_la_debug; // debug output
    #endif


//
// CCE
//
    Riss::IntOption opt_cceSteps;
    Riss::IntOption opt_ccelevel;
    Riss::IntOption opt_ccePercent;
    #ifndef NDEBUG
    Riss::IntOption cce_debug_out;
    #endif
    Riss::IntOption  opt_cceInpStepInc;

//
// Options for rat elimination
//
    Riss::BoolOption rate_orderComplements;
    Riss::Int64Option rate_Limit;
    Riss::Int64Option ratm_Limit;
    #ifndef NDEBUG
    Riss::IntOption opt_rate_debug;
    #endif
    Riss::BoolOption opt_rate_brat; // test resolvent not only for AT, but also for being blocked
    Riss::IntOption rate_minSize;
    Riss::BoolOption opt_rate_rate;
    Riss::BoolOption opt_rate_bcs; // perform blocked clause substitution
    Riss::BoolOption opt_rate_ratm;
    Riss::BoolOption opt_rate_ratm_extended;
    Riss::BoolOption opt_rate_ratm_rounds;

//
// Dense
//
    #ifndef NDEBUG
    Riss::IntOption dense_debug_out;
    #endif
    Riss::BoolOption opt_dense_inprocess;
    Riss::IntOption  opt_dense_fragmentation;
    Riss::BoolOption opt_dense_keep_assigned;

//
// Entailed
//
    #if defined TOOLVERSION && TOOLVERSION < 360
    const int opt_entailed_minClsSize;
    #else
    Riss::IntOption opt_entailed_minClsSize;
    #endif

    #ifndef NDEBUG
    Riss::IntOption  entailed_debug;
    #endif

    #ifndef NDEBUG
    Riss::IntOption  modprep_debug;
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
    Riss::IntOption  opt_ee_level           ;
    Riss::IntOption  opt_ee_gate_limit      ;
    Riss::IntOption  opt_ee_circuit_iters   ;
    Riss::BoolOption opt_ee_eagerEquivalence;
    Riss::BoolOption opt_eeGateBigFirst     ;
    Riss::StringOption opt_ee_aagFile       ;
    #endif
    #ifndef NDEBUG
    Riss::IntOption  ee_debug_out           ;
    #endif
    Riss::BoolOption opt_eeSub            ;
    Riss::BoolOption opt_eeFullReset      ;
    Riss::IntOption  opt_ee_limit         ;
    Riss::IntOption  opt_ee_inpStepInc    ;
    Riss::IntOption  opt_ee_bigIters      ;
    Riss::BoolOption opt_ee_iterative     ;
    Riss::BoolOption opt_EE_checkNewSub   ;
    Riss::BoolOption opt_ee_eager_frozen  ;
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
    Riss::BoolOption circ_AND;
    Riss::BoolOption circ_ITE;
    Riss::BoolOption circ_XOR;
    Riss::BoolOption circ_ExO;
    Riss::BoolOption circ_genAND;
    Riss::BoolOption circ_FASUM;

    Riss::BoolOption circ_BLOCKED;
    Riss::BoolOption circ_AddBlocked;
    Riss::BoolOption circ_NegatedI;
    Riss::BoolOption circ_Implied;
    #endif
/// temporary Boolean flag to quickly enable debug output for the whole file
    #ifndef NDEBUG
    Riss::BoolOption circ_debug_out;
    #endif


//
// Fourier Motzkin
//
    Riss::IntOption  opt_fm_max_constraints;
    Riss::Int64Option opt_fmLimit    ;
    Riss::Int64Option opt_fmSearchLimit    ;
    Riss::IntOption  opt_fmMaxAMO   ;
    Riss::IntOption  opt_fmGrow     ;
    Riss::IntOption  opt_fmGrowT    ;
    Riss::BoolOption opt_atMostTwo  ;
    Riss::BoolOption opt_fm_twoPr   ;
    Riss::BoolOption opt_fm_sem     ;
    Riss::BoolOption opt_findUnit   ;
    Riss::BoolOption opt_merge      ;
    Riss::BoolOption opt_fm_avoid_duplicates ;
    Riss::BoolOption opt_fm_multiVarAMO ;
    Riss::BoolOption opt_multiVarAMT;
    Riss::BoolOption opt_cutOff     ;
    Riss::IntOption opt_newAmo      ;
    Riss::BoolOption opt_keepAllNew ;
    Riss::IntOption opt_newAlo      ;
    Riss::IntOption opt_newAlk      ;
    Riss::BoolOption opt_checkSub   ;
    Riss::BoolOption opt_rem_first  ;
    Riss::BoolOption opt_fm_garbageColelct ;
    Riss::BoolOption opt_fm_prooftrace ;
    Riss::IntOption opt_fm_printtrace ;
    Riss::IntOption opt_minCardClauseSize;
    Riss::IntOption opt_maxCardClauseSize;
    Riss::IntOption opt_maxCardSize      ;
    Riss::Int64Option opt_semSearchLimit ;
    #ifndef NDEBUG
    Riss::BoolOption opt_semDebug        ;
    #endif
    Riss::BoolOption opt_noReduct        ;

    #ifndef NDEBUG
    Riss::IntOption fm_debug_out       ;
    #endif

//
// Hidden Tautology Elimination
//
    Riss::IntOption opt_hte_steps;
    #if defined TOOLVERSION && TOOLVERSION < 302
    const bool opt_par_hte;
    #else
    Riss::BoolOption opt_par_hte;
    #endif
    #ifndef NDEBUG
    Riss::IntOption hte_debug_out;
    #endif
    Riss::BoolOption opt_hteTalk ;
    Riss::IntOption  opt_hte_inpStepInc;

//
// Probing
//
    Riss::BoolOption pr_probe      ;
    Riss::IntOption pr_uip;
    Riss::BoolOption opt_pr_probeBinary;
    Riss::BoolOption pr_double     ;
    Riss::BoolOption pr_rootsOnly  ;
    Riss::BoolOption pr_repeat     ;
    Riss::IntOption pr_clsSize     ;
    Riss::BoolOption pr_LHBR       ; // LHBR during probing
    Riss::IntOption pr_prLimit     ;
    Riss::BoolOption pr_EE         ;
    Riss::BoolOption pr_vivi       ;
    Riss::BoolOption pr_lcm        ;
    Riss::IntOption pr_lcm_vivi    ;
    Riss::IntOption pr_lcm_min_size;
    Riss::IntOption pr_keepLearnts ;
    Riss::IntOption pr_keepImplied ;
    Riss::IntOption pr_viviPercent ;
    Riss::IntOption pr_viviLimit   ;
    Riss::IntOption pr_lcmLimit    ;
    Riss::IntOption pr_lcm_iterations;
    Riss::IntOption  pr_opt_inpStepInc1      ;
    Riss::IntOption  pr_opt_inpStepInc2      ;
    Riss::IntOption  pr_keepLHBRs  ;
    Riss::BoolOption pr_necBinaries  ;
    Riss::IntOption opt_probe_vars;    // variable limit to enable
    Riss::IntOption opt_probe_cls;     // clause limit to enable
    Riss::IntOption opt_probe_lits;    // total literals limit to enable
    Riss::IntOption opt_viv_vars;  // variable limit to enable
    Riss::IntOption opt_viv_cls;   // clause limit to enable
    Riss::IntOption opt_viv_lits;  // total literals limit to enable
    Riss::IntOption opt_lcm_vars;  // variable limit to enable
    Riss::IntOption opt_lcm_cls;   // clause limit to enable
    Riss::IntOption opt_lcm_lits;  // total literals limit to enable
    #ifndef NDEBUG
    Riss::IntOption pr_debug_out;
    #endif

//
// Unit Propagation
//
    #ifndef NDEBUG
    Riss::IntOption up_debug_out;
    #endif

//
// Resolution and Redundancy Addition
//
    Riss::BoolOption   opt_res3_use_binaries ;
    Riss::IntOption    opt_res3_steps    ;
    Riss::IntOption    opt_res3_newCls   ;
    Riss::BoolOption   opt_res3_reAdd    ;
    Riss::BoolOption   opt_res3_use_subs ;
    Riss::DoubleOption opt_add2_percent  ;
    Riss::BoolOption   opt_add2_red      ;
    Riss::BoolOption   opt_add2_red_level;
    Riss::BoolOption   opt_add2_red_lea  ;
    Riss::BoolOption   opt_add2_red_start;
    Riss::IntOption  opt_res3_inpStepInc ;
    Riss::IntOption  opt_add2_inpStepInc ;
/// enable this parameter only during debug!
    #ifndef NDEBUG
    Riss::BoolOption res3_debug_out      ;
    #endif

//
// Rewriter
//
    Riss::IntOption  opt_rew_min   ;
    Riss::IntOption  opt_rew_iter   ;
    Riss::IntOption  opt_rew_minAMO ;
    Riss::IntOption  opt_rew_limit  ;
    Riss::IntOption  opt_rew_Varlimit ;
    Riss::IntOption  opt_rew_Addlimit ;
    Riss::BoolOption opt_rew_amo    ;
    Riss::BoolOption opt_rew_imp    ;
    Riss::BoolOption opt_rew_scan_exo ;
    Riss::BoolOption opt_rew_merge_amo;
    Riss::BoolOption opt_rew_rem_first;
    Riss::BoolOption opt_rew_avg     ;
    Riss::BoolOption opt_rew_ratio  ;
    Riss::BoolOption opt_rew_once     ;
    Riss::BoolOption opt_rew_stat_only;
    Riss::IntOption  opt_rew_min_imp_size     ;
    Riss::BoolOption opt_rew_impl_pref_small   ;
    Riss::IntOption  opt_rew_inpStepInc     ;
    #ifndef NDEBUG
    Riss::IntOption rew_debug_out;
    #endif

//
// Shuffle
//
    Riss::IntOption opt_shuffle_seed;
    Riss::BoolOption opt_shuffle_order;
    #ifndef NDEBUG
    Riss::IntOption shuffle_debug_out;
    #endif

//
// Sls
//
    #ifndef NDEBUG
    Riss::BoolOption opt_sls_debug ;
    #endif
    Riss::IntOption  opt_sls_ksat_flips ;
    Riss::IntOption  opt_sls_rand_walk  ;
    Riss::BoolOption opt_sls_adopt      ;

//
// Subsumption
//
    Riss::BoolOption  opt_sub_naivStrength;
    Riss::IntOption   opt_sub_allStrengthRes;
    Riss::BoolOption  opt_sub_strength     ;
    Riss::BoolOption  opt_sub_preferLearned;
    Riss::IntOption   opt_sub_subLimit     ;
    Riss::IntOption   opt_sub_strLimit     ;
    Riss::IntOption   opt_sub_callIncrease ;
    Riss::IntOption  opt_sub_inpStepInc    ;
    #if defined TOOLVERSION && TOOLVERSION < 302
    const int   opt_sub_par_strength ;
    const bool  opt_sub_lock_stats   ;
    const int   opt_sub_par_subs     ;
    const int   opt_sub_par_subs_counts;
    const int   opt_sub_chunk_size     ;
    const int   opt_sub_par_str_minCls ;
    #else
    Riss::IntOption   opt_sub_par_strength   ;
    Riss::BoolOption  opt_sub_lock_stats     ;
    Riss::IntOption   opt_sub_par_subs       ;
    Riss::IntOption   opt_sub_par_subs_counts;
    Riss::IntOption   opt_sub_chunk_size     ;
    Riss::IntOption   opt_sub_par_str_minCls ;
    #endif
    #ifndef NDEBUG
    Riss::IntOption   opt_sub_debug  ;
    #endif

//
// Symmetry Breaker
//
    Riss::BoolOption    sym_opt_hsize          ;
    Riss::BoolOption    sym_opt_hpol           ;
    Riss::BoolOption    sym_opt_hpushUnit      ; // there should be a parameter delay-units already!
    Riss::IntOption     sym_opt_hmin           ;
    Riss::DoubleOption  sym_opt_hratio         ;
    Riss::IntOption     sym_opt_iter           ;
    Riss::BoolOption    sym_opt_pairs          ;
    Riss::BoolOption    sym_opt_print          ;
    Riss::BoolOption    sym_opt_exit           ;
    Riss::BoolOption    sym_opt_hprop          ;
    Riss::BoolOption    sym_opt_hpropF         ;
    Riss::BoolOption    sym_opt_hpropA         ;
    Riss::BoolOption    sym_opt_cleanLearn     ;
    Riss::IntOption     sym_opt_conflicts      ;
    Riss::IntOption     sym_opt_total_conflicts;
    #ifndef NDEBUG
    Riss::IntOption sym_debug_out;
    #endif

//
// Unhide
//
    Riss::IntOption  opt_uhd_Iters     ;
    Riss::BoolOption opt_uhd_Trans     ;
    Riss::IntOption  opt_uhd_UHLE      ;
    Riss::BoolOption opt_uhd_UHTE      ;
    Riss::BoolOption opt_uhd_NoShuffle ;
    Riss::BoolOption opt_uhd_EE        ;
    Riss::BoolOption opt_uhd_TestDbl   ;
    Riss::IntOption  opt_uhd_probe     ;
    Riss::IntOption  opt_uhd_fullProbe ;
    Riss::BoolOption opt_uhd_probeEE   ;
    Riss::BoolOption opt_uhd_fullBorder;
    #ifndef NDEBUG
    Riss::IntOption  opt_uhd_Debug;
    #endif

//
// Xor
//
    Riss::IntOption  opt_xorMatchLimit ;
    Riss::IntOption  opt_xorFindLimit  ;
    Riss::IntOption  opt_xor_selectX     ;
    Riss::BoolOption opt_xor_keepUsed    ;
    Riss::BoolOption opt_xor_findSubsumed;
    Riss::BoolOption opt_xor_findResolved;
    Riss::IntOption  opt_xor_backdoor;

    Riss::BoolOption opt_xor_dropPure;
    Riss::IntOption  opt_xor_encodeSize;
    Riss::BoolOption opt_xor_checkNewSubsume;
    Riss::BoolOption opt_xor_addAsLearnt;
    Riss::IntOption  opt_xor_setPolarity;
    Riss::BoolOption opt_xor_addOnNewlyAdded;

    #ifndef NDEBUG
    Riss::IntOption  opt_xor_debug;
    #endif
  private:
    int dummy;
};

}

#endif
