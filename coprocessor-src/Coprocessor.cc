/**********************************************************************************[Coprocessor.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Coprocessor.h"

#include <iostream>

static const char* _cat = "COPROCESSOR 3";

static IntOption opt_threads  (_cat, "cp3_threads",  "Number of extra threads that should be used for preprocessing", 2, IntRange(0, INT32_MAX));

Coprocessor::Coprocessor( ClauseAllocator& _ca, Solver* _solver, int32_t _threads)
: threads( _threads < 0 ? opt_threads : _threads)
, solver( _solver )
, ca( _ca )
, data( _ca )
// attributes and all that

// classes for preprocessing methods
, subsumption( _ca )
, propagation( _ca )
{
  
}

Coprocessor::~Coprocessor()
{
  
}
  
lbool Coprocessor::preprocess()
{
  std::cerr << "c start preprocessing with coprocessor" << std::endl;
  
  // first, remove all satisfied clauses
  if( !solver->simplify() ) return l_False;
  
  lbool status = l_Undef;
  // delete clauses from solver
  
  // do preprocessing

  if( status == l_Undef ) status = propagation.propagate(data, solver);
  
  if( status == l_Undef ) subsumption.subsumeStrength();  // cannot change status, can generate new unit clauses
  
  
  // clear / update clauses and learnts vectores and statistical counters
  
  // attach all clauses to their watchers again
  
  return l_Undef;
}

lbool Coprocessor::inprocess()
{
  // TODO: do something before preprocessing? e.g. some extra things with learned / original clauses
  
  
  return preprocess();
}

lbool Coprocessor::preprocessScheduled()
{
  // TODO execute preprocessing techniques in specified order
  return l_Undef;
}
  