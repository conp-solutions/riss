#include <iostream>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <ext/stdio_filebuf.h>
#include <cstring>
#include <signal.h>

using namespace std;

int iabs(int a) { return a < 0 ? -a : a; }

void exit_verifier(int result_code, int exit_code) {
    cout << endl << result_code << endl;
    exit(exit_code);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: ./certifiedSAT <instance> <solveroutput>" << endl;
        exit_verifier(0, 1);
    }
    ifstream instance(argv[1]);
    ifstream solver_output(argv[2]);
    string line;

    map<int, int> variables;
    bool SAT_answer = false;
    bool UNSAT_answer = false;
    while (getline(solver_output, line)) {
        istringstream lss(line);
        string prefix;
        lss >> prefix;
        if (prefix == "s") {
            string answer;
            lss >> answer;
            if (answer == "UNKNOWN") {
                cout << "Solver reported unknown." << endl;
                exit_verifier(0, 1);
            }
            else if (answer == "SATISFIABLE") {
                cout << "Solver reported satisfiable. Checking." << endl;
                SAT_answer = true;
            }
            else if (answer == "UNSATISFIABLE") {
                cout << "Solver reported unsatisfiable. Checking." << endl;
                UNSAT_answer = true;
                break; // Proof should follow.
            }
        }
        else if (prefix == "v") {
            // read solution
            int v;
            while (lss >> v && v != 0) {
                variables[iabs(v)] = v;
            }
        }
    }
    if (SAT_answer) {
        while (getline(instance, line)) {
            if (line.substr(0, 1) == "c" || line.substr(0, 1) == "p") continue;
            istringstream lss(line);
            int v;
            bool sat_clause = false;
            int num_vars = 0;
            while (lss >> v && v != 0) {
                num_vars++;
                if (variables[iabs(v)] == v) {
                    sat_clause = true;
                    break;
                }
            }
            if (!sat_clause && num_vars > 0) {
                cout << "Clause " << line << " not satisfied" << endl;
                cout << "Wrong solution." << endl;
                exit_verifier(-1, 1);
            }
        }
        cout << "Solution verified." << endl;
        exit_verifier(11, 0);
    } else if (UNSAT_answer) {
        // Verify the solution that should be given in DRUP or BDRUP or TRACECHECK format following the solution line.
        // First line after the solution "s..." line is used to detect the proof format
        string proof_format_line;
        solver_output.seekg(0);
        string format = "";
        while (getline(solver_output, proof_format_line)) {
            if (proof_format_line.find("o proof BDRUP") != string::npos) {
                format = "brup";
                break;
            } else if (proof_format_line.find("o proof DRUP") != string::npos) {
                format = "drup";
                break;
            } else if (proof_format_line.find("o proof TC") != string::npos) {
                format = "tc";
                break;
            }
        }
        
        if (format == "") {
            cout << "Could not find 'o proof <format>' line with proof format identifier." << endl;
            exit_verifier(0, 1);
        }

        // Prepare communication pipes with checker programs
        int outfd[2];
        int infd[2];
        pipe(outfd);
        pipe(infd);

        char* checker_cmd = new char[4096];
        if (format == "brup") {
            strcpy(checker_cmd, "./brup.sh");
        } else if (format == "drup") {
            strcpy(checker_cmd, "./drup-check");
        } else if (format == "tc") {
            strcpy(checker_cmd, "./stc");
        }
        
        cout << "Calling " << checker_cmd << " " << argv[1] << endl;
        char* argvc[] = {checker_cmd, argv[1], 0 };
        
        int pid = fork();
        if (!pid) {
            // This is the child, connect pipes and launch checker program
            close(STDOUT_FILENO); // Close default stdout
            close(STDIN_FILENO); // Close default stdin

            dup2(outfd[0], STDIN_FILENO); // Connect read end of parent-child pipe with stdin
            dup2(infd[1], STDOUT_FILENO); // Connect write end of parent-child pipe with stdout

            // Close now unneeded pipe FDs
            close(outfd[0]);
            close(outfd[1]);
            close(infd[0]);
            close(infd[1]);
                     
            cout << "c drup proofer starts" << endl;   
            execv(argvc[0], argvc);
        } else {
            // This is the parent (verifier), write the proof to the checker's stdin and read result from stdout.
            close(outfd[0]); // Don't need the read end of the pipe to the child
            close(infd[1]); // Don't need the write end of the pipe from the child

            int t0 = time(NULL);
            cout << "c write data to checker" << endl;
            //write(outfd[1], first_line.c_str(), first_line.length());
            int linecount = 0;
            while (getline(solver_output, line)) {
                linecount ++;
                if (line[0] == 'c' || line[0] == 'o' || line[0] == 's') continue;
//                cout << "write line " << linecount << " stream good: " << solver_output.good() << ": " << line << endl;
                line = line + "\n"; // skip timestamp, add newline
                write(outfd[1], line.c_str(), line.length());
//                cout << "done" << endl;
            }
            close(outfd[1]); // This should signal the checker the end of the proof
            cout << "finished writing" << endl;
            __gnu_cxx::stdio_filebuf<char> filebuf(infd[0], std::ios::in);
            istream checker_output(&filebuf);
            string line;
            cout << "Reading output from checker." << endl;
            
            int t2 = time(NULL);
		        cout << "Send file to verifier took " << (t2 - t0) << " seconds. " << endl;
		        
            // wait for cihld here!
            cout << "Wait for verifier" << endl;
            
            int countLine = 0;
            while ( getline(checker_output, line) ) {
		          cout << "Checker [" << countLine++ << "]: " << line << endl;
		          if (line.find("s VERIFIED") != string::npos || line.find("s TRIVIAL UNSAT") != string::npos) {
		              close(outfd[1]);
		              close(infd[0]);
		              
            int t1 = time(NULL);
		        cout << "Verification took " << (t1 - t0) << " seconds. " << endl;
		              
		              exit_verifier(10, 0);
		          }
            }
            
            int t1 = time(NULL);
		        cout << "Verification took " << (t1 - t0) << " seconds. " << endl;
        
        }
    }
    cout << "Could not find a valid solution" << endl;
    exit_verifier(0, 1);
}
