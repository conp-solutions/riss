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
// attributes and all that

// classes for preprocessing methods
, subsumption( _ca )
{
  
}

Coprocessor::~Coprocessor()
{
  
}
  
lbool Coprocessor::preprocess()
{
  std::cerr << "c start preprocessing with coprocessor" << std::endl;
  lbool status = l_Undef;
  // delete clauses from solver
  
  // do preprocessing
  
  while( subsumption.hasToSubsume() || subsumption.hasToStrengthen() )
  {
    if( status == l_Undef ) status = subsumption.fullSubsumption();
    if( status == l_Undef ) status = subsumption.fullStrengthening();
  }
  
  
  // clear / update clauses and learnts vectores and statistical counters
  
  // attach all clauses to their watchers again
  
  return l_Undef;
}

lbool Coprocessor::inprocess()
{

  return l_Undef;
}

lbool Coprocessor::preprocessScheduled()
{

  return l_Undef;
}
  