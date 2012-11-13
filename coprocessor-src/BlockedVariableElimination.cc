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
       vector < vector < Lit > > resolvents;
       int v = variable_queue[i];

       //TODO will this call give me learnts? -> then skip learnts
       vector<CRef> & pos = data.list(mkLit(v,false));
       vector<CRef> & neg = data.list(mkLit(v,true));
       
       // resolvents <- F_x * F_¬x 
       // TODO : can it happen, that one of the lists is empty?
       resolveSet(pos, neg,  v, resolvents);
       
       // if resolving reduces number of clauses: 
       //    delete old clauses
       //    add resolvents
       if (resolvents.size() <= pos.size() + neg.size())
       {
            removeClauses(data, pos);
            removeClauses(data, neg);

            //TODO : expects preprocessor to attach everything later on
            //TODO : avoid the overhead by copying around (caused by problems with Minisat-vetor)
            for (int res = 0; res < resolvents.size(); ++res)
            {
                vec < Lit > clause;
                for (int l = 0; l < resolvents[res].size(); ++l)
                {
                    clause.push(resolvents[res][l]);
                }
                CRef cr = ca.alloc(clause, false);
                data.addClause(cr);

            }
       }
    }
}

inline void BlockedVariableElimination::removeClauses(CoprocessorData & data, vector<CRef> & list)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        data.removedClause(list[cr_i]);
        //TODO : remove Clauses properly 
        ca[list[cr_i]].mark(1);
    }

}

inline void BlockedVariableElimination::resolveSet(vector<CRef> & positive, vector<CRef> & negative, int v, vector < vector < Lit > > resolvents)
{
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[cr_p];
        //TODO : use of flag correct?
        if (p.mark())
            continue;
        for (int cr_n = 0; cr_n < positive.size(); ++cr_n)
        {
            Clause & n = ca[cr_n];

            //TODO : use of flag correct?
            if (n.mark())
                continue;
            vector<Lit> ps;
            if (!resolve(p, n, v, ps))
                resolvents.push_back(ps);

        }

    }
}

//expects c to contain v positive and d to contain v negative
//returns true, if resolvent is satisfied
//        else, otherwise
inline bool BlockedVariableElimination::resolve(Clause & c, Clause & d, int v, vector<Lit> & resolvent)
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
           if (checkPush(resolvent, c[i]))
                return true;
           else 
                ++i;
        }
        else 
        {
           if (checkPush(resolvent, d[j]))
              return true;
           else
                ++j; 
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
            ++i;   
        else if (checkPush(resolvent, c[i]))
            return true;
        else 
            ++i;
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))
            ++j;
        else if (checkPush(resolvent, d[j]))
            return true;
        else 
            ++j;

    }
    return false;
}

inline bool BlockedVariableElimination::checkPush(vector<Lit> & ps, Lit l)
{
    if (ps.size() > 0)
    {
        if (ps.back() == l)
         return false;
        if (ps.back() == ~l)
         return true;
    }
    ps.push_back(l);
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
 
