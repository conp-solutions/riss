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

    /** set all options back to their default value */
    void reset();
};

inline
Config::Config(vec<Option*>* ptr, const std::string& presetOptions)
    : optionListPtr(ptr)
    , parsePreset(false)
    , defaultPreset(presetOptions)
{
}

inline
void Config::reset() {
	if(optionListPtr == 0 ) return;
	for( int i = 0 ; i < optionListPtr->size(); ++ i  ){
		(*optionListPtr)[i]->reset();
	}
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
    
//     std::cerr << "parse preset: " << optionSet << std::endl;

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
    } else if (optionSet == "STRONGUNSAT") {
        parseOptions( std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE ")
	             + std::string("-cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense ")
		     + std::string("-probe -no-pr-vivi -pr-bins -pr-lhbr ")
		     + std::string("-xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed ")
		     + std::string("-cp3_iters=2 -up -ee -cp3_ee_level=3 -cp3_ee_it -cp3_uhdProbe=4 -cp3_uhdPrSize=5 -rlevel=2 -bve_early ")
		     + std::string("-revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uepgsxvf ")
		     + std::string("-sUhdProbe=3 -sUHLEsize=30 ")
		     , false);
    } else if (optionSet == "equival") {
        parseOptions( std::string("-enabled_cp3 -subsimp -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 ")
	             + std::string("-cp3_uhdTrans ")
		     + std::string("-xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed ")
		     + std::string("-cp3_iters=2 -up ")
		     , false);
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
    }  else if (optionSet == "INCSIMP") {
        parseOptions("-enabled_cp3:-subsimp:-fm:-no-cp3_fm_vMulAMO:-unhide:-cp3_uhdIters=5:-cp3_uhdEE:-cp3_uhdTrans:-xor:-no-xorFindSubs:-xorEncSize=3:-xorLimit=100000:-no-xorKeepUsed:-cp3_iters=2:-no-randInp:-cp3_inp_cons=50000:-cp3_iinp_cons=1000000", false);
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
    } 
    // "505" is defined below
    else if (optionSet == "RISSLGL3") {
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
	/** old test configs for Blackbox */
    } else if (optionSet == "OldRealTime.data1") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data2" || optionSet == "505-O") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=4 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data3") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -lbd-core-th=5 "), false);
    } else if (optionSet == "OldRealTime.data4") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000"), false);
    } else if (optionSet == "OldRealTime.data5" || optionSet == "505-N" ) {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based"), false);
    } else if (optionSet == "OldRealTime.data6" || optionSet == "505-Q" ) {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=1000 -rinc=1.5"), false);
    } else if (optionSet == "OldRealTime.data7") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -firstReduceDB=200000 -rtype=1 -rfirst=100 -rinc=1.5 -act-based"), false);
    } else if (optionSet == "OldRealTime.data8") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=20000 -cp3_itechs=ue -no-dense -up"), false);
    } else if (optionSet == "OldRealTime.data9" || optionSet == "505-M" || optionSet == "505") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -keepWorst=0.01"), false);
    } else if (optionSet == "OldRealTime.data10") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -probe -no-pr-vivi -pr-bins -pr-lhbr -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data11") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -bva -cp3_bva_limit=5000000"), false);
    } else if (optionSet == "OldRealTime.data12") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -bce -bce-bce -no-bce-cle -bce-bin "), false);
    } else if (optionSet == "OldRealTime.data13") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data14") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=500000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "OldRealTime.data15") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=1000000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "OldRealTime.data16") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -inprocess -cp3_inp_cons=30000 -cp3_itechs=uev -no-dense -up"), false);
    } else if (optionSet == "OldRealTime.data17") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -sUHLEsize=30 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data18") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -symm -sym-clLearn -sym-prop -sym-propF -sym-cons=128 -sym-consT=128000 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    } else if (optionSet == "OldRealTime.data19" || optionSet == "505-R" ) {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048 -longConflict"), false);
    } else if (optionSet == "OldRealTime.data20" || optionSet == "505-P") {
      parseOptions(std::string("-enabled_cp3 -cp3_stats -bve -bve_red_lits=1 -fm -no-cp3_fm_vMulAMO -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -bce -bce-cle -no-bce-bce -dense -xor -no-xorFindSubs -xorEncSize=3 -xorLimit=100000 -no-xorKeepUsed  -biAsserting -biAsFreq=4 -cp3_iters=2 -ee -cp3_ee_level=3 -cp3_ee_it -rlevel=2 -bve_early -revMin -init-act=3 -actStart=2048"), false);
    }

    /* new blackbox configurations */
    else if (optionSet == "RealTime.data1") {
      parseOptions(std::string("-keepWorst=0.01 -init-act=3 -rlevel=2 -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data2") {
      parseOptions(std::string("-firstReduceDB=200000 -init-act=3 -rlevel=2 -rtype=1 -rfirst=1000 -rinc=1.5 -act-based -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data3") {
      parseOptions(std::string("-init-act=4 -rlevel=2 -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data4") {
      parseOptions(std::string("-biAsserting -biAsFreq=4 -init-act=3 -rlevel=2 -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3 -master"), false);
    } else if (optionSet == "RealTime.data5") {
      parseOptions(std::string("-firstReduceDB=200000 -init-act=3 -rlevel=2 -rtype=1 -rfirst=1000 -rinc=1.5 -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data6") {
      parseOptions(std::string("-init-act=3 -rlevel=2 -longConflict -actStart=2048 -revMin -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data7") {
      parseOptions(std::string("-init-act=3 -rlevel=2 -actStart=2048 -revMin -sUHLEsize=30 -enabled_cp3 -cp3_iters=2 -cp3_stats -bce -ee -bve -unhide -dense -fm -xor -bve_early -no-bce-bce -bce-cle -cp3_ee_level=3 -cp3_ee_it -no-cp3_fm_vMulAMO -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorLimit=100000 -no-xorKeepUsed -no-xorFindSubs -xorEncSize=3"), false);
    } else if (optionSet == "RealTime.data8") {
      parseOptions(std::string("-specialIncReduceDB=900 -minLBDFrozenClause=50 -minSizeMinimizingClause=15 -var-decay-e=0.99 -cla-decay=0.995 -rnd-freq=0.005 -rnd-seed=9.20756e+06 -phase-saving=1 -rtype=2 -rfirst=1000 -gc-frac=0.3 -alluiphack=2 -enabled_cp3 -cp3_randomized -up -subsimp -hte -cce -bve -bva -unhide -dense -cp3_ptechs=u3svghpwc -sls -cp3_bve_limit=2500000")
       + std::string(" -bve_unlimited -no-bve_gates -bve_cgrow=20 -bve_BCElim -bve_heap_updates=2 -bve_early -cp3_bva_push=1 -cp3_bva_limit=12000000 -cp3_bva_incInp=200 -no-cp3_bva_compl -cp3_cce_level=1 -cp3_hte_steps=214748 -sls-rnd-walk=500 -sls-adopt-cls -all_strength_res=3 -no-cp3_strength -cp3_str_limit=400000000 -cp3_call_inc=50 -cp3_uhdUHLE=0 -cp3_uhdNoShuffle"), false);
    } else if (optionSet == "RealTime.data9") {
      parseOptions(std::string("-incReduceDB=200 -minLBDFrozenClause=50 -minSizeMinimizingClause=15 -var-decay-b=0.99 -var-decay-e=0.99 -cla-decay=0.995 -rnd-freq=0.005 -rnd-seed=9.20756e+06 -rtype=2 -rfirst=1000 -gc-frac=0.3 -alluiphack=2 -laHack -hlaLevel=3 -hlaevery=8 -hlabound=1024 -enabled_cp3 -cp3_randomized -subsimp -bve -bva -unhide -3resolve -dense -cp3_ptechs=u3sghpvwc -sls -cp3_bve_limit=2500000 -no-bve_strength -no-bve_gates")
      + std::string(" -cp3_bve_heap=1 -bve_cgrow=-1 -bve_cgrow_t=10000 -bve_totalG -bve_early -cp3_bva_limit=12000000 -cp3_bva_incInp=20000 -no-cp3_bva_compl -cp3_bva_subOr -cp3_res3_steps=100000 -cp3_res_inpInc=2000 -sls-ksat-flips=-1 -sls-adopt-cls -all_strength_res=4 -cp3_sub_limit=400000000 -cp3_call_inc=50 -cp3_uhdIters=1 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdNoShuffle"), false);
    } else if (optionSet == "RealTime.data10") {
      parseOptions(std::string(" -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -sUHLEsize=30 -sUHLElbd=12 -bve_early -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce"), false);
    } else if (optionSet == "RealTime.data11") {
      parseOptions(std::string("-K=0.7 -R=1.2 -firstReduceDB=2000 -specialIncReduceDB=1100 -incLBD -keepWorst=0.001 -biAsserting -biAsFreq=16 -var-decay-b=0.75 -var-decay-i=0.99 -var-decay-d=10000 -cla-decay=0.995 -init-act=3 -init-pol=5 -rlevel=2 -rtype=2 -rfirst=32 -rinc=3 -alluiphack=2 -varActB=2 -clsActB=1 -actIncMode=2 -rMax=1024 -rMaxInc=1.2 -laHack -laEEl -laEEp=66 -hlaLevel=1 -hlaevery=0 -hlaTop=512 -otfss -otfssMLDB=2 -learnDecP=50 -no-rer-l -rer-r=1 -rer-min-size=15 -rer-max-size=2 -rer-minLBD=30 -rer-maxLBD=15 -rer-new-act=4 -er-size=16 -er-lbd=18 -sUhdProbe=1 -sUhdPrSh=2 ")
                 + std::string("-sUHLEsize=64 -sUHLElbd=12 -enabled_cp3 -cp3_vars=1000000 -cp3_cls=2000000 -no-cp3_limited -cp3_inp_cons=200000 -cp3_iters=2 -inc-inp -up -subsimp -rate -ee -bva -probe -dense -symm -cp3_ptechs= -cp3_itechs= -sls-flips=-1 -xor -cp3_bve_limit=50000000 -cp3_bve_heap=1 -bve_cgrow_t=10000 -bve_totalG -bve_heap_updates=2 -bve_early -cp3_bva_Vlimit=1000000 -cp3_bva_limit=12000000 -cp3_Xbva=2 -cp3_Ibva=2 -cp3_bva_Xlimit=0 -bce-limit=200000000 -no-rat-compl -rate-limit=900000000 -rate-min=5 -cp3_ee_glimit=100000 -cp3_ee_limit=2000000 -cp3_ee_bIter=400000000 ")
                 + std::string(" -cp3_ee_it -cp3_fm_maxConstraints=0 -cp3_fm_maxA=3 -cp3_fm_grow=5 -cp3_fm_growT=1000 -no-cp3_fm_vMulAMO -cp3_fm_newAlk=1 -card_Elimit=600000 -pr-probeL=500000 -pr-keepL=0 -pr-viviP=60 -cp3_res_bin -cp3_res3_steps=2000000 -cp3_res3_ncls=1000000 -cp3_res_percent=0.005 -sls-rnd-walk=2200 -all_strength_res=4 -cp3_str_limit=3000000 -sym-min=4 -sym-ratio=0.1 -sym-iter=0 -sym-propF -sym-clLearn -sym-consT=100000 -cp3_uhdIters=8 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdProbe=3 -cp3_uhdPrSize=4 -cp3_uhdPrEE -xorLimit=120000 -xorSelect=1 -no-xorKeepUsed -no-xorFindSubs"), false);
    } else if (optionSet == "RealTime.data12") {
      parseOptions(std::string(" -K=0.7 -szLBDQueue=30 -szTrailQueue=4000 -specialIncReduceDB=1100 -minLBDFrozenClause=15 -minLBDMinimizingClause=9 -var-decay-b=0.99 -var-decay-e=0.99 -cla-decay=0.995 -rnd-seed=9.20756e+06 -phase-saving=0 -gc-frac=0.1 -alluiphack=2 -bve_early"), false);
    } else if (optionSet == "RealTime.data13") {
      parseOptions(std::string("-K=0.85 -R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -firstReduceDB=8000 -incReduceDB=450 -specialIncReduceDB=2000 -lbdIgnL0 -quickRed -keepWorst=0.001 -biAsserting -biAsFreq=16 -minSizeMinimizingClause=50 -minLBDMinimizingClause=9 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-i=0.001 -var-decay-d=10000 -cla-decay=0.995")
                 + std::string(" -rnd-freq=0.01 -init-act=3 -init-pol=5 -rlevel=1 -rtype=1 -rfirst=32 -alluiphack=2 -clsActB=2 -laHack -dyn -laEEl -laEEp=66 -hlaMax=25 -hlaLevel=1") + std::string(" -hlaevery=8 -hlabound=-1 -hlaTop=512 -sInterval=1 -otfss -otfssL -otfssMLDB=16 -learnDecP=80 -no-rer-l -rer-rn -er-size=16 -er-lbd=12 -ics -ics_window=40000 -ics_processLast=50000 -ics_keepNew -ics_relLBD=0.5 -ics_relSIZE=1.2 -sUhdProbe=1 -sUhdPrSh=8 -sUHLEsize=64 -sUHLElbd=12 -cp3_stats -bce -bve -bva -unhide -fm -bve_early -cp3_bva_limit=120000 -no-bce-bce -bce-cle -no-cp3_fm_vMulAMO -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce -cp3_res_bin -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE -xorMaxSize=9 -xorSelect=1 -no-xorKeepUsed -no-xorFindSubs"), false);
    } else if (optionSet == "RealTime.data14") {
      parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -sUHLEsize=30 -sUHLElbd=12 -enabled_cp3 -cp3_stats -bce -bve -unhide -fm -bve_early -no-bce-bce -bce-cle -cp3_ee_bIter=400000000 -no-cp3_fm_vMulAMO -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce -cp3_uhdIters=5 -cp3_uhdTrans -cp3_uhdEE"), false);
    }

    /* missing 505 configurations */    
    else if (optionSet == "505-W") {
      parseOptions(std::string("-K=0.7 -szLBDQueue=30 -szTrailQueue=4000 -specialIncReduceDB=1100 -minLBDFrozenClause=15 -minLBDMinimizingClause=9 -var-decay-b=0.99 -var-decay-e=0.99 -cla-decay=0.995 -rnd-seed=9.20756e+06 -phase-saving=0 -gc-frac=0.1 -alluiphack=2 -bve_early"), false);
    } else if (optionSet == "505-A") {
      parseOptions(std::string("-rmf -no-refConflict -K=0.00953763 -R=2.69566 -szLBDQueue=500 -szTrailQueue=215 -sbr=2014602 -firstReduceDB=48309241 -incReduceDB=23 -specialIncReduceDB=1481 -minLBDFrozenClause=1408661 -lbdIgnLA -lbdupd=0 -quickRed -keepWorst=0.00286563 -biAsserting -biAsFreq=472528808 -minSizeMinimizingClause=12739472 -minLBDMinimizingClause=319077377 -var-decay-b=0.987968 -var-decay-e=0.0269589 -var-decay-i=0.20846 -var-decay-d=18944007 -cla-decay=0.00296903 -rnd-freq=2.50577e-06 -rnd-seed=1.86955 -ccmin-mode=0 -phase-saving=1 -init-act=5 -init-pol=2 -rlevel=1 -rfirst=11892305 -rinc=8150.15 -irlevel=108396 -gc-frac=0.000287351 -alluiphack=1 -vsids-s=0.00399595 -vsids-e=6.39627e-06 -vsids-i=0.002067 -vsids-d=4460 -varActB=2 -clsActB=1 -prob-step-width=550027167 -prob-limit=100196123 -cir-bump=640998539 -act-based -lbd-core-th=749824521 -reduce-frac=0.000134581 -size-core=1136316431 -no-updLearnAct -actIncMode=3") 
      + std::string(" -actStart=0.00581872 -actDec=0.289836 -polMode -rMax=333233048 -rMaxInc=1.97763e+08 -laHack -no-tabu -laEEl -laEEp=55 -hlaMax=2857010 -hlaLevel=3 -hlaevery=94676 -hlabound=367063191 -hlaTop=1487924763 -sInterval=1 -learnDecP=63 -learnDecMS=165691314 -learnDecRER -rer -rer-rn -no-rer-f -rer-max-size=3866 -rer-minLBD=1214549607 -rer-maxLBD=34435313 -rer-window=1340491470 -rer-new-act=2 -rer-freq=0.623353 -rer-ga=2594.84 -er-size=438462 -er-lbd=27 -ics_window=2102 -ics_processLast=5477 -ics_keepNew -ics_dyn -ics_relLBD=1.99964 -ics_relSIZE=150.422 -revMin -revMinSize=231 -revMinLBD=2 -sUhdPrSh=1954637665 -sUHLEsize=2017366675 -sUHLElbd=1457637369 -verb-proof=1 -proof-oft-check=2 -incResAct=311117081 -incResPol=2143238832 -incClean=56254586 -incClSize=68991 -incClLBD=52 -incResCnt=521770513 -enabled_cp3 -cp3_vars=490 -cp3_cls=8 -cp3_lits=14 -cp3_ipvars=246 -cp3_ipcls=231064 -cp3_iplits=1486841 -cp3_randomized ") 
      + std::string(" -cp3_inp_cons=371 -cp3_iters=219597260 -no-randInp -inc-inp -cp3_stats -up -hte -ee -unhide -probe -3resolve -dense -no-simplify -symm -sls -sls-flips=1915331369 -xor -rew -2sat -cp3_susi_vars=1199 -cp3_susi_cls=1 -cp3_susi_lits=4 -cp3_hte_vars=9 -cp3_hte_cls=76872145 -cp3_hte_lits=7609 -cp3_bce_vars=33 -cp3_bce_cls=32 -cp3_bce_lits=1532726 -cp3_ent_vars=21158 -cp3_ent_cls=1282848129 -cp3_ent_lits=6285 -cp3_la_vars=3 -cp3_la_cls=1093251879 -cp3_la_lits=93 -cp3_cce_vars=8 -cp3_cce_cls=69 -cp3_cce_lits=889847390 -cp3_rate_vars=1067025103 -cp3_rate_cls=120 -cp3_rate_lits=5374 -cp3_ee_vars=326850269 -cp3_ee_cls=210912446 -cp3_ee_lits=1513 -cp3_bve_vars=4080 -cp3_bve_cls=377851 -cp3_bve_lits=534146 -cp3_bva_vars=39 -cp3_bva_cls=5156 -cp3_bva_lits=37254 -cp3_unhide_vars=9082452 -cp3_unhide_cls=1409538 -cp3_unhide_lits=1 -cp3_tRes_vars=123968232 -cp3_tRes_cls=264420431 -cp3_tRes_lits=2478132 -cp3_aBin_vars=42007 ") 
      + std::string("-cp3_aBin_cls=703256 -cp3_aBin_lits=3 -cp3_symm_vars=16848 -cp3_symm_cls=1952 -cp3_symm_lits=794448 -cp3_fm_vars=733685 -cp3_fm_cls=32999049 -cp3_fm_lits=8310 -cp3_xor_vars=337273 -cp3_xor_cls=742135456 -cp3_xor_lits=182216 -cp3_sls_vars=25691688 -cp3_sls_cls=742442 -cp3_sls_lits=276211 -cp3_rew_vars=585 -cp3_rew_cls=14347 -cp3_rew_lits=1 -cp3_par_bve=2 -cp3_bve_verbose=2 -cp3_bve_limit=147517919 -cp3_bve_learnt_growth=665112151 -bve_unlimited -no-bve_strength -bve_force_gates -cp3_bve_heap=3 -bve_cgrow=-456006145 -bve_cgrow_t=13404165 -bve_BCElim -bve_heap_updates=2 -bve_early -bce_only -cp3_bve_inpInc=244 -par_bve_th=6 -postp_lockd_neighb=72283830 -cp3_bva_Vlimit=79942657 -cp3_bva_limit=11108091 -cp3_bva_Amax=4685 -cp3_bva_incInp=9077955 -cp3_Abva_heap=10 -no-cp3_bva_dupli -cp3_bva_subOr -cp3_bva_Xlimit=160388 -cp3_bva_Ilimit=505667 -cp3_bva_Xmax=9251 -cp3_bva_Imax=48116707 -cp3_Ibva_heap=3 ") 
      + std::string("-cp3_Ibva_vars=1082000515 -cp3_Ibva_cls=1037025 -cp3_Ibva_lits=165848 -cp3_Xbva_vars=14 -cp3_Xbva_cls=692931 -cp3_Xbva_lits=10474 -no-bce-compl -bce-bin -bce-limit=666747509 -bce-cle -bce-cla -bce-incInp=7 -bce-verbose=3 -cla-limit=1677229528 -la-claStep=703283417 -la-claMax=229559765 -la-claIter=42670056 -ala-limit=1404901741 -la-alaIter=787556187 -cp3_cce_steps=775048091 -cp3_cce_sizeP=16 -cp3_cce_inpInc=959670 -no-rat-compl -rate-limit=226589305169244832 -ratm-limit=3313 -rate-brat -rate-min=370948794 -rate-bcs -rate-ratm -cp3_dense_frag=7 -ent-min=8 -cp3_ee_level=2 -cp3_ee_glimit=3053443 -cp3_ee_cIter=492046896 -no-cp3_BigThenGate -ee_sub -ee_reset -cp3_ee_limit=1975332 -cp3_ee_inpInc=2 -cp3_ee_bIter=46886353 -cp3_extITE -cp3_extXOR -cp3_extExO -cp3_extBlocked -cp3_addBlocked -no-cp3_extNgtInput -no-cp3_extImplied -cp3_fm_maxConstraints=156 -cp3_fm_limit=7416 -cp3_fm_Slimit=391891330071 -cp3_fm_maxA=30") 
      + std::string(" -cp3_fm_grow=212171432 -cp3_fm_growT=41 -no-cp3_fm_amt -no-cp3_fm_twoPr -no-cp3_fm_merge -cp3_fm_vMulAMT -no-cp3_fm_cut -cp3_fm_newAlk=1 -no-cp3_fm_newSub -card_minC=46 -card_maxC=254 -card_max=2475 -card_Elimit=1 -card_noUnits -cp3_hte_steps=1838219993 -cp3_par_hte -cp3_hte_inpInc=1 -pr-probe -pr-uips=1606707401 -no-pr-bins -pr-csize=31617613 -no-pr-lhbr -pr-probeL=1007262910 -no-pr-EE -pr-vivi -pr-keepL=1 -pr-keepI=0 -pr-viviP=1 -pr-viviL=129537 -cp3_pr_inpInc=135294 -cp3_viv_inpInc=2 -pr-keepLHBR=1 -cp3_probe_vars=475778 -cp3_probe_cls=222325 -cp3_probe_lits=835679 -cp3_viv_vars=23515624 -cp3_viv_cls=835476 -cp3_viv_lits=32 -cp3_res3_steps=1699139202 -cp3_res3_ncls=17757 -no-cp3_res_eagerSub -cp3_res_percent=1.2788e-06 -cp3_res_add_red -cp3_res_add_lea -cp3_res_inpInc=2444 -cp3_add_inpInc=10 -cp3_rew_min=49635640 -cp3_rew_iter=7154 -cp3_rew_minA=42476 -cp3_rew_limit=4852075 -cp3_rew_Vlimit=907 ") 
      + std::string("-cp3_rew_Addlimit=3515070 -cp3_rew_imp -no-cp3_rew_exo -no-cp3_rew_ratio -no-cp3_rew_once -cp3_rew_stats -cp3_rewI_min=1161 -no-cp3_rewI_small -cp3_rew_inpInc=126596918 -shuffle-seed=977693 -sls-ksat-flips=485800666 -sls-rnd-walk=63 -sls-adopt-cls -all_strength_res=519357 -no-cp3_strength -cp3_sub_limit=1 -cp3_str_limit=5 -cp3_call_inc=9645 -cp3_sub_inpInc=195862 -cp3_par_strength=2 -cp3_par_subs=2 -susi_chunk_size=3829 -par_str_minCls=4563404 -sym-pol -sym-unit -sym-min=52 -sym-ratio=3.38788e+06 -sym-iter=157146 -sym-show -sym-print -sym-exit -sym-clLearn -sym-cons=539 -sym-consT=2848 -no-2sat-cq -cp3_uhdIters=345068920 -cp3_uhdNoShuffle -cp3_uhdTstDbl -cp3_uhdPrSize=175231543 -no-cp3_uhdPrSiBo -xorMaxSize=47 -xorLimit=20 -xorSelect=1 -xorEncSize=77400"), false);
    } else if (optionSet == "505-D") {
      parseOptions(std::string("-revRevC -K=0.259181 -R=1.58166 -szLBDQueue=273447 -szTrailQueue=14731822 -sbr=25751473 -firstReduceDB=939478133 -incReduceDB=31 -specialIncReduceDB=389 -minLBDFrozenClause=1267656326 -lbdIgnLA -lbdupd=2 -keepWorst=0.000283708 -biAsserting -biAsFreq=16 -minSizeMinimizingClause=25 -minLBDMinimizingClause=3570450 -var-decay-b=0.226218 -var-decay-e=0.000286884 -var-decay-i=0.000937729 -var-decay-d=4209 -cla-decay=0.613394 -rnd-freq=0.00132861 -rnd-seed=69629.2 -ccmin-mode=1 -rnd-init -init-act=2 -init-pol=2 -rfirst=558868 -rinc=7.1168e+07 -irlevel=2549125 -gc-frac=0.000305883 -alluiphack=2 -vsids-s=0.0351118 -vsids-e=0.0201183 -vsids-i=0.00135331 -vsids-d=108 -varActB=2 -pq-order -prob-step-width=346645344 -prob-limit=2950 -cir-bump=581434520 -lbd-core-th=444249954 -reduce-frac=0.0842508 -size-core=1981775705 -longConflict -actIncMode=2 -actStart=5.87409e+08 -actDec=0.0881315 -polMode -rMax=972075568 ") 
      + std::string("-rMaxInc=28.3962 -laEEp=14 -hlaMax=21909 -hlaLevel=2 -hlaevery=2144 -hlabound=1243595888 -hlaTop=1408433183 -sInterval=36 -learnDecP=33 -learnDecMS=38434 -learnDecRER -no-rer-l -rer-r=2 -rer-min-size=908020036 -rer-max-size=4 -rer-minLBD=19024488 -rer-maxLBD=11271287 -rer-window=164353150 -rer-new-act=2 -rer-ite -rer-freq=0.388908 -rer-e -rer-g -rer-ga=141312 -er-size=31 -er-lbd=21291 -ics -ics_window=1605779029 -ics_processLast=17021331 -ics_relLBD=187.415 -ics_relSIZE=226.307 -revMinSize=93583 -revMinLBD=5 -sUhdPrSh=1269709328 -sUHLEsize=1454013636 -sUHLElbd=1511907241 -verb-proof=2 -rup-only -proof-oft-check=5 -incResAct=408197604 -incResPol=1101937500 -incClean=1989473281 -incClSize=43 -incClLBD=50221 -incResCnt=458523480 -enabled_cp3 -inprocess -cp3_vars=3 -cp3_cls=10 -cp3_lits=21 -cp3_ipvars=6977872 -cp3_ipcls=2 -cp3_iplits=30377 -no-cp3_limited -cp3_inp_cons=1437 -cp3_iters=42919 -cp3_stats -subsimp -hte") 
      + std::string(" -ent -exp -la -cce -rate -ee -bve -unhide -addRed2 -shuffle -symm -fm -sls-flips=765025219 -xor -2sat-phase -cp3_susi_vars=783104 -cp3_susi_cls=445615911 -cp3_susi_lits=1216 -cp3_hte_vars=1756 -cp3_hte_cls=9389 -cp3_hte_lits=313 -cp3_bce_vars=7 -cp3_bce_cls=132619279 -cp3_bce_lits=1948771 -cp3_ent_vars=254775111 -cp3_ent_cls=13748137 -cp3_ent_lits=59 -cp3_la_vars=10330699 -cp3_la_cls=381697127 -cp3_la_lits=121 -cp3_cce_vars=12790 -cp3_cce_cls=13862206 -cp3_cce_lits=90013937 -cp3_rate_vars=1 -cp3_rate_cls=36 -cp3_rate_lits=258920 -cp3_ee_vars=304482960 -cp3_ee_cls=918047298 -cp3_ee_lits=37664987 -cp3_bve_vars=1101 -cp3_bve_cls=2 -cp3_bve_lits=6700090 -cp3_bva_vars=212 -cp3_bva_cls=22025996 -cp3_bva_lits=756060 -cp3_unhide_vars=8988609 -cp3_unhide_cls=61392 -cp3_unhide_lits=29 -cp3_tRes_vars=255960 -cp3_tRes_cls=48163594 -cp3_tRes_lits=1901156620 -cp3_aBin_vars=2399110 -cp3_aBin_cls=4471 -cp3_aBin_lits=30387 ") 
      + std::string("-cp3_symm_vars=438 -cp3_symm_cls=2930512 -cp3_symm_lits=794667 -cp3_fm_vars=1117149 -cp3_fm_cls=2710 -cp3_fm_lits=304 -cp3_xor_vars=127732783 -cp3_xor_cls=58974928 -cp3_xor_lits=685626742 -cp3_sls_vars=120328572 -cp3_sls_cls=4207 -cp3_sls_lits=564501 -cp3_rew_vars=1067 -cp3_rew_cls=2136 -cp3_rew_lits=2361154 -cp3_bve_limit=1919295626 -cp3_bve_learnt_growth=196934650 -bve_unlimited -bve_force_gates -cp3_bve_heap=3 -bve_cgrow=-1388291916 -bve_cgrow_t=1192193978 -bve_BCElim -bve_heap_updates=0 -bve_early -bve_progress -cp3_bve_inpInc=734529043 -par_bve_th=17217 -postp_lockd_neighb=55 -cp3_bva_Vlimit=1604749782 -cp3_Abva -cp3_bva_limit=120474835 -cp3_bva_Amax=19892120 -cp3_bva_incInp=21 -cp3_Abva_heap=2 -no-cp3_bva_compl -no-cp3_bva_dupli -cp3_Xbva=2 -cp3_bva_Xlimit=10 -cp3_bva_Ilimit=542 -cp3_bva_Xmax=2 -cp3_bva_Imax=2087591203 -cp3_Xbva_heap=6 -cp3_Ibva_heap=6 -cp3_Ibva_vars=499 -cp3_Ibva_cls=33884745 ") 
      + std::string("-cp3_Ibva_lits=3821381 -cp3_Xbva_vars=175248222 -cp3_Xbva_cls=8503140 -cp3_Xbva_lits=1 -bce-bin -bce-limit=29435 -bce-bcm -bce-incInp=31162974 -bce-verbose=3 -no-la-cla -cla-limit=612654675 -la-claStep=625889700 -la-claMax=7828233 -la-claIter=10 -ala-limit=165 -la-alaIter=8349 -la-alaBin -cp3_cce_steps=911822791 -cp3_cce_sizeP=8 -cp3_cce_inpInc=27182 -rate-limit=110326402510 -ratm-limit=83775323624499376 -rate-brat -rate-min=1918920 -rate-ratm -rate-ratm_ext -rate-ratm_rounds -cp3_dense_frag=84 -cp3_keep_set -ent-min=14 -cp3_ee_level=2 -cp3_ee_glimit=4 -cp3_ee_cIter=1174653211 -no-cp3_eagerGates -no-cp3_BigThenGate -ee_sub -ee_reset -cp3_ee_limit=78 -cp3_ee_inpInc=151 -cp3_ee_bIter=611990149 -cp3_ee_subNew -no-ee_freeze_eager -cp3_extXOR -cp3_extExO -cp3_genAND -cp3_extHASUM -cp3_addBlocked -no-cp3_extNgtInput -cp3_fm_maxConstraints=880 -cp3_fm_limit=1145621427822 -cp3_fm_Slimit=12806532458604590 ") 
      + std::string("-cp3_fm_maxA=6 -cp3_fm_grow=135 -cp3_fm_growT=25163 -no-cp3_fm_amt -no-cp3_fm_sem -no-cp3_fm_merge -no-cp3_fm_dups -no-cp3_fm_vMulAMO -cp3_fm_vMulAMT -cp3_fm_newAmo=0 -cp3_fm_newAlo=0 -cp3_fm_newAlk=0 -no-cp3_fm_newSub -cp3_fm_1st -card_minC=6 -card_maxC=22461789 -card_max=14377910 -card_Elimit=7846 -card_noUnits -cp3_hte_steps=336314810 -cp3_hteTalk -cp3_hte_inpInc=8 -pr-uips=1716446623 -no-pr-roots -pr-repeat -pr-csize=270302 -pr-probeL=390940514 -no-pr-EE -pr-vivi -pr-keepL=1 -pr-viviP=65 -pr-viviL=495828873 -cp3_pr_inpInc=4 -cp3_viv_inpInc=1692513 -pr-keepLHBR=1 -no-pr-nce -cp3_probe_vars=4095 -cp3_probe_cls=3 -cp3_probe_lits=4 -cp3_viv_vars=14639 -cp3_viv_cls=1 -cp3_viv_lits=755 -cp3_res3_steps=192 -cp3_res3_ncls=1296470 -no-cp3_res_eagerSub -cp3_res_percent=1.83315e-05 -cp3_res_add_lea -cp3_res_ars -cp3_res_inpInc=623411 -cp3_add_inpInc=2 -cp3_rew_min=71 -cp3_rew_iter=52 -cp3_rew_minA=979270704") 
      + std::string(" -cp3_rew_limit=51299948 -cp3_rew_Vlimit=2 -cp3_rew_Addlimit=897282 -no-cp3_rew_amo -cp3_rew_imp -cp3_rew_1st -cp3_rew_stats -cp3_rewI_min=190922815 -no-cp3_rewI_small -cp3_rew_inpInc=35 -shuffle-seed=1341022377 -sls-ksat-flips=2002368932 -sls-rnd-walk=236 -sls-adopt-cls -naive_strength -all_strength_res=53228 -no-cp3_strength -no-cp3_inpPrefL -cp3_sub_limit=88 -cp3_str_limit=22453915 -cp3_call_inc=2207487 -cp3_sub_inpInc=102587 -cp3_par_strength=2 -cp3_lock_stats -cp3_par_subs=2 -par_subs_counts=0 -susi_chunk_size=2096651458 -par_str_minCls=19036 -sym-size -sym-pol -sym-unit -sym-min=14380307 -sym-ratio=622.435 -sym-iter=237 -sym-print -sym-prop -sym-clLearn -sym-cons=4333 -sym-consT=141115 -no-2sat-cq -cp3_uhdIters=18273 -cp3_uhdTrans -cp3_uhdUHLE=2 -no-cp3_uhdUHTE -cp3_uhdNoShuffle -cp3_uhdProbe=2 -xorMaxSize=18 -xorLimit=218967 -no-xorKeepUsed -no-xorFindSubs -xorFindRes -xorDropPure"), false);
    } else if (optionSet == "505-F") {
      parseOptions(std::string("-no-refConflict -revRevC -K=0.495593 -R=3.78881 -szLBDQueue=1211858 -szTrailQueue=354 -sbr=13 -firstReduceDB=768684178 -incReduceDB=4679244 -specialIncReduceDB=11148728 -minLBDFrozenClause=3109 -lbdIgnL0 -lbdIgnLA -lbdupd=0 -incLBD -quickRed -keepWorst=0.00282595 -biAsFreq=3 -minSizeMinimizingClause=633794 -minLBDMinimizingClause=12729593 -var-decay-b=0.000138792 -var-decay-e=0.00183928 -var-decay-i=0.0146623 -var-decay-d=173 -cla-decay=0.00101437 -rnd-freq=0.0822436 -rnd-seed=462.778 -rnd-init -init-act=3 -init-pol=6 -rlevel=2 -rtype=1 -rfirst=185907064 -rinc=60119.5 -irlevel=1675 -gc-frac=0.079024 -alluiphack=2 -vsids-s=0.00038024 -vsids-e=0.000319707 -vsids-i=0.0850618 -vsids-d=882634785 -varActB=2 -prob-step-width=738395922 -prob-limit=18433 -cir-bump=128834465 -lbd-core-th=272944610 -reduce-frac=0.00160231 -size-core=363853139 -longConflict -actIncMode=1 -actStart=55943.4 -actDec=0.00590041") 
      + std::string(" -rMax=286287067 -rMaxInc=1.18378e+09 -no-tabu -laEEp=83 -hlaMax=119643 -hlaLevel=4 -hlaevery=4721 -hlabound=223115630 -hlaTop=7361518 -sInterval=56568033 -learnDecP=8 -learnDecMS=2064587625 -rer -no-rer-f -rer-min-size=3 -rer-max-size=43607663 -rer-minLBD=458540538 -rer-maxLBD=266805642 -rer-window=5 -rer-new-act=4 -rer-freq=0.996323 -rer-e -rer-ga=2.67572e+08 -er-size=6403277 -er-lbd=1 -ics_window=972 -ics_processLast=16342 -ics_dyn -ics_shrinkNew -ics_relLBD=2.35659e+06 -ics_relSIZE=422649 -revMin -revMinSize=234 -revMinLBD=47715 -sUhdPrSh=1025105393 -sUHLEsize=1098167257 -sUHLElbd=909571452 -rup-only -incResAct=536489813 -incResPol=2136754610 -incClean=1965745261 -incClSize=44226548 -incClLBD=18333 -incResCnt=1945140116 -enabled_cp3 -inprocess -cp3_vars=1270 -cp3_cls=501 -cp3_lits=2481291 -cp3_ipvars=6396288 -cp3_ipcls=4183 -cp3_iplits=2 -cp3_randomized -cp3_inp_cons=35422 -cp3_stats -up -hte -ent -ee ") 
      + std::string("-bve -addRed2 -no-simplify -sls-phase -sls-flips=237126723 -xor -2sat-phase -cp3_susi_vars=64677 -cp3_susi_cls=37055515 -cp3_susi_lits=15049873 -cp3_hte_vars=1180 -cp3_hte_cls=1 -cp3_hte_lits=2 -cp3_bce_vars=133 -cp3_bce_cls=781 -cp3_bce_lits=15352823 -cp3_ent_vars=178086 -cp3_ent_cls=2 -cp3_ent_lits=2 -cp3_la_vars=39923976 -cp3_la_cls=2707651 -cp3_la_lits=1 -cp3_cce_vars=173458 -cp3_cce_cls=845 -cp3_cce_lits=45724 -cp3_rate_vars=29385211 -cp3_rate_cls=10 -cp3_rate_lits=2677657 -cp3_ee_vars=50 -cp3_ee_cls=147212951 -cp3_ee_lits=729110943 -cp3_bve_vars=5691 -cp3_bve_cls=195 -cp3_bve_lits=1085803601 -cp3_bva_vars=3 -cp3_bva_cls=39080 -cp3_bva_lits=116 -cp3_unhide_vars=83101 -cp3_unhide_cls=680516810 -cp3_unhide_lits=1153 -cp3_tRes_vars=426527202 -cp3_tRes_cls=907 -cp3_tRes_lits=47 -cp3_aBin_vars=10457 -cp3_aBin_cls=4586 -cp3_aBin_lits=149 -cp3_symm_vars=32599 -cp3_symm_cls=3705681 -cp3_symm_lits=9 ") 
      + std::string("-cp3_fm_vars=312 -cp3_fm_cls=90290024 -cp3_fm_lits=138 -cp3_xor_vars=16082 -cp3_xor_cls=1 -cp3_xor_lits=58263328 -cp3_sls_vars=2188750 -cp3_sls_cls=3720 -cp3_sls_lits=1440439 -cp3_rew_vars=221520 -cp3_rew_cls=16042860 -cp3_rew_lits=2150271 -cp3_bve_verbose=4 -cp3_bve_limit=480300774 -cp3_bve_learnt_growth=1546797434 -cp3_bve_resolve_learnts=2 -bve_unlimited -no-bve_gates -bve_force_gates -bve_fdepOnly -cp3_bve_heap=8 -bve_cgrow=1765666691 -bve_cgrow_t=76621 -bve_totalG -bve_BCElim -bve_heap_updates=2 -bve_early -cp3_bve_inpInc=127927 -par_bve_th=3331 -postp_lockd_neighb=98 -par_bve_min_upd -cp3_bva_Vlimit=1661923339 -cp3_bva_limit=1587 -cp3_bva_Amax=231366 -cp3_bva_incInp=2028 -cp3_Abva_heap=5 -no-cp3_bva_dupli -cp3_Xbva=1 -cp3_bva_Xlimit=472 -cp3_bva_Ilimit=8901166 -cp3_bva_Xmax=86 -cp3_bva_Imax=105 -cp3_Xbva_heap=3 -cp3_Ibva_heap=8 -cp3_Ibva_vars=6857852 -cp3_Ibva_cls=2 -cp3_Ibva_lits=178021832 ") 
      + std::string("-cp3_Xbva_vars=197482 -cp3_Xbva_cls=17 -cp3_Xbva_lits=2 -bce-bin -bce-limit=217757199 -no-bce-bce -bce-cla -bce-cle-cons -bce-incInp=2 -bce-verbose=3 -la-ala -cla-limit=95 -la-claMax=1201986 -la-claIter=366719 -ala-limit=64 -la-alaIter=107788 -la-alaBin -cp3_cce_steps=895491946 -cp3_cce_sizeP=8 -cp3_cce_inpInc=7469824 -no-rat-compl -rate-limit=160428713 -ratm-limit=36620841821 -rate-brat -rate-min=889 -rate-ratm -cp3_dense_frag=52 -ent-min=13796 -cp3_ee_level=2 -cp3_ee_glimit=4478 -cp3_ee_cIter=991676759 -no-cp3_eagerGates -no-cp3_BigThenGate -ee_sub -ee_reset -cp3_ee_limit=48091990 -cp3_ee_inpInc=21760 -cp3_ee_bIter=145 -cp3_extITE -cp3_extXOR -cp3_extExO -cp3_extBlocked -no-cp3_extNgtInput -no-cp3_extImplied -cp3_fm_maxConstraints=206952 -cp3_fm_limit=178 -cp3_fm_Slimit=697185 -cp3_fm_maxA=1110620 -cp3_fm_grow=51196 -cp3_fm_growT=3 -no-cp3_fm_twoPr -no-cp3_fm_sem -cp3_fm_newAmo=1 -no-cp3_fm_keepM ") 
      + std::string("-cp3_fm_newAlo=0 -cp3_fm_1st -card_minC=33543389 -card_maxC=181312 -card_max=221885 -card_Elimit=17986966688792024 -cp3_hte_steps=103253771 -cp3_hte_inpInc=912323057 -pr-uips=1540601641 -no-pr-roots -pr-csize=14874275 -pr-probeL=1 -pr-vivi -pr-keepI=0 -pr-viviP=35 -pr-viviL=1440281661 -cp3_pr_inpInc=2072295 -cp3_viv_inpInc=812 -pr-keepLHBR=1 -no-pr-nce -cp3_probe_vars=11871 -cp3_probe_cls=1684675196 -cp3_probe_lits=38423 -cp3_viv_vars=922071 -cp3_viv_cls=1959696343 -cp3_viv_lits=1 -cp3_res_bin -cp3_res3_steps=6785005 -cp3_res3_ncls=176571 -no-cp3_res_eagerSub -cp3_res_percent=2.32801e-05 -no-cp3_res_add_lev -cp3_res_ars -cp3_res_inpInc=1 -cp3_add_inpInc=197 -cp3_rew_min=7949 -cp3_rew_minA=2233 -cp3_rew_limit=12 -cp3_rew_Vlimit=7200896 -cp3_rew_Addlimit=232 -no-cp3_rew_exo -cp3_rew_1st -no-cp3_rew_ratio -cp3_rewI_min=11 -cp3_rew_inpInc=190802 -shuffle-seed=34732 -sls-ksat-flips=892925536 -sls-rnd-walk=25") 
      + std::string(" -sls-adopt-cls -all_strength_res=9527614 -no-cp3_inpPrefL -cp3_sub_limit=61944541 -cp3_str_limit=6 -cp3_call_inc=27789 -cp3_sub_inpInc=129207 -cp3_par_strength=0 -par_subs_counts=0 -susi_chunk_size=381366679 -par_str_minCls=469201 -sym-size -sym-pol -sym-unit -sym-min=131788643 -sym-ratio=0.000579736 -sym-iter=209135 -sym-show -sym-exit -sym-prop -sym-propA -sym-cons=22 -sym-consT=1 -2sat-units -no-2sat-cq -cp3_uhdIters=25993039 -cp3_uhdTrans -cp3_uhdUHLE=2 -no-cp3_uhdUHTE -cp3_uhdTstDbl -cp3_uhdProbe=1 -cp3_uhdPrSize=6310 -no-cp3_uhdPrSiBo -xorMaxSize=23 -xorLimit=1196346 -xorSelect=1 -no-xorKeepUsed -xorFindRes -xorDropPure -xorEncSize=222"), false);
    } else if (optionSet == "505-H") {
      parseOptions(std::string("-solve_stats -rmf -revRevC -K=0.00631477 -R=1.14255 -szLBDQueue=254 -szTrailQueue=70 -sbr=2 -firstReduceDB=2 -incReduceDB=466 -specialIncReduceDB=107370 -minLBDFrozenClause=4669 -lbdIgnLA -lbdupd=0 -incLBD -keepWorst=0.258452 -biAsFreq=190012 -minSizeMinimizingClause=4 -minLBDMinimizingClause=13647 -var-decay-b=0.000323151 -var-decay-e=0.000471818 -var-decay-i=0.0665267 -var-decay-d=1 -cla-decay=0.0842043 -rnd-freq=7.428e-06 -rnd-seed=1014.58 -ccmin-mode=0 -init-pol=5 -rlevel=2 -rfirst=95748769 -rinc=1389.24 -irlevel=4446509 -gc-frac=0.141312 -alluiphack=1 -vsids-s=0.0143075 -vsids-e=0.000386022 -vsids-i=2.43987e-06 -vsids-d=161018244 -prob-step-width=1920591441 -prob-limit=179676111 -cir-bump=1695979065 -act-based -lbd-core-th=1467425154 -reduce-frac=0.0230113 -size-core=1714355798 -longConflict -actStart=0.115433 -actDec=0.00177151 -polMode -rMax=807436435 -rMaxInc=5.90254e+06 -laHack ") 
      + std::string("-no-tabu -laEEl -laEEp=53 -hlaMax=18 -hlaLevel=2 -hlaevery=78714 -hlabound=1584430378 -hlaTop=1237556477 -sInterval=540520247 -learnDecP=8 -learnDecMS=36895384 -learnDecRER -rer -no-rer-l -rer-rn -rer-min-size=8362721 -rer-max-size=103377 -rer-minLBD=287552949 -rer-maxLBD=1 -rer-window=778 -rer-new-act=2 -rer-ite -rer-freq=0.589225 -rer-g -rer-ga=841246 -er-size=73 -er-lbd=98 -ics_window=6417 -ics_processLast=1 -ics_shrinkNew -ics_relLBD=353754 -ics_relSIZE=10.7829 -revMin -revMinSize=5797 -revMinLBD=55006 -sUhdPrSh=1698455492 -sUHLEsize=112408269 -sUHLElbd=653096038 -verb-proof=2 -rup-only -proof-oft-check=3 -incResAct=1934763424 -incResPol=196212952 -incClean=7410454 -incClSize=58 -incClLBD=127 -incResCnt=249964169 -inprocess -cp3_vars=38990341 -cp3_cls=13758458 -cp3_lits=1 -cp3_ipvars=104 -cp3_ipcls=2152 -cp3_iplits=124955 -no-cp3_limited -cp3_randomized -cp3_inp_cons=535558 -cp3_iters=31 ") 
      + std::string("-no-randInp -inc-inp -cp3_stats -up -subsimp -hte -ent -exp -cce -rate -bve -probe -addRed2 -no-simplify -symm -fm -sls-phase -sls-flips=874496840 -xor -cp3_susi_vars=3 -cp3_susi_cls=1 -cp3_susi_lits=1 -cp3_hte_vars=143340051 -cp3_hte_cls=545103 -cp3_hte_lits=963718557 -cp3_bce_vars=10685 -cp3_bce_cls=17729987 -cp3_bce_lits=1151 -cp3_ent_vars=9 -cp3_ent_cls=380 -cp3_ent_lits=2633404 -cp3_la_vars=2122 -cp3_la_cls=1 -cp3_la_lits=8246 -cp3_cce_vars=268 -cp3_cce_cls=92998 -cp3_cce_lits=13508184 -cp3_rate_vars=27012 -cp3_rate_cls=3610315 -cp3_rate_lits=845 -cp3_ee_vars=1076767252 -cp3_ee_cls=35675 -cp3_ee_lits=6 -cp3_bve_vars=2382957 -cp3_bve_cls=9896 -cp3_bve_lits=204 -cp3_bva_vars=182 -cp3_bva_cls=4654 -cp3_bva_lits=343247 -cp3_unhide_vars=565 -cp3_unhide_cls=17913160 -cp3_unhide_lits=37792573 -cp3_tRes_vars=5 -cp3_tRes_cls=195834 -cp3_tRes_lits=1147101926 -cp3_aBin_vars=1 -cp3_aBin_cls=758 ") 
      + std::string("-cp3_aBin_lits=1656459765 -cp3_symm_vars=7 -cp3_symm_cls=15 -cp3_symm_lits=162 -cp3_fm_vars=72468650 -cp3_fm_cls=41 -cp3_fm_lits=2 -cp3_xor_vars=3384 -cp3_xor_cls=115 -cp3_xor_lits=58242349 -cp3_sls_vars=3897782 -cp3_sls_cls=490598 -cp3_sls_lits=5447 -cp3_rew_vars=561156 -cp3_rew_cls=392884545 -cp3_rew_lits=317090976 -cp3_par_bve=2 -cp3_bve_verbose=3 -cp3_bve_limit=1357166830 -cp3_bve_learnt_growth=929561745 -cp3_bve_resolve_learnts=1 -bve_unlimited -no-bve_gates -bve_force_gates -bve_fdepOnly -cp3_bve_heap=9 -bve_cgrow=-856785519 -bve_cgrow_t=110343650 -bve_BCElim -bve_heap_updates=0 -bve_early -bce_only -cp3_bve_inpInc=20525 -par_bve_th=8 -postp_lockd_neighb=2 -par_bve_min_upd -cp3_bva_push=0 -cp3_bva_Vlimit=131720212 -cp3_bva_limit=36 -cp3_bva_Amax=468239109 -cp3_bva_incInp=22192456 -no-cp3_bva_dupli -cp3_Ibva=2 -cp3_bva_Xlimit=223192 -cp3_bva_Ilimit=34830494 -cp3_bva_Xmax=1427 ") 
      + std::string("-cp3_bva_Imax=1256621905 -cp3_Xbva_heap=4 -cp3_Ibva_heap=2 -cp3_Ibva_vars=1762350337 -cp3_Ibva_cls=1 -cp3_Ibva_lits=3 -cp3_Xbva_vars=1714 -cp3_Xbva_cls=1061 -cp3_Xbva_lits=1710 -no-bce-compl -bce-limit=7150219 -no-bce-bce -bce-cle-cons -bce-incInp=135800 -bce-verbose=1 -la-ala -cla-limit=26078963 -la-claStep=4851111 -la-claMax=1 -la-claIter=12 -ala-limit=26386768 -la-alaIter=547673 -la-alaBin -cp3_cce_steps=127240559 -cp3_cce_sizeP=3 -cp3_cce_inpInc=174344 -no-rat-compl -rate-limit=7194 -ratm-limit=985893 -rate-min=6570 -rate-ratm -rate-ratm_ext -cp3_dense_frag=60 -cp3_dense_forw -ent-min=15 -cp3_ee_level=2 -cp3_ee_glimit=8148 -cp3_ee_cIter=1782032129 -ee_sub -cp3_ee_limit=157441595 -cp3_ee_inpInc=1246985574 -cp3_ee_bIter=28591 -cp3_extITE -cp3_genAND -no-cp3_extNgtInput -no-cp3_extImplied -cp3_fm_maxConstraints=6 -cp3_fm_limit=184862012040 -cp3_fm_Slimit=11990477 ") 
      + std::string("-cp3_fm_maxA=28459264 -cp3_fm_grow=8493 -cp3_fm_growT=30000 -no-cp3_fm_twoPr -no-cp3_fm_sem -no-cp3_fm_unit -no-cp3_fm_dups -cp3_fm_vMulAMT -no-cp3_fm_cut -cp3_fm_newAmo=0 -cp3_fm_newAlk=1 -no-cp3_fm_newSub -cp3_fm_1st -card_minC=41026 -card_maxC=2456 -card_max=2107573 -card_Elimit=4288663 -cp3_hte_steps=1181318684 -cp3_par_hte -cp3_hte_inpInc=36831 -pr-probe -pr-uips=134490651 -pr-repeat -pr-csize=5 -no-pr-lhbr -pr-probeL=55135 -pr-keepI=0 -pr-viviP=64 -pr-viviL=3 -cp3_pr_inpInc=7 -cp3_viv_inpInc=38746470 -cp3_probe_vars=1965327 -cp3_probe_cls=3 -cp3_probe_lits=47565 -cp3_viv_vars=6276 -cp3_viv_cls=442 -cp3_viv_lits=53095 -cp3_res_bin -cp3_res3_steps=25701892 -cp3_res3_ncls=57 -cp3_res_percent=3.89724e-05 -no-cp3_res_add_lev -cp3_res_add_lea -cp3_res_ars -cp3_res_inpInc=114 -cp3_add_inpInc=4074 -cp3_rew_min=401776 -cp3_rew_iter=1200335 -cp3_rew_minA=57 -cp3_rew_limit=4") 
      + std::string(" -cp3_rew_Vlimit=154646162 -cp3_rew_Addlimit=24118 -no-cp3_rew_amo -cp3_rew_imp -no-cp3_rew_exo -cp3_rew_merge -cp3_rew_1st -no-cp3_rew_ratio -no-cp3_rew_once -cp3_rewI_min=385 -cp3_rew_inpInc=945 -shuffle-seed=828692996 -no-shuffle-order -sls-ksat-flips=1738354881 -sls-rnd-walk=1313 -all_strength_res=17330189 -no-cp3_inpPrefL -cp3_sub_limit=243 -cp3_str_limit=105356442 -cp3_call_inc=2 -cp3_sub_inpInc=126 -cp3_par_strength=0 -par_subs_counts=0 -susi_chunk_size=46723766 -par_str_minCls=155356310 -sym-size -sym-pol -sym-unit -sym-min=28949363 -sym-ratio=8.05885e+08 -sym-iter=17278248 -sym-prop -sym-propA -sym-clLearn -sym-cons=354553 -sym-consT=30 -no-2sat-cq -cp3_uhdIters=858 -cp3_uhdUHLE=2 -cp3_uhdNoShuffle -cp3_uhdEE -cp3_uhdTstDbl -cp3_uhdProbe=1 -cp3_uhdPrSize=2061 -cp3_uhdPrEE -no-cp3_uhdPrSiBo -xorMaxSize=57 -xorLimit=14112 -xorSelect=1 -xorFindRes -xorEncSize=5023589"), false);
    } else if (optionSet == "505-S") {
      parseOptions(std::string("-specialIncReduceDB=900 -minLBDFrozenClause=50 -minSizeMinimizingClause=15 -var-decay-e=0.99 -cla-decay=0.995 -rnd-freq=0.005 -rnd-seed=9.20756e+06 -phase-saving=1 -rtype=2 -rfirst=1000 -gc-frac=0.3 -alluiphack=2 -enabled_cp3 -cp3_randomized -up -subsimp -hte -cce -bve -bva -unhide -dense -cp3_ptechs=u3svghpwc -sls -cp3_bve_limit=2500000 -bve_unlimited -no-bve_gates -bve_cgrow=20 -bve_BCElim -bve_heap_updates=2 -bve_early -cp3_bva_push=1 -cp3_bva_limit=12000000 -cp3_bva_incInp=200 -no-cp3_bva_compl -cp3_cce_level=1 -cp3_hte_steps=214748 -sls-rnd-walk=500 -sls-adopt-cls -all_strength_res=3 -no-cp3_strength -cp3_str_limit=400000000 -cp3_call_inc=50 -cp3_uhdUHLE=0 -cp3_uhdNoShuffle"), false);
    } else if (optionSet == "505-T") {
      parseOptions(std::string("-incReduceDB=200 -minLBDFrozenClause=50 -minSizeMinimizingClause=15 -var-decay-b=0.99 -var-decay-e=0.99 -cla-decay=0.995 -rnd-freq=0.005 -rnd-seed=9.20756e+06 -rtype=2 -rfirst=1000 -gc-frac=0.3 -alluiphack=2 -laHack -hlaLevel=3 -hlaevery=8 -hlabound=1024 -enabled_cp3 -cp3_randomized -subsimp -bve -bva -unhide -3resolve -dense -cp3_ptechs=u3sghpvwc -sls -cp3_bve_limit=2500000 -no-bve_strength -no-bve_gates -cp3_bve_heap=1 -bve_cgrow=-1 -bve_cgrow_t=10000 -bve_totalG -bve_early -cp3_bva_limit=12000000 -cp3_bva_incInp=20000 -no-cp3_bva_compl -cp3_bva_subOr -cp3_res3_steps=100000 -cp3_res_inpInc=2000 -sls-ksat-flips=-1 -sls-adopt-cls -all_strength_res=4 -cp3_sub_limit=400000000 -cp3_call_inc=50 -cp3_uhdIters=1 -cp3_uhdTrans -cp3_uhdUHLE=0 -cp3_uhdNoShuffle"), false);
    } else if (optionSet == "505-U") {
      parseOptions(std::string("-R=1.2 -szLBDQueue=60 -szTrailQueue=4000 -lbdIgnL0 -quickRed -keepWorst=0.001 -var-decay-b=0.85 -var-decay-e=0.99 -var-decay-d=10000 -rnd-freq=0.005 -init-act=1 -init-pol=2 -rlevel=1 -alluiphack=2 -clsActB=2 -actIncMode=2 -laHack -dyn -laEEl -hlaLevel=1 -hlaevery=32 -hlabound=-1 -hlaTop=512 -sInterval=1 -learnDecP=80 -er-size=16 -er-lbd=12 -sUhdProbe=1 -sUHLEsize=30 -sUHLElbd=12 -bve_early -cp3_ee_bIter=400000000 -card_maxC=7 -card_max=2 -pr-uips=0 -pr-keepI=0 -no-pr-nce"), false);
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
