/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"


Subsumption::Subsumption( ClauseAllocator& _ca )
: ca( _ca )
{
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