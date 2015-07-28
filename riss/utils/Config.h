/****************************************************************************************[Config.h]

Copyright (c) 2014, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_Config_h
#define RISS_Config_h

#include "riss/utils/Options.h"

#include <string>
#include <vector>

#include <iostream>

//#define TOOLVERSION 427

namespace Riss
{

/** class that implements most of the configuration features */
class Config
{
  protected:
    vec<Option*>* optionListPtr;    // list of options used in the instance of the configuration
    bool parsePreset;           // set to true if the method addPreset is calling the parse-Option method
    std::string defaultPreset;      // default preset configuration

  public:

    Config(vec<Option*>* ptr, const std::string& presetOptions = "");

    /** parse all options from the command line
      * @return true, if "help" has been found in the parameters
      */
    bool parseOptions(int& argc, char** argv, bool strict = false, int activeLevel = -1);

    /** parse options that are present in one std::string
     * @return true, if "help" has been found in the parameters
     */
    bool parseOptions(const std::string& options, bool strict = false, int activeLevel = -1);

    /** set all the options of the specified preset option sets (multiple separated with : possible) */
    void setPreset(const std::string& optionSet);

    /** set options of the specified preset option set (only one possible!)
     *  @return true, if the option set is known and has been set!
     */
    bool addPreset(const std::string& optionSet);

    /** show print for the options of this object */
    void printUsageAndExit(int  argc, char** argv, bool verbose = false, int activeLevel = -1);

    /** checks all specified constraints */
    bool checkConfiguration();

    /** return preset std::string */
    std::string presetConfig() const { return defaultPreset; }

    /** fill the std::string stream with the command that is necessary to obtain the current configuration */
    void configCall(std::stringstream& s);

    /** print specification of the options that belong to this configuration */
    void printOptions(FILE* pcsFile, int printLevel = -1);

    /** print dependencies of the options that belong to this configuration */
    void printOptionsDependencies(FILE* pcsFile, int printLevel = -1);
};

inline
Config::Config(vec<Option*>* ptr, const std::string& presetOptions)
    : optionListPtr(ptr)
    , parsePreset(false)
    , defaultPreset(presetOptions)
{
}

inline
void Config::setPreset(const std::string& optionSet)
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
bool Config::addPreset(const std::string& optionSet)
{
    parsePreset = true;
    bool ret = true;

    if (optionSet == "PLAIN") {
        parseOptions(" ", false);
    }

    /* // copy this block to add another preset option set!
    else if( optionSet == "" ) {
      parseOptions(" ",false);
    }
    */

    else if (optionSet == "QUIET") {
        parseOptions(" -no-cp3_stats -solververb=0", false);
    } else if (optionSet == "VERBOSE") {
        parseOptions(" -cp3_stats -solververb=2", false);
    } else if (optionSet == "DEBUG") {
        parseOptions(" -cp3_stats -solververb=2 -cp3_bve_verbose=2 -cp3-debug -cp3-check=2 -cp3_verbose=3", false);
    } else if (optionSet == "NOCP") {
        parseOptions(" -no-enabled_cp3", false);
    }

    /*
     *  Options for CVC4 (intermediate version Riss 5.0.1)
     */
    else if (optionSet == "CVC4") {
        parseOptions("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -rer-g -rer-ga=3", false);
    } else if (optionSet == "CVC4inc") {
        parseOptions("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -cp3_iters=2 -rlevel=2 -rer-g -rer-ga=3", false);
    }

    /*
     *  Options for Open-WBO
     */
    else if (optionSet == "MAXSAT") {
        parseOptions("-incsverb=1", false);
    } else if (optionSet == "INCSOLVE") {
        parseOptions("-rmf -sInterval=16 -lbdIgnLA -var-decay-b=0.85 -var-decay-e=0.85 -irlevel=1024 -rlevel=2 -incResCnt=3", false);
    } else if (optionSet == "PPMAXSAT2015") {
        parseOptions("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bce-bcm -cp3_iters=2 -rlevel=2", false);
    } else if (optionSet == "CORESIZE2") {
        parseOptions("-size_core=2", false);
    }
    /*
     *  Options for Riss 427
     */
    else if (optionSet == "CSSC2014") {
        parseOptions(
            // set everything like in the EDACC6 configuration
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            , false);
    }

    /*
     * Configurations found for BMC solving
     */
    else if (optionSet == "BMC1") {
        parseOptions(
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            + std::string(" -no-enabled_cp3 -no-rer -R=1.2 -szLBDQueue=70 -szTrailQueue=4000 -incReduceDB=450 -specialIncReduceDB=2000 -minLBDFrozenClause=15 -lbdIgnL0 -keepWorst=0.001 -biAsFreq=8 -minLBDMinimizingClause=9 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -agil-r -rnd-freq=0.01 -init-act=6 -init-pol=2 -rlevel=1 -rfirst=1000 -rinc=1.5 -alluiphack=2 -varActB=1 -dontTrust -lhbr=3 -lhbr-max=16000 -lhbr-sub -no-hack-cost -actStart=2048 -hlaLevel=1 -hlaevery=0 -hlabound=-1 -hlaTop=1024 -learnDecP=66 -ics_window=80000 -ics_processLast=10000 -ics_keepNew -ics_dyn -ics_relSIZE=0.5 -sUhdProbe=1 -no-sUhdPrRb -sUhdPrSh=2 -sUHLEsize=30")

            , false);
    } else if (optionSet == "BMC2") {
        parseOptions(
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            + std::string(" -no-enabled_cp3 -no-rer -R=1.2 -szLBDQueue=60 -szTrailQueue=3500 -quickRed -biAsserting -biAsFreq=4 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -cla-decay=0.995 -rnd-freq=0.005 -init-act=1 -rlevel=1 -alluiphack=2 -varActB=1 -clsActB=2 -dontTrust -lhbr-sub -no-hack-cost -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -otfssL -learnDecP=80 -ics_window=5050 -sUhdProbe=2 -no-sUhdPrRb -sUHLEsize=30")
            , false);
    } else if (optionSet == "BMC3") {

        parseOptions(
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            + std::string(" -no-enabled_cp3 -no-rer -K=0.85 -szTrailQueue=4000 -firstReduceDB=8000 -incReduceDB=450 -specialIncReduceDB=2000 -keepWorst=0.001 -biAsserting -biAsFreq=16 -minSizeMinimizingClause=50 -minLBDMinimizingClause=9 -var-decay-e=0.99 -var-decay-i=0.001 -agil-r -agil-limit=0.33 -agil-decay=0.99 -agil-add=512 -cla-decay=0.995 -rnd-freq=0.01 -init-act=3 -init-pol=5 -rtype=1 -rfirst=32 -alluiphack=2 -clsActB=2 -lhbr=4 -lhbr-max=1024 -hack=1 -dyn -laEEl -laEEp=66 -hlaMax=25 -hlaLevel=1 -hlaevery=8 -hlabound=-1 -otfss -otfssL -otfssMLDB=16 -ics -ics_window=40000 -ics_processLast=50000 -ics_keepNew -ics_relLBD=0.5 -ics_relSIZE=1.2 -sUhdProbe=1 -sUhdPrSh=8 -sUHLEsize=64"), false);
    } else if (optionSet == "BMC4") {
        parseOptions(
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            + std::string(" -no-enabled_cp3 -no-rer -R=1.5 -szLBDQueue=60 -firstReduceDB=8000 -keepWorst=0.05 -biAsFreq=16 -minSizeMinimizingClause=50 -minLBDMinimizingClause=9 -var-decay-b=0.85 -var-decay-d=10000 -agil-limit=0.33 -agil-decay=0.99 -agil-init=0.01 -cla-decay=0.995 -init-act=2 -rlevel=1 -rinc=1.5 -alluiphack=2 -varActB=1 -dontTrust -hack=1 -actStart=2048 -rMax=1024 -hlaMax=1000 -hlaLevel=1 -hlaevery=8 -hlabound=16000 -sInterval=3 -otfssL -ics -ics_window=80000 -ics_processLast=20000 -ics_shrinkNew -ics_relLBD=1.2 -ics_relSIZE=0.5 -sUhdPrSh=2"), false);
    } else if (optionSet == "BMC5") {
        parseOptions(
            std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO ")
            + std::string(" -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000")
            + std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 ")
            + std::string(" -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 ")
            + std::string(" -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust ")
            + std::string(" -lhbr=3 -lhbr-sub -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 ")
            + std::string(" -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 ")
            + std::string(" -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce ")

            // but additionally set some better preset default side parameters
            + std::string(" -pr-bins -cp3_res_bin -no-xorFindSubs -no-xorKeepUsed -xorSelect=1 -xorMaxSize=9")
            + std::string(" -biAsFreq=4 -no-rer-l -rer-rn -otfssL")
            + std::string(" -no-enabled_cp3 -no-rer -R=1.2 -szLBDQueue=60 -szTrailQueue=3500 -lbdIgnL0 -quickRed -keepWorst=0.001 -biAsFreq=4 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -no-hack-cost -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -otfss -otfssL -learnDecP=80 -ics_window=5050 -no-sUhdPrRb -sUHLEsize=8"), false);
    }

    else if (optionSet == "BIASSERTING") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -biAsserting -biAsFreq=4"), false);
    } else if (optionSet == "EDACC1") {
        parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce") , false);
    } else if (optionSet == "EDACC2") {
        parseOptions(std::string("-K=0.7 -R=1.5 -szLBDQueue=45 -firstReduceDB=2000 -minLBDFrozenClause=15 -incLBD -quickRed -keepWorst=0.01 -var-decay-e=0.99 -var-decay-d=10000 -cla-decay=0.995 -rnd-freq=0.005 -init-act=2 -init-pol=2 -rlevel=2 -varActB=1 -lhbr=4 -lhbr-max=16000 -hack=1 -actIncMode=2 -sInterval=2 -otfss -otfssL -learnDecP=80 -er-size=16 -er-lbd=18 -sUhdProbe=1 -sUhdPrSh=4 -sUHLEsize=30 -cp3_ee_bIter=10 -card_maxC=7 -no-pr-double -pr-keepI=0") , false);
    } else if (optionSet == "EDACC3") {
        parseOptions(std::string("-K=0.7 -R=1.5 -szLBDQueue=45 -lbdIgnL0 -incLBD -var-decay-b=0.99 -var-decay-e=0.85 -var-decay-i=0.99 -cla-decay=0.995 -rnd-freq=0.005 -phase-saving=0 -init-act=2 -init-pol=2 -rlevel=2 -varActB=2 -lhbr=3 -lhbr-max=16000 -hack=1 -actIncMode=1 -sInterval=2 -otfss -otfssL -learnDecP=50 -er-size=8 -er-lbd=12 -sUhdProbe=2 -sUhdPrSh=4 -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=10 -card_minC=6 -card_maxC=7 -card_max=32 -no-pr-double -pr-lhbr -pr-probeL=500000 -pr-keepL=0") , false);
    } else if (optionSet == "EDACC4") {
        parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce -enabled_cp3 -cp3_stats -bve -bve_red_lits=1") , false);
    } else if (optionSet == "EDACC5") {
        parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce  -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce") , false);
    } else if (optionSet == "EDACC6") {
        parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_bva_limit=120000") , false);
    } else if (optionSet == "FASTRESTART") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -rlevel=1") , false);
    } else if (optionSet == "NOTRUST") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dontTrust") , false);
    } else if (optionSet == "PRB") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -probe -no-pr-vivi -pr-bins -pr-lhbr -no-pr-nce") , false);
    } else if (optionSet == "RATEBCEUNHIDE") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -bce-bce -rate -rate-limit=50000000000") , false);
    } else if (optionSet == "RERRW") {
        parseOptions(std::string("-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -cp3_uhdUHLE=0 -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu -hlabound=4096 -rer -rer-rn -no-rer-l") , false);
    } else if (optionSet == "Riss3g") {
        parseOptions(std::string("-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -cp3_uhdUHLE=0 -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu -hlabound=4096 ") , false);
    } else if (optionSet == "Riss3gND") {
        parseOptions(std::string("-enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -cp3_uhdUHLE=0 -cp3_uhdIters=5 -hlaevery=1 -hlaLevel=5 -laHack -tabu -hlabound=4096 ") , false);
    } else if (optionSet == "Riss427i") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -dense -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -cp3_ptechs=fgvb -cp3_itechs=gsewxp -inprocess -cp3_inp_cons=8000000 -probe -no-pr-vivi -pr-bins -pr-lhbr -no-pr-nce -subsimp -ee -cp3_ee_it -cp3_ee_level=2 -bva -cp3_bva_limit=120000 -cp3_Xbva=2 -xor -dense") , false);
    } else if (optionSet == "Riss427nd" || optionSet == "RissND427") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce") , false);
    } else if (optionSet == "Riss427") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense") , false);
    } else if (optionSet == "Riss427-NoCLE") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -dense"), false);
    } else if (optionSet == "RISSLGL3") {
        parseOptions(
            std::string("-R=1.5 -szLBDQueue=45 -incReduceDB=450 -specialIncReduceDB=1100 -minLBDFrozenClause=15 -lbdIgnL0 -lbdupd=0 -incLBD -keepWorst=0.001 -biAsFreq=16 -var-decay-b=0.85 -var-decay-e=0.85 -agil-limit=0.33 -agil-init=0.01 -agil-add=32 -rnd-freq=0.005 -phase-saving=0 -init-act=1 -init-pol=4 -rlevel=1 -rtype=1 -rfirst=32 -rinc=4 -clsActB=1 -lhbr-max=16000 -longConflict -actIncMode=1 -rMax=1024 -rMaxInc=1.2 -laHack -laEEp=50 -hlaLevel=1 -hlaevery=8 -hlabound=16000 -sInterval=1 -otfss -otfssMLDB=64 -learnDecP=80 -no-rer-l -rer-r=1 -rer-min-size=6 -rer-max-size=30 -rer-maxLBD=6 -rer-new-act=2 -rer-freq=0.1")
            + std::string("-er-size=64 -sUhdProbe=2 -no-sUhdPrRb -sUHLEsize=64 -cp3_vars=2000000 -cp3_cls=4000000 -no-cp3_limited -cp3_inp_cons=500000 -cp3_iters=2 -enabled_cp3 -inprocess -no-randInp -subsimp -rate -bve -probe -symm -fm -cp3_ptechs= -cp3_itechs= -cp3_bve_limit=2500000 -bve_cgrow=-1 -bve_cgrow_t=10000 -bve_heap_updates=2 -cp3_bva_Vlimit=5000000 -no-cp3_Abva -cp3_bva_Xlimit=0 -cp3_bva_Ilimit=0 -cp3_Ibva=2 -bce-limit=200000000 -no-bce-bce -rate-limit=18000000000 -rate-min=2 -cp3_ee_glimit=1000000 -cp3_ee_limit=2000000 -cp3_ee_bIter=400000000 -no-cp3_extAND -cp3_extITE -cp3_extXOR -cp3_fm_limit=12000000 ")
            + std::string("-cp3_fm_maxA=50 -cp3_fm_newAmo=0 -no-cp3_fm_keepM -cp3_fm_newAlo=0 -card_maxC=7 -card_max=2 -pr-uips=1 -pr-csize=4 -pr-lhbr -pr-probeL=500000 -pr-viviL=7500000 -pr-keepLHBR=1 -cp3_res3_ncls=10000 -cp3_res_percent=0.005 -sls-adopt-cls -all_strength_res=4 -no-cp3_strength -cp3_sub_limit=3000000 -cp3_str_limit=400000000 -sym-iter=2 -sym-propF -sym-clLearn -sym-cons=100 -cp3_uhdIters=8 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdPrSize=8 -xorMaxSize=6 -xorSelect=1 -no-xorKeepUsed -no-xorFindSubs")
            , false);
    } else if (optionSet == "RISSLGL4") {
        parseOptions(
            std::string("-K=0.7 -R=1.2 -firstReduceDB=2000 -specialIncReduceDB=1100 -incLBD -keepWorst=0.001 -biAsserting -biAsFreq=16 -var-decay-b=0.75 -var-decay-i=0.99 -var-decay-d=10000 -agil-limit=0.33 -cla-decay=0.995 -init-act=3 -init-pol=5 -rlevel=2 -rtype=2 -rfirst=32 -rinc=3 -alluiphack=2 -varActB=2 -clsActB=1 -dontTrust -no-hack-cost -actIncMode=2 -rMax=1024 -rMaxInc=1.2 -laHack -laEEl -laEEp=66 -hlaLevel=1 -hlaevery=0 -hlaTop=512 -otfss -otfssMLDB=2 -learnDecP=50 -no-rer-l -rer-r=1 -rer-min-size=15 ")
            + std::string("-rer-max-size=2 -rer-minLBD=30 -rer-maxLBD=15 -rer-new-act=4 -er-size=16 -er-lbd=18 -sUhdProbe=1 -sUhdPrSh=2 -sUHLEsize=64 -sUHLElbd=12 -cp3_vars=1000000 -cp3_cls=2000000 -no-cp3_limited -cp3_inp_cons=200000 -cp3_iters=2 -enabled_cp3 -inc-inp -up -subsimp -rate -ee -bva -probe -dense -symm ")
            + std::string("-cp3_ptechs= -cp3_itechs= -sls-flips=-1 -xor -cp3_bve_limit=50000000 -cp3_bve_heap=1 -bve_cgrow_t=10000 -bve_totalG -bve_heap_updates=2 -cp3_bva_Vlimit=1000000 -cp3_bva_limit=12000000 -cp3_bva_Xlimit=0 -cp3_Xbva=2 -cp3_Ibva=2 -bce-limit=200000000 -no-rat-compl -rate-limit=900000000 -rate-min=5 -cp3_ee_glimit=100000 -cp3_ee_limit=2000000 -cp3_ee_bIter=400000000 -cp3_ee_it -cp3_fm_maxConstraints=0 -cp3_fm_maxA=3 -cp3_fm_grow=5 -cp3_fm_growT=1000 -no-cp3_fm_vMulAMO -cp3_fm_newAlk=1 -card_Elimit=600000 ")
            + std::string("-pr-bins -pr-lhbr -pr-probeL=500000 -pr-keepL=0 -pr-viviP=60 -cp3_res_bin -cp3_res3_steps=2000000 -cp3_res3_ncls=1000000 -cp3_res_percent=0.005 -sls-rnd-walk=2200 -all_strength_res=4 -cp3_str_limit=3000000 -sym-min=4 -sym-ratio=0.1 -sym-iter=0 -sym-propF -sym-clLearn -sym-consT=100000 -cp3_uhdIters=8 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdProbe=3 -cp3_uhdPrSize=4 -cp3_uhdPrEE -xorLimit=120000 -xorSelect=1 -no-xorKeepUsed -no-xorFindSubs ")
            , false);
    } else if (optionSet == "SUHLE") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -sUHLEsize=30") , false);
    } else if (optionSet == "XBVA") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -bva -cp3_Xbva=2 -cp3_bva_limit=120000") , false);
    } else if (optionSet == "XOR") {
        parseOptions(std::string(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -xor") , false);
    } else if (optionSet == "LABSDRAT") {
        parseOptions(
            std::string("-rnd-seed=9207562  -no-cp3_res3_reAdd -hack=0 -no-bve_unlimited -dense -no-up -cp3_bva_Vlimit=3000000 -bve -cp3_res_inpInc=2000 -no-longConflict -cp3_res_eagerSub -no-ee -unhide -specialIncReduceDB=1000 -bve_cgrow=-1 -no-cp3_bva_compl -rtype=2 -rfirst=1000 -cp3_sub_limit=400000000 -enabled_cp3 -phase-saving=2 -laHack -bve_totalG -3resolve -cp3_bva_dupli -rinc=2 -bve_cgrow_t=10000 -cp3_call_inc=50 -bve_heap_updates=1 -ccmin-mode=2 -cp3_bva_limit=12000000 -hlabound=1024 -no-bve_gates -no-probe -cp3_uhdUHLE=0 -sls-rnd-walk=2000 -no-bve_strength -no-bve_BCElim -cp3_randomized -minLBDFrozenClause=50 -sls -hlaevery=8 -bva -cp3_ptechs=u3sghpvwc -gc-frac=0.3 -tabu -subsimp -rnd-freq=0.005 -cp3_bva_subOr -cp3_res3_ncls=100000 -cp3_uhdNoShuffle -cla-decay=0.995 -no-inprocess -cp3_uhdIters=1 -cp3_bva_push=2 -bve_red_lits=1 -hlaLevel=5 -no-rew -alluiphack=2 -cp3_bve_heap=1 -no-hte -var-decay-e=0.99 -var-decay-b=0.99 -all_strength_res=4 -firstReduceDB=4000 ") +
            std::string("-no-cp3_res_bin -cp3_bva_incInp=20000 -cp3_res3_steps=100000 -no-dyn -minLBDMinimizingClause=6 -cp3_strength -cp3_uhdUHTE -no-cce -sls-adopt-cls -cp3_str_limit=300000000 -sls-ksat-flips=-1 -cp3_uhdTrans -cp3_bve_limit=2500000 -minSizeMinimizingClause=15 -incReduceDB=200 -hlaTop=-1")
            , false);
    } else if (optionSet == "EDACC5DRAT") {
        parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -dontTrust -lhbr=3 -lhbr-sub -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -no-sUhdPrRb -sUHLEsize=30 -sUHLElbd=12 -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce  -enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -no-fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce") , false);
    } else if (optionSet == "RISSLGL4DRAT") {
        parseOptions(
            std::string("-K=0.7 -R=1.2 -firstReduceDB=2000 -specialIncReduceDB=1100 -incLBD -keepWorst=0.001 -biAsserting -biAsFreq=16 -var-decay-b=0.75 -var-decay-i=0.99 -var-decay-d=10000 -agil-limit=0.33 -cla-decay=0.995 -init-act=3 -init-pol=5 -rlevel=2 -rtype=2 -rfirst=32 -rinc=3 -alluiphack=2 -varActB=2 -clsActB=1 -dontTrust -no-hack-cost -actIncMode=2 -rMax=1024 -rMaxInc=1.2 -no-laHack -laEEl -laEEp=66 -hlaLevel=1 -hlaevery=0 -hlaTop=512 -otfss -otfssMLDB=2 -learnDecP=50 -no-rer-l -rer-r=1 -rer-min-size=15 -rer-max-size=2 -rer-minLBD=30 -rer-maxLBD=15 -rer-new-act=4 -er-size=16 -er-lbd=18 -sUhdProbe=1 -sUhdPrSh=2 -sUHLEsize=64 -sUHLElbd=12 -cp3_vars=1000000 -cp3_cls=2000000 -no-cp3_limited -cp3_inp_cons=200000 -cp3_iters=2 -enabled_cp3 -inc-inp -up -subsimp -rate -ee -bva -probe -dense -symm -cp3_ptechs= -cp3_itechs= -sls-flips=-1 -cp3_bve_limit=50000000 -cp3_bve_heap=1 -bve_cgrow_t=10000 -bve_totalG -bve_heap_updates=2 -cp3_bva_Vlimit=1000000 -cp3_bva_limit=12000000") +
            std::string(" -cp3_bva_Xlimit=0 -cp3_Xbva=2 -cp3_Ibva=2 -bce-limit=200000000 -no-rat-compl -rate-limit=900000000 -rate-min=5 -cp3_ee_glimit=100000 -cp3_ee_limit=2000000 -cp3_ee_bIter=400000000 -cp3_ee_it -cp3_fm_maxConstraints=0 -cp3_fm_maxA=3 -cp3_fm_grow=5 -cp3_fm_growT=1000 -no-cp3_fm_vMulAMO -cp3_fm_newAlk=1 -card_Elimit=600000 -pr-bins -pr-lhbr -pr-probeL=500000 -pr-keepL=0 -pr-viviP=60 -cp3_res_bin -cp3_res3_steps=2000000 -cp3_res3_ncls=1000000 -cp3_res_percent=0.005 -sls-rnd-walk=2200 -all_strength_res=4 -cp3_str_limit=3000000 -sym-min=4 -sym-ratio=0.1 -sym-iter=0 -sym-propF -sym-clLearn -sym-consT=100000 -cp3_uhdIters=8 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdProbe=3 -cp3_uhdPrSize=4 -cp3_uhdPrEE")
            , false);
    }
    /*
     *  End Options for Riss427
     */



    else if (optionSet == "FOCUS") {
        parseOptions(" -var-decay-b=0.85 -var-decay-e=0.85", false);
    } else if (optionSet == "STRONGFOCUS") {
        parseOptions(" -var-decay-b=0.75 -var-decay-e=0.75", false);
    } else if (optionSet == "riss4") {
        parseOptions(std::string(" -lbdupd=1 -enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -cp3_uhdUHLE=0 -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu -hlabound=4096 ")
                     , false);
    } else if (optionSet == "riss3g") {
        parseOptions(std::string(" -lbdupd=0 -enabled_cp3 -cp3_stats -up -subsimp -all_strength_res=3 -bva -cp3_bva_limit=120000 -bve -bve_red_lits=1 -no-bve_BCElim -cce -cp3_cce_steps=2000000 -cp3_cce_level=1 -cp3_cce_sizeP=100 -unhide -cp3_uhdUHLE=0 -cp3_uhdIters=5 -dense -hlaevery=1 -hlaLevel=5 -laHack -tabu -hlabound=4096 ")
                     , false);
    } else if (optionSet == "plain_SUSI") {
        parseOptions(" -enabled_cp3 -cp3_stats -subsimp", false);
    } else if (optionSet == "plain_ASTR") {
        parseOptions(" -enabled_cp3 -cp3_stats -all_strength_res=3 -subsimp", false);
    } else if (optionSet == "plain_BVE") {
        parseOptions(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1", false);
    } else if (optionSet == "BVEEARLY") {
        parseOptions(" -bve_early", false);
    } else if (optionSet == "plain_ABVA") {
        parseOptions(" -enabled_cp3 -cp3_stats -bva", false);
    } else if (optionSet == "plain_XBVA") {
        parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Xbva=2 -no-cp3_Abva", false);
    } else if (optionSet == "plain_IBVA") {
        parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Ibva=2 -no-cp3_Abva", false);
    } else if (optionSet == "plain_BVA") {
        parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Xbva=2 -cp3_Ibva=2", false);
    } else if (optionSet == "plain_BCE") {
        parseOptions(" -enabled_cp3 -cp3_stats -bce ", false);
    } else if (optionSet == "plain_CLE") {
        parseOptions(" -enabled_cp3 -cp3_stats -bce -bce-cle -no-bce-bce", false);
    } else if (optionSet == "plain_BCM") {
        parseOptions(" -enabled_cp3 -cp3_stats -bce -bce-bcm -no-bce-bce", false);
    } else if (optionSet == "plain_HTE") {
        parseOptions(" -enabled_cp3 -cp3_stats -hte", false);
    } else if (optionSet == "plain_CCE") {
        parseOptions(" -enabled_cp3 -cp3_stats -cce", false);
    } else if (optionSet == "plain_RATE") {
        parseOptions(" -enabled_cp3 -cp3_stats -rate -rate-limit=50000000000", false);
    } else if (optionSet == "plain_PRB") {
        parseOptions(" -enabled_cp3 -cp3_stats -probe -no-pr-vivi -pr-bins -pr-lhbr ", false);
    } else if (optionSet == "plain_VIV") {
        parseOptions(" -enabled_cp3 -cp3_stats -probe -no-pr-probe -pr-bins", false);
    } else if (optionSet == "plain_3RES") {
        parseOptions(" -enabled_cp3 -cp3_stats -3resolve", false);
    } else if (optionSet == "plain_UNHIDE") {
        parseOptions(" -enabled_cp3 -cp3_stats -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans ", false);
    } else if (optionSet == "plain_UHD-PR") {
        parseOptions(" -enabled_cp3 -cp3_stats -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3", false);
    } else if (optionSet == "plain_ELS") {
        parseOptions(" -enabled_cp3 -cp3_stats -ee -cp3_ee_it -cp3_ee_level=2 ", false);
    } else if (optionSet == "plain_FM") {
        parseOptions(" -enabled_cp3 -cp3_stats -fm -no-cp3_fm_vMulAMO", false);
    } else if (optionSet == "plain_XOR") {
        parseOptions(" -enabled_cp3 -cp3_stats -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed", false);
    } else if (optionSet == "plain_2SAT") {
        parseOptions(" -enabled_cp3 -cp3_stats -2sat", false);
    } else if (optionSet == "plain_SLS") {
        parseOptions(" -enabled_cp3 -cp3_stats -sls -sls-flips=16000000", false);
    } else if (optionSet == "plain_SYMM") {
        parseOptions(" -enabled_cp3 -cp3_stats -symm -sym-clLearn -sym-prop -sym-propF -sym-cons=128 -sym-consT=128000", false);
    } else if (optionSet == "plain_DENSE") {
        parseOptions(" -enabled_cp3 -cp3_stats -dense", false);
    } else if (optionSet == "plain_DENSEFORW") {
        parseOptions(" -enabled_cp3 -cp3_stats -cp3_dense_forw -dense", false);
    } else if (optionSet == "plain_LHBR") {
        parseOptions(" -lhbr=3 -lhbr-sub", false);
    } else if (optionSet == "plain_OTFSS") {
        parseOptions(" -otfss", false);
    } else if (optionSet == "plain_AUIP") {
        parseOptions(" -alluiphack=2", false);
    } else if (optionSet == "plain_LLA") {
        parseOptions(" -laHack -tabu -hlabound=-1", false);
    } else if (optionSet == "plain_SUHD") {
        parseOptions(" -sUhdProbe=3", false);
    } else if (optionSet == "plain_SUHLE") {
        parseOptions(" -sUHLEsize=30", false);
    } else if (optionSet == "plain_HACKTWO") {
        parseOptions(" -hack=2", false);
    } else if (optionSet == "plain_NOTRUST") {
        parseOptions(" -dontTrust", false);
    } else if (optionSet == "plain_DECLEARN") {
        parseOptions(" -learnDecP=100 -learnDecMS=6", false);
    } else if (optionSet == "plain_BIASSERTING") {
        parseOptions(" -biAsserting -biAsFreq=4", false);
    } else if (optionSet == "plain_LBD") {
        parseOptions(" -lbdIgnL0 -lbdupd=0", false);
    } else if (optionSet == "plain_RER") {
        parseOptions(" -rer", false);
    } else if (optionSet == "plain_RERRW") {
        parseOptions(" -rer -rer-rn -no-rer-l ", false);
    } else if (optionSet == "plain_ECL") {
        parseOptions(" -ecl -ecl-min-size=12 -ecl-maxLBD=6", false);
    } else if (optionSet == "plain_FASTRESTART") {
        parseOptions(" -rlevel=1 ", false);
    } else if (optionSet == "plain_SEMIFASTRESTART") {
        parseOptions(" -rlevel=2 ", false);
    } else if (optionSet == "plain_AGILREJECT") {
        parseOptions(" -agil-r -agil-limit=0.35", false);
    } else if (optionSet == "plain_LIGHT") {
        parseOptions("  -rlevel=1 -ccmin-mode=1 -lbdupd=0 -minSizeMinimizingClause=3 -minLBDMinimizingClause=3 -no-updLearnAct", false);
    }
// ShiftBMC configurations
    else if (optionSet == "BMC_FULL") {
        parseOptions(" -enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -bva -cp3_Xbva=2 -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -ee -cp3_ee_it -cp3_ee_level=2 -bce -bce-cle -probe -no-pr-vivi -pr-bins -all_strength_res=0 -cp3_stats -ee_freeze_eager -cp3_stats -no-pr-nce", false);
    } else if (optionSet == "BMC_BVEPRBAST") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -probe -no-pr-vivi -pr-bins -all_strength_res=3  -cp3_stats  -no-pr-nce", false);
    } else if (optionSet == "BMC_FULLNOPRB") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -bva -cp3_Xbva=2 -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -ee -cp3_ee_it -cp3_ee_level=2 -bce -bce-cle -all_strength_res=3 -ee_freeze_eager -cp3_stats", false);
    } else if (optionSet == "BMC_BVEUHDAST") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -all_strength_res=3  -cp3_stats", false);
    } else if (optionSet == "BMC_BVEBVAAST") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -bva -cp3_Xbva=2  -all_strength_res=3  -cp3_stats  -no-pr-nce", false);
    } else if (optionSet == "BMC_BVECLE") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -bce -bce-cle -cp3_stats", false);
    } else if (optionSet == "BMC_BEBE") {
        parseOptions("-enabled_cp3 -dense -cp3_dense_forw -bve -bve_red_lits=1 -bce -cp3_iters=2 -cp3_stats", false);
    } else if (optionSet == "BMC_NODENSE") {
        parseOptions("-no-dense", false);
    }
// CSSC 2013 configurations
    else if (optionSet == "BMC08ext") {
        parseOptions(std::string(" -rnd-seed=9207562  -rMax=10000 -szLBDQueue=70 -no-enabled_cp3 -lhbr-max=20000000 -init-act=3 -alluiphack=2 -szTrailQueue=4000 -phase-saving=2 -longConflict -lhbr=1 -hack=0 ")
                     + std::string(" -updLearnAct -otfssL -no-laHack -firstReduceDB=8000 -ccmin-mode=2 -minLBDFrozenClause=15 -learnDecP=100 -incReduceDB=450 -gc-frac=0.1 -rtype=0 -rMaxInc=2 -otfssMLDB=30 -init-pol=4 ")
                     + std::string(" -var-decay-e=0.85 -var-decay-b=0.85 -minSizeMinimizingClause=30 -no-lhbr-sub -specialIncReduceDB=900 -rnd-freq=0 -otfss -minLBDMinimizingClause=6 -cla-decay=0.995 -R=1.6 -K=0.7 ")
                     , false);
    } else if (optionSet == "BMC08") {
        parseOptions(std::string(" -rnd-seed=9207562  -rtype=0 -no-bve_force_gates -minLBDFrozenClause=15 -no-bve_totalG -bve_cgrow=0 -cp3_cce_steps=2000000 -up -enabled_cp3 -szLBDQueue=30 -cp3_cce_level=3 -cp3_uhdUHLE=1 -cce -all_strength_res=0")
                     +   std::string(" -no-ee -no-tabu -hlaTop=-1 -cp3_sub_limit=3000000 -bve_gates -bve_strength -szTrailQueue=5000 -bve_heap_updates=2 -R=1.2 -alluiphack=0 -no-bva -gc-frac=0.2 -cla-decay=0.995 -no-cp3_randomized")
                     +   std::string(" -no-hte -firstReduceDB=16000 -phase-saving=2 -no-sls -specialIncReduceDB=1000 -no-cp3_uhdTrans -cp3_uhdUHTE -hlaLevel=5 -hlaevery=1 -no-longConflict -dense -rMax=-1 -no-dyn -hack=0 -incReduceDB=300 -laHack")
                     +   std::string(" -cp3_cce_sizeP=40 -cp3_uhdIters=3 -ccmin-mode=2 -no-cp3_uhdNoShuffle -minSizeMinimizingClause=30 -cp3_bve_limit=25000000 -no-inprocess -bve_red_lits=0 -cp3_call_inc=100 -no-bve_unlimited -cp3_str_limit=300000000")
                     +   std::string(" -hlabound=4096 -no-probe -K=0.8 -bve -unhide -cp3_strength -cp3_bve_heap=0 -var-decay-e=0.85 -var-decay-b=0.85 -rnd-freq=0.005 -minLBDMinimizingClause=6 -subsimp -no-rew -no-3resolve -bve_BCElim ")
                     , false);
    } else if (optionSet == "CircuitFuzz") {
        parseOptions(std::string(" -rnd-seed=9207562  -no-probe -rfirst=1000 -sls -bve_strength -cp3_bve_limit=2500000 -phase-saving=1 -bve_heap_updates=2 -no-laHack -hack=1 -cp3_bva_Vlimit=3000000 -cp3_ptechs=u3svghpwc -cp3_cce_level=1 ")
                     + std::string(" -sls-ksat-flips=20000000 -bve -cp3_bva_incInp=200 -cp3_cce_sizeP=40 -cp3_uhdUHTE -no-rew -all_strength_res=3 -hte -no-cp3_bva_subOr -rinc=2 -cp3_randomized -bve_unlimited -unhide -rtype=2 -minLBDFrozenClause=50")
                     + std::string(" -no-hack-cost -cp3_uhdNoShuffle -no-3resolve -cce -sls-rnd-walk=500 -minSizeMinimizingClause=15 -no-inprocess -cla-decay=0.995 -cp3_cce_steps=2000000 -cp3_bva_limit=12000000 -subsimp -specialIncReduceDB=900")
                     + std::string(" -up -incReduceDB=300 -bva -no-ee -cp3_bva_push=1 -cp3_hte_steps=214748 -cp3_bve_heap=0 -no-bve_totalG -enabled_cp3 -rnd-freq=0.005 -var-decay-e=0.99 -ccmin-mode=2 -dense -cp3_call_inc=50 -gc-frac=0.3 -sls-adopt-cls -bve_BCElim")
                     + std::string(" -cp3_uhdUHLE=0 -bve_red_lits=1 -no-cp3_uhdTrans -no-cp3_strength -bve_cgrow=20 -cp3_str_limit=400000000 -cp3_uhdIters=3 -no-longConflict -minLBDMinimizingClause=6 -no-bve_gates -cp3_sub_limit=300000000 -firstReduceDB=4000 -cp3_bva_dupli -no-cp3_bva_compl -alluiphack=2 ")
                     , false);
    }

    else if (optionSet == "GIext") {
        parseOptions(std::string(" -rnd-seed=9207562  -learnDecP=75 -incReduceDB=200 -var-decay-e=0.85 -var-decay-b=0.85 -otfssL -lhbr-max=200000 -otfssMLDB=16 -minLBDMinimizingClause=9 -gc-frac=0.3 -vsids-s=0 -vsids-e=0  -rfirst=1")
                     + std::string(" -minLBDFrozenClause=15 -firstReduceDB=2000 -ccmin-mode=2 -no-longConflict -hack=0 -init-pol=5 -cla-decay=0.995 -specialIncReduceDB=900 -phase-saving=2 -no-lhbr-sub -rtype=1 -otfss -no-laHack -no-enabled_cp3 -updLearnAct -rnd-freq=0.5 -lhbr=3 -init-act=0 -alluiphack=2 -minSizeMinimizingClause=50 ")
                     , false);
    }

    else if (optionSet == "GI") {
        parseOptions(std::string(" -rnd-seed=9207562  -hack=0 -alluiphack=2 -szTrailQueue=4000 -szLBDQueue=30 -cla-decay=0.995 -minSizeMinimizingClause=30 -minLBDFrozenClause=15 -no-longConflict -incReduceDB=300 -var-decay-b=0.99 -var-decay-e=0.99 -rtype=0 -minLBDMinimizingClause=9 -firstReduceDB=4000 -rnd-freq=0 -gc-frac=0.1 -specialIncReduceDB=1100 -phase-saving=0 -no-laHack -no-enabled_cp3 -rMax=-1 -R=1.4 -K=0.7 -ccmin-mode=2 "), false);
    } else if (optionSet == "IBMext") {
        parseOptions(std::string(" -rnd-seed=9207562  -updLearnAct -szTrailQueue=6000 -rnd-freq=0.5 -no-cp3_randomized -no-cp3_res3_reAdd -cp3_res3_steps=100000 -hack-cost -bve_cgrow_t=10000 -no-dyn -no-bve_BCElim -cp3_cce_level=1 -sym-clLearn -pr-csize=4 -cp3_hte_steps=2147483 -init-act=2 -hlabound=-1 -no-ee -cp3_rew_ratio -no-unhide -sym-ratio=0.3 -sym-consT=1000 -firstReduceDB=2000 -K=0.7 -3resolve -var-decay-e=0.85 -var-decay-b=0.85")
                     + std::string(" -sym-prop -rtype=0 -pr-viviL=5000000 -no-fm -bve -no-xor -sls-ksat-flips=2000000000 -sls-adopt-cls -no-otfss -minSizeMinimizingClause=30 -lhbr=4 -learnDecP=100 -cp3_cce_inpInc=60000 -cp3_hte_inpInc=60000 -sls-rnd-walk=4000 -rMax=-1 -no-pr-probe -inprocess -alluiphack=2 -cp3_rew_Vlimit=1000000 -no-bve_unlimited -cp3_res_bin -gc-frac=0.1 -rew -no-randInp -minLBDMinimizingClause=3 -laHack -cp3_cce_steps=3000000")
                     + std::string(" -cp3_res_eagerSub -cp3_bve_resolve_learnts=2 -cp3_rew_min=2 -hack=1 -cp3_bve_learnt_growth=0 -symm -no-subsimp -cp3_itechs=up -cp3_rew_minA=13 -ccmin-mode=0 -no-cp3_rew_1st -sym-min=4 -cce -bve_heap_updates=1 -sym-size -sym-cons=0 -probe -cp3_res_inpInc=2000 -cp3_inp_cons=80000 -cp3_bve_heap=1 -up -specialIncReduceDB=900 -no-longConflict -bve_red_lits=0 -R=2.0 -bve_strength -cp3_bve_inpInc=50000")
                     + std::string(" -lhbr-sub -no-dense -cp3_rew_Addlimit=10000 -cla-decay=0.995 -cp3_bve_limit=25000000 -sym-propA -hlaTop=64 -cp3_viv_inpInc=100000 -no-bva -pr-viviP=60 -minLBDFrozenClause=30 -bve_cgrow=10 -sym-iter=0 -incReduceDB=450 -no-bve_gates -init-pol=0 -szLBDQueue=70 -no-sym-propF -sls -phase-saving=1 -hlaLevel=3 -cp3_ptechs=u3svghpwcv -enabled_cp3 -lhbr-max=20000000 -cp3_rew_limit=120000 -cp3_res3_ncls=100000 -hlaevery=8 -cp3_cce_sizeP=80 -inc-inp -hte -tabu -pr-vivi -bve_totalG ")
                     , false);
    } else if (optionSet == "IBM") {
        parseOptions(std::string(" -rnd-seed=9207562  -incReduceDB=300 -no-cce -no-3resolve -up -no-sls -enabled_cp3 -no-bve_totalG -minSizeMinimizingClause=30 -firstReduceDB=4000 -no-cp3_randomized -cp3_ptechs="" -gc-frac=0.1 -no-ee -dense -cp3_bve_heap=0 -bve_strength -szLBDQueue=50 -cp3_bve_limit=25000000 -cla-decay=0.999 -no-bve_unlimited -no-unhide -hack=0 -cp3_call_inc=100 -ccmin-mode=2 -bve_heap_updates=2 -bve_gates")
                     + std::string(" -R=1.4 -K=0.8 -rnd-freq=0.005 -no-hte -szTrailQueue=5000 -specialIncReduceDB=1000 -no-rew -minLBDFrozenClause=30 -bve_red_lits=1 -var-decay-e=0.85 -var-decay-b=0.85 -subsimp -no-inprocess -cp3_sub_limit=300000 -cp3_strength -cp3_str_limit=300000000 -no-bve_force_gates -bve_BCElim -bve -no-bva -alluiphack=0 -all_strength_res=0 -rMax=-1 -phase-saving=2 -minLBDMinimizingClause=3 -no-longConflict -bve_cgrow=0 -rtype=0 -no-probe -no-laHack ")
                     , false);
    } else if (optionSet == "LABSext") {
        parseOptions(std::string(" -rnd-seed=9207562  -rMax=10000 -no-lhbr-sub -gc-frac=0.3 -alluiphack=2 -no-enabled_cp3 -no-tabu -hlaMax=75 -var-decay-e=0.95 -var-decay-b=0.95 -R=1.6 -K=0.8 -rtype=0 -no-otfssL -minSizeMinimizingClause=50")
                     + std::string(" -laHack -specialIncReduceDB=900 -rnd-freq=0.5 -hlabound=1024 -hack-cost -szTrailQueue=4000 -otfss -minLBDFrozenClause=50 -no-longConflict -firstReduceDB=8000 -learnDecP=75 -init-act=5 -hlaevery=64 -ccmin-mode=2 -updLearnAct -otfssMLDB=30 -minLBDMinimizingClause=6 -lhbr=3 -init-pol=5 -incReduceDB=300 -hlaLevel=4 -dyn -rMaxInc=2 -phase-saving=0 -szLBDQueue=30 -lhbr-max=2000000 -hlaTop=64 -hack=1 -cla-decay=0.5 ")
                     , false);
    } else if (optionSet == "LABS") {
        parseOptions(std::string(" -rnd-seed=9207562  -no-cp3_res3_reAdd -hack=0 -no-bve_unlimited -dense -no-up -cp3_bva_Vlimit=3000000 -bve -cp3_res_inpInc=2000 -no-longConflict -cp3_res_eagerSub -no-ee -unhide -specialIncReduceDB=1000 -bve_cgrow=-1 -no-cp3_bva_compl -rtype=2 -rfirst=1000 -cp3_sub_limit=400000000 -enabled_cp3 -phase-saving=2 -laHack -bve_totalG -3resolve -cp3_bva_dupli -rinc=2 -bve_cgrow_t=10000 -cp3_call_inc=50")
                     + std::string(" -bve_heap_updates=1 -ccmin-mode=2 -cp3_bva_limit=12000000 -hlabound=1024 -no-bve_gates -no-probe -cp3_uhdUHLE=0 -sls-rnd-walk=2000 -no-bve_strength -no-bve_BCElim -cp3_randomized -minLBDFrozenClause=50 -sls -hlaevery=8 -bva -cp3_ptechs=u3sghpvwc -gc-frac=0.3 -tabu -subsimp -rnd-freq=0.005 -cp3_bva_subOr -cp3_res3_ncls=100000 -cp3_uhdNoShuffle -cla-decay=0.995 -no-inprocess -cp3_uhdIters=1")
                     + std::string(" -cp3_bva_push=2 -bve_red_lits=1 -hlaLevel=3 -no-rew -alluiphack=2 -cp3_bve_heap=1 -no-hte -var-decay-e=0.99 -var-decay-b=0.99 -all_strength_res=4 -firstReduceDB=4000 -no-cp3_res_bin -cp3_bva_incInp=20000 -cp3_res3_steps=100000 -no-dyn -minLBDMinimizingClause=6 -cp3_strength -cp3_uhdUHTE -no-cce -sls-adopt-cls -cp3_str_limit=300000000 -sls-ksat-flips=-1 -cp3_uhdTrans -cp3_bve_limit=2500000 -minSizeMinimizingClause=15 -incReduceDB=200 -hlaTop=-1 ")
                     , false);
    } else if (optionSet == "SWVext") {
        parseOptions(std::string(" -rnd-seed=9207562  -no-otfss -learnDecP=66 -alluiphack=2 -var-decay-e=0.7 -var-decay-b=0.7 -rnd-freq=0 -incReduceDB=200 -gc-frac=0.2 -minLBDFrozenClause=15 -init-pol=2 -ccmin-mode=0 -minLBDMinimizingClause=6 -no-longConflict -hack=0  -cla-decay=0.5 -rfirst=1000 -phase-saving=2 -firstReduceDB=8000 -no-enabled_cp3 -updLearnAct -rtype=2 -lhbr=0 -specialIncReduceDB=1100 -no-laHack -rinc=3 -minSizeMinimizingClause=30 -init-act=3 "), false);
    } else if (optionSet == "SWV") {
        parseOptions(std::string(" -rnd-seed=9207562  -no-unhide -szTrailQueue=6000 -no-sls -no-probe -gc-frac=0.3 -dense -bve -no-bva -minSizeMinimizingClause=30 -no-laHack -up -rMax=40000  -cla-decay=0.999 -no-bve_totalG -bve_cgrow=10 -subsimp -cp3_str_limit=300000000 -cp3_bve_limit=2500000 -no-cce -R=2.0 -K=0.95 -rtype=0 -cp3_strength -cp3_call_inc=200 -no-bve_BCElim -phase-saving=2 -minLBDMinimizingClause=6 -no-inprocess -no-bve_gates")
                     + std::string(" -alluiphack=2 -all_strength_res=5 -var-decay-e=0.7 -var-decay-b=0.7 -rMaxInc=1.5 -longConflict -cp3_sub_limit=300000 -specialIncReduceDB=1000 -rnd-freq=0 -minLBDFrozenClause=15 -enabled_cp3 -ccmin-mode=2 -no-bve_unlimited -incReduceDB=450 -hack=0 -firstReduceDB=16000 -no-ee -no-cp3_randomized -szLBDQueue=30 -no-rew -no-hte -cp3_bve_heap=0 -bve_strength -bve_red_lits=0 -bve_heap_updates=2 -no-3resolve ")
                     , false);
	/** test configs for Blackbox */
    } else if (optionSet == "Realtime.data1") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data2") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=4 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data3") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -lbd-core-th=5 "), false);
    } else if (optionSet == "Realtime.data4") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000"), false);
    } else if (optionSet == "Realtime.data5") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based"), false);
    } else if (optionSet == "Realtime.data6") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5"), false);
    } else if (optionSet == "Realtime.data7") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=100 -rinc=1.5 -act-based"), false);
    } else if (optionSet == "Realtime.data8") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=20000 -cp3_itechs=ue -no-dense -up"), false);
    } else if (optionSet == "Realtime.data9") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -keepWorst=0.01"), false);
    } else if (optionSet == "Realtime.data10") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -probe -no-pr-vivi -pr-bins -pr-lhbr -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data11") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -bva -cp3_bva_limit=5000000"), false);
    } else if (optionSet == "Realtime.data12") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -bce -bce-bce -no-bce-cle -bce-bin "), false);
    } else if (optionSet == "Realtime.data13") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data14") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=500000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "Realtime.data15") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "Realtime.data16") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "Realtime.data17") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -sUHLEsize=30 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data18") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -symm -sym-clLearn -sym-prop -sym-propF -sym-cons=128 -sym-consT=128000 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "Realtime.data19") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -longConflict"), false);
    } else if (optionSet == "Realtime.data20") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed  -biAsserting -biAsFreq=4 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } 

    else {
        ret = false; // indicate that no configuration has been found here!
        if (optionSet != "") { parseOptions(optionSet); }     // parse the string that has been parsed as commandline
    }
    parsePreset = false;
    return ret; // return whether a preset configuration has been found
}


inline
bool Config::parseOptions(const std::string& options, bool strict, int activeLevel)
{
    if (options.size() == 0) { return false; }
    // split std::string into sub std::strings, separated by ' '
    std::vector<std::string> optionList;
    int lastStart = 0;
    int findP = 0;
    while (findP < options.size()) {   // tokenize std::string
        findP = options.find(" ", lastStart);
        if (findP == std::string::npos) { findP = options.size(); }

        if (findP - lastStart - 1 > 0) {
            optionList.push_back(options.substr(lastStart , findP - lastStart));
        }
        lastStart = findP + 1;
    }
//  std::cerr << "c work on option std::string " << options << std::endl;
    // create argc - argv combination
    char* argv[ optionList.size() + 1]; // one dummy in front!
    for (int i = 0; i < optionList.size(); ++ i) {
//    std::cerr << "add option " << optionList[i] << std::endl;
        argv[i + 1] = (char*)optionList[i].c_str();
    }
    int argc = optionList.size() + 1;

    // call conventional method
    bool ret = parseOptions(argc, argv, strict, activeLevel);
    return ret;
}


inline
bool Config::parseOptions(int& argc, char** argv, bool strict, int activeLevel)
{
    if (optionListPtr == 0) { return false; }  // the options will not be parsed

//     if( !parsePreset ) {
//       if( defaultPreset.size() != 0 ) { // parse default preset instead of actual options!
//  setPreset( defaultPreset ); // setup the preset configuration
//  defaultPreset = "" ;        // now, nothing is preset any longer
//  return false;
//       }
//     }

    // usual way to parse options
    int i, j;
    bool ret = false; // printed help?
    for (i = j = 1; i < argc; i++) {
        const char* str = argv[i];
        if (match(str, "--") && match(str, Option::getHelpPrefixString()) && match(str, "help")) {
            if (*str == '\0') {
                this->printUsageAndExit(argc, argv, false, activeLevel);
                ret = true;
            } else if (match(str, "-verb")) {
                this->printUsageAndExit(argc, argv, true, activeLevel);
                ret = true;
            }
            argv[j++] = argv[i]; // keep -help in parameters!
        } else {
            bool parsed_ok = false;

            for (int k = 0; !parsed_ok && k < optionListPtr->size(); k++) {
                parsed_ok = (*optionListPtr)[k]->parse(argv[i]);

                // fprintf(stderr, "checking %d: %s against flag <%s> (%s)\n", i, argv[i], Option::getOptionList()[k]->name, parsed_ok ? "ok" : "skip");
            }

            if (!parsed_ok)
                if (strict && match(argv[i], "-")) {
                    fprintf(stderr, "ERROR! Unknown flag \"%s\". Use '--%shelp' for help.\n", argv[i], Option::getHelpPrefixString()), exit(1);
                } else {
                    argv[j++] = argv[i];
                }
        }
    }

    argc -= (i - j);
    return ret; // return indicates whether a parameter "help" has been found
}

inline
void Config::printUsageAndExit(int  argc, char** argv, bool verbose, int activeLevel)
{
    const char* usage = Option::getUsageString();
    if (usage != nullptr) {
        fprintf(stderr, "\n");
        fprintf(stderr, usage, argv[0]);
    }

    sort((*optionListPtr), Option::OptionLt());

    const char* prev_cat  = nullptr;
    const char* prev_type = nullptr;

    for (int i = 0; i < (*optionListPtr).size(); i++) {

        if (activeLevel >= 0 && (*optionListPtr)[i]->getDependencyLevel() > activeLevel) { continue; }  // can jump over full categories

        const char* cat  = (*optionListPtr)[i]->category;
        const char* type = (*optionListPtr)[i]->type_name;

        if (cat != prev_cat) {
            fprintf(stderr, "\n%s OPTIONS:\n\n", cat);
        } else if (type != prev_type) {
            fprintf(stderr, "\n");
        }

        (*optionListPtr)[i]->help(verbose);

        prev_cat  = (*optionListPtr)[i]->category;
        prev_type = (*optionListPtr)[i]->type_name;
    }
}

inline
bool Config::checkConfiguration()
{
    return true;
}

inline
void Config::printOptions(FILE* pcsFile, int printLevel)
{
    sort((*optionListPtr), Option::OptionLt());

    const char* prev_cat  = nullptr;
    const char* prev_type = nullptr;

    // all options in the global list
    for (int i = 0; i < (*optionListPtr).size(); i++) {

        if (printLevel >= 0 && (*optionListPtr)[i]->getDependencyLevel() > printLevel) { continue; }  // can jump over full categories

        const char* cat  = (*optionListPtr)[i]->category;
        const char* type = (*optionListPtr)[i]->type_name;

        // print new category
        if (cat != prev_cat) {
            fprintf(pcsFile, "\n#\n#%s OPTIONS:\n#\n", cat);
        } else if (type != prev_type) {
            fprintf(pcsFile, "\n");
        }

        // print the actual option
        (*optionListPtr)[i]->printOptions(pcsFile, printLevel);

        // set prev values, so that print is nicer
        prev_cat  = (*optionListPtr)[i]->category;
        prev_type = (*optionListPtr)[i]->type_name;
    }
}

inline
void Config::printOptionsDependencies(FILE* pcsFile, int printLevel)
{
    sort((*optionListPtr), Option::OptionLt());

    const char* prev_cat  = nullptr;
    const char* prev_type = nullptr;

    // all options in the global list
    for (int i = 0; i < (*optionListPtr).size(); i++) {

        if ((*optionListPtr)[i]->dependOnNonDefaultOf == 0 ||    // no dependency
                (printLevel >= 0 && (*optionListPtr)[i]->getDependencyLevel() > printLevel)) { // or too deep in the dependency level
            continue;
        }  // can jump over full categories

        const char* cat  = (*optionListPtr)[i]->category;
        const char* type = (*optionListPtr)[i]->type_name;

        // print new category
        if (cat != prev_cat) {
            fprintf(pcsFile, "\n#\n#%s OPTIONS:\n#\n", cat);
        } else if (type != prev_type) {
            fprintf(pcsFile, "\n");
        }

        // print the actual option
        (*optionListPtr)[i]->printOptionsDependencies(pcsFile, printLevel);

        // set prev values, so that print is nicer
        prev_cat  = (*optionListPtr)[i]->category;
        prev_type = (*optionListPtr)[i]->type_name;
    }
}

inline
void Config::configCall(std::stringstream& s)
{
    if (optionListPtr == 0) { return; }
    // fill the stream for all the options
    for (int i = 0; i < optionListPtr->size(); i++) {
        // if there is an option that has not its default value, print its call
        if (!(*optionListPtr)[i]->hasDefaultValue()) {
            (*optionListPtr)[i]->printOptionCall(s);
            s << " ";
        }
    }
}

};


#endif
