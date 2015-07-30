/*********************************************************************************[PfolioConfig.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "pcasso/PcassoComConfig.h"

namespace Riss
{

static const char* _cat = "PCASSO";

PcassoComConfig::PcassoComConfig(const std::string& presetOptions)  // add new options here!
    :
    Config(&configOptions, presetOptions)

//    , opt_proofCounting("PCASSO - PROOF", "pcasso-pc",  "enable avoiding duplicate clauses in the pfolio DRUP proof", true, optionListPtr)
    , opt_verboseProof("PCASSO - PROOF", "pcasso-pv",  "verbose proof (2=with comments to clause authors,1=comments by master only, 0=off)", 1, IntRange(0, 2), optionListPtr)
//    , opt_internalProofCheck("PCASSO - PROOF", "pcasso-pic", "use internal proof checker during run time", false, optionListPtr)
//    , opt_verbosePfolio("PCASSO - PROOF", "pcasso-ppv", "verbose pfolio execution", false, optionListPtr)

//    , opt_defaultSetup("PCASSO - INIT", "psetup", "how to setup client solvers", 0, optionListPtr)
//    , opt_incarnationSetups("PCASSO - INIT", "pIncSetup", "incarnation configurations [N]confign[N+1]configN+1", 0, optionListPtr)
//    , opt_ppconfig("PCASSO - INIT", "ppconfig", "the configuration to be used for the simplifier", 0, optionListPtr)
//    , opt_allIncPresets("PCASSO - INIT", "pAllSetup", "add to all incarnations (after other setups)", 0, optionListPtr)

    , opt_storageSize("PCASSO - INIT", "pcasso-storageSize", "Number of clauses in one ring buffer (0 == no sharing)", 0, IntRange(0, INT32_MAX), optionListPtr)

    , opt_share("SEND", "pcasso-ps", "enable clause sharing for all clients", true, optionListPtr)
    , opt_receive("SEND", "pcasso-pr", "enable receiving clauses for all clients", true, optionListPtr)

    , opt_protectAssumptions("SEND", "pcasso-protectA", "should assumption variables not be considered for calculating send-limits", false, optionListPtr)
    , opt_sendSize("SEND", "pcasso-sendSize", "Minimum Lbd of clauses to send  (also start value)", 10, DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_sendLbd("SEND", "pcasso-sendLbd", "initial value, also minimum limit (first checks size)", 5, DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_sendMaxSize("SEND", "pcasso-sendMaxSize", "upper bound for clause size (larger clause is never shared)", 128, DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_sendMaxLbd("SEND",  "pcasso-sendMaxLbd",  "upper bound for clause size (larger clause is never shared)", 32,  DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_sizeChange("SEND",  "pcasso-sizeChange",  "set to value greater than 0 to see dynamic limit changes! (e.g. 0.05)", 0.05,   DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_lbdChange("SEND",   "pcasso-lbdChange",   "set to value greater than 0 to see dynamic limit changes! (e.g. 0.02)", 0.02,   DoubleRange(0, true, HUGE_VAL, true), optionListPtr)
    , opt_sendRatio("SEND",   "pcasso-sendRatio",   "", 0.1, DoubleRange(0, true, 1, true), optionListPtr)

    , opt_doBumpClauseActivity("SEND", "pcasso-bumpSentCA", "bump activity of received clauses", true, optionListPtr)
    , opt_sendIncModel("SEND", "pcasso-sendIncModel", "allow sending with variables where the number of models potentially increased", true, optionListPtr)
    , opt_sendDecModel("SEND", "pcasso-sendDecModel", "llow sending with variables where the number of models potentially deecreased", false, optionListPtr)
    , opt_useDynamicLimits("SEND", "pcasso-dynLimits", "update sharing limits dynamically", true, optionListPtr)
    , opt_sendEquivalences("SEND", "pcasso-shareEE", "share equivalent literals", true, optionListPtr)

{
  if (defaultPreset.size() != 0) { 
//     DOUT( std::cerr << "c set preset for pcasso: " << defaultPreset << std::endl; );
    setPreset(defaultPreset);
  }    // set configuration options immediately
}

} // namespace Riss
