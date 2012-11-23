/*******************************************************************[BlockedVariableElimination.cc]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/
#include "coprocessor-src/BlockedVariableElimination.h"
#include "coprocessor-src/Propagation.h"

using namespace Coprocessor;
using namespace std;

static const char* _cat = "COPROCESSOR 3 - BVE";

static IntOption  opt_verbose    (_cat, "cp3_bve_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3));

BlockedVariableElimination::BlockedVariableElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation )
: Technique( _ca, _controller ), 
  propagation( _propagation)
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
  for (int v = 0; v < data.nVars() /*&& v < 2*/ ; ++v)
      variable_queue.push_back(v);

  bve_worker(data, 0, variable_queue.size(), false);
  variable_queue.clear();
}  


static void printLitErr(Lit l)
{
    if (toInt(l) % 2)
           cerr << "-";
        cerr << (toInt(l)/2) + 1 << " ";
}


static void printClause(Clause & c)
{
    cerr << "c ";
    for (int i = 0; i < c.size(); ++i)
        printLitErr(c[i]);
    cerr << endl;

}

static void printLitVec(vec<Lit> & litvec)
{
    cerr <<"c "; 
    for (int i = 0; i < litvec.size(); ++i)
    {   
        printLitErr(litvec[i]);
    }
    cerr << endl;

}

static void printClauses(ClauseAllocator & ca, vector<CRef> list, bool skipDeleted)
{
    for (unsigned i = 0; i < list.size(); ++i)
    {
        if (skipDeleted && ca[list[i]].can_be_deleted())
            continue; 
        printClause(ca[list[i]]);
    }

}

//expects filled variable processing queue
//
// force -> forces resolution
//
//
void BlockedVariableElimination::bve_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool force, bool doStatistics)   
{
    for (unsigned i = start; i < end; i++)
    {
       int v = variable_queue[i];
       vector<CRef> & pos = data.list(mkLit(v,false)); 
       vector<CRef> & neg = data.list(mkLit(v,true));
           
       // ---Printing all Clauses with v --------------------------//
       if (opt_verbose > 2)
       {
           cerr << "c Variable: " << v+1 << endl;
           cerr <<"c Clauses with Literal  " << v+1 <<":" << endl;
           printClauses(ca, pos, false);
           cerr <<"c Clauses with Literal ¬" << v+1 <<":" << endl;
           printClauses(ca, neg, false); 
       }
       // ---------------------------------------------------------//
       int pos_count = 0; 
       int neg_count = 0;
       int lit_clauses_old = 0;
       int lit_learnts_old = 0;
       int clauseCount = 0;  // |F_x| + |F_¬x| 

       if (opt_verbose > 0)
       {
           cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
           cerr << "c Counting Clauses" << endl;
       }
       
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
       if (opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
       // handle pure Literal;
       if (pos_count == 0 || neg_count == 0)
       {    
            lbool state;
            if      ((pos_count == 0) && (neg_count >  0))
            {
                state = data.enqueue(mkLit(v, false));             
                if(opt_verbose > 0) cerr << "c handling pure literal" << endl;
            }
            else if ((pos_count >  0) && (neg_count == 0))
            {
                state = data.enqueue(mkLit(v, true));
                if(opt_verbose > 0) cerr << "c handling pure literal" << endl;
            }
            else 
            {  
                if(opt_verbose > 0) cerr << "c no occurences of " << v+1 << endl;
                if(opt_verbose > 0) cerr << "c =============================================================================" << endl;
                continue;  // no positive and no negative occurrences of v 
            }              // -> nothing to assign
            if      (state == l_False) 
                return;                 // level 0 conflict TODO ABORT ???
            else if (state == l_Undef)
                ;                       // variable already assigned
            else if (state == l_True) 
                propagation.propagate(data);                       // new assignment -> TODO propagation 
            else 
                assert(0);              // something went wrong
            
            if(opt_verbose > 0) cerr << "c =============================================================================" << endl;
            continue;
       }

       // Declare stats variables;        
       char pos_stats[pos_count];
       char neg_stats[neg_count];
       int lit_clauses;
       int lit_learnts;
       
       if (!force) 
           if (anticipateElimination(data, pos, neg,  v, pos_stats, neg_stats, pos_count, neg_count, lit_clauses, lit_learnts) == l_False) 
               return;  // level 0 conflict found while anticipation TODO ABORT
       
       //mark Clauses without resolvents for deletion -> makes stats-array indexing inconsistent
       
       if(opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
       if(opt_verbose > 0) cerr << "c removing blocked clauses from F_" << v+1 << endl;
       removeBlockedClauses(data, pos, pos_stats, mkLit(v, false));
       if(opt_verbose > 0) cerr << "c removing blocked clauses from F_¬" << v+1 << endl;
       removeBlockedClauses(data, neg, neg_stats, mkLit(v, true));
        
       // if resolving reduces number of literals in clauses: 
       //    add resolvents
       //    mark old clauses for deletion
       if (force || lit_clauses <= lit_clauses_old)
       {
            if(opt_verbose > 0)  cerr << "c resolveSet" <<endl;
            resolveSet(data, pos, neg, v);
            removeClauses(data, pos, mkLit(v,false));
            removeClauses(data, neg, mkLit(v,true));
       }
       if(opt_verbose > 0)   cerr << "c =============================================================================" << endl;
    }
}
/*
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 */
inline void BlockedVariableElimination::removeClauses(CoprocessorData & data, vector<CRef> & list, Lit l)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        if (!ca[list[cr_i]].can_be_deleted())
        {
            data.removedClause(list[cr_i]);
            ca[list[cr_i]].set_delete(true);
            data.addToExtension(list[cr_i], l);
            if(opt_verbose > 1){
                cerr << "c removed clause: "; 
                printClause(ca[list[cr_i]]);
            }
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
inline lbool BlockedVariableElimination::anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, char* pos_stats , char* neg_stats, int pos_count, int neg_count, int & lit_clauses, int & lit_learnts)
{
    if(opt_verbose > 2)  cerr << "c starting anticipate BVE" << endl;
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
    for (int i = 0; i < pos_count; ++i)
        pos_stats[i] = 0;
    for (int i = 0; i < neg_count; ++i)
        neg_stats[i] = 0;
    
    int pc = 0;
    int nc = 0;

    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[positive[cr_p]];
        if (p.can_be_deleted())
        {  
            if(opt_verbose > 2)
            {  cerr << "c    skipped p"; 
               printClause(p);
            }
            continue;
        }
        nc = 0;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & n = ca[negative[cr_n]];
            if (n.can_be_deleted())
            {   
                if(opt_verbose > 2)
                {
                    cerr << "c    skipped n";
                    printClause(n);
                }
                continue;
            }
            int newLits = tryResolve(p, n, v);
            

            if(opt_verbose > 2) cerr << "c    resolvent size " << newLits << endl;

            if (newLits > 1)
            {
                if(opt_verbose > 2)  
                {   
                    cerr << "c    Clause P: ";
                    printClause(p);
                    cerr <<  "c    Clause N: ";
                    printClause(n);
                    cerr  << "c    Resolvent: ";
                    vec<Lit> resolvent; 
                    resolve(p,n,v,resolvent); 
                    printLitVec(resolvent);
                }
                ++pos_stats[pc];
                ++neg_stats[nc];
                if (p.learnt() && n.learnt())
                    lit_learnts += newLits;
                else 
                    lit_clauses += newLits;
            }
            
            // empty Clause
            else if (newLits == 0)
            {
                if(opt_verbose > 2) 
                {
                    cerr << "c    empty reslovent" << endl;
                    cerr << "c    Clause P: ";
                    printClause(p);
                    cerr << "c    Clause N: ";
                    printClause(n);
                    cerr << "c    finished anticipate_bve by finding empty clause" << endl;
                }
                data.setFailed();
                return l_False;
            }
            
            // unit Clause
            else if (newLits == 1)
            {
                vec <Lit > resolvent;
                resolve(p,n,v,resolvent); 
                assert(resolvent.size() == 1);
                if(opt_verbose > 2) 
                {
                    cerr << "c    Unit Resolvent: ";
                    printLitVec(resolvent);
                    cerr << "c    Clause P: ";
                    printClause(p); 
                    cerr  << "c     Clause N: ";
                    printClause(n);               
                }
                lbool status = data.enqueue(resolvent[0]); //check for level 0 conflict
                if (status == l_False)
                {
                    if(opt_verbose > 2) cerr << "c finished anticipate_bve with conflict" << endl;
                    return l_False;
                }
                else if (status == l_Undef)
                     ; // variable already assigned
                else if (status == l_True)
                    propagation.propagate(data);  //TODO propagate  
                else 
                    assert (0); //something went wrong
            }

            if(opt_verbose > 2) cerr << "c ------------------------------------------" << endl;
            ++nc;
        }
        ++pc;
    }
    if(opt_verbose > 2) 
    {
        for (int i = 0; i < pos_count; ++i)
            cerr << "c pos stat("<< i <<"): " << (unsigned) pos_stats[i] << endl;;
        for (int i = 0; i < neg_count; ++i)
            cerr << "c neg stat("<< i <<"): " << (unsigned) neg_stats[i] << endl;;

        cerr << "c finished anticipate_bve normally" << endl;
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
        Clause & p = ca[positive[cr_p]];
        if (p.can_be_deleted())
            continue;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & n = ca[negative[cr_n]];
            if (n.can_be_deleted())
                continue;
            vec<Lit> ps;
            if (!resolve(p, n, v, ps))
            {
               if (ps.size()>1)
               {
                    CRef cr = ca.alloc(ps, p.learnt() && n.learnt());
                    data.addClause(cr);
                    if (p.learnt() && n.learnt())
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);
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
  BVEWorkData*      workData = (BVEWorkData*) arg;
  workData->bve->bve_worker(*(workData->data), workData->start,workData->end, false);
  return 0;
}
 


/* this function removes Clauses that have no resolvents
 * i.e. all resolvents are tautologies
 *
 */
inline void BlockedVariableElimination::removeBlockedClauses(CoprocessorData & data, vector< CRef> & list, char stats[], Lit l )
{
   //TODO -> if learnt, check if it was reason ? i think this is done!
   int stats_index = 0; 
   for (unsigned ci = 0; ci < list.size(); ++ci)
   {    
        Clause & c =  ca[list[ci]];
        if (c.can_be_deleted())
            continue;
        if (stats[stats_index] == 0)
        { 
            c.set_delete(true);
            data.removedClause(list[ci]);
            data.addToExtension(list[ci], l);
            if(opt_verbose > 1) 
            {
                cerr << "c removed clause: "; 
                printClause(ca[list[ci]]);
            }
        }
        ++stats_index;
   }
}           
