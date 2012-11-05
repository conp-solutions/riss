/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"

using namespace Coprocessor;

Subsumption::Subsumption( ClauseAllocator& _ca )
: Technique( _ca )
{
}

void Subsumption::subsumeStrength(CoprocessorData& data)
{
  while( hasToSubsume() || hasToStrengthen() )
  {
    if( hasToSubsume() )    fullSubsumption(data);
    if( hasToStrengthen() ) fullStrengthening(data);
  }
}



bool Subsumption::hasToSubsume()
{
  return clause_processing_queue.size() > 0; 
}

lbool Subsumption::fullSubsumption(CoprocessorData& data)
{
  // run subsumption for the whole queue
  subsumption_worker(data,0,clause_processing_queue.size());
  // clear queue afterwards
  clause_processing_queue.clear();
  // no result to tell to the outside
  return l_Undef; 
}

void Subsumption :: subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end)
{
    for (; end > start;)
    {
        --end;
	const CRef cr = clause_processing_queue[end];
        Clause &c = ca[cr];
        if (c.can_be_deleted())
            continue;
        //find Lit with least occurrences
        Lit min = c[0];
        for (int l = 1; l < c.size(); l++)
        {
            if (data[c[l]] < data[min])
               min = c[l]; 
        }
        vector<CRef> & list = data.list(min);
        for (unsigned i = 0; i < list.size(); ++i)
        {
	  
            if (list[i] == cr) {
                continue;
	    } else if (ca[list[i]].size() < c.size()) {
                continue;
	    } else if (ca[list[i]].can_be_deleted()) {
                continue;
	    } else if (c.ordered_subsumes(ca[list[i]]))
            {
                ca[list[i]].set_delete(true);
                if (!ca[list[i]].learnt() && c.learnt())
                {
                    c.set_learnt(false);
                }
            }
        }
        //set can_subsume to false
        c.set_subsume(false);
    }    
}

bool Subsumption::hasToStrengthen()
{
  return false;  
}

lbool Subsumption::fullStrengthening(CoprocessorData& data)
{
  return l_Undef;   
}

void Subsumption::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if (c.can_subsume() && !c.can_be_deleted())
    clause_processing_queue.push_back(cr);
}