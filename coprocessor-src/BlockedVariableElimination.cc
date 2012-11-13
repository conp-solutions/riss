/*******************************************************************[BlockedVariableElimination.cc]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/BlockedVariableElimination.h"

using namespace Coprocessor;
using namespace std;

BlockedVariableElimination::BlockedVariableElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller )
: Technique( _ca, _controller )
{
}

bool BlockedVariableElimination::hasToEliminate()
{
  return variable_queue.size() > 0; 
}

void BlockedVariableElimination::runBVE(Coprocessor::CoprocessorData& data)
{
  
}

lbool BlockedVariableElimination::fullBVE(CoprocessorData& data)
{

  return l_Undef;
}  
void BlockedVariableElimination::bve_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics)   
{

    // usefull: Literal Occurrence Structure -> data.occs
    //
    // for all x in variable_queue(start,end) DO
    //    generate F_x * F_¬x 
    //    remove satisfied Clauses
    // OD 
}

//expects c to contian v positive and d to contian v negative
//returns true, if resolvent is satisfied
//        else, otherwise
bool BlockedVariableElimination::resolve(Clause & c, Clause & d, int v, vector<Lit> & resolvent)
{
    unsigned i = 0, j = 0;
    while (i < c.size() && j < d.size())
    {   
        if (c[i] == mkLit(v,false))
        {
            ++i;   
        }
        else if (d[j] == mkLit(v,true))
            ++j;
        else if (c[i] < d[j])
        {
            if (resolvent.back() == c[i])
                continue;
            else if (resolvent.back() == ~c[i])
                return true;
            resolvent.push_back(c[i]);
            ++i;
        }
        else 
        {
            if (resolvent.back() == d[j])
                continue;
            else if (resolvent.back() == ~d[j])
                return true;
            resolvent.push_back(d[j]);
            ++j;
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
        {
            ++i;   
        }
        else 
        {
            if (resolvent.back() == c[i])
                continue;
            else if (resolvent.back() == ~c[i])
                return true;
            resolvent.push_back(c[i]);
            ++i;
        }
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))
            ++j;
        else 
        {
            if (resolvent.back() == d[j])
                continue;
            else if (resolvent.back() == ~d[j])
                return true;
            resolvent.push_back(d[j]);
            ++j;
        }

    }
    return false;
}

void parallelBVE(CoprocessorData& data)
{

}

void* BlockedVariableElimination::runParallelBVE(void* arg)
{
  BVEWorkData* workData = (BVEWorkData*) arg;
  workData->bve->bve_worker(*(workData->data), workData->start,workData->end, false);
  return 0;
}
 
