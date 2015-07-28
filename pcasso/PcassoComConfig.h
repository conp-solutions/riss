/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_PcassoComConfig_h
#define RISS_PcassoComConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"

namespace Riss
{

/** This class should contain all options that can be specified for the pcasso communication
 */
class PcassoComConfig : public Config
{
    /** pointer to all options in this object - used for parsing and printing the help! */
    Riss::vec<Option*> configOptions;


  public:
    /** default constructor, which sets up all options in their standard format */
    PcassoComConfig(const std::string& presetOptions = "");


//    BoolOption opt_proofCounting;
    IntOption  opt_verboseProof;
//    BoolOption opt_internalProofCheck;
//    BoolOption opt_verbosePfolio;

//    StringOption opt_defaultSetup;          // presets to run priss in given setup (DRUP, BMC, ...)
////     StringOption opt_firstPPconfig;         // configuration for preprocessor of first solver object
//    StringOption opt_incarnationSetups;     // configurations for all the incarnations
//    StringOption opt_ppconfig;              // configuration for global preprocessor
//    StringOption opt_allIncPresets;         // to be added to all incarnations (after all other setups)

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
void PcassoComConfig::setPreset(const std::string& optionSet)
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
bool PcassoComConfig::addPreset(const std::string& optionSet)
{
    parsePreset = true;
    bool ret = true;

    if (optionSet == "small") {
        parseOptions("-pcasso-storageSize=100", false);
    } else if (optionSet == "usual") {
        parseOptions("-pcasso-storageSize=16000", false);
    } else if (optionSet == "large") {
        parseOptions("-pcasso-storageSize=100000", false);
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


} // end namespace

#endif
