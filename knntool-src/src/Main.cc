#include <boost/program_options.hpp>

#include <iostream> 
#include <string> 
#include <fstream>
#include "Trainer.h"
#include "Parser.h"

namespace 
{ 
  const size_t ERROR_IN_COMMAND_LINE = 1; 
  const size_t SUCCESS = 0; 
  const size_t ERROR_UNHANDLED_EXCEPTION = 2; 
 
} // namespace 

using namespace std;

int main(int argc, char** argv) 
{ 
  try 
  {
    /** Define and parse the program options 
     */ 
    namespace po = boost::program_options; 
    po::options_description desc("Options"); 
    desc.add_options()  
  /////////////////////////////////////////////////////////////////////
      ("help,h", 							"Print help messages")
      ("features,f", 		po::value<std::string>()->required(), 	"feature.csv")
      ("times,t", 		po::value<std::string>()->required(), 	"times.dat")
      ("out,o", 		po::value<std::string>(),	 	"database")
      ("featurecalc,c", 	po::value<std::string>(),		"Select the classes with respect to the time which is needed for the feature-calculation")
      ("method,m",							"Specify the training methods") //TODO
      ("dim, d", 							"Specify the dimension of the database")
      ("generateheader,g",						"Generates a c++ header file, which is usable in RISS")
      ("verbose,v",							"Verbose");
  /////////////////////////////////////////////////////////////////////
 
    po::variables_map vm; 
    try 
    { 
      po::store(po::parse_command_line(argc, argv, desc), vm); // can throw 
 
      /** --help option 
       */ 
      if ( vm.count("help")  ) 
      { 
        std::cout << "Basic Command Line Parameter App" << std::endl 
                  << desc << std::endl; 
        return SUCCESS; 
      } 
 
      po::notify(vm); // throws on error, so do after help in case 
                      // there are any problems 
    } 
    catch(po::error& e) 
    { 
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl; 
      std::cerr << desc << std::endl; 
      return ERROR_IN_COMMAND_LINE; 
    } 
 /////////////////////////////////////////////////////////////////////
  
 // file-handling
  ifstream featuresFile (vm["features"].as<std::string>().c_str(), ios_base::in);
  ifstream timesFile (vm["times"].as<std::string>().c_str(), ios_base::in);
  
  if(!featuresFile) {
    cerr << "ERROR cannot open file " << vm["features"].as<std::string>() << " for reading" << endl;
    exit(1);
  }  
  if(!timesFile) {
    cerr << "ERROR cannot open file " << vm["times"].as<std::string>() << " for reading" << endl;
    exit(1);
  }
  
  if ( vm.count("featurecalc") ) {
    ifstream featureCalculation ( vm["featurecalc"].as<std::string>().c_str(), ios_base::in);
   
    if(!timesFile) {
      cerr << "c ERROR cannot open file " << vm["featurecalc"].as<std::string>() << " for reading" << endl;
      exit(1);
    }
  }
  
  int timeout = 7200; //TODO
  
  Trainer T;
  
  cout << endl << "Begin Parsing..." << endl << endl;
  
  parseFiles(T, featuresFile, timesFile, timeout);
  
  timesFile.close();
  featuresFile.close();
  
 
  
  //ofstream ofs (vm["out"].as<std::string>().c_str(), ofstream::out);
 
 
 ////////////////////////////////////////////////////////////////////
  } 
  catch(std::exception& e) 
  { 
    std::cerr << "Unhandled Exception reached the top of main: " 
              << e.what() << ", application will now exit" << std::endl; 
    return ERROR_UNHANDLED_EXCEPTION; 
 
  } 
 
  return SUCCESS; 
 
} // main 
