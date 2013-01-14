/*******************************************************************[BoundedVariableElimination.cc]
Copyright (c) 2012, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/
#include "coprocessor-src/BoundedVariableElimination.h"
#include "coprocessor-src/Propagation.h"
#include "coprocessor-src/Subsumption.h"
#include "mtl/Heap.h"
using namespace Coprocessor;
using namespace std;

static const char* _cat = "COPROCESSOR 3 - BVE";

static IntOption  opt_verbose    (_cat, "cp3_bve_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3));
static IntOption  opt_learnt_growth (_cat, "cp3_bve_learnt_growth", "Keep C (x) D, where C or D is learnt, if |C (x) D| <= max(|C|,|D|) + N", 0, IntRange(-1, INT32_MAX));
static IntOption  opt_resolve_learnts (_cat, "cp3_bve_resolve_learnts", "Resolve learnt clauses: 0: off, 1: original with learnts, 2: 1 and learnts with learnts", 0, IntRange(0,2));

BoundedVariableElimination::BoundedVariableElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation, Coprocessor::Subsumption & _subsumption )
: Technique( _ca, _controller )
, propagation( _propagation)
, subsumption( _subsumption)
//, heap_comp(NULL)
//, variable_heap(heap_comp)
{
}

bool BoundedVariableElimination::hasToEliminate()
{
  return false; //variable_heap.size() > 0; 
}

lbool BoundedVariableElimination::fullBVE(Coprocessor::CoprocessorData& data)
{
  return l_Undef;
}
 
lbool BoundedVariableElimination::runBVE(CoprocessorData& data)
{
  VarOrderBVEHeapLt comp(data);
  Heap<VarOrderBVEHeapLt> newheap(comp);
  data.getActiveVariables(lastDeleteTime(), newheap);

  //Propagation (TODO Why does omitting the propagation
  // and no PureLit Propagation cause wrong model extension?)
  propagation.propagate(data, true);
  
  if( false ) {
   cerr << "formula after propagation: " << endl;
   for( int i = 0 ; i < data.getClauses().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
   for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
  }

  bve_worker(data, newheap, 0, variable_queue.size(), false);
  newheap.clear();
  
  if (data.getSolver()->okay())
    return l_Undef;
  else 
    return l_False;
}  


static void printLitErr(const Lit l) 
{
    if (toInt(l) % 2)
           cerr << "-";
        cerr << (toInt(l)/2) + 1 << " ";
}


static void printClause(const Clause & c)
{
    cerr << "c ";
    for (int i = 0; i < c.size(); ++i)
        printLitErr(c[i]);
    cerr << endl;

}

static void printLitVec(const vec<Lit> & litvec)
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

/*
void BoundedVariableElimination::par_bve_worker ()
{
    // TODO 
    // Var v <- getVariable (threadsafe, probably randomly)
    // if (heuristic cutoff (v))
    //      continue;
    // vector < Var > neighbors <- calc sorted vector with neighbors of v 
    //      use a globally existing mark array per thread
    // Lock neighbors in order
    // Perform the normal bve-check (with unit enqueueing)
    //  -> remove Blocked Clauses
    //  -> if (less literes by bve)
    //      -> perform bve
    //  maybe include some subsumption and strengthening check at this point, since all clauses are already cached and locked (really?)
    // Free neighbors in reverse order
}
*/

//expects filled variable processing queue
//
// force -> forces resolution
//
//
void BoundedVariableElimination::bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, unsigned int start, unsigned int end, const bool force, const bool doStatistics)   
{
    vector<Var> touched_variables;
    while (heap.size() > 0 )
    {
        //Subsumption / Strengthening
        subsumption.subsumeStrength(data); 
        if( false ) {
            cerr << "formula after subsumeStrength: " << endl;
            for( int i = 0 ; i < data.getClauses().size(); ++ i )
                if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
            for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
                if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
        }

        updateDeleteTime(data.getMyDeleteTimer());
        //for (unsigned i = start; i < end; i++)
        while (heap.size() > 0)
        {
           int v = heap.removeMin();
	   // TODO: do not work on this variable, if it will be unit-propagated! if all units are eagerly propagated, this is not necessary
       // TODO: CUT-off if ! unlimited and data[mkLit(v,false)] > 10 && data[mkLit(v,true)] > 10 or data[v] > 20 -> adopt values
	   // if( data.value( mkLit(v,true) ) != l_Undef ) continue;
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
           
           
           // handle pure Literal -> don't do this, blocked Clause Elimination will remove the clauses
           if (false && (pos_count == 0 || neg_count == 0))
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
           {
               for (int i = 0; i < pos.size(); ++i)
                    pos_stats[i] = 0;
               for (int i = 0; i < neg.size(); ++i)
                    neg_stats[i] = 0;

               // anticipate only, there are positiv and negative occurrences of var 
               if (pos_count != 0 &&  neg_count != 0)
                   if (anticipateElimination(data, pos, neg,  v, pos_stats, neg_stats, lit_clauses, lit_learnts) == l_False) 
                       return;  // level 0 conflict found while anticipation TODO ABORT
           
               //mark Clauses without resolvents for deletion
               if(opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
               if(opt_verbose > 1) cerr << "c removing blocked clauses from F_" << v+1 << endl;
               removeBlockedClauses(data, pos, pos_stats, mkLit(v, false));
               if(opt_verbose > 1) cerr << "c removing blocked clauses from F_¬" << v+1 << endl;
               removeBlockedClauses(data, neg, neg_stats, mkLit(v, true));
           }

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

           //subsumption with new clauses!!
           subsumption.subsumeStrength(data);
           if(opt_verbose > 1)   cerr << "c =============================================================================" << endl;
          
        }

        // add active variables and clauses to variable heap and subsumption queues
        data.getActiveVariables(lastDeleteTime(), touched_variables);
        touchedVarsForSubsumption(data, touched_variables);
        heap.clear();
        for (int i = 0; i < touched_variables.size(); ++i)
        {
            heap.insert(touched_variables[i]);
        }
        touched_variables.clear();
    }
}

/*
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 */
inline void BoundedVariableElimination::removeClauses(CoprocessorData & data, const vector<CRef> & list, const Lit l)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        Clause & c = ca[list[cr_i]];
        CRef cr = list[cr_i];
        if (!c.can_be_deleted())
        {
	    // also updated deleteTimer
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
inline lbool BoundedVariableElimination::anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts)
{
    if(opt_verbose > 2)  cerr << "c starting anticipate BVE" << endl;
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
   
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
                if (p.learnt() || n.learnt())
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
 * TODO how to deal with learnt clauses
 */
lbool BoundedVariableElimination::resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const bool keepLearntResolvents, const bool force)
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
            // Dont compute resolvents for learnt clauses (this is done in case of force,
            // since blocked clauses and units have not been computed by anticipate)
            if (!force)
            {
                if (opt_resolve_learnts == 0 && (p.learnt() || n.learnt()))
                    continue;
                if (opt_resolve_learnts == 1 && (p.learnt() && n.learnt()))
                    continue;
            }        
            vec<Lit> ps;
            if (!resolve(p, n, v, ps))
            {
               // | resolvent | > 1
               if (ps.size()>1)
               {
                    // Depending on opt_resovle_learnts, skip clause creation
                    if (force)
                    { 
                        if (opt_resolve_learnts == 0 && (p.learnt() || n.learnt()))
                            continue;
                        if (opt_resolve_learnts == 1 && (p.learnt() && n.learnt()))
                            continue;
                    }
                    if ((p.learnt() || n.learnt()) && ps.size() > max(p.size(),n.size()) + opt_learnt_growth)
                        continue;
                    CRef cr = ca.alloc(ps, p.learnt() || n.learnt());
                    data.addClause(cr);
                    if (p.learnt() && n.learnt())
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);
                    // push Clause on subsumption-queue
                    subsumption.initClause(cr);
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
inline int BoundedVariableElimination::tryResolve(const Clause & c, const Clause & d, const int v)
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
inline bool BoundedVariableElimination::resolve(const Clause & c, const Clause & d, const int v, vec<Lit> & resolvent)
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

inline char BoundedVariableElimination::checkUpdatePrev(Lit & prev, const Lit l)
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

inline bool BoundedVariableElimination::checkPush(vec<Lit> & ps, const Lit l)
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

void* BoundedVariableElimination::runParallelBVE(void* arg)
{
  BVEWorkData*      workData = (BVEWorkData*) arg;
  //workData->bve->bve_worker(*(workData->data), workData->start,workData->end, false);
  return 0;
}
 


/* this function removes Clauses that have no resolvents
 * i.e. all resolvents are tautologies
 *
 */
inline void BoundedVariableElimination::removeBlockedClauses(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l )
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
                cerr << "c removed clause: " << ca[list[ci]] << endl;
                cerr << "c added to extension with Lit " << l << endl;;
            } 
        }
   }
}


// All clauses that have been modified, can possibly be subsumed by clauses
// that share a subset of their literals
// Therefore we add those clauses to the processing list (if they are not contained in it already).
/*void BoundedVariableElimination :: append_modified (CoprocessorData & data, std::vector<CRef> & modified_list)
{
    for (int i = 0; i < modified_list.size(); ++i)
    {
        Clause & c = ca[modified_list[i]];
        for (int l = 0; l < c.size(); l++)
        {
            vector<CRef> & clauses = data.list(c[l]);
            for (int j = 0; j < clauses.size(); ++j)
            {
                Clause & d = ca[clauses[j]];
                if (! d.can_subsume())
                {
                    d.set_subsume(true);
                    subsumption.initClause(d);
                    //subsumption_processing_queue.push_back(occs[l][j]);
                }
            }

        }
        //c.set_modified(false);
    }
}*/
inline void BoundedVariableElimination :: touchedVarsForSubsumption (CoprocessorData & data, const std::vector<Var> & touched_vars)
{
    for (int i = 0; i < touched_vars.size(); ++i)
    {
        Var v = touched_vars[i]; 
        addClausesToSubsumption(data, data.list(mkLit(v,false)));
        addClausesToSubsumption(data, data.list(mkLit(v,true)));
        
    }
}       
inline void BoundedVariableElimination::addClausesToSubsumption (CoprocessorData & data, const vector<CRef> & clauses)
{
    for (int j = 0; j < clauses.size(); ++j)
    {
        Clause & d = ca[clauses[j]];
        if (!d.can_be_deleted() && !d.can_subsume())
        {
            d.set_subsume(true);
            d.set_strengthen(true);
            subsumption.initClause(clauses[j]);
            //subsumption_processing_queue.push_back(occs[l][j]);
        }
    }    
}
