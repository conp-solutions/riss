/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef CoreConfig_h
#define CoreConfig_h

#include "utils/Options.h"

namespace Minisat {

/** This class should contain all options that can be specified for the solver, and its tools.
 * Furthermore, constraints/assertions on parameters can be specified, and checked.
 */
class CoreConfig {
 
public:
 /** default constructor, which sets up all options in their standard format */
 CoreConfig ();

 /** parse all options from the command line */
 void parseOptions (int& argc, char** argv, bool strict = false);
 
 /** checks all specified constraints */
 bool checkConfiguration();
 
 /** 
 * List of all used options, public members, can be changed and read directly
 */
 
 DoubleOption opt_K; 
 DoubleOption opt_R; 
 IntOption opt_size_lbd_queue;
 IntOption opt_size_trail_queue;

 IntOption opt_first_reduce_db;
 IntOption opt_inc_reduce_db;
 IntOption opt_spec_inc_reduce_db;
 IntOption opt_lb_lbd_frozen_clause;
 BoolOption opt_lbd_ignore_l0; // do not consider literals that have toplevel assignments for LBD calculation

 IntOption opt_lb_size_minimzing_clause;
 IntOption opt_lb_lbd_minimzing_clause;


 DoubleOption opt_var_decay;
 DoubleOption opt_clause_decay;
 DoubleOption opt_random_var_freq;
 DoubleOption opt_random_seed;
 IntOption opt_ccmin_mode;
 IntOption opt_phase_saving;
 BoolOption opt_rnd_init_act;
 IntOption opt_init_act;
 IntOption opt_init_pol;


 IntOption opt_restarts_type;
 IntOption opt_restart_first;
 DoubleOption opt_restart_inc;

 DoubleOption opt_garbage_frac;

 IntOption opt_allUipHack;
 BoolOption opt_uipHack;
 IntOption opt_uips;
 BoolOption opt_vmtf;
 IntOption opt_LHBR;
 IntOption opt_LHBR_max;
 BoolOption opt_LHBR_sub;
 BoolOption opt_printLhbr;

 BoolOption opt_updateLearnAct;

 IntOption opt_hack;
 BoolOption opt_hack_cost;
 BoolOption opt_dbg;

 BoolOption opt_long_conflict;


// extra 
 IntOption opt_act;
 DoubleOption opt_actStart;
 DoubleOption pot_actDec;
 StringOption actFile;
 BoolOption opt_pol;
 StringOption polFile;
 BoolOption opt_printDecisions;

 IntOption opt_rMax;
 DoubleOption opt_rMaxInc;

 BoolOption dx; 
 BoolOption hk; 
 BoolOption tb; 
 BoolOption opt_laDyn;
 BoolOption opt_laEEl;
 IntOption opt_laEEp;
 IntOption opt_laMaxEvery;
 IntOption opt_laLevel;
 IntOption opt_laEvery;
 IntOption opt_laBound;
 IntOption opt_laTopUnit;

 BoolOption opt_prefetch; 
 BoolOption opt_hpushUnit; 
 IntOption opt_simplifyInterval; 

 BoolOption opt_otfss;
 BoolOption opt_otfssL;
 IntOption opt_otfssMaxLBD;
 BoolOption debug_otfss;

 IntOption opt_learnDecPrecent;

IntOption opt_verboseProof; 
BoolOption opt_rupProofOnly; 
 
 IntOption opt_verb;
 
 BoolOption opt_usePPpp;
 BoolOption opt_usePPip;
 
//
// for incremental solving
//
 IntOption resetActEvery;
 IntOption resetPolEvery;
 IntOption intenseCleaningEvery;
 IntOption incKeepSize;
 IntOption incKeepLBD;
};
 
}

#endif