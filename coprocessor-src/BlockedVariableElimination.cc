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

//expects filled variable processing queue
void BlockedVariableElimination::bve_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics)   
{
    for (unsigned i = start; i < end; i++)
    {
       vector < vec < Lit > > resolvents;
       int v = variable_queue[i];

       //will this call provide only learnts?
       vector<CRef> & pos = data.list(mkLit(v,false));
       vector<CRef> & neg = data.list(mkLit(v,true));
       
       // resolvents <- F_x * F_¬x 
       resolveSet(pos, neg,  v, resolvents);
       
       // if resolving reduces number of clauses: 
       //    delete old clauses
       //    add resolvents
       if (resolvents.size() <= pos.size() + neg.size())
       {
            removeClauses(data, pos);
            removeClauses(data, neg);

            //TODO :: add resolvents!!!
       }
    }
}

inline void BlockedVariableElimination::removeClauses(CoprocessorData & data, vector<CRef> & list)
{
    for (int cr_i = 0; cr_i < list.length(); ++cr_i)
    {
        data.removedClause(list[cr_i]);
        //TODO : remove Clauses properly
        solver->remove(list[cr_i]);
    }

}

inline void BlockedVariableElimination::resolveSet(vector<CRef> & positive, vector<CRef> & negative, int v, vector < vec < Lit > > resolvents)
{
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[cr_p];
        for (int cr_n = 0; cr_n < positive.size(); ++cr_n)
        {
            Clause & n = ca[cr_n];
            vec<Lit> ps;
            if (!resolve(p, n, v, ps))
                resolvents.push_back(ps);

        }

    }
}

//expects c to contain v positive and d to contain v negative
//returns true, if resolvent is satisfied
//        else, otherwise
inline bool BlockedVariableElimination::resolve(Clause & c, Clause & d, int v, vec<Lit> & resolvent)
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
           if (checkPush(resolvent, c[i])
                return true;
           else 
                ++i;
        }
        else 
        {
           if (checkPush(resolvent, d[j])
              return true;
           else
                ++j; 
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
            ++i;   
        else if checkPush(resolvent, c[i])
            return true;
        else 
            ++i;
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))
            ++j;
        else if checkPush(resolvent, d[j])
            return true;
        else 
            ++j;

    }
    return false;
}

inline bool BlockedVariableElimination::checkPush(vec<Lit> & ps, Lit l)
{
    if (ps.size() > 0)
    {
        if (ps.last() == l)
         return false;
        if (ps.last() == ~l)
         return true;
    }
    ps.push(l);
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
 
