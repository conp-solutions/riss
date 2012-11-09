#include "coprocessor-src/Coprocessor.h"
#include <stdio.h>
using namespace Coprocessor;

void Preprocessor::outputFormula(const char *file)
{
    FILE* f = fopen(file, "wr");
    if (f == NULL)
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    printFormula(f);
    fclose(f);
}

void Preprocessor::printFormula(FILE * fd) 
{
    // print assignments
    for (int i = 0; i < (solver->trail).size(); ++i)
    {
        //if ((solver->trail_lim)[i] == 0) 
        //{
            printLit(fd, toInt(Minisat::toInt((solver->trail)[i])));
            fprintf(fd,"0\n");
        //}
    }
    // print clauses
    for (int i = 0; i < (solver->clauses).size(); ++i)
    {
        printClause(fd, (solver->clauses)[i]);
    }   
}

inline void Preprocessor::printClause(FILE * fd, CRef cr) 
{
    Clause & c = ca[cr];
    for (int i = 0; i < c.size(); ++i)
    {
        //check if clause is obsolete
        if (!c.mark())
            printLit(fd, Minisat::toInt(c[i]));
    }
    fprintf(fd, "0\n");
}

inline void Preprocessor::printLit(FILE * fd, int l) 
{
    if (l % 2 == 0)
        fprintf(fd, "%i ", (l / 2) + 1);
    else
        fprintf(fd, "-%i ", (l/2) + 1);
}
