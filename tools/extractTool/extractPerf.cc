#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "useful.h"


int main(int argc, char* argv[])
{
    if ( argc == 1 ) { // no arguments, then print the headline of a data file for the extracted data
        cout << "Instance Status ExitCode RealTime CpuTime Memory "
             << "CP3-pp-time CP3-ip-time cache-references cache-misses cycles stalled-cycles-frontend stalled-cycles-backend instructions branches branch-misses"
             << endl;
        exit(0);
    }

    if ( argc < 3 ) { cerr << "wrong number of parameters" << endl; exit(1); }


    const int extractNumber = 16; // number of string-lines below, plus 1 (starts counting with 0...)
    bool handled[ extractNumber ];
    for ( int i = 0 ; i < extractNumber; ++ i ) { handled[i] = false; }


//  Instance Status ExitCode RealTime CpuTime Memory Decisions Conflicts CP3PPtime CP3IPtime CP3remcls

    float time = 0;
    int exitCode = 0;

    string Status;  // 0
    string ExitCode;  // 1
    string RealTime;  // 2
    string CpuTime;  // 3
    string Memory;  // 4

    // CP3 Stats
    string CP3PPtime, CP3IPtime;       // 7

    // Perf Stats
    string cacheReferences;   // 8
    string cacheMisses;       // 9
    string cycles;        // 10
    string stalledCyclesFrontend; // 11
    string stalledCyclesBackend;  // 12
    string instructions;      // 13
    string branches;      // 14
    string branchMisses;      // 15


    // parse all files that are given as parameters
    bool verbose = false;
    int fileNameIndex = 1;
    for ( int i = 2; i < argc; ++ i ) {

        if ( i == 2 && string(argv[i]) == "--debug" ) {
            verbose = true;
            // fileNameIndex ++;
            continue;
        }

        // if( i == fileNameIndex ) continue; // do not parse the CNF file!


        if (verbose) { cerr << "use file [ " << i << " ] " << argv[i] << endl; }

        ifstream fileptr ( argv[i], ios_base::in);
        if (!fileptr) {
            cerr << "c ERROR cannot open file " << argv[i] << " for reading" << endl;
            continue;
        }

        string line;        // current line
        // parse every single line
        while ( getline (fileptr, line) ) {
            if (verbose) { cerr << "c current line: " << line << endl; }
            if ( !handled[0] && line.find("[runlim] status:") == 0 ) {
                if (verbose) { cerr << "hit status" << endl; }
                handled[0] = true;
                Status =  Get(line, 2).c_str();
                if ( Status == "out" ) {
                    Status = Get(line, 4).c_str();
                    if ( Status == "time" ) { Status = "timeout"; }
                    else if (Status == "memory" ) { Status = "memoryout"; }
                }
            }

            else if ( !handled[1] && line.find("[runlim] result:") == 0 ) {
                if (verbose) { cerr << "hit exit code" << endl; }
                handled[1] = true;
                ExitCode = Get(line, 2);
            }

            else if ( !handled[2] && line.find("[runlim] real:") == 0 ) {
                if (verbose) { cerr << "hit real time" << endl; }
                handled[2] = true;
                RealTime =  Get(line, 2).c_str();
            }

            else if ( !handled[3] && line.find("[runlim] time:") == 0 ) {
                if (verbose) { cerr << "hit time" << endl; }
                handled[3] = true;
                CpuTime =  Get(line, 2).c_str();
            }

            else if ( !handled[4] && line.find("[runlim] space:") == 0 ) {
                handled[4] = true;
                Memory =  Get(line, 2).c_str();
            } else if ( !handled[7] && line.find("c [STAT] CP3 ") == 0 ) {
                if (verbose) { cerr << "hit cp3 stats" << endl; }
                handled[7] = true;
                CP3PPtime = Get(line, 3);
                CP3IPtime = Get(line, 5);
            } else if ( !handled[8] && line.find("cache-references") <= 1000 ) {
                if (verbose) { cerr << "hit exit cache-references" << endl; }
                handled[8] = true;
                cacheReferences = Get(line, 0);
            } else if ( !handled[9] && line.find("cache-misses") <= 1000 ) {
                if (verbose) { cerr << "hit exit cache-misses" << endl; }
                handled[9] = true;
                cacheMisses = Get(line, 0);
            } else if ( !handled[10] && line.find("cycles") <= 1000 ) {
                if (verbose) { cerr << "hit exit cycles" << endl; }
                handled[10] = true;
                cycles = Get(line, 0);
            } else if ( !handled[11] && line.find("stalled-cycles-frontend") <= 1000 ) {
                if (verbose) { cerr << "hit exit stalled-cycles-frontend" << endl; }
                handled[11] = true;
                stalledCyclesFrontend = Get(line, 0);
            } else if ( !handled[12] && line.find("stalled-cycles-backend") <= 1000 ) {
                if (verbose) { cerr << "hit exit stalled-cycles-backend" << endl; }
                handled[12] = true;
                stalledCyclesBackend = Get(line, 0);
            } else if ( !handled[13] && line.find("instructions") <= 1000 ) {
                if (verbose) { cerr << "hit exit instructions" << endl; }
                handled[13] = true;
                instructions = Get(line, 0);
            } else if ( !handled[14] && line.find("branches") <= 1000 ) {
                if (verbose) { cerr << "hit exit branches" << endl; }
                handled[14] = true;
                branches = Get(line, 0);
            } else if ( !handled[15] && line.find("branch-misses") <= 1000 ) {
                if (verbose) { cerr << "hit exit branch-misses" << endl; }
                handled[15] = true;
                branchMisses = Get(line, 0);
            }
        }
    }

    // output collected information
    cout << argv[ fileNameIndex ] << " ";
    if ( handled[0] || Status != "" ) { cout << Status ; } else { cout << "fail"; }
    cout << " ";
    if ( handled[1] || ExitCode != "" ) { cout << ExitCode ; } else { cout << "-"; }
    cout << " ";
    if ( handled[2] || RealTime != "" ) { cout << RealTime ; } else { cout << "-"; }
    cout << " ";
    if ( handled[3] || CpuTime != "" ) { cout << CpuTime ; } else { cout << "-"; }
    cout << " ";
    if ( handled[4] || Memory != "" ) { cout << Memory ; } else { cout << "-"; }
    cout << " ";
    if ( handled[7] || CP3PPtime != "" || CP3IPtime != "" ) { cout << CP3PPtime << " " << CP3IPtime; } else { cout << "- -"; }
    cout << " ";
    if ( handled[8] || cacheReferences != "" ) { cout << cacheReferences; } else { cout << "-"; }
    cout << " ";
    if ( handled[9] || cacheMisses != "" ) { cout << cacheMisses; } else { cout << "-"; }
    cout << " ";
    if ( handled[10] || cycles != "" ) { cout << cycles; } else { cout << "-"; }
    cout << " ";
    if ( handled[11] || stalledCyclesFrontend != "" ) { cout << stalledCyclesFrontend; } else { cout << "-"; }
    cout << " ";
    if ( handled[12] || stalledCyclesBackend != "" ) { cout << stalledCyclesBackend; } else { cout << "-"; }
    cout << " ";
    if ( handled[13] || instructions != "" ) { cout << instructions; } else { cout << "-"; }
    cout << " ";
    if ( handled[14] || branches != "" ) { cout << branches; } else { cout << "-"; }
    cout << " ";
    if ( handled[15] || branchMisses != "" ) { cout << branchMisses; } else { cout << "-"; }
    cout << " ";
    cout << endl;

    return 0;

}
