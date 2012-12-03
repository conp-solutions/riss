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
: Technique( _ca, _controller )
, propagation( _propagation)
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

lbool BlockedVariableElimination::runBVE(CoprocessorData& data)
{
  assert(variable_queue.size() == 0);
  
  //  put all variables in queue 
  for (int v = 0; v < data.nVars() /*&& v < 2*/ ; ++v)
      variable_queue.push_back(v);

  bve_worker(data, 0, variable_queue.size(), false);
  variable_queue.clear();
  if (data.getSolver()->okay())
    return l_Undef;
  else 
    return l_False;
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

       if (opt_verbose > 1)
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
            ++pos_count;
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
            ++neg_count;
       }
       if (opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
       // handle pure Literal;
       if (pos_count == 0 || neg_count == 0)
       {    
            lbool state;
            if      ((pos_count > 0) && (neg_count ==  0))
            {
                state = data.getSolver()->value(mkLit(v, false));             
                if (state != l_False)
                {    
                    state = data.enqueue(mkLit(v, false));
                }
                if(opt_verbose > 1) cerr << "c handling pure literal" << endl;
                if(opt_verbose > 0) cerr << "c Pure Lit " << v+1 << endl;
                if(opt_verbose > 1) cerr << "c Pure Lit " << (state == l_False ? "negation enqueued before" : "enqueued successful") << endl;
            }
            else if ((pos_count ==  0) && (neg_count > 0))
            {
                state = data.getSolver()->value(mkLit(v, true));
                if (state != l_False)
                {
                    state = data.enqueue(mkLit(v, true));
                } 
                if(opt_verbose > 1) cerr << "c handling pure literal" << endl;
                if(opt_verbose > 0) cerr << "c Pure Lit ¬" << v+1 << endl;
                if(opt_verbose > 1) cerr << "c Pure Lit " << (state == l_False ? "negation enqueued before" : "enqueued successful") << endl;
            }
            else 
            {  
                if(opt_verbose > 1) cerr << "c no occurences of " << v+1 << endl;
                if(opt_verbose > 1) cerr << "c =============================================================================" << endl;
                continue;  // no positive and no negative occurrences of v 
            }              // -> nothing to assign
            if      (state == l_False)  // this is not an UNSAT case -> there may are other lits, that make the clauses true.
                ; 
            else if (state == l_Undef)
                ;                       // variable already assigned
            else if (state == l_True) 
                propagation.propagate(data, true);                       // new assignment -> TODO propagate own lits only 
            else 
                assert(0);              // something went wrong
            
            if(opt_verbose > 1) cerr << "c =============================================================================" << endl;
            continue;
       }

       // Declare stats variables;        
       int32_t pos_stats[pos.size()];
       int32_t neg_stats[neg.size()];
       int lit_clauses;
       int lit_learnts;
       
       if (!force) 
           if (anticipateElimination(data, pos, neg,  v, pos_stats, neg_stats, lit_clauses, lit_learnts) == l_False) 
               return;  // level 0 conflict found while anticipation TODO ABORT
       
       //mark Clauses without resolvents for deletion -> makes stats-array indexing inconsistent
       
       if(opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
       if(opt_verbose > 1) cerr << "c removing blocked clauses from F_" << v+1 << endl;
       removeBlockedClauses(data, pos, pos_stats, mkLit(v, false));
       if(opt_verbose > 1) cerr << "c removing blocked clauses from F_¬" << v+1 << endl;
       removeBlockedClauses(data, neg, neg_stats, mkLit(v, true));
        
       // if resolving reduces number of literals in clauses: 
       //    add resolvents
       //    mark old clauses for deletion
       if (force || lit_clauses <= lit_clauses_old)
       {
            if(opt_verbose > 1)  cerr << "c resolveSet" <<endl;
            if (resolveSet(data, pos, neg, v) == l_False)
                return;
            removeClauses(data, pos, mkLit(v,false));
            removeClauses(data, neg, mkLit(v,true));
            if (opt_verbose > 0) cerr << "c Resolved " << v+1 <<endl;
       }
       if(opt_verbose > 1)   cerr << "c =============================================================================" << endl;
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
        Clause & c = ca[list[cr_i]];
        CRef cr = list[cr_i];
        if (!c.can_be_deleted())
        {
            data.removedClause(cr);
            c.set_delete(true);
            data.addToExtension(cr, l);
            if(opt_verbose > 1){
                cerr << "c removed clause: "; 
                printClause(c);
            }
        }

        //Delete Clause from all Occ-Lists TODO too much overhead?
        /*for (int j = 0; j < c.size(); ++j)
            if (c[j] != l)
                data.removeClauseFrom(cr,c[j]);
        list[cr_i] = list[list.size() - 1];
        list.pop_back();*/
    }

}


/*
 *  anticipates following numbers:
 *  -> number of resolvents derived from specific clause:       pos_stats / neg_stats
 *  -> total number of literals in clauses after resolution:    lit_clauses
 *  -> total number of literals in learnts after resolution:    lit_learnts
 *
 */
inline lbool BlockedVariableElimination::anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts)
{
    if(opt_verbose > 2)  cerr << "c starting anticipate BVE" << endl;
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
    for (int i = 0; i < positive.size(); ++i)
        pos_stats[i] = 0;
    for (int i = 0; i < negative.size(); ++i)
        neg_stats[i] = 0;
    
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
                ++pos_stats[cr_p];
                ++neg_stats[cr_n];
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
                    cerr << "c    empty resolvent" << endl;
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
                if(opt_verbose > 0) 
                {
                    cerr << "c    Unit Resolvent: ";
                    printLitVec(resolvent);
                }   
                if(opt_verbose > 2)
                {
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
                {
                    propagation.propagate(data, true);  //TODO propagate own lits only (parallel)
                    if (p.can_be_deleted())
                        break;
                }
                else 
                    assert (0); //something went wrong
            }

            if(opt_verbose > 2) cerr << "c ------------------------------------------" << endl;
        }
    }
    if(opt_verbose > 2) 
    {
        for (int i = 0; i < positive.size(); ++i)
            cerr << "c pos stat("<< i <<"): " << (unsigned) pos_stats[i] << endl;;
        for (int i = 0; i < negative.size(); ++i)
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
 */
lbool BlockedVariableElimination::resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, int v, bool force)
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
               // | resolvent | > 1
               if (ps.size()>1)
               {
                    CRef cr = ca.alloc(ps, p.learnt() && n.learnt());
                    data.addClause(cr);
                    if (p.learnt() && n.learnt())
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);
               }
               else if (force)
               {
                    // | resolvent | == 0  => UNSAT
                    if (ps.size() == 0)
                    {
                        data.setFailed();
                        return l_False;
                    }
                    
                    // | resolvent | == 1  => unit Clause
                    else if (ps.size() == 1)
                    {
                        lbool status = data.enqueue(ps[0]); //check for level 0 conflict
                        if (status == l_False)
                            return l_False;
                        else if (status == l_Undef)
                             ; // variable already assigned
                        else if (status == l_True)
                        {
                            propagation.propagate(data, true);  //TODO propagate own lits only (parallel)
                            if (p.can_be_deleted())
                                break;
                        }
                        else 
                            assert (0); //something went wrong
                    }
                }   

           }
        }

    }
    return l_Undef;
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
           char status = checkUpdatePrev(prev, c[i]);
           if (status == -1)
             return -1;
           else 
           {     
               ++i;
               r+=status;;
           }
        }
        else 
        {
           char status = checkUpdatePrev(prev, d[j]);
           if (status == -1)
              return -1;
           else
           {     
               ++j; 
               r+=status;
           }
        }
    }
    while (i < c.size())
    {
        if (c[i] == mkLit(v,false))
            ++i;   
        else
        {   
            char status = checkUpdatePrev(prev, c[i]);
            if (status == -1)
                return -1;
            else 
            {
                ++i;
                r+=status;
            }
        }
    }
    while (j < d.size())
    {
        if (d[j] == mkLit(v,true))
            ++j;
        else 
        {
            char status = checkUpdatePrev(prev, d[j]);
            if (status == -1)
                return -1;
            else 
            {
                ++j;
                r+=status;
            }
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

inline char BlockedVariableElimination::checkUpdatePrev(Lit & prev, Lit l)
{
    if (prev != lit_Undef)
    {
        if (prev == l)
            return 0;
        if (prev == ~l)
            return -1;
    }
    prev = l;
    return 1;
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
inline void BlockedVariableElimination::removeBlockedClauses(CoprocessorData & data, vector< CRef> & list, int32_t stats[], Lit l )
{
   for (unsigned ci = 0; ci < list.size(); ++ci)
   {    
        Clause & c =  ca[list[ci]];
        if (c.can_be_deleted())
            continue;
        if (stats[ci] == 0)
        { 
            c.set_delete(true);
            data.removedClause(list[ci]);
            data.addToExtension(list[ci], l);
            if(opt_verbose > 1 || (opt_verbose > 0 && ! c.learnt())) 
            {
                cerr << "c removed clause: "; 
                printClause(ca[list[ci]]);
            } 
        }
   }
}           
