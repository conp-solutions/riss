#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>

using namespace std;

bool biggerRndFiles = true;


void createRndMaxSATinstance();

int main(int argc, char *argv[])
{
	if( argc > 2 ) { cerr << "msFuzz seed" << endl; exit(0); }

	int rnd	=0;
	if( argc == 2 ) {
		rnd = atoi( argv[1] );
		cerr << "seed " << rnd << endl;
		srand( rnd );
	} else {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		rnd = now.tv_sec*10000 + now.tv_nsec;
 		srand( rnd ); 
	}
	cout << "c seed " << rnd << endl;
	createRndMaxSATinstance();
	
	return 0;
}



void createRndMaxSATinstance()
{
    // ofstream cout(file_name.c_str(), std::ofstream::out | std::ofstream::trunc);

    const int variable_cnt = biggerRndFiles ?  400: 40;
    const int64_t WCNF_MAX_WEIGHT = 1000;
    const int max_soft_clauses = biggerRndFiles ? 6000 :50;
    const int min_soft_clauses = biggerRndFiles ? 1200:40;

    const int max_hard_clauses = biggerRndFiles ?  16:8;
    const int min_hard_clauses = biggerRndFiles ?  6:3;

    const int soft_clause_cnt = rand() % (max_soft_clauses - min_soft_clauses + 1) + min_soft_clauses;
    const int hard_clause_cnt = rand() % (max_hard_clauses - min_hard_clauses + 1) + min_hard_clauses;

    // weighted or non weighted instance
    int64_t max_weight = rand() % 10 > 0 ? 1000 : 1;
    const int64_t top_weight = soft_clause_cnt * max_weight + 1;

    // header
    cout << "p cnf " << variable_cnt << " " << soft_clause_cnt + hard_clause_cnt << " " << endl; // << top_weight << endl;

    // hard clauses
    for (int i = 0; i < hard_clause_cnt; ++i)
    {
//        cout << top_weight;
        int lit_count = rand() % 3 + 2;
        for (int j = 0; j < lit_count; ++j)
        {
            if (rand() % 2 == 1)
                cout << " " << -(rand() % variable_cnt + 1);
            else
                cout << " " << (rand() % variable_cnt + 1);
        }

        cout << " 0" << endl;
    }

    // soft clauses
    for (int i = 0; i < soft_clause_cnt; ++i)
    {
        //cout << rand() % max_weight + 1;
        int lit_count = rand() % 3 + 2;
        for (int j = 0; j < lit_count; ++j)
        {
            if (rand() % 2 == 1)
                cout << " " << -(rand() % variable_cnt + 1);
            else
                cout << " " << (rand() % variable_cnt + 1);
        }

        cout << " 0" << endl;
    }
    cout.flush();
}
