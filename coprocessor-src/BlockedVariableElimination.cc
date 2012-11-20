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

lbool BlockedVariableElimination::fullBVE(Coprocessor::CoprocessorData& data)
{
  return l_Undef;
}

void BlockedVariableElimination::runBVE(CoprocessorData& data)
{
  assert(variable_queue.size() == 0);
  
  //  put all variables in queue 
  for (int v = 0; v < data.nVars(); ++v)
      variable_queue.push_back(v);

  bve_worker(data, 0, variable_queue.size(), false);
}  

//expects filled variable processing queue
//
// force -> forces resolution
void BlockedVariableElimination::bve_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool force, bool doStatistics)   
{
    for (unsigned i = start; i < end; i++)
    {
       int v = variable_queue[i];

       vector<CRef> & pos = data.list(mkLit(v,false)); 
       vector<CRef> & neg = data.list(mkLit(v,true));
       
       int pos_count = 0; 
       int neg_count = 0;
       int lit_clauses_old = 0;
       int lit_learnts_old = 0;
       int clauseCount = 0;  // |F_x| + |F_¬x| 
       for (int i = 0; i < pos.size(); ++i)
       {
            Clause & c = ca[pos[i]];
            if (c.can_be_deleted())
                continue;
            if (c.learnt())
                lit_learnts_old += c.size();
            else 
                lit_clauses_old += c.size();
            pos_count+=1;
       }      
       for (int i = 0; i < neg.size(); ++i)
       {
            Clause & c = ca[neg[i]];
            if (c.can_be_deleted())
                continue;
            if (c.learnt())
                lit_learnts_old += c.size();
            else 
                lit_clauses_old += c.size();
            neg_count+=1;
       }
       
       // handle pure Literal;
       if (pos_count == 0 || neg_count == 0)
       {    lbool state;
            if      ((pos_count == 0) && (neg_count >  0))
                state = data.enqueue(mkLit(v, false));             
            else if ((pos_count >  0) && (neg_count == 0))
                state = data.enqueue(mkLit(v, true));
            else 
                continue;  // no positive and no negative occurrences of v 
                           // -> nothing to assign
            if      (state == l_False) 
                return;                 // level 0 conflict TODO ABORT ???
            else if (state == l_Undef)
                ;                       // variable already assigned
            else if (state == l_True) 
                ;                       // new assignment -> TODO propagation 
            else 
                assert(0);              // something went wrong
            
            continue;
       }

       // Declare stats variables;        
       char pos_stats[pos_count];
       char neg_stats[neg_count];
       int lit_clauses;
       int lit_learnts;
       
       if (!force) 
           if (anticipateElimination(data ,pos, neg,  v, pos_stats, neg_stats, lit_clauses, lit_learnts) == l_False) 
               return;  // level 0 conflict found while anticipation TODO ABORT
       
       //mark Clauses without resolvents for deletion -> makes stats-array indexing inconsistent
       removeBlockedClauses(pos, pos_stats);
       removeBlockedClauses(neg, neg_stats);
        
       // if resolving reduces number of literals in clauses: 
       //    add resolvents
       //    mark old clauses for deletion
       if (force || lit_clauses <= lit_clauses_old)
       {
            resolveSet(data, pos, neg, v);
            removeClauses(data, pos);
            removeClauses(data, neg);
       }
    }
}
/*
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 */
inline void BlockedVariableElimination::removeClauses(CoprocessorData & data, vector<CRef> & list)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        if (!ca[list[cr_i]].can_be_deleted())
        {
            data.removedClause(list[cr_i]);
            ca[list[cr_i]].set_delete(true);
        }
    }

}


/*
 *  anticipates following numbers:
 *  -> number of resolvents derived from specific clause:       pos_stats / neg_stats
 *  -> total number of literals in clauses after resolution:    lit_clauses
 *  -> total number of literals in learnts after resolution:    lit_learnts
 *
 *  be aware, that stats are just generated for clauses that are not marked for deletion
 *            and that indices of deleted clauses are NOT skipped in stats-array  
 *       i.e. |pos_stats| <= |positive| and |neg_stats| <= |negative|            !!!!!!
 */
inline lbool BlockedVariableElimination::anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, char* pos_stats , char* neg_stats, int & lit_clauses, int & lit_learnts)
{
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
    for (int i = 0; i < (sizeof pos_stats)/(sizeof pos_stats[0]); ++i)
        pos_stats[i] = 0;
    for (int i = 0; i < (sizeof neg_stats)/(sizeof neg_stats[0]); ++i)
        neg_stats[i] = 0;
    
    int pc = 0;
    int nc = 0;
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[cr_p];
        if (p.can_be_deleted())
            continue;
        
        nc = 0;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & n = ca[cr_n];
            if (n.can_be_deleted())
                continue;
            
            int newLits = tryResolve(p, n, v);
            if (newLits > 1)
            {
                ++pos_stats[pc];
                ++neg_stats[nc];
                if (p.learnt() || n.learnt())
                    ++lit_learnts;
                else 
                    ++lit_clauses;
            }
            
            // empty Clause
            else if (newLits == 0)
            {
                data.setFailed();
                return l_False;
            }
            
            // unit Clause
            else if (newLits == 1)
            {
                vec <Lit > resolvent;
                resolve(p,n,v,resolvent); 
                assert(resolvent.size() == 1);
                lbool status = data.enqueue(resolvent[0]); //check for level 0 conflict
                if (status == l_False)
                    return l_False; 
                else if (status == l_Undef)
                     ; // variable already assigned
                else if (status == l_True)
                     ; //TODO propagate  
                else 
                    assert (0); //something went wrong
            }

            
            ++nc;
        }
        ++pc;
    }
    return l_Undef;
}


/*
 *   performes resolution and 
 *   allocates resolvents c, with |c| > 1, in ClauseAllocator 
 *   as learnts or clauses respectively
 *   and adds them in data object
 *
 *   - resolvents that are tautologies are skipped 
 *   - unit clauses and empty clauses are not handeled here
 *          -> this is already done in anticipateElimination 
 *             TODO Force Case 
 */
inline void BlockedVariableElimination::resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, bool force)
{
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[cr_p];
        if (p.can_be_deleted())
            continue;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & n = ca[cr_n];
            if (n.can_be_deleted())
                continue;
            vec<Lit> ps;
            if (!resolve(p, n, v, ps))
            {
               if (ps.size()>1)
               {
                    CRef cr = ca.alloc(ps, p.learnt() || n.learnt());
                    data.addClause(cr);
               }
            }   

        }

    }
}

//expects c to contain v positive and d to contain v negative
//returns -1, if resolvent is satisfied
//        number of resolvents Literals, otherwise
inline int BlockedVariableElimination::tryResolve(Clause & c, Clause & d, int v)
{
    unsigned i = 0, j = 0, r = 0;
    Lit prev = lit_Undef;
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
           if (checkUpdatePrev(prev, c[i]))
                return -1;
           else 
           {     
               ++i;
               ++r;
           }
        }
        else 
        {
           if (checkUpdatePrev(prev, d[j]))
              return -1;
           else
           {     
               ++j; 
               ++r;
           }
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
            ++i;   
        else if (checkUpdatePrev(prev, c[i]))
            return -1;
        else 
        {
            ++i;
            ++r;
        }
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))
            ++j;
        else if (checkUpdatePrev(prev, d[j]))
            return -1;
        else 
        {
            ++j;
            ++r;
        }
    }
    return r;
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

inline bool BlockedVariableElimination::checkUpdatePrev(Lit & prev, Lit l)
{
    if (prev != lit_Undef)
    {
        if (prev == l)
            return false;
        if (prev == ~l)
            return true;
    }
    prev = l;
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
 


/* this function removes Clauses that have no resolvents
 * i.e. all resolvents are tautologies
 *
 */
inline void BlockedVariableElimination::removeBlockedClauses(vector< CRef> data, char stats[] )
{
   //TODO -> if learnt, check if it was reason ?
   int stats_index = 0; 
   for (unsigned ci = 0; ci < data.size(); ++ci)
   {    
        Clause & c =  ca[data[ci]];
        if (c.can_be_deleted())
            continue;
        if (stats[stats_index] == 0)
            c.set_delete(true);
        ++stats_index;
   }
}
