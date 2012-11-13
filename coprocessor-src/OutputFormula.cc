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
    vec<Lit> & trail = solver->trail;
    vec<CRef> & clauses = solver->clauses;
    
    // count level 0 assignments 
    int level0 = 0;
    for (int i = 0; i < trail.size(); ++i)
    {
        if ((solver->level)(var(trail[i])) == 0) 
        {
            ++level0;
        }
    }
    // print header
    fprintf(fd,"p cnf %u %i\n", (solver->nVars()) ,level0 + clauses.size());
    // print assignments
    for (int i = 0; i < trail.size(); ++i)
    {
        if ((solver->level)(var(trail[i])) == 0)
        {
            printLit(fd,  toInt(trail[i]));
            fprintf(fd,"0\n");
        }
    }
    // print clauses
    for (int i = 0; i < clauses.size(); ++i)
    {
        printClause(fd, clauses[i]);
    }   
}

inline void Preprocessor::printClause(FILE * fd, CRef cr) 
{
    Clause & c = ca[cr];
    for (int i = 0; i < c.size(); ++i)
    {
        //check if clause is obsolete
        if (!c.mark())
            printLit(fd, toInt(c[i]));
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
