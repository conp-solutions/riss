
#include <fstream>
#include <sstream>
#include <iostream>

#include "useful.h"


int main(int argc, char* argv[])
{
    if (argc == 1) {  // no arguments, then print the headline of a data file for the extracted data
        cout << "Instance Status ExitCode RealTime CpuTime Memory SolverExit ToolExit Models" << endl;
        exit(0);
    }

    if (argc < 3) { cerr << "wrong number of parameters" << endl; exit(1); }


    const int extractNumber = 34; // number of string-lines below, plus 1 (starts counting with 0...)
    bool handled[ extractNumber ];
    for (int i = 0 ; i < extractNumber; ++ i) { handled[i] = false; }


//  Instance Status ExitCode RealTime CpuTime Memory Decisions Conflicts CP3PPtime CP3IPtime CP3remcls

    float time = 0;
    int exitCode = 0;
    string Status;      // 0
    string ExitCode;    // 1
    string RealTime;    // 2
    string CpuTime;     // 3
    string Memory;      // 4
    string SolverCode;  // 5
    string ToolCode;    // 6
    string Models;      // 7

    // parse all files that are given as parameters
    bool verbose = false;
    int fileNameIndex = 1;
    for (int i = 2; i < argc; ++ i) {

        if (i == 2 && string(argv[i]) == "--debug") {
            verbose = true;
            continue;
        }


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

            else if (!handled[5] && line.find("solver exit code: ") == 0) {
                if (verbose) { cerr << "found exit codes" << endl; }
                handled[5] = true;

                // found output without exit codes
                if (line.find("solver exit code:  parseOutput exit code:") == 0) {

                } else {
                    SolverCode = Get(line, 3);
                    ToolCode = Get(line, 7);
                }
            }

            else if (!handled[6] && (line.find("found answers: ") == 0 || line.find("solution(s) found"))) {
                handled[6] = true;
                if (line.find("found answers: ") == 0) { Models =  Get(line, 2).c_str(); }
                else { Models =  Get(line, 0).c_str(); }

                if (! is_number(Models)) { Models = "" ; handled[6] = false; }
            }

        }


    }

    // if we missed to parse something, set status to fail
    if (!handled[0] || (Status == "")) { Status = "fail"; }

    // good tool codes:   30, 10, 20
    // good solver codes: 30, 10, 20

    // output collected information
    cout << argv[ fileNameIndex ] << " ";
    if (handled[0] || Status != "") { cout << Status ; }   else { cout << "fail"; }
    cout << " ";
    if (handled[1] || ExitCode != "") { cout << ExitCode ; }   else { cout << "-"; }
    cout << " ";
    if (handled[2] || RealTime != "") { cout << RealTime ; }   else { cout << "-"; }
    cout << " ";
    if (handled[3] || CpuTime != "") { cout << CpuTime ; }   else { cout << "-"; }
    cout << " ";
    if (handled[4] || Memory != "") { cout << Memory ; }   else { cout << "-"; }
    cout << " ";
    if (handled[5]) {
        if (SolverCode != "") { cout << SolverCode; }   else { cout << "-"; }
        cout << " ";
        if (ToolCode != "") { cout << ToolCode; }   else { cout << "-"; }
    } else { cout << "- -"; }
    cout << " ";
    if (handled[6] || Models != "") { cout << Models ; }   else { cout << "-"; }
    cout << " ";


    cout << endl;

    return 0;

}