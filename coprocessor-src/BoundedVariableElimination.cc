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
static IntOption  opt_learnt_growth   (_cat, "cp3_bve_learnt_growth", "Keep C (x) D, where C or D is learnt, if |C (x) D| <= max(|C|,|D|) + N", 0, IntRange(-1, INT32_MAX));
static IntOption  opt_resolve_learnts (_cat, "cp3_bve_resolve_learnts", "Resolve learnt clauses: 0: off, 1: original with learnts, 2: 1 and learnts with learnts", 0, IntRange(0,2));
static BoolOption opt_unlimited_bve   (_cat, "bve_unlimited",  "perform bve test for Var v, if there are more than 10 + 10 or 15 + 5 Clauses containing v", false);
static BoolOption opt_bve_findGate    (_cat, "bve_gates",  "try to find variable AND gate definition before elimination", false);
static IntOption  opt_bve_heap        (_cat, "cp3_bve_heap"     ,  "0: minimum heap, 1: maximum heap, 2: random", 0, IntRange(0,2));

BoundedVariableElimination::BoundedVariableElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation, Coprocessor::Subsumption & _subsumption )
: Technique( _ca, _controller )
, propagation( _propagation)
, subsumption( _subsumption)
, heap_option(opt_unlimited_bve)
, removedClauses(0)
, removedLiterals(0)
, removedLearnts(0)
, learntLits(0)
, createdClauses(0)
, createdLiterals(0)
, newLearnts(0)
, newLearntLits(0)
, testedVars(0)
, anticipations(0)
, eliminatedVars(0)
, removedBC(0)
, blockedLits(0)
, removedBlockedLearnt(0)
, learntBlockedLit(0)
, skippedVars(0)
, unitsEnqueued(0)
, foundGates(0)
, usedGates(0)
, processTime(0)
, subsimpTime(0)
, gateTime(0)
//, heap_comp(NULL)
//, variable_heap(heap_comp)
{
}

void BoundedVariableElimination::printStatistics(ostream& stream)
{
    stream << "c [STAT] BVE(1) " << processTime     << " s, " 
                                 << subsimpTime     << " s spent on subsimp, "
                                 << testedVars       << " vars tested, "
                                 << anticipations    << " anticipations, "  //  = tested vars?
                                 << skippedVars      << " vars skipped "
			       << endl;
    stream << "c [STAT] BVE(2) " << removedClauses  <<  " rem cls, " 
                   << "with "    << removedLiterals << " lits, "
                                 << removedLearnts  << " learnts rem, "
                   << "with "    << learntLits      << " lits, "
                                 << createdClauses  << " new cls, "
                   << "with "    << createdLiterals << " lits, "
                                 << newLearnts      << " new learnts, "
                   << "with "    << newLearntLits   << " lits, " 
			       << endl;
    stream << "c [STAT] BVE(3) " << eliminatedVars   << " vars eliminated, "
                                 << unitsEnqueued    << " units enqueued, "
                                 << removedBC        << " BC removed, "
                   << "with "    << blockedLits      << " lits, "
                                 << removedBlockedLearnt << " blocked learnts removed, "
                   << "with "    << learntBlockedLit << " lits, "
                                 << endl;  
    stream << "c [STAT] BVE(4) " << foundGates << " gateDefs, "
				  << usedGates << " usedGates, "
                                 << gateTime << " gateSeconds, " 
				  << endl;
}


bool BoundedVariableElimination::hasToEliminate()
{
  return false; //variable_heap.size() > 0; 
}

lbool BoundedVariableElimination::fullBVE(Coprocessor::CoprocessorData& data)
{
  return l_Undef;
}
 
lbool BoundedVariableElimination::runBVE(CoprocessorData& data, const bool doStatistics)
{
  if (doStatistics)
  {
    processTime = cpuTime() - processTime;   
  }
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
  //Propagation (TODO Why does omitting the propagation
  // and no PureLit Propagation cause wrong model extension?)
  if (propagation.propagate(data, true) == l_False)
      return l_False;
  
  if( false ) {
   cerr << "formula after propagation: " << endl;
   for( int i = 0 ; i < data.getClauses().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
   for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
  }

  if( false ) {
   cerr << "96 lists before bve: " << endl;
   for( int i = 0 ; i < data.list(mkLit(95,false)).size(); ++ i )
     if( !ca[  data.list(mkLit(95,false))[i] ].can_be_deleted() ) cerr << ca[  data.list(mkLit(95,false))[i] ] << endl;
   for( int i = 0 ; i < data.list(mkLit(95,true)).size(); ++ i )
     if( !ca[  data.list(mkLit(95,true))[i] ].can_be_deleted() ) cerr << ca[  data.list(mkLit(95,true))[i] ] << endl;    
  }
  
  data.ma.resize( data.nVars() * 2 );

  bve_worker(data, newheap, 0, variable_queue.size(), false);
  if (opt_bve_heap != 2)
  {
    newheap.clear();
  }
  else 
  {
    variable_queue.clear();
  }

  if (doStatistics)    processTime = cpuTime() - processTime;   
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
void BoundedVariableElimination::par_bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, Heap<NeighborLt> & neighbor_heap, vector< SpinLock > & var_lock, ReadersWriterLock & rwlock, const bool force, const bool doStatistics)   
{
    SpinLock & data_lock = var_lock[data.nVars()]; 
    SpinLock & heap_lock = var_lock[data.nVars() + 1];
    vector < Var > neighbors;
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

        timeStamp = lastTouched.getIndex(v); // get last modification of v
        rwlock.readUnlock();

        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  End: read locked part 
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
        
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
        
        if (!force) 
        {
            for (int i = 0; i < pos.size(); ++i)
                 pos_stats[i] = 0;
            for (int i = 0; i < neg.size(); ++i)
                 neg_stats[i] = 0;

            // anticipate only, if there are positiv and negative occurrences of var 
            if (pos_count != 0 &&  neg_count != 0)
                if (anticipateEliminationThreadsafe(data, pos, neg,  v, pos_stats, neg_stats, lit_clauses, lit_learnts, data_lock) == l_False) 
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
        if (force || lit_clauses <= lit_clauses_old)
        {
             
        ///////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Begin: write locked part 
        //
        //  Until the read lock was released, no clauses were added to list(v) or list(~v).
        //  -> TODO What could have changed? (especially with strengthening and subsumption)
        //
        ///////////////////////////////////////////////////////////////////////////////////////////
       
             rwlock.writeLock(); 
             if (!data.ok())
             {
                 // UNSAT case -> end thread, but first release all locks
                 rwlock.writeUnlock();
                 // free all locks on Vars in descending order 
                 for (int i = neighbors.size() - 1; i >= 0; --i)
                 {
                     var_lock[neighbors[i]].unlock();
                 }
                 return; 
             }
             
             if(opt_verbose > 1)  cerr << "c resolveSet" <<endl;

             if (resolveSetThreadSafe(data, pos, neg, v) == l_False) 
             {
                 // UNSAT case -> end thread, but first release all locks
                 rwlock.writeUnlock();
                 // free all locks on Vars in descending order 
                 for (int i = neighbors.size() - 1; i >= 0; --i)
                 {
                     var_lock[neighbors[i]].unlock();
                 }
                 return; 
             }

             // remove Clauses that were resolved
             // (since write lock, no threadsafe implementation needed)
             removeClauses(data, pos, mkLit(v,false));
             removeClauses(data, neg, mkLit(v,true));
             if (opt_verbose > 0) cerr << "c Resolved " << v+1 <<endl;
 
             // touch variables:
             lastTouched.nextStep();
             for (int i = 0; i < neighbors.size(); ++i)
             {
                lastTouched.setCurrentStep(neighbors[i]); 
             }

             //subsumption with new clauses!!
             //TODO implement a threadsafe variant for this
             // -> subsumption as usual
             // -> strengthening without local queue -> just tagging
             //subsumption.subsumeStrength(data); 

             rwlock.writeUnlock();
        
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


//expects filled variable processing queue
//
// force -> forces resolution
//
//
void BoundedVariableElimination::bve_worker (CoprocessorData& data, Heap<VarOrderBVEHeapLt> & heap, unsigned int start, unsigned int end, const bool force, const bool doStatistics)   
{
    vector<Var> touched_variables;
    while ((opt_bve_heap != 2 && heap.size() > 0) || (opt_bve_heap == 2 && variable_queue.size() > 0))
    {
        //Subsumption / Strengthening
        if (doStatistics) subsimpTime = cpuTime() - subsimpTime;  
        subsumption.subsumeStrength(data); 
        if (doStatistics) subsimpTime = cpuTime() - subsimpTime;  
       
        if (!data.ok())
            return;
        
        if( false ) {
            cerr << "formula after subsumeStrength: " << endl;
            for( int i = 0 ; i < data.getClauses().size(); ++ i )
                if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
            for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
                if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
        }

        updateDeleteTime(data.getMyDeleteTimer());
        //for (unsigned i = start; i < end; i++)
        while ((opt_bve_heap != 2 && heap.size() > 0) || (opt_bve_heap == 2 && variable_queue.size() > 0))
        {
           Var v = var_Undef;
           if (opt_bve_heap != 2)
             v = heap.removeMin();
           else 
           {
                int rand = data.getSolver()->irand(data.getSolver()->random_seed, variable_queue.size());
                v = variable_queue[rand];
                if (variable_queue.size() > 1)
                {
                    variable_queue[rand] = variable_queue[variable_queue.size() - 2];
                }
                variable_queue.pop_back();
           }
           assert (v != var_Undef && "variable heap or queue failed");

           // do not work on this variable, if it will be unit-propagated! if all units are eagerly propagated, this is not necessary
           if  (data.value(mkLit(v,true)) != l_Undef || data.value(mkLit(v,false)) != l_Undef)
               continue;
           
           // Heuristic Cutoff Gate-Search
           if (!opt_unlimited_bve && false) //TODO make up a heuristic
           {
               if (doStatistics) ++skippedVars;
               continue;
           }

           // Search for Gates
           int p_limit = data.list(mkLit(v,false)).size();
	   int n_limit = data.list(mkLit(v,true)).size();
	   bool foundGate = false;
	   if( opt_bve_findGate ) {
	    foundGate = findGates(data, v, p_limit, n_limit);
	    foundGates ++;
	   }
            
           // Heuristic Cutoff Anticipation (if no Gate Found)
           if (!opt_unlimited_bve && !foundGate && (data[mkLit(v,true)] > 10 && data[mkLit(v,false)] > 10 || data[v] > 15 && (data[mkLit(v,true)] > 5 || data[mkLit(v,false)] > 5)))
           {
               if (doStatistics) ++skippedVars;
               continue;
           }
            
           if (doStatistics) ++testedVars;

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
           
           // Declare stats variables;        
           int32_t pos_stats[pos.size()];
           int32_t neg_stats[neg.size()];
           int lit_clauses;
           int lit_learnts;
          
                  
           if (!force) 
           {
               // TODO memset here!
               for (int i = 0; i < pos.size(); ++i)
                    pos_stats[i] = 0;
               for (int i = 0; i < neg.size(); ++i)
                    neg_stats[i] = 0;

               // anticipate only, if there are positiv and negative occurrences of var 
               if (pos_count != 0 &&  neg_count != 0)
               {
                   if (doStatistics) ++anticipations;
                   if (anticipateElimination(data, pos, neg,  v, p_limit, n_limit, pos_stats, neg_stats, lit_clauses, lit_learnts) == l_False) 
                       return;  // level 0 conflict found while anticipation TODO ABORT
               }

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
		usedGates = (foundGate ? usedGates + 1 : usedGates ); // statistics
                if(opt_verbose > 1)  cerr << "c resolveSet" <<endl;
                if (resolveSet(data, pos, neg, v, p_limit, n_limit) == l_False)
                    return;
                if (doStatistics) ++eliminatedVars;
                removeClauses(data, pos, mkLit(v,false));
                removeClauses(data, neg, mkLit(v,true));
               
                if (opt_verbose > 0) cerr << "c Resolved " << v+1 <<endl;
                //subsumption with new clauses!!
                if (doStatistics) subsimpTime = cpuTime() - subsimpTime;  
                subsumption.subsumeStrength(data);
                if (doStatistics) subsimpTime = cpuTime() - subsimpTime;  

                if (!data.ok())
                    return;
           }

           if(opt_verbose > 1)   cerr << "c =============================================================================" << endl;
          
        }

        // add active variables and clauses to variable heap and subsumption queues
        data.getActiveVariables(lastDeleteTime(), touched_variables);
        touchedVarsForSubsumption(data, touched_variables);
        if (opt_bve_heap != 2)
            heap.clear();
        else
            variable_queue.clear();
        for (int i = 0; i < touched_variables.size(); ++i)
        {
            if (opt_bve_heap != 2)
                heap.insert(touched_variables[i]);
            else 
                variable_queue.push_back(touched_variables[i]);
        }
        touched_variables.clear();
    }
}

/*
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 */
inline void BoundedVariableElimination::removeClauses(CoprocessorData & data, const vector<CRef> & list, const Lit l, const bool doStatistics)
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
            if (doStatistics)
            {
                ++removedClauses;
                removedLiterals += c.size();
            }
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
inline lbool BoundedVariableElimination::anticipateElimination(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const int p_limit, const int  n_limit, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts, const bool doStatistics)
{
    if(opt_verbose > 2)  cerr << "c starting anticipate BVE" << endl;
    // Clean the stats
    lit_clauses=0;
    lit_learnts=0;
    const bool hasDefinition = (p_limit < positive.size() || n_limit < negative.size() );
    
    for (int cr_p = 0; cr_p < positive.size() ; ++cr_p)
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
	    // do not check resolvents, which would touch two clauses out of the variable definition
	    if( cr_p >= p_limit && cr_n >= n_limit ) continue; 
	    if( hasDefinition && cr_p < p_limit && cr_n < n_limit ) continue; // no need to resolve the definition clauses with each other NOTE: assumes that these clauses result in tautologies
	    
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
                    //assert(false && "all units should be discovered before (while strengthening)!");
                    if (doStatistics) ++ unitsEnqueued;
                    if (propagation.propagate(data, true) == l_False)
                        return l_False;  
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
 *  anticipates following numbers:
 *  -> number of resolvents derived from specific clause:       pos_stats / neg_stats
 *  -> total number of literals in clauses after resolution:    lit_clauses
 *  -> total number of literals in learnts after resolution:    lit_learnts
 *
 */
inline lbool BoundedVariableElimination::anticipateEliminationThreadsafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, int32_t* pos_stats , int32_t* neg_stats, int & lit_clauses, int & lit_learnts, SpinLock & data_lock)
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
                    cerr << "c    Clause P: ";
                    printClause(p); 
                    cerr  << "c     Clause N: ";
                    printClause(n);               
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
lbool BoundedVariableElimination::resolveSet(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const int p_limit, const int n_limit, const bool keepLearntResolvents, const bool force, const bool doStatistics)
{
    vec<Lit> ps; // TODO: make this a member variable!!
    const bool hasDefinition = (p_limit < positive.size() || n_limit < negative.size() );
    
  
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[positive[cr_p]];
        if (p.can_be_deleted())
            continue;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
	    // no need to resolve two clauses that are both not within the variable definition
	    if( cr_p >= p_limit && cr_n >= n_limit ) continue;
	    if( hasDefinition && cr_p < p_limit && cr_n < n_limit ) continue; // no need to resolve the definition clauses with each other NOTE: assumes that these clauses result in tautologies
	    
	    
            Clause & p = ca[positive[cr_p]]; // renew reference as it could got invalid while clause allocation
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
            ps.clear();
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
                    // IMPORTANT! dont use p and n in this block, as they could got invalid
                    Clause & resolvent = ca[cr];
                    data.addClause(cr);
                    if (resolvent.learnt()) 
                        data.getLEarnts().push(cr);
                    else 
                        data.getClauses().push(cr);
                    // push Clause on subsumption-queue
                    subsumption.initClause(cr);

                    if (doStatistics)
                    {
                        if (resolvent.learnt())
                        {
                            ++newLearnts;
                            ++newLearntLits;

                        } else 
                        {
                            ++createdClauses;
                            createdLiterals += resolvent.size();
                        }
                    }
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
                        //assert(false && "all units should be discovered before (while strengthening)!");
                        lbool status = data.enqueue(ps[0]); //check for level 0 conflict
                        if (status == l_False)
                            return l_False;
                        else if (status == l_Undef)
                             ; // variable already assigned
                        else if (status == l_True)
                        {
                            if (doStatistics) ++ unitsEnqueued;
                            if (propagation.propagate(data, true) == l_False)  //TODO propagate own lits only (parallel)
                                return l_False;
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
lbool BoundedVariableElimination::resolveSetThreadSafe(CoprocessorData & data, vector<CRef> & positive, vector<CRef> & negative, const int v, const bool keepLearntResolvents, const bool force)
{
    for (int cr_p = 0; cr_p < positive.size(); ++cr_p)
    {
        Clause & p = ca[positive[cr_p]];
        if (p.can_be_deleted())
            continue;
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n)
        {
            Clause & p = ca[positive[cr_p]]; // renew reference as it could got invalid while clause allocation
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
                    // IMPORTANT! dont use p and n in this block, as they could got invalid
                    Clause & resolvent = ca[cr];
                    data.addClause(cr);
                    if (resolvent.learnt()) 
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
                            /*propagation.propagate(data, true);  //TODO propagate own lits only (parallel)
                            if (p.can_be_deleted())
                                break;*/
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
inline void BoundedVariableElimination::removeBlockedClauses(CoprocessorData & data, const vector< CRef> & list, const int32_t stats[], const Lit l , const bool doStatistics)
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
            if (doStatistics)
            {
                if (c.learnt())
                {
                    ++removedBlockedLearnt;
                    learntBlockedLit += c.size();
                }
                else
                {
                    ++removedBC; 
                    blockedLits += c.size();
                }
            }
            if(opt_verbose > 1 || (opt_verbose > 0 && ! c.learnt())) 
            {
                cerr << "c removed clause: " << ca[list[ci]] << endl;
                cerr << "c added to extension with Lit " << l << endl;;
            } 
        }
   }
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

inline bool BoundedVariableElimination::findGates(CoprocessorData & data, const Var v, int & p_limit, int & n_limit, MarkArray * helper)
{
  // do not touch lists that are too small for benefit
  if( data.list(mkLit(v,false)).size() < 3 &&  data.list( mkLit(v,true) ).size() < 3 ) return false;
  if( data.list(mkLit(v,false)).size() < 2 || data.list( mkLit(v,true) ).size() < 2 ) return false;
 
  MethodTimer gateTimer ( &gateTime ); // measure time spend in this method
  MarkArray& markArray = (helper == 0 ? data.ma : *helper);
  
  for( uint32_t pn = 0 ; pn < 2; ++ pn ) {
    vector<CRef>& pList = data.list(mkLit(v, pn == 0 ? false : true ));
    vector<CRef>& nList = data.list(mkLit(v, pn == 0 ? true : false ));
    const Lit pLit = mkLit(v,pn == 0 ? false : true);
    const Lit nLit = mkLit(v,pn == 0 ? true : false);
    int& pClauses = pn == 0 ? p_limit : n_limit;
    int& nClauses = pn == 0 ? n_limit : p_limit;
    
    // check binary of pos variable
    markArray.nextStep();
    for( uint32_t i = 0 ; i < pList.size(); ++ i ) {
      const Clause& clause = ca[ pList[i] ];
      if( clause.can_be_deleted() || clause.learnt() || clause.size() != 2 ) continue; // NOTE: do not use learned clauses for gate detection!
      Lit other = clause[0] == pLit ? clause[1] : clause[0];
      markArray.setCurrentStep( toInt(~other) );
    }
    for( uint32_t i = 0 ; i < nList.size(); ++ i ) {
      const Clause& clause = ca[ nList[i] ];
      if( clause.can_be_deleted() || clause.learnt() ) continue;
      uint32_t j = 0; for(  ; j < clause.size(); ++ j ) {
	const Lit cLit = clause[j];
	if( cLit == nLit ) continue; // do not consider variable that is to eliminate
	if( !markArray.isCurrentStep( toInt(cLit) ) ) break;
      }
      if( j == clause.size() ) {
	assert( !clause.can_be_deleted() && "a participating clause of the gate cannot be learned, because learned clauses will be removed completely during BVE");
	if( opt_verbose > 0 ) {cerr << "c [BVE] found " << (pn == 0 ? "pos" : "neg") << " gate with size " << j << " p: " << pList.size() << " n:" << nList.size() << " :=" << clause << endl;}
	// setup values
	pClauses = clause.size() - 1;
	nClauses = 1;
	 // do not add unnecessary clauses
	for( uint32_t k = 0 ; k < clause.size(); ++k ) markArray.reset( toInt(clause[k]) );
	CRef tmp = nList[0]; nList[0] = nList[i]; nList[i] = tmp; // swap responsible clause in list to front
	// swap responsible clauses in list to front
	uint32_t placedClauses = 0;
	for( uint32_t k = 0 ; k < pList.size(); ++ k ) {
	  const Clause& clause = ca[ pList[k] ];
	  if( clause.learnt() || clause.can_be_deleted() || clause.size() != 2 ) continue;
	  Lit other = clause[0] == pLit ? clause[1] : clause[0];
	  if(  !markArray.isCurrentStep ( toInt(~other) ) ) {
	    CRef tmp = pList[placedClauses];
	    pList[placedClauses++] = pList[k];
	    pList[k] = tmp;
	    markArray.setCurrentStep( toInt(~other) ); // no need to add the same binary twice
	  }
	}
	if( opt_verbose > 0 ) {
	  cerr << "c [BVE] GATE clause: " << ca[ nList[0] ] << " placed clauses: " << placedClauses << endl;
	  for( uint32_t k = 0 ; k < placedClauses; ++ k ) {
	    cerr << "c [BVE] bin clause[" << k << "]: "<< ca[ pList[k] ] << endl;
	  }
	  cerr << "c return parameter: pos:" << p_limit << " neg: " << n_limit << endl;
	  
	  cerr << "c clause lists: " << endl;
	  cerr << "for " << mkLit(v,false) << endl;
	  for( int i = 0 ; i < data.list( mkLit(v,false) ).size(); ++ i ) {
	    if( ca[  data.list( mkLit(v,false) )[i] ].can_be_deleted() ) continue;
	    else cerr << i << " : " << ca[  data.list( mkLit(v,false) )[i] ] << endl;
	  }
	  cerr << "for " << mkLit(v,true) << endl;
	  for( int i = 0 ; i < data.list( mkLit(v,true) ).size(); ++ i ) {
	    if( ca[  data.list( mkLit(v,true) )[i] ].can_be_deleted() ) continue;
	    else cerr << i << " : " << ca[  data.list( mkLit(v,true) )[i] ] << endl;
	  }
	}
	
	if( pClauses != placedClauses ) cerr << cerr << "c [BVE-G] placed: " << placedClauses << ", participating: " << pClauses << endl;
	assert( pClauses == placedClauses && "number of moved binary clauses and number of participating clauses has to be the same");
	return true;
      }
    }
  }

  return false;
}
