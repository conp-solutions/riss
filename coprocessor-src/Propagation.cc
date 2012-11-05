/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Propagation.h"


Propagation::Propagation( ClauseAllocator& _ca )
: Technique( _ca )
, lastPropagatedLiteral( 0 )
{
}


lbool Propagation::propagate(CoprocessorData& data, Solver* solver)
{
  return l_Undef;
}