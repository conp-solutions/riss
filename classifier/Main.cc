/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2012-2014, Norbert Manthey, All rights reserved.
**************************************************************************************************/


#include <limits>
#define INT32_MAX std::numeric_limits<int>::max()
#define DOUBLE_MAX std::numeric_limits<double>::max()

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>
#include <fstream>
#include <algorithm>

#include "classifier/CNFClassifier.h"
#include "classifier/WekaDataset.h"
#include "classifier/Configurations.h"
#include "classifier/Classifier.h"

#include "riss/utils/System.h"
#include "riss/utils/ParseUtils.h"
#include "riss/utils/Options.h"
#include "riss/core/Dimacs.h"
#include "riss/core/Solver.h"
#include "riss/utils/version.h" // include the file that defines the solver version

#include "coprocessor/Coprocessor.h"

#include "classifier/CompareSolver.h"

using namespace Riss;
using namespace Coprocessor;
using namespace std;

//=================================================================================================
static bool receivedInterupt = false;


IntOption    verb("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
// Extra options:
//
StringOption dimacs("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
IntOption    cpu_lim("MAIN", "cpu-lim", "Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
IntOption    mem_lim("MAIN", "mem-lim", "Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

BoolOption clausesf("FEATURES", "clausesf", "turn on/off computation of clauses graph features.", true);
BoolOption resolutionf("FEATURES", "resf", "turn on/off computation of resolution graph features.", true);
BoolOption varf("FEATURES", "varf", "turn on/off computation of variables graph features.", true);
BoolOption derivative("FEATURES", "derivativef", "turn on/off computation of derivative of the sequences features.", true);

// norbert: new options
BoolOption opt_big("FEATURES", "big", "turn on/off computation of binary implication graph features.", true);
BoolOption opt_constraints("FEATURES", "const", "turn on/off computation of constraints and related graph features.", true);
BoolOption rwh("FEATURES", "rwhf", "turn on/off computation of Recursive Weight Heuristic features.", true);
BoolOption opt_xor("FEATURES", "xor", "turn on/off computation of XORs and related features.", true);

// TODO: should have an option for scaling down the feature values!
IntOption    qcount("FEATURES", "qcount", "Quantiles count for distributions statistics.\n", 4, IntRange(0, INT32_MAX));
DoubleOption    gainth("CLASSIFY", "gainth", "The Information Gain TH of the attributes to filter when training.\n", -1, DoubleRange(0.0, true, DOUBLE_MAX, false));
DoubleOption    timeout("CLASSIFY", "timeout", "The the timeout for SAT solving.\n", 900, DoubleRange(0.0, false, DOUBLE_MAX, false));
DoubleOption    classifytime("CLASSIFY", "classifytime", "The estimated time to classify an instance by the complete model.\n", 5, DoubleRange(0.0, true, DOUBLE_MAX, false));

StringOption plotFile("CLASSIFY", "plotfile", "If given, the values used to compute distributions are written to this file.");
StringOption attrFile("CLASSIFY", "dataset", "the (input/output) file with computed values in weka format", "weka.arff");
BoolOption attr("CLASSIFY", "attr", "if off the feature is appended to dataset.", false);
StringOption configInfo("CLASSIFY", "configInfo", "information about configurations of parameters of the solver.", "configurations.txt");
StringOption attrInfo("CLASSIFY", "attrInfo", " (input/output) information about class attributes indexes and indexes to remove", "attrInfo.txt");
//      BoolOption opt_big("CLASSIFY", "big","turn on/off computation of binary implication graph features.", true);

StringOption prefixClassifier("CLASSIFY", "classifier", "the prefix of path of the classifier", "bestClassifier");
BoolOption train("CLASSIFY", "train", "turn on/off training (needs dataset, attrInfo, classifier)", false);
BoolOption test("CLASSIFY", "test", "turn on/off testing (needs dataset, attrInfo, classifier)", false);
BoolOption classify("CLASSIFY", "classify", "turn on/off classify (needs attrInfo, classifier)", false);
BoolOption preclassify("CLASSIFY", "preclassify", "computes features needed for classify (needs attrInfo, classifier)", false);
BoolOption fileoutput("CLASSIFY", "fileoutput", "turn on/off the output of features to stdout", false);
BoolOption nonZero("CLASSIFY", "nonZero", "consider a configuration as being good, only if its predicted propability is greater than 0", false);

BoolOption tmpfilecommunication("CLASSIFY", "tmpfilecommunication", "sets if the weka interface uses temp files or stdout", false);


StringOption wekaLocation("WEKA", "weka", "location to weka tool", "/usr/share/java/weka.jar");
StringOption predictorLocation("WEKA", "predictor", "location to the predictor", "./predictor.jar");


BoolOption compareCNF("COMPARATOR", "compareCNF", "take the first two parameters as file names and compare the CNF files (formulas, not p-lines)", false);

void setTimeLimit(int timeout)
{
    // Set limit on CPU-time:
    if (timeout != INT32_MAX) {
        rlimit rl;
        getrlimit(RLIMIT_CPU, &rl);
        if (rl.rlim_max == RLIM_INFINITY || (rlim_t)timeout < rl.rlim_max) {
            rl.rlim_cur = timeout;
            if (setrlimit(RLIMIT_CPU, &rl) == -1) {
                printf("c WARNING! Could not set resource limit: CPU-time.\n");
            }
        }
    }
}

void setMEMLimit(int memout)
{
    // Set limit on virtual memory:
    if (memout != INT32_MAX) {
        rlim_t new_mem_lim = (rlim_t)memout * 1024 * 1024;
        rlimit rl;
        getrlimit(RLIMIT_AS, &rl);
        if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max) {
            rl.rlim_cur = new_mem_lim;
            if (setrlimit(RLIMIT_AS, &rl) == -1) {
                printf("c WARNING! Could not set resource limit: Virtual memory.\n");
            }
        }
    }
}

vector<double> features;
Configurations* configuration;
istringstream* split;
bool runtimesInfo;
CNFClassifier* cnfclassifier;
ostream* out = nullptr;
ofstream* fileout = nullptr;
const char* cnffile;
double time1 = 0;


string splitString(const std::string& s, unsigned int n, char delim = ' ')
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim) || n-- > 0) {

    }
    return item;
}

static bool isInfinite(const double pV)
{
    return std::isinf(fabs(pV));
}

void dumpData()
{
    ostream* output = &cout;
    if (fileoutput) {
        output = fileout;
    }
    ostream& fout = *output;

    if (fileoutput) {
        (*fileout).open(attrFile, fstream::out | fstream::app);
    }
    fout.setf(ios::fixed);
    fout.precision(10);
    if (attr && fileoutput) {
        configuration->removeIndex(1);
        int featuresSize = features.size();
        if (classify || preclassify) {
            fout << "@attribute total_computation_time real" << endl;
            fout << configuration->attrInfo(featuresSize + 2 + 1);
        }
        fout << "@data" << endl;
        configuration->removeIndexes(2, cnfclassifier->getTimeIndexes());
    }
    fout << "'" << cnffile << "'";
    for (int k = 0; k < features.size(); ++k) {
        stringstream s;
        s.setf(ios::fixed);
        s.precision(10);
        if (isInfinite(features[k])) { s << ",?"; }     // weka cannot handle "inf" -- FIXME should be replaced with a high cut-off value
        else { s << "," << features[k]; }
        if (s.str().find("inf") != string::npos) { cerr << "c there are infinite features (index " << k << " -- " << features[k] << " isInf: " << isInfinite(features[k]) <<  ")" << endl; }
        fout << s.str();
    }
    if (classify || preclassify) {
        fout << "," << time1;
        fout << configuration->nullInfo();
    }
    fout << endl;
    fout.flush();
}

void printFeatures(int argc, char** argv)
{
    string line = "";
    fileout = new ofstream();
    runtimesInfo = (argc == 1);

    cnffile = argv[1];

    CoreConfig coreConfig;
    Solver S(&coreConfig);

    double initial_time = cpuTime();
    S.verbosity = 0;

    if (verb > 0) {
        printf("c trying %s\n", cnffile);
    }

    gzFile in = gzopen(cnffile, "rb");
    if (in == nullptr) {
        printf("c ERROR! Could not open file: %s\n",
               argc == 1 ? "<stdin>" : cnffile);
    }

    parse_DIMACS(in, S);
    gzclose(in);

    if (S.verbosity > 0) {
        printf(
            "c |  Number of variables:  %12d                                                                  |\n",
            S.nVars());
        printf(
            "c |  Number of clauses:    %12d                                                                   |\n",
            S.nClauses());
    }

    double parsed_time = cpuTime();
    if (S.verbosity > 0)
        printf(
            "c |  Parse time:           %12.2f s                                                                 |\n",
            parsed_time - initial_time);

    // here convert the found unit clauses of the solver back as real clauses!
    if (S.trail.size() > 0) {
        if (verb > 0) {
            cerr << "c found " << S.trail.size()
                 << " unit clauses after propagation" << endl;
        }
        // build the reduct of the formula wrt its unit clauses
        S.buildReduct();

        vec<Lit> ps;
        for (int j = 0; j < S.trail.size(); ++j) {
            const Lit l = S.trail[j];
            ps.clear();
            ps.push(l);
            CRef cr = S.ca.alloc(ps, false);
            S.clauses.push(cr);
        }
    }

    cnfclassifier = new CNFClassifier(S.ca, S.clauses, S.nVars());
    cnfclassifier->setVerb(verb);
    cnfclassifier->setComputingClausesGraph(clausesf);
    cnfclassifier->setComputingResolutionGraph(resolutionf);
    cnfclassifier->setComputingRwh(rwh);
    cnfclassifier->setComputeBinaryImplicationGraph(opt_big);
    cnfclassifier->setComputeConstraints(opt_constraints);
    cnfclassifier->setComputeXor(opt_xor);
    cnfclassifier->setQuantilesCount(qcount);
    cnfclassifier->setPlotsFileName(plotFile);
    cnfclassifier->setComputingVarGraph(varf);
    if (attr) {
        cnfclassifier->setAttrFileName(attrFile);
    } else {
        cnfclassifier->setAttrFileName(nullptr);
    }
    cnfclassifier->setComputingDerivative(derivative);

    // in output features the actual calculation is done
    cnfclassifier->extractFeatures(features); // also print the formula name!!

    if (verb > 1) {
        cout.setf(ios::fixed);
        cout.precision(10);
        vector<string> names = cnfclassifier->getFeaturesNames();
	cout << "Instance";
        for (int k = 0; k < features.size(); ++k) {
	    std::replace( names[k].begin(), names[k].end(), ' ', '_');
	    cout << "," << names[k];
        }
        cout << endl;
    }
    time1 = cpuTime() - time1;
    dumpData();
//  }
    if (fileoutput) {
        (*fileout).close();
    }
}


// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
void static SIGINT_exit(int signum)
{
    printf("\n"); printf("c *** INTERRUPTED ***\n");
    _exit(1); // immediately exit!
    if (!receivedInterupt) { _exit(1); }
    else { receivedInterupt = true; }
}



void compareFormulas(int argc, char** argv) {
  if( argc < 3 ) { printf("c ERROR! two files have to be specified to be compared"); exit(1); }
  
  gzFile in1 = gzopen(argv[1], "rb");
  if (in1 == nullptr) {
      printf("c ERROR! Could not open file: %s\n", argv[1]), exit(1);
  } 
  gzFile in2 = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[2], "rb");
  if (in2 == nullptr) {
      printf("c ERROR! Could not open file: %s\n", argv[2]), exit(1);
  }
  
  CompareSolver c1;
  parse_DIMACS(in1, c1);
  CompareSolver c2;
  parse_DIMACS(in2, c2);
  
  if( c1.equals( c2 ) ) {
    cout << "c formulas are equal" << endl;
  } else {
    cout << "c formulas are not equal" << endl;
  }
}

//=================================================================================================
// Main:

int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");

        time1 = cpuTime();

//      StringOption outFile ("CLASSIFY", "outfile", "If given, the values computed are written to this file, otherwise to stdout.");



        // parse the options
        bool foundHelp = parseOptions(argc, argv, true);
	if (foundHelp) { exit(0); }  // stop after printing the help information
	
	
	if( compareCNF ) {
	  compareFormulas( argc, argv );
	  exit(0);
	}
	
        if (classify || preclassify) {
            attr = true;
            fileoutput = true;
        }
        // TODO dump to file

        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU, SIGINT_exit);



        // Set limit on CPU-time:
        setTimeLimit(cpu_lim);
        // Set limit on virtual memory:
        setMEMLimit(mem_lim);

        if (argc == 1) {
            printf("c Reading from standard input... Use '--help' for help.\n");
        }


        if (verb > 0) {
//            printf("c =============================[   CNF Classifier 3   ]====================================================\n");
            printf("c ===========================[   CNF Classifier %s %s ]==================================================\n", solverVersion, gitSHA1);
//        printf("c ============================[     riss %5.2f     ]=======================================================\n", solverVersion);
            printf("c|  Norbert Manthey. The use of the tool is limited to research only!                                     |\n");
            printf("c | Contributors:                                                                                         |\n");
            printf("c |     Enrique Matos Alfonso: Implementation of the classification and base features                     |\n");
            printf("c ============================[ Problem Statistics ]=======================================================\n");
            printf("c |                                                                                                       |\n");
        }

        configuration = new Configurations(configInfo, attrInfo);

        if (test) {
            Classifier classifier(*configuration, prefixClassifier);
            classifier.setWekaLocation(string(wekaLocation));
            classifier.setNonZero(nonZero);
            classifier.setVerbose(verb);
            classifier.test(attrFile);
        } else if (train) {
            Classifier classifier(*configuration, prefixClassifier);
            classifier.setWekaLocation(string(wekaLocation));
            classifier.setNonZero(nonZero);
            classifier.setVerbose(verb);
            classifier.setGainThreshold(gainth);
            int correct = classifier.train(attrFile);
            cout << correct;
        } else {
            if (argc > 1) {
                printFeatures(argc, argv);
                if (attr && fileoutput) {
                    configuration->printRunningInfo();
                }
            }
            time1 = cpuTime();
            if (!(runtimesInfo) && classify) {
                Classifier classifier(*configuration, prefixClassifier);
                classifier.setWekaLocation(string(wekaLocation));
                classifier.setPredictorLocation(string(predictorLocation));
                classifier.setNonZero(nonZero);
                classifier.setVerbose(verb);
                classifier.setUseTempFiles(tmpfilecommunication);
                classifier.setGainThreshold(gainth);
                vector<int> classes = classifier.classifyJava(attrFile);
                time1 = cpuTime() - time1;
                cout << "c classify time " << time1 << endl;
                if (classes.size() == 0) { return 1; }   // there is no class, so you cannot print anything!
                if (classes.size() != 1) {
                    cout << "@class " << splitString(configuration->getNames()[classes[0]], 1, '.') << endl;
                    return 0;

                    /*
                    for (int j = 0; j < classes.size(); ++j) {
                      cout << "@class " << splitString(configuration->getNames()[classes[j]], 1, '.') << endl;
                    }
                    return 1; // indicate that we have multiple classes here
                    */
                } else { // print the only class name that is present!
                    cout << "@class " << splitString(configuration->getNames()[classes[0]], 1, '.') << endl;
                    return 0;
                }
            }

        }

        return 0;

        /**
         *  that's it for now
         */
    } catch (OutOfMemoryException&) {
//         printf("c =========================================================================================================\n");
//         printf("s UNKNOWN\n");
        exit(1); // something went wrong ...
    }
}
