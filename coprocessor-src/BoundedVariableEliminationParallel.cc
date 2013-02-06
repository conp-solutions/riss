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
static IntOption  opt_verbose         (_cat, "cp3_bve_verbose",    "Verbosity of preprocessor", 0, IntRange(0, 3));
static IntOption  opt_resolve_learnts (_cat, "cp3_bve_resolve_learnts", "Resolve learnt clauses: 0: off, 1: original with learnts, 2: 1 and learnts with learnts", 0, IntRange(0,2));
static BoolOption opt_unlimited_bve   (_cat, "bve_unlimited",  "perform bve test for Var v, if there are more than 10 + 10 or 15 + 5 Clauses containing v", false);
static IntOption  opt_learnt_growth   (_cat, "cp3_bve_learnt_growth", "Keep C (x) D, where C or D is learnt, if |C (x) D| <= max(|C|,|D|) + N", 0, IntRange(-1, INT32_MAX));

static IntOption  opt_bve_heap        (_cat, "cp3_bve_heap"     ,  "0: minimum heap, 1: maximum heap, 2: random", 0, IntRange(0,2));

static void printLitErr(const Lit l) 
{
    if (toInt(l) % 2)
           cerr << "-";
        cerr << (toInt(l)/2) + 1 << " ";
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
/** parallel version of bve worker
 *
 *  Preconditions:
 *  all unit clauses were propagated
 *
 *  Assumptions:
 *  -> occurrence lists are valid, 
 *          i.e. CR in list(l) iff l \in ca[CR]
 *     they may contain deleted clauses (blocked clauses or clauses that were resolved)
 *     resolvents are immediately added to the list
 *     strengthened clauses are removed from the corresponding list
 *     => this is achived by a Readers-Writer-Lock
 *
 *  The locks are acquired in the following order
 *  X     Heap Lock
 *  1.    Locks on Variables (from Low to High)
 *  2.a   Read  Lock on both, clause allocator and occurrences
 *  2.a.1 Write-Lock on data-object (for statistics), enqueuing of Units, extensions
 *  2.b   Write Lock on both, clasue allocates and occurrences
 *
 *  -> i.e. you must not acquire a write and a read lock at the same time
 *          you must not acquire a write lock on the data object, if alread 2.b was acquired
 *          you either hold a heap lock or any of the other locks 
 */
void BoundedVariableElimination::par_bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, Heap<NeighborLt> & neighbor_heap, vector <CRef> & subsumeQueue, vector <CRef> & sharedSubsumeQueue, vector < CRef > & strengthQueue, vector < CRef > & sharedStrengthQeue, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, const bool force, const bool doStatistics)   
{
    SpinLock & data_lock = var_lock[data.nVars()]; 
    SpinLock & heap_lock = var_lock[data.nVars() + 1];
    SpinLock & ca_lock   = var_lock[data.nVars() + 2];

    vector < Var > neighbors;
    vec < Lit > ps;

    int32_t timeStamp;  
    while ( data.ok() ) // if solver state = false => abort
    {
        Var v = var_Undef; 
        // lock Heap and acquire variable
        heap_lock.lock();
        if (heap.size() > 0)
            v = heap.removeMin();
        heap_lock.unlock();
        
        // if Heap was empty, break
        if (v == var_Undef)
            break;
        
    calculate_neighbors:
    
        // Heuristic Cutoff
        if (!opt_unlimited_bve && (data[mkLit(v,true)] > 10 && data[mkLit(v,false)] > 10 || data[v] > 15 && (data[mkLit(v,true)] > 5 || data[mkLit(v,false)] > 5)))
            continue;
        
        // Lock v 
        var_lock[v].lock();

        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Begin: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
        
        rwlock.readLock();
        // Get the clauses with v
        vector<CRef> & pos = data.list(mkLit(v,false)); 
        vector<CRef> & neg = data.list(mkLit(v,true));
           
        // Build neighbor-vector
        // expecting valid occurrence-lists, i.e. v really occurs in lists 
        for (int r = 0; r < pos.size(); ++r)
        {
            Clause & c = ca[pos[r]];
            if (c.can_be_deleted())
                continue;
            for (int l = 0; l < c.size(); ++ l)
            {
                Var v = var(c[l]);
                if (! neighbor_heap.inHeap(v))
                    neighbor_heap.insert(v);
            }
        }
        for (int r = 0; r < neg.size(); ++r)
        {
            Clause & c = ca[neg[r]];
            if (c.can_be_deleted())
                continue;
            for (int l = 0; l < c.size(); ++ l)
            {
                Var v = var(c[l]);
                if (! neighbor_heap.inHeap(v))
                    neighbor_heap.insert(v);
            }
        }
        // TODO: do we need a data-lock here?
        timeStamp = lastTouched.getIndex(v); // get last modification of v

        rwlock.readUnlock();

        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  End: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
        
        // unlock v
        var_lock[v].unlock();

        while (neighbor_heap.size() > 0)
            neighbors.push_back(neighbor_heap.removeMin());
        // neighbor contains all neighbors in ascending order
       
        // lock all Vars in ascending order 
        for (int i = 0; i < neighbors.size(); ++i)
        {
           var_lock[neighbors[i]].lock();
        }
        
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Begin: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
        rwlock.readLock(); 
        
        // check if the variable-sub-graph is still valid
        // or UNSAT
        if (lastTouched.getIndex(v) != timeStamp || !data.ok())
        {
            rwlock.readUnlock(); // Early Exit Read Lock
            // free all locks on Vars in descending order 
            for (int i = neighbors.size() - 1; i >= 0; --i)
            {
               var_lock[neighbors[i]].unlock();
            }

            // if UNSAT -> return
            if (!data.ok())
                return;

            // Cleanup
            neighbors.clear();
            neighbor_heap.clear();
            
            goto calculate_neighbors;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  The process now holds all exclusive locks for the current neighbors of v, 
        //  and perhaps some additional, since some Clauses could have 
        //  been removed from pos and neg, but for sure none were added. 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
        
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
        
        // Declare stats variables;        
        int32_t pos_stats[pos.size()];
        int32_t neg_stats[neg.size()];
        int lit_clauses;
        int lit_learnts;
        int new_clauses; 
        int new_learnts;
        
        if (!force) 
        {
            for (int i = 0; i < pos.size(); ++i)
                 pos_stats[i] = 0;
            for (int i = 0; i < neg.size(); ++i)
                 neg_stats[i] = 0;

            // anticipate only, if there are positiv and negative occurrences of var 
            if (pos_count != 0 &&  neg_count != 0)
                if (anticipateEliminationThreadsafe(data, pos, neg,  v, pos_stats, neg_stats, lit_clauses, lit_learnts, new_clauses, new_learnts, data_lock) == l_False) 
                {
                    // UNSAT: free locks and abort
                    rwlock.readUnlock(); // Early Exit Read Lock
                    // free all locks on Vars in descending order 
                    for (int i = neighbors.size() - 1; i >= 0; --i)
                    {
                        var_lock[neighbors[i]].unlock();
                    }
                    return;  
                }
        
            //mark Clauses without resolvents for deletion
            if(opt_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
            if(opt_verbose > 1) cerr << "c removing blocked clauses from F_" << v+1 << endl;
            removeBlockedClausesThreadSafe(data, pos, pos_stats, mkLit(v, false), data_lock); 
            if(opt_verbose > 1) cerr << "c removing blocked clauses from F_¬" << v+1 << endl;
            removeBlockedClausesThreadSafe(data, neg, neg_stats, mkLit(v, true), data_lock); 
        }
        rwlock.readUnlock();
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  End: Read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
       
        
        // if resolving reduces number of literals in clauses: 
        //    add resolvents
        //    mark old clauses for deletion
        if (new_clauses > 0 && (force || lit_clauses <= lit_clauses_old))
        {
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Reservation of Allocator-Memory 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
            
            ca_lock.lock();      
            AllocatorReservation memoryReservation = ca.reserveMemory( new_clauses, lit_clauses, 0, rwlock);
            ca_lock.unlock();

        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Begin: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
       
             rwlock.readLock(); 
             if (!data.ok())
             {
                 // UNSAT case -> end thread, but first release all locks
                 rwlock.readUnlock();
                 // free all locks on Vars in descending order 
                 for (int i = neighbors.size() - 1; i >= 0; --i)
                 {
                     var_lock[neighbors[i]].unlock();
                 }
                 return; 
             }
             
             if(opt_verbose > 1)  cerr << "c resolveSet" <<endl;

             if (resolveSetThreadSafe(data, pos, neg, v, ps, memoryReservation) == l_False) 
             {
                 // UNSAT case -> end thread, but first release all locks
                 rwlock.readUnlock();
                 // free all locks on Vars in descending order 
                 for (int i = neighbors.size() - 1; i >= 0; --i)
                 {
                     var_lock[neighbors[i]].unlock();
                 }
                 return; 
             }

             // remove Clauses that were resolved
             // (since write lock, no threadsafe implementation needed)
             removeClausesThreadSafe(data, pos, mkLit(v,false), data_lock);
             removeClausesThreadSafe(data, neg, mkLit(v,true), data_lock);
             if (opt_verbose > 0) cerr << "c Resolved " << v+1 <<endl;
 
             // touch variables:
             data_lock.lock();
             lastTouched.nextStep();
             for (int i = 0; i < neighbors.size(); ++i)
             {
                lastTouched.setCurrentStep(neighbors[i]); 
             }
             data_lock.unlock();

             //subsumption with new clauses!!
             //TODO implement a threadsafe variant for this
             // -> subsumption as usual
             // -> strengthening without local queue -> just tagging
             //subsumption.subsumeStrength(data); 

             rwlock.readUnlock();
        
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  End: write locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
       
        }
        
        if(opt_verbose > 1)   cerr << "c =============================================================================" << endl;

        // free all locks on Vars in descending order 
        for (int i = neighbors.size() - 1; i >= 0; --i)
        {
           var_lock[neighbors[i]].unlock();
        }
        // Cleanup
        neighbors.clear();
        neighbor_heap.clear();
    }

}

/**
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 *
 *      TODO don't use the data_lock, since we already have the global writeLock
 */
inline void BoundedVariableElimination::removeClausesThreadSafe(CoprocessorData & data, const vector<CRef> & list, const Lit l, SpinLock & data_lock)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        Clause & c = ca[list[cr_i]];
        CRef cr = list[cr_i];
        if (!c.can_be_deleted())
        {
	    // also updated deleteTimer
            c.set_delete(true);
            data_lock.lock();
            data.removedClause(cr);
            data.addToExtension(cr, l);
            data_lock.unlock();
            if(opt_verbose > 1){
                cerr << "c removed clause: " << c << endl; 
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
inline lbool BoundedVariableElimination::anticipateEliminationThreadsafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts, int & new_clauses, int & new_learnts, SpinLock & data_lock)
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
            if(opt_verbose > 2) cerr << "c    skipped p " << p << endl;
            continue;
        }
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & n = ca[negative[cr_n]];
            if (n.can_be_deleted())
            {   
                if(opt_verbose > 2)
                {
                    cerr << "c    skipped n " << n << endl;
                }
                continue;
            }
            int newLits = tryResolve(p, n, v);

            if(opt_verbose > 2) cerr << "c    resolvent size " << newLits << endl;
            
            // Clause Size > 1
            if (newLits > 1)
            {
                if(opt_verbose > 2)  
                {   
                    cerr << "c    Clause P: " << p << endl; 
                    cerr << "c    Clause N: " << n << endl; 
                    cerr  << "c    Resolvent: "; 
                    vec<Lit> resolvent; 
                    resolve(p,n,v,resolvent); 
                    printLitVec(resolvent);
                }
                ++pos_stats[cr_p];
                ++neg_stats[cr_n];
                if (p.learnt() || n.learnt())
                {
                    lit_learnts += newLits;
                    ++new_learnts;
                }
                else 
                {
                    lit_clauses += newLits;
                    ++new_clauses;
                }
            }
            
            // empty Clause
            else if (newLits == 0)
            {
                if(opt_verbose > 2) 
                {
                    cerr << "c    empty resolvent" << endl;
                    cerr << "c    Clause P: " << p << endl; 
                    cerr << "c    Clause N: " << n << endl;
                    cerr << "c    finished anticipate_bve by finding empty clause" << endl;
                }
                data_lock.lock();
                data.setFailed();
                data_lock.unlock();
                return l_False; //howto deal with false ?
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
                    cerr << "c    Clause P: " << p << endl; 
                    cerr  << "c     Clause N: " << n << endl; 
                }
                data_lock.lock();
                lbool status = data.enqueue(resolvent[0]); //check for level 0 conflict
                data_lock.unlock();
                if (status == l_False)
                {
                    if(opt_verbose > 2) cerr << "c finished anticipate_bve with conflict" << endl;
                    return l_False;
                }
                else if (status == l_Undef)
                     ; // variable already assigned
                else if (status == l_True)
                { 
                // TODO -> omit propagation for now
                /*    propagation.propagate(data, true);  //TODO propagate own lits only (parallel)
                    if (p.can_be_deleted())
                        break;*/
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

/**
 *   threadsafe variant of resolve set, 
 *   assumption: Write Lock is holded, i.e. exclusive read and write access to data object and clause allocator
 *
 *   performes resolution and 
 *   allocates resolvents c, with |c| > 1, in ClauseAllocator 
 *   as learnts or clauses respectively
 *   and adds them in data object
 *
 *   - resolvents that are tautologies are skipped 
 *   - unit clauses and empty clauses are not handeled here
 *          -> this is already done in anticipateElimination 
 */
lbool BoundedVariableElimination::resolveSetThreadSafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, vec<Lit> & ps, AllocatorReservation & memoryReservation, const bool keepLearntResolvents, const bool force)
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

            // Dont compute resolvents for learnt clauses
            if (opt_resolve_learnts == 0 && (p.learnt() || n.learnt()))
               continue;
            if (opt_resolve_learnts == 1 && (p.learnt() && n.learnt()))
               continue;

            ps.clear();
            if (!resolve(p, n, v, ps))
            {
               // | resolvent | > 1
               if (ps.size()>1)
               {
                    if ((p.learnt() || n.learnt()) && ps.size() > max(p.size(),n.size()) + opt_learnt_growth)
                        continue;
                    CRef cr = ca.allocThreadsafe(memoryReservation, ps, p.learnt() || n.learnt()); 
                    Clause & resolvent = ca[cr];
                    data.addClause(cr);
                    if (resolvent.learnt()) 
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);

                    // TODO push Clause to local subsumption-queue
                    // subsumption.initClause(cr);
               }
           }
        }

    }
    return l_Undef;
}

/* this function removes Clauses that have no resolvents, threadsafe version with data_lock
 * i.e. all resolvents are tautologies
 *
 */
inline void BoundedVariableElimination::removeBlockedClausesThreadSafe(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l, SpinLock & data_lock )
{
   for (unsigned ci = 0; ci < list.size(); ++ci)
   {    
        Clause & c =  ca[list[ci]];
        if (c.can_be_deleted())
            continue;
        if (stats[ci] == 0)
        { 
            c.set_delete(true);
            data_lock.lock();
            data.removedClause(list[ci]);
            data.addToExtension(list[ci], l);
            if(opt_verbose > 1 || (opt_verbose > 0 && ! c.learnt())) 
            {
                cerr << "c removed clause: " << ca[list[ci]] << endl;
                cerr << "c added to extension with Lit " << l << endl;;
            } 
            data_lock.unlock();
        }
   }
}

void* BoundedVariableElimination::runParallelBVE(void* arg)
{
  BVEWorkData*      workData = (BVEWorkData*) arg;
  workData->bve->par_bve_worker(*(workData->data), *(workData->heap), *(workData->neighbor_heap), *(workData->subsumeQueue), *(workData->sharedSubsumeQueue), *(workData->strengthQueue), *(workData->sharedStrengthQeue), *(workData->var_locks), *(workData->rw_lock)); 
  return 0;
}

void BoundedVariableElimination::parallelBVE(CoprocessorData& data)
{
  cerr << "c parallel bve with " << controller.size() << " threads" << endl;
  
  lastTouched.resize(data.nVars());
  
  VarOrderBVEHeapLt comp(data, opt_bve_heap);
  Heap<VarOrderBVEHeapLt> newheap(comp);
  if (opt_bve_heap != 2)
  {
    data.getActiveVariables(lastDeleteTime(), newheap);
  }
  else 
  {
    data.getActiveVariables(lastDeleteTime(), variable_queue);
  }
  if (propagation.propagate(data, true) == l_False)
      return;
  
  BVEWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  vector< SpinLock > var_locks (data.nVars() + 3); // 3 extra SpinLock for data, heap, ca
  vector< vector < CRef > > subsumeQueues (controller.size());
  vector< vector < CRef > > strengthQueues (controller.size());
  NeighborLt comperator;
  Heap<NeighborLt>  * neighbor_heaps[controller.size()];
  for (int i = 0; i < controller.size(); ++ i)
  {
      neighbor_heaps[i] = new Heap<NeighborLt>(comperator);
  }
  vector< CRef > sharedSubsumeQueue; 
  vector< CRef > sharedStrengthQeue;
  ReadersWriterLock rw_lock;
  
  for ( int i = 0 ; i < controller.size(); ++ i ) 
  {
    workData[i].bve   = this;
    workData[i].data  = &data; 
    workData[i].var_locks = & var_locks;
    workData[i].rw_lock = & rw_lock;
    workData[i].heap  = &newheap;
    workData[i].neighbor_heap = neighbor_heaps[i];
    workData[i].subsumeQueue = & subsumeQueues[i];
    workData[i].strengthQueue = & strengthQueues[i];
    workData[i].sharedSubsumeQueue = & sharedSubsumeQueue;
    workData[i].sharedStrengthQeue = & sharedSubsumeQueue;
    //workData[i].stats = & localStats[i];
    jobs[i].function  = BoundedVariableElimination::runParallelBVE;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );

  //propagate units
  if (data.hasToPropagate())
      propagation.propagate(data, true);

}

