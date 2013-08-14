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
 
};
 
}

#endif