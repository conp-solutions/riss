#ifndef SATSOLVERCLAUSEDATABASE_H
#define SATSOLVERCLAUSEDATABASE_H

#include <vector>
#include "lib/clausedatabase.h"

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

namespace PBLib
{

class SATSolverClauseDatabase : public ClauseDatabase
{
protected:
  NSPACE::vec<NSPACE::Lit> tmpLits;
  NSPACE::Solver * satsolver;
  virtual void addClauseIntern(std::vector<int32_t> const & clause)
  {
    NSPACE::Var maxVar = 0;
    tmpLits.clear();
    
    for( uint j = 0 ; j < clause.size(); ++ j ) 
    {
      const int l = clause[j];
      tmpLits.push( l > 0 ? NSPACE::mkLit(l - 1, false) : NSPACE::mkLit( -l -1, true) ); 
      maxVar = maxVar >= NSPACE::var(tmpLits.last()) ? maxVar : NSPACE::var(tmpLits.last());
    }

    // take care that solver knows about all variables!
    while( maxVar >= satsolver->nVars() ) satsolver->newVar();
    
    satsolver->addClause(tmpLits);
  }

public:
    void setSolver(NSPACE::Solver * new_satsolver)
    {
      satsolver = new_satsolver;
    };
    
    SATSolverClauseDatabase(PBConfig config, NSPACE::Solver* satsolver): ClauseDatabase(config), satsolver(satsolver)
    {    };
    
};

}
#endif // SATSOLVERCLAUSEDATABASE_H
