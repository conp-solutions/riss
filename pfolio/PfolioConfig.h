/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_PfolioConfig_h
#define RISS_PfolioConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"

namespace Riss
{

/** This class should contain all options that can be specified for the pfolio solver
 */
class PfolioConfig : public Config
{
    /** pointer to all options in this object - used for parsing and printing the help! */
    Riss::vec<Option*> configOptions;


  public:
    /** default constructor, which sets up all options in their standard format */
    PfolioConfig(const std::string& presetOptions = "");

    
    BoolOption opt_proofCounting;
    IntOption  opt_verboseProof;
    BoolOption opt_internalProofCheck;
    BoolOption opt_verbosePfolio;
    
    IntOption  threads;
    StringOption opt_defaultSetup;          // presets to run priss in given setup (DRUP, BMC, ...)
    
    // sharing options
    BoolOption opt_share;
    BoolOption opt_receive; 
    
    BoolOption opt_protectAssumptions;      // should assumption variables not be considered for calculating send-limits?
    DoubleOption opt_sendSize;              // Minimum Lbd of clauses to send  (also start value)
    DoubleOption opt_sendLbd;               // Minimum size of clauses to send (also start value)
    DoubleOption opt_sendMaxSize;           // Maximum size of clauses to send
    DoubleOption opt_sendMaxLbd;            // Maximum Lbd of clauses to send
    DoubleOption opt_sizeChange;            // How fast should size send limit be adopted?
    DoubleOption opt_lbdChange;             // How fast should lbd send limit be adopted?
    DoubleOption opt_sendRatio;             // How big should the ratio of send clauses be?
    BoolOption opt_doBumpClauseActivity;    // Should the activity of a received clause be increased from 0 to current activity

    BoolOption opt_sendIncModel;            // allow sending with variables where the number of models potentially increased
    BoolOption opt_sendDecModel;            // allow sending with variables where the number of models potentially deecreased
    BoolOption opt_useDynamicLimits;
};

}

#endif
