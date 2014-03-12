/****************************************************************************************[Config.h]

Copyright (c) 2014, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Config_h
#define Config_h

#include "utils/Options.h"

#include <string>
#include <vector>

#include <iostream>

namespace Minisat {

/** class that implements most of the configuration features */
class Config {
protected:
    vec<Option*>* optionListPtr;	// list of options used in the instance of the configuration
    bool parsePreset; 			// set to true if the method addPreset is calling the parse-Option method
    std::string defaultPreset;		// default preset configuration
    
public:
  
    Config (vec<Option*>* ptr, const std::string & presetOptions = "");
    
    /** parse all options from the command line 
      * @return true, if "help" has been found in the parameters
      */
    bool parseOptions (int& argc, char** argv, bool strict = false);
    
    /** parse options that are present in one string
     * @return true, if "help" has been found in the parameters
     */
    bool parseOptions (const std::string& options, bool strict = false);
    
    /** set all the options of the specified preset option sets (multiple separated with : possible) */
    void setPreset( const std::string& optionSet );
    
    /** set options of the specified preset option set (only one possible!)
     *  @return true, if the option set is known and has been set!
     */
    bool addPreset( const std::string& optionSet );
    
    /** show print for the options of this object */
    void printUsageAndExit(int  argc, char** argv, bool verbose = false);
    
    /** checks all specified constraints */
    bool checkConfiguration();
};

inline 
Config::Config(vec<Option*>* ptr, const std::string & presetOptions)
: optionListPtr(ptr)
, parsePreset(false)
, defaultPreset( presetOptions )
{

}

inline 
void Config::setPreset(const std::string& optionSet)
{
  // split string into sub strings, separated by ':'
  std::vector<std::string> optionList;
  int lastStart = 0;
  int findP = 0;
  while ( findP < optionSet.size() ) { // tokenize string
      findP = optionSet.find(":", lastStart);
      if( findP == std::string::npos ) { findP = optionSet.size(); }
      
      if( findP - lastStart - 1 > 0 ) {
	addPreset( optionSet.substr(lastStart ,findP - lastStart)  );
      }
      lastStart = findP + 1;
  }
}

inline
bool Config::addPreset(const std::string& optionSet)
{
  parsePreset = true;
  bool ret = true;
  
  if( optionSet == "PLAIN" ) {
    parseOptions(" ",false);
  }
  
  /* // copy this block to add another preset option set!
  else if( optionSet == "" ) {
    parseOptions(" ",false);
  }
  */
  
else if( optionSet == "SUSI" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -subsimp",false);
}
else if( optionSet == "ASTR" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -all_strength_res=3 -subsimp",false);
}
else if( optionSet == "BVE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bve -bve_red_lits=1",false);
}
else if( optionSet == "ABVA" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bva",false);
}
else if( optionSet == "XBVA" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Xbva=2 -no-cp3_Abva",false);
}
else if( optionSet == "IBVA" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Ibva=2 -no-cp3_Abva",false);
}
else if( optionSet == "BVA" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bva -cp3_Xbva=2 -cp3_Ibva=2",false);
}
else if( optionSet == "BCE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bce ",false);
}
else if( optionSet == "CLE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -bce -bce-cle -no-bce-bce",false);
}
else if( optionSet == "HTE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -hte",false);
}
else if( optionSet == "CCE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -cce",false);
}
else if( optionSet == "RATE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -rate -rate-limit=50000000000",false);
}
else if( optionSet == "PRB" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -probe -no-pr-vivi -pr-bins -pr-lhbr ",false);
}
else if( optionSet == "VIV" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -probe -no-pr-probe -pr-bins",false);
}
else if( optionSet == "3RES" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -3resolve",false);
}
else if( optionSet == "UNHIDE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans ",false);
}
else if( optionSet == "UHD-PR" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3",false);
}
else if( optionSet == "ELS" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -ee -cp3_ee_it -cp3_ee_level=2 ",false);
}
else if( optionSet == "FM" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -fm -no-cp3_fm_vMulAMO",false);
}
else if( optionSet == "XOR" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -xor",false);
}
else if( optionSet == "2SAT" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -2sat",false);
}
else if( optionSet == "SLS" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -sls -sls-flips=16000000",false);
}
else if( optionSet == "SYMM" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -symm -sym-clLearn -sym-prop -sym-propF -sym-cons=128 -sym-consT=128000",false);
}
else if( optionSet == "DENSE" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -dense",false);
}
else if( optionSet == "DENSEFORW" ) {
    parseOptions(" -enabled_cp3 -cp3_stats -cp3_dense_forw -dense",false);
}
else if( optionSet == "LHBR" ) {
    parseOptions(" -lhbr=3 -lhbr-sub",false);
}
else if( optionSet == "OTFSS" ) {
    parseOptions(" -otfss",false);
}
else if( optionSet == "AUIP" ) {
    parseOptions(" -alluiphack=2",false);
}
else if( optionSet == "LLA" ) {
    parseOptions(" -laHack -tabu -hlabound=-1",false);
}
else if( optionSet == "SUHD" ) {
    parseOptions(" -sUhdProbe=3",false);
}
else if( optionSet == "SUHLE" ) {
    parseOptions(" -sUHLEsize=30",false);
}
else if( optionSet == "HACKTWO" ) {
    parseOptions(" -hack=2",false);
}
else if( optionSet == "NOTRUST" ) {
    parseOptions(" -dontTrust",false);
}
else if( optionSet == "DECLEARN" ) {
    parseOptions(" -learnDecP=100 -learnDecMS=6",false);
}
else if( optionSet == "BIASSERTING" ) {
    parseOptions(" -biAsserting -biAsFreq=4",false);
}
else if( optionSet == "LBD" ) {
    parseOptions(" -lbdIgnL0 -lbdupd=0",false);
}
else if( optionSet == "RER" ) {
    parseOptions(" -rer",false);
}
else if( optionSet == "RERRW" ) {
    parseOptions(" -rer -rer-rn -no-rer-l ",false);
}
else if( optionSet == "ECL" ) {
    parseOptions(" -ecl -ecl-min-size=12 -ecl-maxLBD=6",false);
}
else if( optionSet == "FASTRESTART" ) {
    parseOptions(" -rlevel=1 ",false);
}
else if( optionSet == "AGILREJECT" ) {
    parseOptions(" -agil-r -agil-limit=0.35",false);
}
else if( optionSet == "LIGHT" ) {
    parseOptions("  -rlevel=1 -ccmin-mode=1 -lbdupd=0 -minSizeMinimizingClause=3 -minLBDMinimizingClause=3 -no-updLearnAct",false);
}
  
  else {
    ret = false; // indicate that no configuration has been found here!
}
  parsePreset = false;
  return ret; // return whether a preset configuration has been found
}


inline
bool Config::parseOptions(const std::string& options, bool strict)
{
  if( options.size() == 0 ) return false;
  // split string into sub strings, separated by ':'
  std::vector<std::string> optionList;
  int lastStart = 0;
  int findP = 0;
  while ( findP < options.size() ) { // tokenize string
      findP = options.find(" ", lastStart);
      if( findP == std::string::npos ) { findP = options.size(); }
      
      if( findP - lastStart - 1 > 0 ) {
	optionList.push_back( options.substr(lastStart ,findP - lastStart) );
      }
      lastStart = findP + 1;
  }
//  std::cerr << "c work on option string " << options << std::endl;
  // create argc - argv combination
  char* argv[ optionList.size() + 1]; // one dummy in front!
  for( int i = 0; i < optionList.size(); ++ i ) {
//    std::cerr << "add option " << optionList[i] << std::endl;
    argv[i+1] = (char*)optionList[i].c_str();
  }
  int argc = optionList.size() + 1;
  
  // call conventional method
  bool ret = parseOptions(argc, argv, strict);
  return ret;
}


inline
bool Config::parseOptions(int& argc, char** argv, bool strict)
{
    if( optionListPtr == 0 ) return false; // the options will not be parsed
  
    if( !parsePreset ) {
      if( defaultPreset.size() != 0 ) { // parse default preset instead of actual options!
	setPreset( defaultPreset );	// setup the preset configuration
	defaultPreset = "" ;		// now, nothing is preset any longer
	return false;
      }
    }

  // usual way to parse options
    int i, j;
    bool ret = false; // printed help?
    for (i = j = 1; i < argc; i++){
        const char* str = argv[i];
        if (match(str, "--") && match(str, Option::getHelpPrefixString()) && match(str, "help")){
            if (*str == '\0') {
                this->printUsageAndExit(argc, argv);
		ret = true;
	    } else if (match(str, "-verb")) {
                this->printUsageAndExit(argc, argv, true);
		ret = true;
	    }
	    argv[j++] = argv[i]; // keep -help in parameters!
        } else {
            bool parsed_ok = false;
        
            for (int k = 0; !parsed_ok && k < optionListPtr->size(); k++){
                parsed_ok = (*optionListPtr)[k]->parse(argv[i]);

                // fprintf(stderr, "checking %d: %s against flag <%s> (%s)\n", i, argv[i], Option::getOptionList()[k]->name, parsed_ok ? "ok" : "skip");
            }

            if (!parsed_ok)
                if (strict && match(argv[i], "-"))
                    fprintf(stderr, "ERROR! Unknown flag \"%s\". Use '--%shelp' for help.\n", argv[i], Option::getHelpPrefixString()), exit(1);
                else
                    argv[j++] = argv[i];
        }
    }

    argc -= (i - j);
    return ret; // return indicates whether a parameter "help" has been found
}

inline
void Config::printUsageAndExit(int  argc, char** argv, bool verbose)
{
    const char* usage = Option::getUsageString();
    if (usage != NULL) {
      fprintf(stderr, "\n");
      fprintf(stderr, usage, argv[0]);
    }

    sort((*optionListPtr), Option::OptionLt());

    const char* prev_cat  = NULL;
    const char* prev_type = NULL;

    for (int i = 0; i < (*optionListPtr).size(); i++){
        const char* cat  = (*optionListPtr)[i]->category;
        const char* type = (*optionListPtr)[i]->type_name;

        if (cat != prev_cat)
            fprintf(stderr, "\n%s OPTIONS:\n\n", cat);
        else if (type != prev_type)
            fprintf(stderr, "\n");

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


};

#endif