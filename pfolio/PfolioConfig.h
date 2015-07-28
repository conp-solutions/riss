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
//     StringOption opt_firstPPconfig;         // configuration for preprocessor of first solver object
    StringOption opt_incarnationSetups;     // configurations for all the incarnations
    BoolOption addExtraSetup;               // add opt_incarnationSetups always, independently of configuration preset
    StringOption opt_ppconfig;              // configuration for global preprocessor
    StringOption opt_allIncPresets;         // to be added to all incarnations (after all other setups)

    IntOption  opt_storageSize;             // size of the storage for clause sharing

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
    BoolOption opt_useDynamicLimits;        // use dynamic limits for clause sharing
    BoolOption opt_sendEquivalences;        // send info about equivalences

    /** set all the options of the specified preset option sets (multiple separated with : possible) */
    void setPreset(const std::string& optionSet);
    
    /** set options of the specified preset option set (only one possible!)
     *  Note: overwrites the method of the class Riss::Config, but calls this method, if no match if found within this class
     *  @return true, if the option set is known and has been set!
     */
    bool addPreset(const std::string& optionSet);
};

inline
void PfolioConfig::setPreset(const std::string& optionSet)
{
    // split std::string into sub std::strings, separated by ':'
    std::vector<std::string> optionList;
    int lastStart = 0;
    int findP = 0;
    while (findP < optionSet.size()) {   // tokenize std::string
        findP = optionSet.find(":", lastStart);
        if (findP == std::string::npos) { findP = optionSet.size(); }

        if (findP - lastStart - 1 > 0) {
            addPreset(optionSet.substr(lastStart , findP - lastStart));
        }
        lastStart = findP + 1;
    }
}

inline
bool PfolioConfig::addPreset(const std::string& optionSet)
{
    parsePreset = true;
    bool ret = true;

    if (optionSet == "alpha") {
        parseOptions("-ppconfig=Riss427:plain_XOR:-cp3_iters=2:-ee:-cp3_ee_level=3:-cp3_ee_it:-rlevel=2:-bve_early -psetup=alpha -storageSize=32000 -pAllSetup=-keepLonger:-no-cp3_stats:-recLBDf=-1", false);
    } else if (optionSet == "beta") {
        parseOptions("-ppconfig=Riss427:plain_XOR:-cp3_iters=2:-ee:-cp3_ee_level=3:-cp3_ee_it:-rlevel=2:-bve_early -psetup=beta -storageSize=32000 -pAllSetup=-keepLonger:-no-cp3_stats:-recLBDf=-1", false);
    } else if (optionSet == "gamma") {
        parseOptions("-ppconfig=Riss427:plain_XOR:-cp3_iters=2:-ee:-cp3_ee_level=3:-cp3_ee_it:-rlevel=2:-bve_early -psetup=gamma -storageSize=32000 -pAllSetup=-keepLonger:-no-cp3_stats:-recLBDf=-1", false);
    } else if (optionSet == "delta") {
        parseOptions("-ppconfig=Riss427:plain_XOR:-cp3_iters=2:-ee:-cp3_ee_level=3:-cp3_ee_it:-rlevel=2:-bve_early -psetup=delta -storageSize=32000 -pAllSetup=-keepLonger:-no-cp3_stats:-recLBDf=-1", false);
    } else if (optionSet == "epsilon") {
        parseOptions("-ppconfig=STRONGUNSAT -psetup=epsilon -storageSize=32000 -pAllSetup=-keepLonger:-no-cp3_stats:-recLBDf=-1", false);
    }
    
    else {
      ret = false; // indicate that no configuration has been found here!
      if (optionSet != "") { 
	Riss::Config::parseOptions(optionSet);     // pass string to parent class, which might find a valid setup
      } 
    }
    parsePreset = false;
    return ret; // return whether a preset configuration has been found
}
}

#endif
