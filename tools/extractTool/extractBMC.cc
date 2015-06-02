
#include <fstream>
#include <sstream>
#include <iostream>

#include "useful.h"


int main(int argc, char* argv[])
{
    if (argc == 1) {   // no arguments, then print the headline of a data file for the extracted data
        cout << "Instance Status ExitCode RealTime CpuTime Memory "
             << "AIG-time CNF-in-time CNF-out-time "
             << "MaxBound "
             << "IMaxVar IInputs ILatches IOutputs IAnds "
             << "EMaxVar EInputs ELatches EOutputs EAnds "
             << "Clauses1 Inputs1 LatchVars1 Variables1 "
             << "Clauses2 Inputs2 LatchVars2 Variables2 "
             << "Clauses3 Inputs3 LatchVars3 Variables3 "
             << "Clauses4 Inputs4 LatchVars4 Variables4 "
             << endl;
        exit(0);
    }

    if (argc < 3) { cerr << "wrong number of parameters" << endl; exit(1); }


    const int extractNumber = 31; // number of string-lines below, plus 1 (starts counting with 0...)
    bool handled[ extractNumber ];
    for (int i = 0 ; i < extractNumber; ++ i) { handled[i] = false; }


//  Instance Status ExitCode RealTime CpuTime Memory Decisions Conflicts CP3PPtime CP3IPtime CP3remcls

    float time = 0;
    int exitCode = 0;

    string Status;  // 0
    string ExitCode;  // 1
    string RealTime;  // 2
    string CpuTime;  // 3
    string Memory;  // 4
    string IMaxVar, IInputs, ILatches, IOutputs, IAnds; // 5
    string EMaxVar, EInputs, ELatches, EOutputs, EAnds; // 6
    string Clauses1, Inputs1, Latch1, Variables1; // 7
    string Clauses2, Inputs2, Latch2, Variables2; // 8
    string Clauses3, Inputs3, Latch3, Variables3; // 9
    string Clauses4, Inputs4, Latch4, Variables4; // 10
    string aigtime, inCNFtime, outCNFtime; // 11
    string maxBound; // 12

    // parse all files that are given as parameters
    bool verbose = false;
    int fileNameIndex = 1;
    for (int i = 2; i < argc; ++ i) {

        if (i == 2 && string(argv[i]) == "--debug") {
            verbose = true;
            // fileNameIndex ++;
            continue;
        }

        // if( i == fileNameIndex ) continue; // do not parse the CNF file!


        if (verbose) { cerr << "use file [ " << i << " ] " << argv[i] << endl; }

        ifstream fileptr(argv[i], ios_base::in);
        if (!fileptr) {
            cerr << "c ERROR cannot open file " << argv[i] << " for reading" << endl;
            continue;
        }

        string line;        // current line
        // parse every single line
        while (getline(fileptr, line)) {
            if (verbose) { cerr << "c current line: " << line << endl; }
            if (!handled[0] && line.find("[runlim] status:") == 0) {
                if (verbose) { cerr << "hit status" << endl; }
                handled[0] = true;
                Status =  Get(line, 2).c_str();
                if (Status == "out") {
                    Status = Get(line, 4).c_str();
                    if (Status == "time") { Status = "timeout"; }
                    else if (Status == "memory") { Status = "memoryout"; }
                }
            }

            else if (!handled[1] && line.find("[runlim] result:") == 0) {
                if (verbose) { cerr << "hit exit code" << endl; }
                handled[1] = true;
                ExitCode = Get(line, 2);
            }

            else if (!handled[2] && line.find("[runlim] real:") == 0) {
                if (verbose) { cerr << "hit real time" << endl; }
                handled[2] = true;
                RealTime =  Get(line, 2).c_str();
            }

            else if (!handled[3] && line.find("[runlim] time:") == 0) {
                if (verbose) { cerr << "hit time" << endl; }
                handled[3] = true;
                CpuTime =  Get(line, 2).c_str();
            }

            else if (!handled[4] && line.find("[runlim] space:") == 0) {
                handled[4] = true;
                Memory =  Get(line, 2).c_str();
            }

            else if (!handled[5] && line.find("[shift-bmc] initial MILOA:") == 0) {
                handled[5] = true;
                IMaxVar = Get(line, 3).c_str();
                IInputs = Get(line, 4).c_str();
                ILatches = Get(line, 5).c_str();
                IOutputs = Get(line, 6).c_str();
                IAnds =   Get(line, 7).c_str();
            } else if (!handled[6] && line.find("[shift-bmc] encode MILOA = ") == 0) {
                handled[6] = true;
                EMaxVar = Get(line, 4).c_str();
                EInputs = Get(line, 5).c_str();
                ELatches = Get(line, 6).c_str();
                EOutputs = Get(line, 7).c_str();
                EAnds =   Get(line, 8).c_str();
            } else if (!handled[7] && line.find("[shift-bmc] [1] transition-formula stats:") == 0) {
                handled[7] = true;
                Variables1 = Get(line, 4).c_str();
                Inputs1 =   Get(line, 6).c_str();
                Latch1 =     Get(line, 8).c_str();
                Clauses1 =  Get(line, 10).c_str();
            } else if (!handled[8] && line.find("[shift-bmc] [2] transition-formula stats:") == 0) {
                handled[8] = true;
                Variables2 = Get(line, 4).c_str();
                Inputs2 =   Get(line, 6).c_str();
                Latch2 =     Get(line, 8).c_str();
                Clauses2 =  Get(line, 10).c_str();
            } else if (!handled[9] && line.find("[shift-bmc] [3] transition-formula stats:") == 0) {
                handled[9] = true;
                Variables3 = Get(line, 4).c_str();
                Inputs3 =   Get(line, 6).c_str();
                Latch3 =     Get(line, 8).c_str();
                Clauses3 =  Get(line, 10).c_str();
            } else if (!handled[10] && line.find("[shift-bmc] [4] transition-formula stats:") == 0) {
                handled[10] = true;
                Variables4 = Get(line, 4).c_str();
                Inputs4 =   Get(line, 6).c_str();
                Latch4 =     Get(line, 8).c_str();
                Clauses4 =  Get(line, 10).c_str();
            } else if (!handled[11] && line.find("[shift-bmc] pp time summary:") == 0) {
                handled[11] = true;
                aigtime   = Get(line, 5).c_str();
                inCNFtime = Get(line, 7).c_str();
                outCNFtime = Get(line, 9).c_str();
            } else if (line.find("u") == 0) {   // check whether this is a bound line
                maxBound = split(line, 1, 'u');  // use the one that occurred last
            }
        }


    }

    // output collected information
    cout << argv[ fileNameIndex ] << " ";
    if (handled[0] && Status != "") { cout << Status ; } else { cout << "fail"; }
    cout << " ";
    if (handled[1] && ExitCode != "") { cout << ExitCode ; } else { cout << "-"; }
    cout << " ";
    if (handled[2] && RealTime != "") { cout << RealTime ; } else { cout << "-"; }
    cout << " ";
    if (handled[3] && CpuTime != "") { cout << CpuTime ; } else { cout << "-"; }
    cout << " ";
    if (handled[4] && Memory != "") { cout << Memory ; } else { cout << "-"; }
    cout << " ";
    if (handled[11]) cout << (aigtime != "" ? aigtime : "-") << " "
                              << (inCNFtime  != "" ? inCNFtime  : "-") << " "
                              << (outCNFtime != "" ? outCNFtime : "-") << " ";
    else { cout << "- - - "; }
    if (maxBound != "") { cout << maxBound << " "; }
    else { cout << "-1 "; } // default is -1
    if (handled[5]) cout << (IMaxVar != "" ? IMaxVar : "-") << " "
                             << (IInputs  != "" ? IInputs  : "-") << " "
                             << (ILatches != "" ? ILatches : "-") << " "
                             << (IOutputs != "" ? IOutputs : "-") << " "
                             << (IAnds    != "" ? IAnds    : "-") << " ";
    else { cout << "- - - - - "; }
    if (handled[6]) cout << (EMaxVar != "" ? EMaxVar : "-") << " "
                             << (EInputs  != "" ? EInputs  : "-") << " "
                             << (ELatches != "" ? ELatches : "-") << " "
                             << (EOutputs != "" ? EOutputs : "-") << " "
                             << (EAnds    != "" ? EAnds    : "-") << " ";
    else { cout << "- - - - - "; }
    if (handled[7]) cout << (Clauses1 != "" ? Clauses1 : "-") << " "
                             << (Inputs1  != "" ? Inputs1  : "-") << " "
                             << (Latch1 != "" ? Latch1 : "-") << " "
                             << (Variables1 != "" ? Variables1 : "-") << " ";
    else { cout << "- - - - "; }
    if (handled[8]) cout << (Clauses2 != "" ? Clauses2 : "-") << " "
                             << (Inputs2  != "" ? Inputs2  : "-") << " "
                             << (Latch2 != "" ? Latch2 : "-") << " "
                             << (Variables2 != "" ? Variables2 : "-") << " ";
    else { cout << "- - - - "; }
    if (handled[9]) cout << (Clauses3 != "" ? Clauses3 : "-") << " "
                             << (Inputs3  != "" ? Inputs3  : "-") << " "
                             << (Latch3 != "" ? Latch3 : "-") << " "
                             << (Variables3 != "" ? Variables3 : "-") << " ";
    else { cout << "- - - - "; }
    if (handled[10])cout << (Clauses4 != "" ? Clauses4 : "-") << " "
                             << (Inputs4  != "" ? Inputs4  : "-") << " "
                             << (Latch4 != "" ? Latch4 : "-") << " "
                             << (Variables4 != "" ? Variables4 : "-") << " ";
    else { cout << "- - - - "; }
    cout << endl;

    return 0;

}
