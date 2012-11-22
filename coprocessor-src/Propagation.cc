/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Propagation.h"

using namespace Coprocessor;

static int upLevel = 1;

Propagation::Propagation( ClauseAllocator& _ca, ThreadController& _controller )
: Technique( _ca, _controller )
, lastPropagatedLiteral( 0 )
{
}


lbool Propagation::propagate(CoprocessorData& data)
{
  Solver* solver = data.getSolver();
  // propagate all literals that are on the trail but have not been propagated
  for( ; lastPropagatedLiteral < solver->trail.size(); lastPropagatedLiteral ++ )
  {
    const Lit l = solver->trail[lastPropagatedLiteral];
    data.log.log(upLevel,"propagate literal",l);
    // remove positives
    vector<CRef> positive = data.list(l);
    for( int i = 0 ; i < positive.size(); ++i )
    {
      ca[ positive[i] ].set_delete(true);
      data.removedClause( positive[i] );
    }
    positive.clear(); // clear list
    
    const Lit nl = ~l;
    int count = 0;
    vector<CRef> negative = data.list(nl);
    for( int i = 0 ; i < negative.size(); ++i )
    {
      Clause& c = ca[ negative[i] ];
      // sorted propagation, no!
      if( !c.can_be_deleted() ) {
        for( int j = 0; j < c.size(); ++ j ) 
          if( c[j] == nl ) { 
	    c.removePositionUnsorted(j);
	    break;
	  }
        count ++;
      }
      // unit propagation
      if( c.size() == 0 || (c.size() == 1 &&  solver->value( c[0] ) == l_False) ) {
        solver->ok = false; // set state to false
        break;              // abort unit propagation
      } else if( c.size() == 1 ) {
	  if( solver->value( c[0] ) == l_Undef ) solver->uncheckedEnqueue(c[0]);
      }
    }
    // update formula data!
    data.removedLiteral(nl, count);
    data.list(nl).clear();
  }
  
//    for (int i = 0; i < clause_list.size(); ++i)
//    {
//         Clause & c = ca[clause_list[i]];
//         int k = 0;
//         for (int l = 0; l < c.size(); ++l)
//         {
//             if (value(c[l]) != l_False)
//                 c[k++] = c[l];
//         }
//         c.shrink(c.size() - k);
//             if (c.has_extra())
//             c.calcAbstraction();
//    }
  
  return l_Undef;
}

void Propagation::initClause( const CRef cr ) {}