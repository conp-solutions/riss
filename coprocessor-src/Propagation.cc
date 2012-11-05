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
  
   for (int i = 0; i < clause_list.size(); ++i)
   {
        Clause & c = ca[clause_list[i]];
        int k = 0;
        for (int l = 0; l < c.size(); ++l)
        {
            if (value(c[l]) != l_False)
                c[k++] = c[l];
        }
        c.shrink(c.size() - k);
            if (c.has_extra())
            c.calcAbstraction();
   }
  
  return l_Undef;
}