
#include <fstream>
#include <sstream>
#include <iostream>

#include "useful.h"


int main(int argc, char* argv[])
{
    // print header, if there is nothing else to be printed
    if ( argc == 1 ) { // no arguments, then print the headline of a data file for the extracted data
        cout << "Instance Status ExitCode RealTime CpuTime Memory Variables Clauses TreeNodes TreeHeight EvaHeight ConflictKilled SharedClauses SafeSharedClauses"
             << endl;
        exit(0);
    }

    if ( argc < 3 ) { cerr << "wrong number of parameters" << endl; exit(1); }


    const int extractNumber = 33; // number of string-lines below, plus 1 (starts counting with 0...)
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
    string variables; // 5
    string clauses; // 6

    string nodes; // number of created nodes
    string height; // height of created tree
    string evaHeight; // height for finding solution
    string conflictKilled; // number of nodes that have been terminated by conflictkilling
    string sharedClauses; // number of totally shared clauses
    string safeSharedClauses; // number of shared clauses that are entailed by the formula



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
            } else if ( !handled[5] && line.find("c |  Number of variables:") == 0 ) {
                handled[5] = true;
                variables =  Get(line, 5);
            } else if ( !handled[6] && line.find("c |  Number of clauses:") == 0 ) {
                handled[6] = true;
                clauses =  Get(line, 5);
            } else if ( !handled[7] && line.find("c [STAT] createdNodes : ") == 0 ) {
                handled[7] = true;
                nodes =  Get(line, 4);
            } else if ( !handled[8] && line.find("c [STAT] treeHeight : ") == 0 ) {
                handled[8] = true;
                height =  Get(line, 4);
            } else if ( !handled[9] && line.find("c [STAT] evaLevel : ") == 0 ) {
                handled[9] = true;
                evaHeight =  Get(line, 4);
            } else if ( !handled[10] && line.find("c [STAT] nConflictKilled : ") == 0 ) {
                handled[10] = true;
                conflictKilled =  Get(line, 4);
            } else if ( !handled[11] && line.find("c [STAT] n_tot_shared : ") == 0 ) {
                handled[11] = true;
                sharedClauses =  Get(line, 4);
            } else if ( !handled[12] && line.find("c [STAT] sum_clauses_pools_lv0 : ") == 0 ) {
                handled[12] = true;
                safeSharedClauses =  Get(line, 4);
            }
        }



    }

    // output collected information
    cout << argv[ fileNameIndex ] << " ";
    if ( handled[0] && Status != "" ) { cout << Status ; } else { cout << "fail"; }
    cout << " ";
    if ( handled[1] && ExitCode != "" ) { cout << ExitCode ; } else { cout << "-"; }
    cout << " ";
    if ( handled[2] && RealTime != "" ) { cout << RealTime ; } else { cout << "-"; }
    cout << " ";
    if ( handled[3] && CpuTime != "" ) { cout << CpuTime ; } else { cout << "-"; }
    cout << " ";
    if ( handled[4] && Memory != "" ) { cout << Memory ; } else { cout << "-"; }
    cout << " ";
    if ( handled[5] && variables != "" ) { cout << variables ; } else { cout << "-"; }
    cout << " ";
    if ( handled[6] && clauses != "" ) { cout << clauses ; } else { cout << "-"; }
    cout << " ";
    if ( handled[7] && nodes != "" ) { cout << nodes ; } else { cout << "-"; }
    cout << " ";
    if ( handled[8] && height != "" ) { cout << height ; } else { cout << "-"; }
    cout << " ";
    if ( handled[9] && evaHeight != "" ) { cout << evaHeight ; } else { cout << "-"; }
    cout << " ";
    if ( handled[10] && conflictKilled != "" ) { cout << conflictKilled ; } else { cout << "-"; }
    cout << " ";
    if ( handled[11] && sharedClauses != "" ) { cout << sharedClauses ; } else { cout << "-"; }
    cout << " ";
    if ( handled[12] && safeSharedClauses != "" ) { cout << safeSharedClauses ; } else { cout << "-"; }
    cout << " ";
    cout << endl;

    return 0;

}
