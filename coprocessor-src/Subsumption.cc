/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"

using namespace Coprocessor;

Subsumption::Subsumption( ClauseAllocator& _ca )
: Technique( _ca )
{
}

void Subsumption::subsumeStrength()
{
  while( hasToSubsume() || hasToStrengthen() )
  {
    if( hasToSubsume() )    fullSubsumption();
    if( hasToStrengthen() ) fullStrengthening();
  }
}



bool Subsumption::hasToSubsume()
{
  return false; 
}

lbool Subsumption::fullSubsumption()
{
  return l_Undef; 
}

bool Subsumption::hasToStrengthen()
{
  return false;  
}

lbool Subsumption::fullStrengthening()
{
  return l_Undef;   
}

void Subsumption::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if (c.can_subsume() && !c.can_be_deleted())
    clause_processing_queue.push_back(cr);
}