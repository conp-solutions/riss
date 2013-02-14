/***********************************************************[BoundedVariableEliminationParallel.cc]
Copyright (c) 2013, Kilian Gebhardt, All rights reserved.
**************************************************************************************************/
#include "coprocessor-src/BoundedVariableElimination.h"
#include "coprocessor-src/Propagation.h"
#include "coprocessor-src/Subsumption.h"
#include "mtl/Heap.h"
using namespace Coprocessor;
using namespace std;

extern BoolOption opt_par_bve;        
extern IntOption  opt_bve_verbose;
extern IntOption  opt_learnt_growth;
extern IntOption  opt_resolve_learnts;
extern BoolOption opt_unlimited_bve;
extern BoolOption opt_bve_findGate; 
extern IntOption  opt_bve_heap; 
static int upLevel = 1;

static inline void printLitErr(const Lit l) 
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


/** parallel version of bve worker
 *  TODO update this
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
void BoundedVariableElimination::par_bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, Heap<NeighborLt> & neighbor_heap, deque < CRef > & strengthQueue, deque < CRef > & sharedStrengthQueue, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, ParBVEStats & stats, const bool force, const bool doStatistics)   
{
    if (doStatistics) stats.processTime = wallClockTime() - stats.processTime;

    SpinLock & data_lock = var_lock[data.nVars()]; 
    SpinLock & heap_lock = var_lock[data.nVars() + 1]; // used for variable-heap or queue
    SpinLock & ca_lock   = var_lock[data.nVars() + 2];
    
    int32_t * pos_stats = (int32_t*) malloc (5 * sizeof(int32_t));
    int32_t * neg_stats = (int32_t*) malloc (5 * sizeof(int32_t));
     
    vector < Var > neighbors;
    vec < Lit > ps;

    int32_t timeStamp;  
    while ( data.ok() ) // if solver state = false => abort
    {
        // Propagate (rw-locks)
        if (data.hasToPropagate())
        {
            if (doStatistics) stats.upTime = wallClockTime() -  stats.upTime;
            par_bve_propagate(data, var_lock, rwlock, dirtyOccs, sharedStrengthQueue);
            if (doStatistics) stats.upTime = wallClockTime() -  stats.upTime;
            continue;
        }
        // Subsimp
        if (sharedStrengthQueue.size() > 0)
        {
            if (doStatistics) stats.subsimpTime = wallClockTime() -  stats.subsimpTime;
            par_bve_strengthening_worker(data, var_lock, rwlock, sharedStrengthQueue, strengthQueue, dirtyOccs, stats, false, doStatistics);
            if (doStatistics) stats.subsimpTime = wallClockTime() -  stats.subsimpTime;
            continue;
        }
       
        // Eliminate
        Var v = var_Undef;
        // lock Heap or Q and acquire variable
        heap_lock.lock();
        if (opt_bve_heap != 2)
        {
            if (heap.size() > 0)
                v = heap.removeMin();
        }
        else if (variable_queue.size() > 0)
        {
            int rand = data.getSolver()->irand(data.getSolver()->random_seed, variable_queue.size());
            v = variable_queue[rand];
            if (variable_queue.size() > 1)
            {
                variable_queue[rand] = variable_queue[variable_queue.size() - 2];
            }
            variable_queue.pop_back();

        }
        heap_lock.unlock();
        
        // if Heap or Queue was empty, break
        if (v == var_Undef)
            break;
        
    calculate_neighbors:
    
        // Heuristic Cutoff
        if (!opt_unlimited_bve && (data[mkLit(v,true)] > 10 && data[mkLit(v,false)] > 10 || data[v] > 15 && (data[mkLit(v,true)] > 5 || data[mkLit(v,false)] > 5)))
        {
            if (doStatistics) ++stats.skippedVars;
            continue;
        }
       
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

        assert(neighbor_heap.size() == 0);   
        
        // Build neighbor-vector
        // expecting valid occurrence-lists, i.e. v really occurs in lists 
        for (int r = 0; r < pos.size(); ++r)
        {
            const CRef cr = pos[r];
            if (CRef_Undef == cr)
                continue;
            Clause & c = ca[cr];
            c.spinlock();
            if (c.can_be_deleted())
            {
              c.unlock();
              continue;
            }
            for (int l = 0; l < c.size(); ++ l)
            {
                Var v = var(c[l]);
                if (! neighbor_heap.inHeap(v))
                    neighbor_heap.insert(v);
            }
            c.unlock();
        }
        for (int r = 0; r < neg.size(); ++r)
        {
            const CRef cr = neg[r];
            if (CRef_Undef == cr)
                continue;
            Clause & c = ca[cr];
            c.spinlock();
            if (c.can_be_deleted())
            {
                c.unlock();
                continue;
            }
            for (int l = 0; l < c.size(); ++ l)
            {
                Var v = var(c[l]);
                if (! neighbor_heap.inHeap(v))
                    neighbor_heap.insert(v);
            }
            c.unlock();
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
                break;

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
        
        if (doStatistics) ++stats.testedVars;

        int pos_count = 0; 
        int neg_count = 0;
        int lit_clauses_old = 0;
        int lit_learnts_old = 0;

        if (opt_bve_verbose > 1)
        {
           cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
           cerr << "c Counting Clauses" << endl;
        }
       
        for (int i = 0; i < pos.size(); ++i)
        {
            const CRef cr = pos[i];
            if (CRef_Undef == cr)
                continue;
            Clause & c = ca[cr];
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
            const CRef cr = neg[i];
            if (CRef_Undef == cr)
                continue;
            Clause & c = ca[cr];
            if (c.can_be_deleted())
                continue;
            if (c.learnt())
                lit_learnts_old += c.size();
            else 
                lit_clauses_old += c.size();
            ++neg_count;
        }
        if (opt_bve_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
        
        // Declare stats variables;        
        pos_stats = (int32_t *) realloc(pos_stats, sizeof( int32_t) * pos.size() );
        neg_stats = (int32_t *) realloc(neg_stats, sizeof( int32_t) * neg.size() );
        int lit_clauses = 0;
        int lit_learnts = 0;
        int new_clauses = 0; 
        int new_learnts = 0;
        
        // set stats to 0
        memset( pos_stats, 0 , sizeof( int32_t) * pos.size() );
        memset( neg_stats, 0 , sizeof( int32_t) * neg.size() );

        // anticipate only, if there are positiv and negative occurrences of var 
        if (pos_count != 0 &&  neg_count != 0)
        {
            if (doStatistics) ++stats.anticipations;
            if (anticipateEliminationThreadsafe(data, pos, neg,  v, ps, pos_stats, neg_stats, lit_clauses, lit_learnts, new_clauses, new_learnts, data_lock) == l_False) 
            {
                // UNSAT: free locks and abort
                rwlock.readUnlock(); // Early Exit Read Lock
                // free all locks on Vars in descending order 
                for (int i = neighbors.size() - 1; i >= 0; --i)
                {
                    var_lock[neighbors[i]].unlock();
                }
                //return;
                break;  
            }
        }
    
        //mark Clauses without resolvents for deletion
        if(opt_bve_verbose > 2) cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
        if(opt_bve_verbose > 1) cerr << "c removing blocked clauses from F_" << v+1 << endl;
        removeBlockedClausesThreadSafe(data, pos, pos_stats, mkLit(v, false), data_lock, stats, doStatistics); 
        if(opt_bve_verbose > 1) cerr << "c removing blocked clauses from F_¬" << v+1 << endl;
        removeBlockedClausesThreadSafe(data, neg, neg_stats, mkLit(v, true), data_lock, stats, doStatistics); 
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
                 //return;
                 break; // break the main-loop, i.e. end of function
             }
             
             if(opt_bve_verbose > 1)  cerr << "c resolveSet" <<endl;

             if (resolveSetThreadSafe(data, pos, neg, v, ps, memoryReservation, strengthQueue, stats, data_lock, doStatistics) == l_False) 
             {
                 // UNSAT case -> end thread, but first release all locks
                 rwlock.readUnlock();
                 // free all locks on Vars in descending order 
                 for (int i = neighbors.size() - 1; i >= 0; --i)
                 {
                     var_lock[neighbors[i]].unlock();
                 }
                 //return;
                 break; // breaks the main-loop, i.e. end of function
             }

             // remove Clauses that were resolved
             // (since write lock, no threadsafe implementation needed)
             removeClausesThreadSafe(data, pos, mkLit(v,false), data_lock, stats, doStatistics);
             removeClausesThreadSafe(data, neg, mkLit(v,true), data_lock, stats, doStatistics);
             if (opt_bve_verbose > 0) cerr << "c Resolved " << v+1 <<endl;
             if (doStatistics) ++stats.eliminatedVars;

             // touch variables:
             data_lock.lock();
             lastTouched.nextStep();
             for (int i = 0; i < neighbors.size(); ++i)
             {
                lastTouched.setCurrentStep(neighbors[i]); 
             }
             data_lock.unlock();

             //subsimp with new clauses!!
            if (doStatistics) stats.subsimpTime = wallClockTime() -  stats.subsimpTime;
             par_bve_strengthening_worker(data, var_lock, rwlock, sharedStrengthQueue, strengthQueue, dirtyOccs, stats, true, doStatistics);
            if (doStatistics) stats.subsimpTime = wallClockTime() -  stats.subsimpTime;

             rwlock.readUnlock();
        
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  End: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
       
        }
        
        if(opt_bve_verbose > 1)   cerr << "c =============================================================================" << endl;

        // free all locks on Vars in descending order 
        for (int i = neighbors.size() - 1; i >= 0; --i)
        {
           var_lock[neighbors[i]].unlock();
        }
        // Cleanup
        neighbors.clear();
        neighbor_heap.clear();
    }

    free(pos_stats);
    free(neg_stats);

    if (doStatistics) stats.processTime = wallClockTime() - stats.processTime;
}

/**
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 *
 *      TODO don't use the data_lock, since we already have the global writeLock
 */
inline void BoundedVariableElimination::removeClausesThreadSafe(CoprocessorData & data, const vector<CRef> & list, const Lit l, SpinLock & data_lock, ParBVEStats & stats, const bool doStatistics)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i)
    {
        const CRef cr = list[cr_i];
        if (CRef_Undef == cr)
            continue;
        Clause & c = ca[cr];
        if (!c.can_be_deleted())
        {
            c.set_delete(true);
            data_lock.lock();
            data.removedClause(cr); // updates stats and deleteTimer
            data.addToExtension(cr, l);
            data_lock.unlock();
            if (doStatistics)
            {
                if (c.learnt())
                {
                    ++stats.removedLearnts;
                    stats.learntLits += c.size();
                }
                else
                {

                    ++stats.removedClauses;
                    stats.removedLiterals += c.size();
                }
            }
            if(opt_bve_verbose > 1){
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
inline lbool BoundedVariableElimination::anticipateEliminationThreadsafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, vec <Lit> & resolvent, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts, int & new_clauses, int & new_learnts, SpinLock & data_lock)
{
    if(opt_bve_verbose > 2)  cerr << "c starting anticipate BVE" << endl;
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
   
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        const CRef crPos = positive[cr_p];
        if (CRef_Undef == crPos)
            continue;
        Clause & p = ca[crPos];
        if (p.can_be_deleted())
        {  
            if(opt_bve_verbose > 2) cerr << "c    skipped p " << p << endl;
            continue;
        }
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            const CRef crNeg = negative[cr_n];
            if (CRef_Undef == crNeg)
                continue;
            Clause & n = ca[crNeg];
            if (n.can_be_deleted())
            {   
                if(opt_bve_verbose > 2)
                {
                    cerr << "c    skipped n " << n << endl;
                }
                continue;
            }
            int newLits = tryResolve(p, n, v);

            if(opt_bve_verbose > 2) cerr << "c    resolvent size " << newLits << endl;
            
            // Clause Size > 1
            if (newLits > 1)
            {
                if(opt_bve_verbose > 2)  
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
                if(opt_bve_verbose > 2) 
                {
                    cerr << "c    empty resolvent" << endl;
                    cerr << "c    Clause P: " << p << endl; 
                    cerr << "c    Clause N: " << n << endl;
                    cerr << "c    finished anticipate_bve by finding empty clause" << endl;
                }
                data.setFailed();
                return l_False; 
            }
            
            // unit Clause
            else if (newLits == 1)
            {
                resolvent.clear();
                resolve(p,n,v,resolvent); 
                assert(resolvent.size() == 1);
                if(opt_bve_verbose > 0) 
                {
                    cerr << "c    Unit Resolvent: ";
                    printLitVec(resolvent);
                }   
                if(opt_bve_verbose > 2)
                {
                    cerr << "c    Clause P: " << p << endl; 
                    cerr  << "c     Clause N: " << n << endl; 
                }
                data_lock.lock();
                lbool status = data.enqueue(resolvent[0]); //check for level 0 conflict
                data_lock.unlock();
                if (status == l_False)
                {
                    if(opt_bve_verbose > 2) cerr << "c finished anticipate_bve with conflict" << endl;
                    return l_False;
                }
                else if (status == l_Undef)
                     ; // variable already assigned
                else if (status == l_True)
                { 
                // TODO -> omit propagation for now
                }
                else 
                    assert (0); //something went wrong
            }

            if(opt_bve_verbose > 2) cerr << "c ------------------------------------------" << endl;
        }
    }
    if(opt_bve_verbose > 2) 
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
lbool BoundedVariableElimination::resolveSetThreadSafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, vec<Lit> & ps, AllocatorReservation & memoryReservation, deque<CRef> & strengthQueue, ParBVEStats & stats, SpinLock & data_lock, const bool doStatistics, const bool keepLearntResolvents)
{
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        const CRef crPos = positive[cr_p];
        if (CRef_Undef == crPos)
            continue;
        Clause & p = ca[crPos];
        if (p.can_be_deleted())
            continue;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            const CRef crNeg = negative[cr_n];
            if (CRef_Undef == crNeg)
                continue;
            Clause & n = ca[crNeg];
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
                    const CRef cr = ca.allocThreadsafe(memoryReservation, ps, p.learnt() || n.learnt()); 
                    Clause & resolvent = ca[cr];
                    data_lock.lock();
                    data.addClause(cr);
                    if (resolvent.learnt()) 
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);
                    data_lock.unlock();
                    // add clause to local subsimp queues
                    strengthQueue.push_back(cr);
                    
                    if (doStatistics)
                    {
                        if (resolvent.learnt())
                        {
                            ++stats.newLearnts;
                            ++stats.newLearntLits;

                        } else 
                        {
                            ++stats.createdClauses;
                            stats.createdLiterals += resolvent.size();
                        }
                    }
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
inline void BoundedVariableElimination::removeBlockedClausesThreadSafe(CoprocessorData & data, const vector< CRef> & list, const int32_t _stats[], const Lit l, SpinLock & data_lock,ParBVEStats & stats, const bool doStatistics)
{
   for (unsigned ci = 0; ci < list.size(); ++ci)
   {    
        const CRef cr = list[ci];
        if (CRef_Undef == cr)
            continue;
        Clause & c =  ca[cr];
        if (c.can_be_deleted())
            continue;
        if (_stats[ci] == 0)
        { 
            c.set_delete(true);
            data_lock.lock();
            data.removedClause(cr);
            data.addToExtension(cr, l);
            data_lock.unlock();
            if (doStatistics)
            {
                if (c.learnt())
                {
                    ++stats.removedBlockedLearnt;
                    stats.learntBlockedLit += c.size();
                }
                else
                {
                    ++stats.removedBC; 
                    stats.blockedLits += c.size();
                }
            }
            if(opt_bve_verbose > 1 || (opt_bve_verbose > 0 && ! c.learnt())) 
            {
                cerr << "c removed clause: " << c << endl;
                cerr << "c added to extension with Lit " << l << endl;;
            }
        }
   }
}

void* BoundedVariableElimination::runParallelBVE(void* arg)
{
  BVEWorkData*      workData = (BVEWorkData*) arg;
  workData->bve->par_bve_worker(*(workData->data), *(workData->heap), *(workData->neighbor_heap), *(workData->strengthQueue), *(workData->sharedStrengthQueue), *(workData->var_locks), *(workData->rw_lock), *(workData->bveStats)); 
  return 0;
}

void BoundedVariableElimination::parallelBVE(CoprocessorData& data)
{
  if (data.hasToPropagate())
    if (propagation.propagate(data, true) == l_False)
      return;

  const bool doStatistics = true;
  if (doStatistics) processTime = wallClockTime() - processTime;
  cerr << "c parallel bve with " << controller.size() << " threads" << endl;
  
  lastTouched.resize(data.nVars());
  VarOrderBVEHeapLt comp(data, opt_bve_heap);
  Heap<VarOrderBVEHeapLt> newheap(comp);
  BVEWorkData workData[ controller.size() ];
  jobs.resize( controller.size() );
  variableLocks.resize(data.nVars() + 5); // 5 extra SpinLock for data, heap, ca, shared subsume-queue, shared strength-queue
  strengthQueues.resize(controller.size());
  parStats.resize(controller.size());
  dirtyOccs.resize(data.nVars() * 2);

  if (neighbor_heaps == NULL) 
  { 
      neighbor_heaps = (Heap<NeighborLt> **) malloc( sizeof(Heap<NeighborLt> * ) * controller.size());
      for (int i = 0; i < controller.size(); ++ i)
      {
        neighbor_heaps[i] = new Heap<NeighborLt>(neighborComperator);
      }
  }

  for ( int i = 0 ; i < controller.size(); ++ i ) 
  {
    workData[i].bve   = this;
    workData[i].data  = &data; 
    workData[i].var_locks = & variableLocks;
    workData[i].rw_lock = & allocatorRWLock;
    workData[i].heap  = &newheap;
    workData[i].neighbor_heap = neighbor_heaps[i];
    workData[i].strengthQueue = & strengthQueues[i];
    workData[i].sharedStrengthQueue = & sharedStrengthQueue;
    workData[i].bveStats = & parStats[i];
  }

  if (opt_bve_heap != 2)
  {
    data.getActiveVariables(lastDeleteTime(), newheap);
  }
  else 
  {
    data.getActiveVariables(lastDeleteTime(), variable_queue);
  }
  
  while ((opt_bve_heap != 2 && newheap.size() > 0) || (opt_bve_heap == 2 && variable_queue.size() > 0))
  {

    updateDeleteTime(data.getMyDeleteTimer());
    
    uint32_t timer = dirtyOccs.nextStep();
  
    for ( int i = 0 ; i < controller.size(); ++ i ) 
    {
      jobs[i].function  = BoundedVariableElimination::runParallelBVE;
      jobs[i].argument  = &(workData[i]);
    }
    controller.runJobs( jobs );

    if (!data.ok())
    {
      if (doStatistics) processTime = wallClockTime() - processTime;
      return;
    }
    // clean dirty occs
    data.cleanUpOccurrences(dirtyOccs,timer);
  
    //propagate units
    if (data.hasToPropagate())
      if (l_False == propagation.propagate(data, true))
      {
        if (doStatistics) processTime = wallClockTime() - processTime;
        return;
      }
    
    // add active variables and clauses to variable heap and subsumption queues
    data.getActiveVariables(lastDeleteTime(), touched_variables);
    touchedVarsForSubsumption(data, touched_variables);
    subsumption.subsumeStrength(data);

    if (opt_bve_heap != 2)
        newheap.clear();
    else
        variable_queue.clear();
    
    for (int i = 0; i < touched_variables.size(); ++i)
    {
        if (opt_bve_heap != 2)
            newheap.insert(touched_variables[i]);
        else 
            variable_queue.push_back(touched_variables[i]);
    }
    touched_variables.clear();
  }
  if (doStatistics) processTime = wallClockTime() - processTime;
}

/** lock-based parallel non-naive strengthening-methode
 * @param data object
 * @param start where to start strengthening in strengtheningqueue
 * @param end where to stop strengthening
 * @param var_lock vector of locks for each variable
 *
 * @param strength_resolvents : if activated, strengthener wont be locked
 *                              to use directly after VE when all locks are
 *                              already hold by the thread
 * 
 * Idea of strengthening Lock:
 * -> lock fst variable of strengthener
 * -> lock strengthener
 * -> if fstvar(other) <= fstvar(strengthener)
 *      -> lock other
 */
void BoundedVariableElimination::par_bve_strengthening_worker(CoprocessorData& data, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, MarkArray & dirtyOccs, ParBVEStats & stats, const bool strength_resolvents, const bool doStatistics)
{ 
  assert(data.nVars() <= var_lock.size() && "var_lock vector to small");
//  if (doStatistics)
//    stats.strengthTime = cpuTime() - stats.strengthTime;
 
  SpinLock & data_lock = var_lock[data.nVars()];
  SpinLock & strength_lock   = var_lock[data.nVars() + 4];

  while (data.ok() && ((!strength_resolvents && sharedStrengthQueue.size() > 0) || localQueue.size() > 0))
  {
    CRef cr = CRef_Undef;
    if( localQueue.size() == 0 ) {
        assert(!strength_resolvents);
        strength_lock.lock();
        if (sharedStrengthQueue.size() > 0)
        {
            cr = sharedStrengthQueue.back();
            sharedStrengthQueue.pop_back();
        }
        strength_lock.unlock();
    } else {
      // TODO: have a counter for this situation!
      cr = localQueue.back();
      localQueue.pop_back();
    }

    if (cr == CRef_Undef)
    {
        assert( ! strength_resolvents );
        break;
    }

    if (!strength_resolvents) rwlock.readLock();

    Var fst = var(ca[cr][0]);
    
    // No locking if strength_resolvents
    if (!strength_resolvents) 
    {
        lock_strengthener_nn: // readlock @ this point
        if (ca[cr].can_be_deleted() || ca[cr].size() == 0)
        {
            rwlock.readUnlock();
            continue;
        }
        fst = var(ca[cr][0]);
        rwlock.readUnlock();

        // lock 1st var 
        var_lock[fst].lock();

        rwlock.readLock();
        // lock strengthener
        ca[cr].spinlock();

        // assure that clause can still strengthen
        if (ca[cr].can_be_deleted() || ca[cr].size() == 0)
        {
            ca[cr].unlock();
            rwlock.readUnlock();
            var_lock[fst].unlock();
            continue;
        }
        
        // assure that first var still valid
        if (var(ca[cr][0]) != fst)
        {
            ca[cr].unlock();
            rwlock.readUnlock();
            var_lock[fst].unlock();
            rwlock.readLock();
            goto lock_strengthener_nn;
        }
    } 
   
    // readlock @ this point
    Clause& strengthener = ca[cr];

    // handle unit of empty strengthener
    if( strengthener.size() < 2 ) {
        if( strengthener.size() == 1 ) 
        {
            data_lock.lock();
                lbool status = data.enqueue(strengthener[0]); 
            data_lock.unlock();

            if (!strength_resolvents) 
            {
                strengthener.unlock();
                rwlock.readUnlock();
                var_lock[fst].unlock(); // unlock fst var
            } // no readlock @ this point

            if( l_False == status ) break;
	        else continue;
        }
        else 
        { 
            data.setFailed();  // can be done asynchronously
            if (!strength_resolvents) 
            {
                strengthener.unlock();
                rwlock.readUnlock();
                var_lock[fst].unlock(); 
            }
            break; 
        }
    }

    assert (strengthener.size() > 1 && "expect strengthener to be > 1");

    //find Lit with least occurrences and its occurrences
    // search lit with minimal occurrences
    Lit min = lit_Undef, nmin = lit_Undef;
    Lit minT = strengthener[0];
    for (int j = 1; j < strengthener.size(); ++j)
    {
       if (data[var(minT)] > data[var(strengthener[j])])
          minT = strengthener[j];
    }
    min = minT;
    nmin = ~minT;
     
    assert(min != nmin && "min and nmin should be different");
    vector<CRef>& list = data.list(min);        // occurrences of minlit from strengthener
    vector<CRef>& list_neg = data.list(nmin);   // occurrences of negated minlit from strengthener
    
    lbool state = strength_check_pos(data, list, sharedStrengthQueue, localQueue, strengthener, cr,      fst, var_lock, dirtyOccs, stats, strength_resolvents, doStatistics);
    if (l_False != state)
        strength_check_neg(data, list_neg,     sharedStrengthQueue, localQueue, strengthener, cr, min, fst, var_lock, dirtyOccs, stats, strength_resolvents, doStatistics);

    strengthener.set_strengthen(false);
    strengthener.set_subsume(false);

    // No locking if strength_resolvents
    if (!strength_resolvents)
    {
        strengthener.unlock();
        rwlock.readUnlock();
        // free lock of first variable
        var_lock[fst].unlock();
    }
  }
  //if (doStatistics) stats.strengthTime = cpuTime() - stats.strengthTime;
}


/**
 * strengthening check for parallel non-naive strengthening
 *
 * Preconditions: 
 * fst is locked by this thread
 *
 * Thread-Saftey-Requirements
 * only smaller variables than fst are locked
 * write acces to data only if var_lock[nVars()] is locked
 * no data.list(Lit) are written
 *
 * Postconditions:
 * all locks, that were aquired, are freed
 *
 * @param strengthener 
 * @param list          a list with CRef to check for strengthening
 * @param var_lock      lock for each variable
 *
 */
inline lbool BoundedVariableElimination::strength_check_pos(CoprocessorData & data, vector < CRef > & list, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Var fst, vector < SpinLock > & var_lock, MarkArray & dirtyOccs, ParBVEStats & stats, const bool strength_resolvents, const bool doStatistics) 
{
    int si, so;           // indices used for "can be strengthened"-testing
    int negated_lit_pos;  // index of negative lit, if we find one
 
    SpinLock & data_lock = var_lock[data.nVars()];
    SpinLock & strength_lock   = var_lock[data.nVars() + 4];
    
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      const CRef crO = list[j];
      if (CRef_Undef == crO) 
          continue;

      Clause& other = ca[crO];
      
      lock_to_strengthen_nn:
      // a clause can not strengthen itself, and a clause can not strengther smaller clauses
      if (other.can_be_deleted() || crO == cr || other.size() < strengthener.size())
        continue;
      Lit other_fst_lit = other[0];
      Var other_fst = var(other[0]);
      

      // if the other_fst > fst, other cannot contain fst, and therefore strengthener cannot strengthen other
      // if other_fst < fst, then other has to be longer 
      if (other_fst > fst || other_fst < fst && other.size() <= strengthener.size())
          continue;

      // lock the other clause (we already tested it for beeing different from cr)
      // if false is returned: the first variable changed, and no lock was performed
      bool locked = other.spinlock(other_fst_lit);
      if (false == locked)
        goto lock_to_strengthen_nn;

      // check if other has been deleted, while waiting for lock
      if (other.can_be_deleted() || other.size() <= strengthener.size())
      {
         other.unlock();
         continue;
      }

      // check if others first lit has changed, while waiting for lock
      if (var(other[0]) != other_fst)
      {
         other.unlock();
         goto lock_to_strengthen_nn;
      }
      
      // now other is locked
      if (doStatistics) ++stats.subsimpSteps;
      // test if we can strengthen other clause
      si = 0;
      so = 0;
      negated_lit_pos = -1;  //the position for neglit cant be 0, so we will use this as "neglit not found"
      while (si < strengthener.size() && so < other.size())
      {
        if(strengthener[si] == other[so])
        {
          // the lits are the same in both clauses
          ++si;
          ++so;
        }
        else if (strengthener[si] == ~other[so])
        {
          // found neglit...
          if (negated_lit_pos == -1)
          {
            // if it's the first neglit found, save its position
            negated_lit_pos = so;
            ++si;
            ++so;
          }
          else
            break;  // found more than 1 negated lit, can't strengthen
        }
        else if (strengthener[si] < other[so])
          break;    // the other clause did not contain this variable => can't strengthen
        else
          ++so;
      }

      // subsumption 
      if (negated_lit_pos == -1 && si == strengthener.size())
      {
         other.set_delete(true);
         if (doStatistics) 
         {
             if (!other.learnt())
             {
                ++stats.subsumedClauses;
                stats.subsumedLiterals += other.size();
             }
             else
             {
                ++stats.subsumedLearnts;
                stats.subsumedLearntLiterals += other.size();
             }
         }
         // TODO learnt subsumes non-learnt !
      }
      // if subsumption successful, strengthen
      else if (negated_lit_pos >= 0 && si == strengthener.size())
      {
          if (doStatistics && other.learnt()) ++stats.strengthtLearntLits;
          else if (doStatistics) ++stats.strengthtLits;

          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[(negated_lit_pos + 1) % 2]);
              data.removedClause(crO);
              data_lock.unlock();   
              if (l_False == state)
              {
                  other.unlock();
                  return l_False;
              }
          }
          //TODO optimize out
          else if (other.size() == 1)
          {
              assert(false && "no unit clauses should be strengthened");
              // empty -> fail
          }
          else
          {
              if( global_debug_out ) cerr << "c remove " << negated_lit_pos << " from clause " << other << endl;
              Lit neg = other[negated_lit_pos];
              
              //remove from occ-list (by overwriting cr with CRef_Undef
              data.removeClauseFromThreadSafe(crO, neg);
              dirtyOccs.setCurrentStep(toInt(neg));

              other.removePositionSortedThreadSafe(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1);
              data_lock.unlock();
/*              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  subsumeQueue.push_back(crO);
              } 
*/
              // keep track of this clause for further strengthening!
              if( !other.can_strengthen() ) 
              {
                other.set_strengthen(true);
                other.set_subsume(true);
                if (!strength_resolvents)
                    localQueue.push_back(crO);
                else 
                {
                    strength_lock.lock();
                    sharedStrengthQueue.push_back(crO);
                    strength_lock.unlock();
                }
              }          
          }
      }
      // unlock other
      other.unlock();
    }
    return l_Undef;
}

/**
 * strengthening check for the negated literal for parallel non-naive strengthening
 * i.e. the Literal that has to occur negated is already known
 *
 * Preconditions: 
 * fst is locked by this thread
 *
 * Thread-Saftey-Requirements
 * only smaller variables than fst are locked
 * write acces to data only if var_lock[nVars()] is locked
 * no data.list(Lit) are written
 *
 * Postconditions:
 * all locks, that were aquired, are freed
 *
 * @param strengthener 
 * @param list          a list with CRef to check for strengthening
 * @param var_lock      lock for each variable
 *
 */
inline lbool BoundedVariableElimination::strength_check_neg(CoprocessorData & data, vector < CRef > & list, deque<CRef> & sharedStrengthQueue, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Lit min, Var fst, vector < SpinLock > & var_lock, MarkArray & dirtyOccs, ParBVEStats & stats, const bool strength_resolvents, const bool doStatistics) 
{
    int si, so;           // indices used for "can be strengthened"-testing
    int negated_lit_pos;  // index of negative lit, if we find one
 
    SpinLock & data_lock = var_lock[data.nVars()];
    SpinLock & strength_lock = var_lock[data.nVars() + 4]; 
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      const CRef crO = list[j];
      if (CRef_Undef == crO)
          continue;
      Clause& other = ca[crO];
      
      lock_to_strengthen_nn:
      // a clause can not strengthen itself, and a clause can not strengther smaller clauses
      if (other.can_be_deleted() || crO == cr || other.size() < strengthener.size())
        continue;
      Lit other_fst_lit = other[0];
      Var other_fst = var(other[0]);

      // if the other_fst > fst, other cannot contain fst, and therefore strengthener cannot strengthen other
      // if other_fst < fst, then other has to be longer 
      if (other_fst > fst || other_fst < fst && other.size() <= strengthener.size())
          continue;

      // lock the other clause (we already tested it for beeing different from cr)
      // if false is returned: the first literal changed, and no lock was performed
      bool locked = other.spinlock(other_fst_lit);
      if (false == locked)
        goto lock_to_strengthen_nn;

      // check if other has been deleted, while waiting for lock
      if (other.can_be_deleted() || other.size() < strengthener.size())
      {
         other.unlock();
         continue;
      }

      // check if others first lit has changed, while waiting for lock
      if (var(other[0]) != other_fst)
      {
         other.unlock();
         goto lock_to_strengthen_nn;
      }
      
      // now other is locked
 
      if (doStatistics) ++stats.subsimpSteps;
      // test if we can strengthen other clause
      si = 0;
      so = 0;
      negated_lit_pos = -1;  //the position for neglit cant be -1, so we will use this as "neglit not found"
      while (si < strengthener.size() && so < other.size())
      {
        if(strengthener[si] == other[so])
        {
          // the lits are the same in both clauses
          ++si;
          ++so;
        }
        else if (other[so] == ~min)
        {
            negated_lit_pos = so;
            ++si;
            ++so;
        }
        else if (strengthener[si] < other[so])
          break;    // the other clause did not contain this variable => can't strengthen
        else
          ++so;
      }
      // subsumption TODO -> maybe just in pos strength check!
      // but it doesn't harm at this point (just takes longer?)
      if (negated_lit_pos == -1 && si == strengthener.size())
      {
         other.set_delete(true);
         if (doStatistics) 
         {
             if (!other.learnt())
             {
                ++stats.subsumedClauses;
                stats.subsumedLiterals += other.size();
             }
             else
             {
                ++stats.subsumedLearnts;
                stats.subsumedLearntLiterals += other.size();
             }
         }
         // TODO learnt subsumes non-learnt !
      }

      // if subsumption successful, strengthen
      else if (negated_lit_pos >= 0 && si == strengthener.size())
      {
          
          if (doStatistics && other.learnt()) ++stats.strengthtLearntLits;
          else if (doStatistics) ++stats.strengthtLits;

          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[(negated_lit_pos + 1) % 2]);
              data.removedClause(crO);
              data_lock.unlock();   
              if (l_False == state)
              {
                  other.unlock();
                  return l_False;
              }
          }
          //TODO optimize out
          else if (other.size() == 1)
          {
              assert(false && "no unit clauses should be strengthened");
              // empty -> fail
          }
          else
          {
              if( global_debug_out ) cerr << "c remove " << negated_lit_pos << " from clause " << other << endl;
              
              Lit neg = other[negated_lit_pos];
              
              //remove from occ-list (by overwriting cr with CRef_Undef
              data.removeClauseFromThreadSafe( crO, neg );
              dirtyOccs.setCurrentStep(toInt(neg));

              other.removePositionSortedThreadSafe(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1);
              data_lock.unlock();
/*              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  subsumeQueue.push_back( crO );
              } */
              // keep track of this clause for further strengthening!
              if( !other.can_strengthen() ) 
              {
                other.set_strengthen(true);
                if (!strength_resolvents)
                    localQueue.push_back( crO );
                else 
                {
                    strength_lock.lock();
                    sharedStrengthQueue.push_back( crO );
                    strength_lock.unlock();
                }
              }
          }
      }
      // unlock other
      other.unlock();
    }
    return l_Undef;
}

lbool BoundedVariableElimination::par_bve_propagate(CoprocessorData& data, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, MarkArray & dirtyOccs, deque < CRef > & sharedSubsimpQueue)
{
  //processTime = cpuTime() - processTime;
  SpinLock & data_lock = var_lock[data.nVars()];
  SpinLock & strength_lock = var_lock[data.nVars() + 4];
  Solver* solver = data.getSolver();
  // propagate all literals that are on the trail but have not been propagated
  // TODO -> make the trail access threadsafe
  while (data.ok() && data.hasToPropagate())
  {
    data.lock();

    const Lit l = (solver->qhead < solver->trail.size()) ? solver->trail[solver->qhead] : lit_Undef;
    if (lit_Undef == l)
    {
      data.unlock();
      break;
    }
    else
    { 
      ++(solver->qhead);
      data.unlock();
    }
    
    const Var v = var(l);

    // Lock variable
    var_lock[v].lock();
    
    if (global_debug_out) cerr << "c UP propagating " << l << endl;

    data_lock.lock();
    data.log.log(upLevel,"propagate literal",l);
    data_lock.unlock();
    
    // Readlock on CA
    rwlock.readLock();

    // remove positives
    vector<CRef> & positive = data.list(l);
    for( int i = 0 ; i < positive.size(); ++i )
    {
      const CRef cr = positive[i];
      if (CRef_Undef == cr)
        continue;
//      cerr << "cr: " << cr << " Undef " << CRef_Undef << endl;
      Clause & satisfied = ca[cr];
      if ( satisfied.can_be_deleted() ) // only track yet-non-deleted clauses
        continue;

      // lock satisfied
      satisfied.spinlock();

      if( global_debug_out ) cerr << "c UP remove " << satisfied << endl;
      //++removedClauses; // = ca[ positive[i] ].can_be_deleted() ? removedClauses : removedClauses + 1;
      satisfied.set_delete(true);
      satisfied.unlock();
      
      // overwrite CRef in Occ 
      
      dirtyOccs.setCurrentStep(toInt(l));
      positive[i] = CRef_Undef;

      data_lock.lock();
      data.removedClause( cr );
      data_lock.unlock();
      
   }

    const Lit nl = ~l;
    int count = 0;
    vector<CRef> & negative = data.list(nl);

    for( int i = 0 ; i < negative.size(); ++i )
    {
      const CRef cr = negative[i];
      if (CRef_Undef == cr)
          continue;

      Clause& c = ca[cr];
      if (c.can_be_deleted())
          continue;

      c.spinlock();

      // sorted propagation, no!
      if ( !c.can_be_deleted() ) 
      {
        for ( int j = 0; j < c.size(); ++ j ) 
          if ( c[j] == nl ) 
          { 
	        if( global_debug_out ) cerr << "c UP remove " << nl << " from " << c << endl;
                c.removePositionSorted(j);
	        break;
	  }
        count ++;
      }
      // unit propagation
      if ( c.size() == 0 || (c.size() == 1 &&  solver->value( c[0] ) == l_False) ) 
      {
        data.setFailed();   // set state to false
        
        if (global_debug_out) cerr << "c UNSAT by UP" << endl;
        c.set_delete(true); // TODO it seems safer to do this, is it necessary
        c.unlock();

        // End Readlock
        rwlock.readUnlock();
        var_lock[v].unlock();

        //processTime = cpuTime() - processTime;
        return l_False;
      } 
      else if( c.size() == 1 ) 
      {
         if( solver->value( c[0] ) == l_Undef && global_debug_out ) 
             cerr << "c UP enqueue " << c[0] << " with previous value " 
                  << (solver->value( c[0] ) == l_Undef ? "undef" : (solver->value( c[0] ) == l_False ? "unsat" : " sat ") ) << endl;
	 if( solver->value( c[0] ) == l_Undef ) 
         {
           data_lock.lock();
           solver->uncheckedEnqueue(c[0]);
           data_lock.unlock();
         }
      }
      else if (true && ! c.can_strengthen())
      {
         c.set_strengthen(true);
         c.set_subsume(true);
         strength_lock.lock();
         sharedSubsimpQueue.push_back(cr);
         strength_lock.unlock();
      }        
      c.unlock();
      negative[i] = CRef_Undef;
      dirtyOccs.setCurrentStep(toInt(nl));
    }

    // End Readlock
    rwlock.readUnlock();

    // update formula data!
    data_lock.lock();
    data.removedLiteral(nl, count);
    data_lock.unlock();
    //removedLiterals += count;

    var_lock[v].unlock();
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
  
  //processTime = cpuTime() - processTime;
  return data.ok() ? l_Undef : l_False;
}
