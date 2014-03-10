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
    vec<Option*>* optionListPtr;
    
public:
  
    Config (vec<Option*>* ptr = 0);
    Config (vec<Option*>* ptr, const std::string & presetOptions);
    
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
    
    /** set options of the specified preset option set (only one possible!)*/
    void addPreset( const std::string& optionSet );
    
    /** show print for the options of this object */
    void printUsageAndExit(int  argc, char** argv, bool verbose = false);
    
    /** checks all specified constraints */
    bool checkConfiguration();
};

inline
Config::Config(vec<Option*>* ptr)
: optionListPtr(ptr)
{

}

inline 
Config::Config(vec<Option*>* ptr, const std::string & presetOptions)
: optionListPtr(ptr)
{

}

inline 
void Config::setPreset(const std::string& optionSet)
{
  assert( false && "needs to be implemented" );
}

inline
void Config::addPreset(const std::string& optionSet)
{
  assert( false && "needs to be implemented" );
}


inline
bool Config::parseOptions(const std::string& options, bool strict)
{
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
  // create argc - argv combination
  char* argv[ options.size() + 1]; // one dummy in front!
  for( int i = 0; i < options.size(); ++ i ) {
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