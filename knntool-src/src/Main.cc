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
  
  int timeout;
  
  try 
  {
    /** Define and parse the program options 
     */ 
    namespace po = boost::program_options; 
 
    
    po::options_description desc("Options"); 
    desc.add_options()  
  /////////////////////////////////////////////////////////////////////
      ("help,h", 								"Print help messages")
      ("input-files", 		po::value<vector<std::string> >()->required(), 	"Specify input/output-files <times.dat> <features.csv> <output>")
      ("featurecalc,c", 	po::value<std::string>(),			"Select the classes with respect to the time which is needed for the feature-calculation")
      ("timeout,t", 		po::value<int>(&timeout)->required(),		"timeout")
      ("method,m",								"Specify the training methods") //TODO
      ("dim,d", 								"Specify the dimension of the database")
      ("generateheader,g",							"Generates a c++ header file, which is usable in RISS")
      ("verbose,v",								"Verbose");
  /////////////////////////////////////////////////////////////////////

    po::positional_options_description p;
    p.add("input-files", -1);

    po::variables_map vm;

    try 
    { 
      po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
      
      //po::store(po::parse_command_line(argc, argv, desc), vm); // can throw 
 
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

  if(vm["input-files"].as<vector<std::string> >().size() != 3){
    cerr << "Wrong Input" << endl;
    exit(1);
  }
  
  cout << "Times File:\t\t\t" << vm["input-files"].as<vector<std::string> >()[0] << endl;
  cout << "Feature File:\t\t\t" << vm["input-files"].as<vector<std::string> >()[1] << endl;
  cout << "Outputfile:\t\t\t" << vm["input-files"].as<vector<std::string> >()[2] << endl;
  cout << "Timeout:\t\t\t" << timeout << "s" << endl;
  
 // file-handling
  ifstream featuresFile (vm["input-files"].as<vector<std::string> >()[1].c_str(), ios_base::in);
  ifstream timesFile (vm["input-files"].as<vector<std::string> >()[0].c_str(), ios_base::in);
  
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
  
  //start parsing
    
  Trainer T;
  cout << endl << "Begin Parsing..." << endl << endl;
  
  parseFiles(T, featuresFile, timesFile, timeout);
  
  timesFile.close();
  featuresFile.close();
  
  cout << "Success!" << endl << endl;
  cout << "Begin the training (using PCA) ..." << endl << endl;  
  
  //start training
  
  T.solvePCA(vm["input-files"].as<vector<std::string> >()[2]);
  
  cout << endl;
  cout << "Success!" << endl;
  
  T.writeData(vm["input-files"].as<vector<std::string> >()[2]);
 
  
  
 
  
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
