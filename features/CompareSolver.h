/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Copyright (c) 2012-2014, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CompareSolver_h
#define RISS_CompareSolver_h

#include "riss/core/Solver.h"
#include "riss/utils/version.h" // include the file that defines the solver version
#include "riss/mtl/Sort.h"

#include <vector>

namespace Riss {

class CompareSolver {
  
  std::vector< std::vector<Lit> > formula; // simple formula storage
  Var var;
  
public:
  
  CompareSolver ();
  
  int     nVars()  const; 
  
  Var     newVar();
  
  void    reserveVars(Var v);
  
  /// Add a clause to the solver, sort the clause
  bool    addClause_(vec<Lit>& ps, bool noRedundancyCheck = false);                           
  
  void    addInputClause_(vec<Lit>& ps);                      /// Add a clause to the online proof checker
  
  
  bool equals(const CompareSolver& other );
};

CompareSolver::CompareSolver()
: var(0)
{
}

void CompareSolver::addInputClause_(vec< Lit >& ps)
{
}

Var CompareSolver::newVar()
{
  return ++var;
}

void CompareSolver::reserveVars(Var v)
{
}


int CompareSolver::nVars() const
{
  return var;
}

bool CompareSolver::addClause_(vec< Lit >& ps, bool noRedundancyCheck)
{
  formula.push_back( vector<Lit>() );
  vector<Lit>& lastV = formula.back();
  
  for( int i = 0 ; i < ps.size(); ++ i ) lastV.push_back( ps[i] );
  sort( lastV );
}

bool CompareSolver::equals(const CompareSolver& other)
{
  // not the same number of clauses
  if( formula.size() != other.formula.size() ) return false;
  
  // not the same number of variables
  if( nVars() != other.nVars() ) return false;
  
  for( int i = 0 ; i < formula.size(); ++ i ) {
    if ( formula[i] != other.formula[i] ) return false;
  }
  
  return true;
}



}

#endif