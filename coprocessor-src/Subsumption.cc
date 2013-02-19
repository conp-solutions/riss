/**********************************************************************************[Subsumption.cc]
Copyright (c) 2012, Kilian Gebhardt, Norbert Manthey, Max Löwen, All rights reserved.
**************************************************************************************************/
/*
 *
 * Global assumptions concerning occurrence-lists and occurrence-stats
 * -> the sequentiell algorithms update this on their own
 * -> the parallel algorithms fill a vector < OccUpdate >, 
 *    which updates are sequencially performed
 *    -> if some operations occur multiple times, 
 *       the first is performed and the others are ignored
 *       TODO guarantee this !
 *       TODO propagate seems to allow this approach, since it clears Occ-Lists and 
 *            updates Stats, but are all it's operations consistent?
 * 
 * TODO -> change time measure function to wall clock time!
 */



#include "coprocessor-src/Subsumption.h"
using namespace Coprocessor;

static const char* _cat = "CP3 SUBSUMPTION";
// options
static BoolOption  opt_naivStrength    (_cat, "naive_strength", "use naive strengthening", false);
static BoolOption  opt_par_strength    (_cat, "cp3_par_strength", "force par strengthening (if threads exist)", false);
static BoolOption  opt_lock_stats      (_cat, "cp3_lock_stats", "measure time waiting in spin locks", false);
static BoolOption  opt_par_subs        (_cat, "cp3_par_subs", "force par subsumption (if threads exist)", false);
static IntOption  opt_par_subs_counts  (_cat, "par_subs_counts" ,  "Updates of counts in par-subs 0: compare_xchange, 1: CRef-vector", 1, IntRange(0,1));
static BoolOption  opt_allStrengthRes  (_cat, "all_strength_res", "Create all self-subsuming resolvents (prob. slow & blowup, only seq)", false); 
Subsumption::Subsumption( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation )
: Technique( _ca, _controller )
, data(_data)
, propagation( _propagation )
, subsumedClauses(0)
, subsumedLiterals(0)
, removedLiterals(0)
, subsumeSteps(0)
, strengthSteps(0)
, processTime(0)
, strengthTime(0)
{
}

void Subsumption::printStatistics(ostream& stream)
{
stream << "c [STAT] SuSi(1) " << processTime << " s, " 
                              << subsumedClauses << " cls, " 
			       << " with " << subsumedLiterals << " lits, "
			       << removedLiterals << " strengthed "
			       << endl;
stream << "c [STAT] SuSi(2) " << subsumeSteps << " subs-steps, " 
                              << strengthSteps << " strenght-steps, " 
			       << strengthTime << "s strengthening "
			       << endl;
    for (int i = 0; i < controller.size(); ++i)
    {
        stream << "c [STAT] SuSi(1)-Thread " << i << " "
                           << localStats[i].processTime << " s, " 
                                      << localStats[i].subsumedClauses << " cls, " 
                           << " with " << localStats[i].subsumedLiterals << " lits, "
                           << localStats[i].removedLiterals << " strengthed "
                           << endl;
        stream << "c [STAT] SuSi(2)-Thread " << i << " " 
                           << localStats[i].subsumeSteps << " subs-steps, " 
                                      << localStats[i].strengthSteps << " strenght-steps, " 
                           << localStats[i].strengthTime << "s strengthening "
                           << localStats[i].lockTime << "s locking "
                           << endl;
    }
}

void Subsumption::resetStatistics()
{
    if (localStats.size() < controller.size()) 
        localStats.resize(controller.size());
    for (int i = 0; i < controller.size(); ++i)
    {
        localStats[i].removedLiterals = 0;
        localStats[i].strengthSteps = 0;
        localStats[i].subsumedClauses = 0;
        localStats[i].subsumedLiterals = 0;
        localStats[i].subsumeSteps = 0;
        localStats[i].processTime = 0;
        localStats[i].strengthTime = 0;
        localStats[i].lockTime = 0;
    }
    

}

void Subsumption::subsumeStrength(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
  while( data.ok() && (hasToSubsume() || hasToStrengthen() ))
  {
    if( hasToSubsume() ){
      fullSubsumption(heap, doStatistics);
      // clear queue afterwards
      data.getSubsumeClauses().clear();
    }
    if( hasToStrengthen() ) {
      if ((opt_par_strength || data.getStrengthClauses().size() > 150000) && controller.size() > 0)
      {
          parallelStrengthening(heap, doStatistics);
          data.correctCounters(); //TODO correct occurrences as well
      }
      else {
          fullStrengthening(heap, doStatistics); // corrects occs and counters by itself
          if (opt_allStrengthRes)
          {
            for (int j = 0; j < toDelete.size(); ++j)
            {  
              Clause & c = ca[toDelete[j]]; 
              if (!c.can_be_deleted())
              {
                  c.set_delete(true);
                  data.removedClause(toDelete[j], heap);
                  if (doStatistics)
                  {
                      removedLiterals += c.size();
                  }
              }
            } 
            data.getStrengthClauses().swap(newClauses);
            newClauses.clear();
          }
      }
      // clear queue afterwards
      if (!opt_allStrengthRes) data.getStrengthClauses().clear();
    }
  }
}



bool Subsumption::hasToSubsume() const
{
  return data.getSubsumeClauses().size() > 0; 
}

lbool Subsumption::fullSubsumption(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
  // run subsumption for the whole queue
  if( heap == 0 && controller.size() > 0 && (opt_par_subs || data.getSubsumeClauses().size() > 100000 || ( data.getSubsumeClauses().size() > 50000 && 10*data.nCls() > 22*data.nVars() ) ) ) {
    parallelSubsumption(doStatistics); // use parallel, is some conditions have been met
    //data.correctCounters();    // 
  } else {
    subsumption_worker(0,data.getSubsumeClauses().size(), heap, doStatistics); // performs all occ and stat-updates
  }
  data.getSubsumeClauses().clear();
  // no result to tell to the outside
  return l_Undef; 
}

bool Subsumption::hasWork() const
{
  return hasToStrengthen() || hasToSubsume();
}


void Subsumption :: subsumption_worker ( unsigned int start, unsigned int end, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
    vector < CRef > occ_updates; // TODO transform this into a member / thread-attribute member
    if (doStatistics)
    {
        processTime = cpuTime() - processTime;   
    }   
    if(global_debug_out) cerr << "subsume from " << start << " to " << end << " with size " << data.getSubsumeClauses().size() << endl;
    for (; end > start;)
    {
        --end;
	const CRef cr = data.getSubsumeClauses()[end];
        Clause &c = ca[cr];
        if (c.can_be_deleted())
            continue;
        //find Lit with least occurrences
        Lit min = c[0];
        for (int l = 1; l < c.size(); l++)
        {
            if (data[c[l]] < data[min])
               min = c[l]; 
        }
        vector<CRef> & list = data.list(min);
        for (unsigned i = 0; i < list.size(); ++i)
        {
	  
            if (list[i] == cr) {
                continue;
	        } else if (ca[list[i]].size() < c.size()) {
                continue;
	        } else if (ca[list[i]].can_be_deleted()) {
                continue;
	        } else if (c.ordered_subsumes(ca[list[i]])) {
                if ( doStatistics ) ++ subsumeSteps;
                if (doStatistics)
                {
                    ++subsumedClauses;
                    subsumedLiterals += ca[list[i]].size();
                }
                ca[list[i]].set_delete(true); 
                occ_updates.push_back(list[i]);
		        if( global_debug_out ) cerr << "c clause " << ca[list[i]] << " is deleted by " << c << endl;
                if (!ca[list[i]].learnt() && c.learnt())
                {
                    c.set_learnt(false);
                }
            } else
                if (doStatistics) ++subsumeSteps;
        }
        //set can_subsume to false
        c.set_subsume(false);
    }  
    if (doStatistics)
    {
        processTime = cpuTime() - processTime;   
    } 
    // Update Stats and remove Clause from all Occurrence-Lists
    for (int i = 0; i < occ_updates.size(); ++i)
    {
        CRef cr = occ_updates[i];
        Clause & c = ca[cr];
        data.removedClause(cr, heap);
        for (int l = 0; l < c.size(); ++l)
        {
            data.removeClauseFrom(cr,c[l]);
        }
    }
}

/**
 * Threadsafe copy of subsumption_worker:
 * Fixes the following race conditions:
 *  - Two equivalent clauses (duplicates) could subsume mutual, 
 *    if subsumption is performed on them in parallel.
 *    Therefore the clause with the smaller CRef will be kept.
 *
 *  - Let A, B be learnt clauses, C non-learnt clause.
 *    If A subsumes B, and in parallel B subsumes C, 
 *      A stays learnt clause since B is learnt, 
 *      B becomes deleted by A, but non-learnt by C
 *      C becomes deleted by B. 
 *    Since C is deleted, A will ommit the subsumption-check. 
 *    Afterwards just A as learnt clause will be kept, and the
 *    non-learnt information of C got lost. 
 *
 *    To solve this problem, the deletion and learnt flags are 
 *    not updated in time, but Clauses are remembered in two vectors.
 *    
 *    Now A will check C for subsumption, and inherit the non-learnt flag.
 *
 * @param start          start index of work-queue 
 * @param stop           stop index (+1) of work-queue, i.e. this index is not included
 * @param to_delete      this clauses, if not deleted, need to be set deleted afterwards
 * @param set_non_learnt this clauses, if not deleted, need to be set non-learnt afterwards 
 * @param stats          local stats
 */
void Subsumption :: par_subsumption_worker ( unsigned int start, unsigned int end, vector<CRef> & to_delete, vector< CRef > & set_non_learnt, struct SubsumeStatsData & stats, const bool doStatistics)
{
    if (doStatistics)
    {
        stats.processTime = wallClockTime() - stats.processTime;   
    }
    while (end > start)
    {
        --end;
        const CRef cr = data.getSubsumeClauses()[end];
        Clause &c = ca[cr];
        if (c.can_be_deleted())
            continue;
        bool learnt_subsumed_non_learnt = false;
        //find Lit with least occurrences
        Lit min = c[0];
        for (int l = 1; l < c.size(); l++)
        {
            if (data[c[l]] < data[min])
               min = c[l]; 
        }
        vector<CRef> & list = data.list(min);
        for (unsigned i = 0; i < list.size(); ++i)
        {
	  
            if (list[i] == cr) {
                continue;
	        } else if (ca[list[i]].size() < c.size()) {
                continue;
	        } else if (ca[list[i]].can_be_deleted()) {
                continue;
	        } else if (c.ordered_subsumes(ca[list[i]])) {
                if (doStatistics) ++stats.subsumeSteps;
                // save at least one duplicate, by deleting the clause with smaller CRef
                if (c.size() == ca[list[i]].size() && cr > list[i])
                {
                    // save the non-learnt information
                    if (!c.learnt() && ca[list[i]].learnt())
                    {
                        to_delete.push_back(cr);
                        set_non_learnt.push_back(list[i]);
                    } 
                    else
                    { 
                        if (opt_par_subs_counts == 0)
                        {
                            bool removed = data.removeClauseThreadSafe(cr);
                            if (doStatistics && removed)
                            {
                                ++stats.subsumedClauses;
                                stats.subsumedLiterals += c.size();
                            }
                        }
                        else 
                        {
                            to_delete.push_back(cr);
                        }
                    }
                    continue;
                }
                // save the non-learnt information
                if (c.learnt() && !ca[list[i]].learnt())
                {
                    learnt_subsumed_non_learnt = true;
                    to_delete.push_back(list[i]);
                    continue;
                }
                if (opt_par_subs_counts == 0)
                {
                    bool removed = data.removeClauseThreadSafe(list[i]);
                    if (doStatistics && removed)
                    {
                        ++stats.subsumedClauses;
                        stats.subsumedLiterals += ca[list[i]].size();
                    }  
                }
                else
                {
                    to_delete.push_back(list[i]);
                }
            } else if (doStatistics)       
                    ++stats.subsumeSteps;
        }
        //set can_subsume to false
        c.set_subsume(false);
        
        // add Clause to non-learnt-queue
        if (learnt_subsumed_non_learnt)
            set_non_learnt.push_back(cr);
    }
    if (doStatistics) 
        stats.processTime = wallClockTime() - stats.processTime;
}


/**
 * locking based strengthening, based on naive subsumption check with negated literal
 * 
 * @param var_lock vector of locks for each variable
 *
 */
void Subsumption::par_strengthening_worker( unsigned int start, unsigned int stop, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, vector< OccUpdate > & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics) 
{
    assert(start <= stop && stop <= data.getStrengthClauses().size() && "invalid indices");
    assert(data.nVars() <= var_lock.size() && "var_lock vector to small");
    double & lock_time = stats.lockTime;
    if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
    
    deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
    SpinLock & data_lock = var_lock[data.nVars()];
    while (stop > start && data.ok())
    {    
        CRef cr = CRef_Undef;
        if( localQueue.size() == 0 ) {
            --stop;
            cr = data.getStrengthClauses()[stop];
        } else {
            // TODO: have a counter for this situation!
            cr = localQueue.back();
            localQueue.pop_back();
        }
 
        Clause & c = ca[cr];//data.getStrengthClauses()[stop]];
        lock_strengthener:
        if (c.can_be_deleted() || c.size() == 0)
            continue;
        Var fst = var(c[0]);
        
        // lock 1st var
        if (opt_lock_stats) lock_time = cpuTime() - lock_time;
        var_lock[fst].lock();
        if (opt_lock_stats) lock_time = cpuTime() - lock_time;

        // assure that clause can still strengthen
        if (c.can_be_deleted() || c.size() == 0)
        {
            var_lock[fst].unlock();
            continue;
        }
        
        // assure that first var still valid
        if (var(c[0]) != fst)
        {
            var_lock[fst].unlock();
            goto lock_strengthener;
        }
        
        // search lit with minimal occurrences
        Lit min = c[0];
        for (int j = 1; j < c.size(); ++j)
            if (data[min] * (c.size() - 1) + data[~min] > data[c[j]] * (c.size() -1) + data[~c[j]])
                min = c[j];


        for (int l = 0; l < c.size(); ++l)
        {
            Lit neg = ~(c[l]);
            c[l] = neg;
            //use minimal list, or the negated list if  min == c[l] 
            vector< CRef> & list = (neg == ~min) ? data.list(neg) : data.list(min);
            // vector< CRef> & list = data.list(neg);
            for (int l_cr = 0; l_cr < list.size(); ++l_cr)
            {
                if (list[l_cr] == cr)
                    continue;
                assert(list[l_cr] != cr && "expect no tautologies here");
                Clause & d = ca[list[l_cr]];
                lock_to_strengthen:
                if (d.can_be_deleted() || d.size() == 0)
                    continue;
                Var d_fst = var(d[0]);

                // if the d_fst > fst, d cannot contain fst, and therefore c cannot strengthen d
                if (d_fst > fst)
                    continue;

                // check if d_fst already locked by this thread, if not: lock
                if (d_fst != fst)
                {
                   if (opt_lock_stats) lock_time = cpuTime() - lock_time;
                   var_lock[d_fst].lock();
                   if (opt_lock_stats) lock_time = cpuTime() - lock_time;

                   // check if d has been deleted, while waiting for lock
                   if (d.can_be_deleted() || d.size() == 0)
                   {
                       var_lock[d_fst].unlock();
                       continue;
                   }

                   // check if d's first lit has changed, while waiting for lock
                   if (var(d[0]) != d_fst)
                   {
                       var_lock[d_fst].unlock();
                       goto lock_to_strengthen;
                   }
                }
                
                // now d_fst is locked and for sure first variable
                // do subsumption check
                if (doStatistics) ++stats.strengthSteps;
                
                int l1 = 0, l2 = 0, pos = -1;
                while (l1 < c.size() && l2 < d.size())
                {
                   if (c[l1] == d[l2])
                   {
                        if (c[l1] == neg)
                            pos = l2;
                        ++l1;
                        ++l2;
                   }
                   // d does not contain c[l1]
                   else if (c[l1] < d[l2])
                        break;
                   else
                        ++l2;
                }
                
                // if subsumption successful, strengthen
                if (l1 == c.size())
                {
                    assert(pos != -1 && "Position invalid"); //TODO -> if this happens, we found normel a subsumption case?, so why not deal with it? this is no error
                    if (doStatistics) ++stats.removedLiterals;
                    // unit found
                    if (d.size() == 2)
                    {
                        d.set_delete(true);
                        //data.lock();
                        if (opt_lock_stats) lock_time = cpuTime() - lock_time; 
                        data_lock.lock();
                        if (opt_lock_stats) lock_time = cpuTime() - lock_time; 
                        lbool state = data.enqueue(d[(pos + 1) % 2]);
                        data_lock.unlock();  
                        data.removedClause(list[l_cr],heap, &data_lock);
                        if (l_False == state)
                        {
                            var_lock[d_fst].unlock();
                            var_lock[fst].unlock();
                            if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
                            return;
                        }
                    }
                    else if (d.size() == 1)
                    {
                        assert(false && "no unit clauses should be strengthened");
                        // empty -> fail
                    }
                    //O if the first lit was strengthend, overwrite it in the end, since the lock would not be efficient any more
                    else
                    {
                        // keep track of this clause for further strengthening!
                        if( !d.can_strengthen() ) {
	                        localQueue.push_back( list[l_cr] );
	                        d.set_strengthen(true);
	                      }
                        occ_updates.push_back(OccUpdate(list[l_cr] , d[pos]));
                        d.removePositionSortedThreadSafe(pos);
                        // TODO to much overhead? 
                        if (opt_lock_stats) lock_time = cpuTime() - lock_time; 
                        data_lock.lock();
                        if (opt_lock_stats) lock_time = cpuTime() - lock_time; 
                        data.removedLiteral(neg, 1, heap);
                        if ( ! d.can_subsume()) 
                        {
                            d.set_subsume(true);
                            data.getSubsumeClauses().push_back(list[l_cr]);
                        }
                        data_lock.unlock();
                    }
                }
                
                // if a new lock was acquired, free it
                if (d_fst != fst)
                {
                    var_lock[d_fst].unlock();
                }
               
            }
            c[l] = ~neg;
        }
        c.set_strengthen(false);
        // free lock of first variable
        var_lock[fst].unlock();
        
    }
    if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
}

/** lock-based parallel non-naive strengthening-methode
 * @param data object
 * @param start where to start strengthening in strengtheningqueue
 * @param end where to stop strengthening
 * @param var_lock vector of locks for each variable
 */
void Subsumption::par_nn_strengthening_worker( unsigned int start, unsigned int end, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, vector<OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{ 
  assert(start <= end && end <= data.getStrengthClauses().size() && "invalid indices");
  assert(data.nVars() <= var_lock.size() && "var_lock vector to small");
  if (doStatistics)
    stats.strengthTime = wallClockTime() - stats.strengthTime;
 
   deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
  SpinLock & data_lock = var_lock[data.nVars()];

  for (; end > start;)
  {
    if (!data.ok()) 
    {
        stats.strengthTime = wallClockTime() - stats.strengthTime;
        return;
    }
    CRef cr = CRef_Undef;
    if( localQueue.size() == 0 ) {
      --end;
      cr = data.getStrengthClauses()[end];
    } else {
      // TODO: have a counter for this situation!
      cr = localQueue.back();
      localQueue.pop_back();
    }
    Clause& strengthener = ca[cr];
 
    lock_strengthener_nn:
    if (strengthener.can_be_deleted() || strengthener.size() == 0)
        continue;
    Var fst = var(strengthener[0]);
    // lock 1st var
    var_lock[fst].lock();
    
    // assure that clause can still strengthen
    if (strengthener.can_be_deleted() || strengthener.size() == 0)
    {
        var_lock[fst].unlock();
        continue;
    }
    
    // assure that first var still valid
    if (var(strengthener[0]) != fst)
    {
        var_lock[fst].unlock();
        goto lock_strengthener_nn;
    }
 
    if( strengthener.size() < 2 ) {
        if( strengthener.size() == 1 ) 
        {
            data_lock.lock();
                lbool status = data.enqueue(strengthener[0]); 
            data_lock.unlock();

            var_lock[fst].unlock(); // unlock fst var

            if( l_False == status ) break;
	        else continue;
        }
        else 
        { 
            data.setFailed();  // can be done asynchronously
            var_lock[fst].unlock(); 
            break; 
        }
    }    
    //find Lit with least occurrences and its occurrences
    // search lit with minimal occurrences
    assert (strengthener.size() > 1 && "expect strengthener to be > 1");
    Lit min = lit_Undef, nmin = lit_Undef;
    Lit /*min0 = strengthener[0], min1 = strengthener[1],*/ minT = strengthener[0];
    for (int j = 1; j < strengthener.size(); ++j)
    {
 /*     if (data[min0] > data[strengthener[j]])
      {
          min1 = min0;
          min0 = strengthener[j];
      }
      else if (data[min1] > data[strengthener[j]])
          min1 = strengthener[j]; */
      if (data[var(minT)] > data[var(strengthener[j])])
          minT = strengthener[j];
    }
/*    if (data[min0] + data[min1] <= data[var(minT)]) 
    {
        min  = min0;
        nmin = min1;
    } 
    else */
    {
        min = minT;
        nmin = ~minT;
    } 
    assert(min != nmin && "min and nmin should be different");
    vector<CRef>& list = data.list(min);        // occurrences of minlit from strengthener
    vector<CRef>& list_neg = data.list(nmin);   // occurrences of negated minlit from strengthener
    
    if (l_False == par_nn_strength_check(data, list, localQueue,  strengthener, cr, fst, var_lock, stats, occ_updates, heap, doStatistics))
    {
        var_lock[fst].unlock();
        if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
        return;
    }
    // if we use ~min, then some optimization can be done, since neg_lit has to be ~min
    if (l_False == par_nn_negated_strength_check(data, list_neg, localQueue, strengthener, cr, min, fst, var_lock, stats, occ_updates, heap, doStatistics))
    {
        var_lock[fst].unlock();
        if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
        return;
    }
/*
    // now test for the occurrences of negated min, we only need to test, if all lits after min in strengthener are also in other
    for (unsigned int j = 0; j < list_neg.size(); ++j)
    {
      Clause& other = ca[list_neg[j]];
      lock_to_strengthen_nn_neg:
      if (other.can_be_deleted() || other.size() == 0)
        continue;
      
      Var other_fst = var(other[0]);

      // if the other_fst > fst, other cannot contain fst, and therefore strengthener cannot strengthen other
      if (other_fst > fst)
          continue;

      // check if other_fst already locked by this thread, if not: lock
      if (other_fst != fst)
      {
         var_lock[other_fst].lock();

         // check if other has been deleted, while waiting for lock
         if (other.can_be_deleted() || other.size() == 0)
         {
             var_lock[other_fst].unlock();
             continue;
         }

         // check if others first lit has changed, while waiting for lock
         if (var(other[0]) != other_fst)
         {
             var_lock[other_fst].unlock();
             goto lock_to_strengthen_nn_neg;
         }
      }
      
      // now other_fst is locked and for sure first variable

      if (doStatistics) ++stats.strengthSteps;

      si = 1;
      so = 0;
      negated_lit_pos = 0;
      // find neg_lit_pos (it should be here, because other is from the occurrences of ~min)
      //        not true for multithreaded version, since we don't update occ-Lists
      while (so < other.size() && other[so] != ~min)
      {
          ++so;
      }
      if (so == other.size())
      {
        //free 1st lit of other
        if (other_fst != fst)
        {
            var_lock[other_fst].unlock();
        }      
        continue;
      }
      negated_lit_pos = so;
      // we found the position of neglit, now test if other contains all lits from strengthener besides neglit
      while (si < strengthener.size() && so < other.size())
      {
        if(strengthener[si] == other[so])
        {
          ++si;
          ++so;
        }
        else if (strengthener[si] < other[so])
          break;
        else
          ++so;
      }
      if (si == strengthener.size())
      {
          if (doStatistics) ++stats.removedLiterals;
          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[1]);
              data.removedClause(list_neg[j]);
              data_lock.unlock();   
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
              // keep track of this clause for further strengthening!
              if( !other.can_strengthen() ) {
                localQueue.push_back( list_neg[j] );
                other.set_strengthen(true);
              }
              Lit neg = other[negated_lit_pos];
              //threadsafe implementation of rPS necessary
              other.removePositionSortedThreadSafe(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1);
              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  data.getSubsumeClauses().push_back(list_neg[j]);
              }
              data_lock.unlock();
          }
      }
      // if a new lock was acquired, free it
      if (other_fst != fst)
      {
          var_lock[other_fst].unlock();
      }
    }*/
    strengthener.set_strengthen(false);

    // free lock of first variable
    var_lock[fst].unlock();
  }
  if (doStatistics) stats.strengthTime = wallClockTime() - stats.strengthTime;
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
inline lbool Subsumption::par_nn_strength_check(CoprocessorData & data, vector < CRef > & list, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Var fst, vector < SpinLock > & var_lock, struct SubsumeStatsData & stats, vector< OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics) 
{
    int si, so;           // indices used for "can be strengthened"-testing
    int negated_lit_pos;  // index of negative lit, if we find one
 
    SpinLock & data_lock = var_lock[data.nVars()];
    
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      Clause& other = ca[list[j]];
      
      lock_to_strengthen_nn:
      if (other.can_be_deleted() || list[j] == cr || other.size() == 0)
        continue;
      Var other_fst = var(other[0]);

      // if the other_fst > fst, other cannot contain fst, and therefore strengthener cannot strengthen other
      if (other_fst > fst)
          continue;

      // check if other_fst already locked by this thread, if not: lock
      if (other_fst != fst)
      {
         var_lock[other_fst].lock();

         // check if other has been deleted, while waiting for lock
         if (other.can_be_deleted() || other.size() == 0)
         {
             var_lock[other_fst].unlock();
             continue;
         }

         // check if others first lit has changed, while waiting for lock
         if (var(other[0]) != other_fst)
         {
             var_lock[other_fst].unlock();
             goto lock_to_strengthen_nn;
         }
      }
      
      // now other_fst is locked and for sure first variable
      if (doStatistics) ++stats.strengthSteps;
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

      // if subsumption successful, strengthen
      if (negated_lit_pos != -1 && si == strengthener.size())
      {
          
          if (doStatistics) ++stats.removedLiterals;
          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[(negated_lit_pos + 1) % 2]);
              data_lock.unlock();
              data.removedClause(list[j], heap, &data_lock);
              if (l_False == state)
              {
                  var_lock[other_fst].unlock();
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
              // keep track of this clause for further strengthening!
              if( !other.can_strengthen() ) {
                localQueue.push_back( list[j] );
                other.set_strengthen(true);
              }
              Lit neg = other[negated_lit_pos];
              occ_updates.push_back(OccUpdate(list[j] , neg));
              other.removePositionSortedThreadSafe(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1, heap);
              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  data.getSubsumeClauses().push_back(list[j]);
              }
              data_lock.unlock();
          }
      }
      // if a new lock was acquired, free it
      if (other_fst != fst)
      {
          var_lock[other_fst].unlock();
      }
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
inline lbool Subsumption::par_nn_negated_strength_check(CoprocessorData & data, vector < CRef > & list, deque<CRef> & localQueue, Clause & strengthener, CRef cr, Lit min, Var fst, vector < SpinLock > & var_lock, struct SubsumeStatsData & stats, vector< OccUpdate> & occ_updates, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics) 
{
    int si, so;           // indices used for "can be strengthened"-testing
    int negated_lit_pos;  // index of negative lit, if we find one
 
    SpinLock & data_lock = var_lock[data.nVars()];
    
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      Clause& other = ca[list[j]];
      
      lock_to_strengthen_nn:
      if (other.can_be_deleted() || list[j] == cr || other.size() == 0)
        continue;
      Var other_fst = var(other[0]);

      // if the other_fst > fst, other cannot contain fst, and therefore strengthener cannot strengthen other
      if (other_fst > fst)
          continue;

      // check if other_fst already locked by this thread, if not: lock
      if (other_fst != fst)
      {
         var_lock[other_fst].lock();

         // check if other has been deleted, while waiting for lock
         if (other.can_be_deleted() || other.size() == 0)
         {
             var_lock[other_fst].unlock();
             continue;
         }

         // check if others first lit has changed, while waiting for lock
         if (var(other[0]) != other_fst)
         {
             var_lock[other_fst].unlock();
             goto lock_to_strengthen_nn;
         }
      }
      
      // now other_fst is locked and for sure first variable
      if (doStatistics) ++stats.strengthSteps;
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

      // if subsumption successful, strengthen
      if (negated_lit_pos != -1 && si == strengthener.size())
      {
          
          if (doStatistics) ++stats.removedLiterals;
          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[(negated_lit_pos + 1) % 2]);
              data_lock.unlock();
              data.removedClause(list[j],heap, &data_lock);
              if (l_False == state)
              {
                 var_lock[other_fst].unlock();
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
              // keep track of this clause for further strengthening!
              if( !other.can_strengthen() ) {
                localQueue.push_back( list[j] );
                other.set_strengthen(true);
              }
              Lit neg = other[negated_lit_pos];
              occ_updates.push_back(OccUpdate(list[j] , neg));
              other.removePositionSortedThreadSafe(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1, heap);
              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  data.getSubsumeClauses().push_back(list[j]);
              }
              data_lock.unlock();
          }
      }
      // if a new lock was acquired, free it
      if (other_fst != fst)
      {
          var_lock[other_fst].unlock();
      }
    }
    return l_Undef;
}

bool Subsumption::hasToStrengthen() const
{
  return data.getStrengthClauses().size() > 0;
}

/** runs a fullstrengthening on data.getStrengthClauses(), needs occurrencelists (naive strengthening seems to be faster, TODO: strengthening on literals with minimal occurrences)
 * @param data occurrenceslists
 * @return 
 */
lbool Subsumption::fullStrengthening(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
    /*
     * TODO strengthening with min oppurances-lits
    for( int pos = 0 ; pos < 2; ++ pos )
    {
       //find lit with minimal occurrences
       vector<CRef>& list = pos == 0 ? data.list(min) :  data.list(~min);
    }
     */
  if( opt_allStrengthRes || !opt_naivStrength ) {
    return strengthening_worker( 0, data.getStrengthClauses().size(), heap);
  }
    if (doStatistics) strengthTime = cpuTime() - strengthTime;
    deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
    vector<OccUpdate> occ_updates;
    //for every clause:
    for (int i = 0; i < data.getStrengthClauses().size();)
    {
        CRef cr = CRef_Undef;
        if( localQueue.size() == 0 ) {
            cr = data.getStrengthClauses()[i];
            ++i;
        } else {
            // TODO: have a counter for this situation!
            cr = localQueue.back();
            localQueue.pop_back();
        }
        Clause &c = ca[cr];
        if (c.can_be_deleted() || !c.can_strengthen())
            continue;   // dont check if it cant strengthen or can be deleted
        // for every literal in this clause:
        
        // search for lit with minimal occurrences;
        Lit min = c[0];
        for (int j = 1; j < c.size(); ++j)
            if (data[min] * (c.size() - 1) + data[~min] > data[c[j]] * (c.size() -1) + data[~c[j]])
                min = c[j];

        for (int j = 0; j < c.size(); ++j)
        {
            // negate this literal and check for subsumptions for every occurrence of its negation:
            Lit neg_lit = ~c[j];
            c[j] = neg_lit;     // temporarily change lit for subsumptiontest
            vector<CRef> & list = (neg_lit == ~min) ? data.list(neg_lit) : data.list(min);  // get occurrences of this lit
            //vector<CRef> & list = data.list(neg_lit);  // get occurrences of this lit
            for (unsigned int k = 0; k < list.size(); ++k)
            {
                if (list[k] == cr)
                    continue;
                Clause & other = ca[list[k]];
                if (other.can_be_deleted())     // dont check if this clause can be deleted
                    continue;
                assert(other.size() > 1 && "Expect other to be > 1");
                if (doStatistics) ++strengthSteps;
                
                // check for subsumption
                int l1 = 0, l2 = 0, pos = -1;
                while (l1 < c.size() && l2 < other.size())
                {
                   if (c[l1] == other[l2])
                   {
                        if (c[l1] == neg_lit)
                            pos = l2;
                        ++l1;
                        ++l2;
                   }
                   // other does not contain c[l1]
                   else if (c[l1] < other[l2])
                        break;
                   else
                        ++l2;
                }
                if (l1 == c.size() && pos != -1)    
                {
                    if (doStatistics) ++removedLiterals;
                    other.removePositionSorted(pos);     // strengthen clause
		            if( global_debug_out ) cerr << "c remove " << neg_lit << " from clause " << other << endl;
                    //if( global_debug_out ) cerr << "c used strengthener (lit negated) " << c << endl;
                    if(other.size() == 1)
                    {
                      // propagate if clause is unit
                      other.set_delete(true);
                      data.enqueue(other[0]);
                      //propagation.propagate(data, true); -> causes problems if effecting c
                    } 
                    else
                    {
                        // add clause since it got smaler and could subsume to subsumption_queue
                        if(!other.can_subsume())
                        {
                            // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
                            other.set_subsume(true);
                            data.getSubsumeClauses().push_back(list[k]);
                        }
                        // keep track of this clause for further strengthening!
                        if( !other.can_strengthen() ) {
	                        localQueue.push_back( list[k] );
	                        other.set_strengthen(true);
	                    }

                        // track occurrence update
                        occ_updates.push_back(OccUpdate(list[k],neg_lit));
                        //list[k] = list[list.size() - 1];
                        //list.pop_back();    // shrink vector
                        
                        // k--; // since k++ in for-loop
                    }
                }
            }
            c[j] = ~neg_lit;    // change the negated lit back
        }
        if (data.hasToPropagate()) 
        {
            if (propagation.propagate(data, true, heap) == l_False)
                return l_False;
        }
        c.set_strengthen(false);
    }

  updateOccurrences( occ_updates, heap);

  if (doStatistics) strengthTime = cpuTime() - strengthTime;
  // no result to tell to the outside
  return l_Undef;   
}

/**
 *creating resolvents in the createAllResolvents Version of Strengthening
 *
 */
lbool Subsumption::createResolvent( const CRef cr, CRef & resolvent, const int negated_lit_pos, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
    Clause & origin = ca[cr];
    assert (origin.size() >= 2);
    assert (0 <= negated_lit_pos && negated_lit_pos < origin.size());
    assert (!origin.can_be_deleted());
    if (origin.size() == 2)
    {
        lbool state = data.enqueue(origin[(negated_lit_pos + 1) % 2]);
        if (l_False == state)
            return l_False;
        else
            return l_True;
    }
    else 
    {
      ps.clear();
      for (int i = 0; i < origin.size(); ++i)
      {
        if (i == negated_lit_pos)
          continue;
        ps.push(origin[i]);
      }
      if (doStatistics)
      { // count resolvents negatively
        removedLiterals -= origin.size() - 1;
      }
      resolvent = ca.alloc(ps);
      data.addClause(resolvent, heap);
      data.getClauses().push(resolvent);
      return l_Undef;
    }
}



/** the strengthening-methode, which runs through the strengtheningqueue from start to end and tries to strengthen with the clause from str-queue on the clauses from data
 * @param data vector of occurrences of clauses
 * @param start where to start strengthening in strengtheningqueue
 * @param end where to stop strengthening
 *
 * 
 */
lbool Subsumption::strengthening_worker( unsigned int start, unsigned int end, Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
  if (doStatistics)
      strengthTime = cpuTime() - strengthTime;
  int si, so;           // indices used for "can be strengthened"-testing
  int negated_lit_pos;  // index of negative lit, if we find one
  deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
  vector < OccUpdate > occ_updates;
  for (; end > start;)
  {
    CRef cr = CRef_Undef;
    if( localQueue.size() == 0 ) {
      --end;
      cr = data.getStrengthClauses()[end];
    } else {
      // TODO: have a counter for this situation!
      cr = localQueue.back();
      localQueue.pop_back();
    }
    Clause& strengthener = ca[cr];
    if (strengthener.can_be_deleted() || !strengthener.can_strengthen())
      continue;
    //find Lit with least occurrences and its occurrences
    // search lit with minimal occurrences

    if( strengthener.size() < 2 ) {
      if( strengthener.size() == 1 ) { 
	if( l_False == data.enqueue(strengthener[0]) ) break;
	else continue;
      }
      else { data.setFailed(); break; }
    }
    
    assert (strengthener.size() > 1 && "expect strengthener to be > 1");
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
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      Clause& other = ca[list[j]];
      if (other.can_be_deleted() || list[j] == cr || other.size() == 0)
        continue;
      // test if we can strengthen other clause
      if (doStatistics) ++strengthSteps;
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
          break;    // the other clause had not the same lit and it was not negated, can't strengthen
        else
          ++so;
      }
      if (negated_lit_pos != -1 && si == strengthener.size()) // TODO if negated_lit_pos == -1 -> normal subsumption case, why not apply  it?
      {
        if (opt_allStrengthRes)
        {
          CRef newCRef;
          toDelete.push_back(list[j]);
          lbool status = createResolvent(list[j], newCRef, negated_lit_pos, heap, doStatistics);
          if (l_False == status)
            return l_False;
          if (l_Undef == status)
          {
            data.getSubsumeClauses().push_back(newCRef);
            newClauses.push_back(newCRef);
          }
          if (global_debug_out) cerr << "c created resolvent while strengthening: " << ca[newCRef] << endl;
          if (global_debug_out) cerr  << "c origin: " << strengthener << " and " << other << endl;
        }
        else
        {
          if (doStatistics) ++ removedLiterals;
          // if neglit found and we went through all lits of strengthener, then the other clause can be strengthened
          if(!other.can_subsume())
          {
            // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
            other.set_subsume(true);
            data.getSubsumeClauses().push_back(list[j]);
          }
          // keep track of this clause for further strengthening!
          if( !other.can_strengthen() ) {
            localQueue.push_back( list[j] );
            other.set_strengthen(true);
          }
          occ_updates.push_back(OccUpdate(list[j] , other[negated_lit_pos]));
          if( global_debug_out ) cerr << "c remove " << other[negated_lit_pos] << " from clause " << other << endl;
          other.removePositionSorted(negated_lit_pos);
          if(other.size() == 1)
          {
            // propagate if clause is only 1 lit big
            assert(var(other[0]) < data.nVars() && "Literal does not exist");
            data.enqueue(other[0]);
            other.set_delete(true);
            //propagation.propagate(data, true);
          }
        }
      }
    }
    //propagation only in a valid state 
    if (data.hasToPropagate()) 
    {
        if (propagation.propagate(data, true, heap) == l_False)
            return l_False;
    }
    // now test for the occurrences of negated min, now the literal, that appears negated has to be min
    for (unsigned int j = 0; j < list_neg.size(); ++j)
    {
      Clause& other = ca[list_neg[j]];
      if (other.can_be_deleted())
        continue;
      if (doStatistics) ++ strengthSteps;
      si = 0;
      so = 0;
      negated_lit_pos = -1;
      while (si < strengthener.size() && so < other.size())
      {
        if(strengthener[si] == other[so])
        {
          ++si;
          ++so;
        } 
        else if (~min == other[so])
        {
            negated_lit_pos = so;
            ++si; 
            ++so; 
        }
        else if (strengthener[si] < other[so])
          break;
        else
          ++so;
      }
      if (si == strengthener.size() && negated_lit_pos != -1)
      {
        if (opt_allStrengthRes)
        {
          CRef newCRef;
          toDelete.push_back(list_neg[j]);
          lbool status = createResolvent(list_neg[j], newCRef, negated_lit_pos, heap, doStatistics);
          if (l_False == status)
            return l_False;
          if (l_Undef == status)
          {
            data.getSubsumeClauses().push_back(newCRef);
            newClauses.push_back(newCRef);
            
          }
          if (global_debug_out) cerr << "c created resolvent while strengthening: " << ca[newCRef] << endl;
          if (global_debug_out) cerr  << "c origin: " << strengthener << " and " << other << endl;
        }
        else
        {
          if (doStatistics) ++removedLiterals;
          // other can be strengthened
          if(!other.can_subsume())
          {
            // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
            other.set_subsume(true);
            data.getSubsumeClauses().push_back(list_neg[j]);
          }
        
          // keep track of this clause for further strengthening!
          if( !other.can_strengthen() ) {
       localQueue.push_back( list_neg[j] );
       other.set_strengthen(true);
        }
          
          occ_updates.push_back(OccUpdate(list_neg[j] , other[negated_lit_pos]));
          if( global_debug_out ) cerr << "c remove " << other[negated_lit_pos] << " from clause " << other << endl;
          other.removePositionSorted(negated_lit_pos);
          
          if(other.size() == 1)
          {
            // propagate if clause is only 1 lit big
            assert(var(other[0]) < data.nVars() && "Literal does not exist");
            data.enqueue(other[0]);
            other.set_delete(true);
            //propagation.propagate(data, true);
          }
        }
      }
    }
    if (data.hasToPropagate()) 
    {
        if (propagation.propagate(data, true, heap) == l_False)
            return l_False;
    }
    strengthener.set_strengthen(false);
  }
  updateOccurrences( occ_updates, heap);
  if (doStatistics) strengthTime = cpuTime() - strengthTime;
}

inline void Subsumption::updateOccurrences(const vector< OccUpdate > & updates, Heap<VarOrderBVEHeapLt> * heap)
{
    for (int i = 0; i < updates.size(); ++i)
    {
        // just remove once from stats -> this is consistent with propagation, since propagation will clear occ-lists 
        // and sets occ-stats by itself
        if (data.removeClauseFrom(updates[i].cr, updates[i].l))
            data.removedLiteral(updates[i].l, 1, heap);
    }
}

void Subsumption::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if( !c.can_be_deleted() ) {
    if (c.can_subsume() )
      data.getSubsumeClauses().push_back(cr);
    if (c.can_strengthen())
      data.getStrengthClauses().push_back(cr);
  }
}


void Subsumption::parallelSubsumption( const bool doStatistics)
{
  cerr << "c parallel subsumption with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  vector< vector < CRef > > toDeletes  (controller.size()); //TODO member var!!!
  vector< vector < CRef > > nonLearnts (controller.size());
  //vector< struct SubsumeStatsData > localStats (controller.size());
  unsigned int queueSize = data.getSubsumeClauses().size();
  unsigned int partitionSize = data.getSubsumeClauses().size() / controller.size();
  // setup data for workers
  for( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].data  = &data;
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    workData[i].to_delete = & toDeletes[i];
    workData[i].set_non_learnt = & nonLearnts[i];
/*    localStats[i].subsumedClauses = 0;
    localStats[i].subsumedLiterals = 0;
    localStats[i].subsumeSteps = 0;
    localStats[i].processTime = 0;*/
    workData[i].stats = & localStats[i];
    jobs[i].function  = Subsumption::runParallelSubsume;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );

  // Setting delete information
  for (int i = 0 ; i < toDeletes.size(); ++ i) 
    for (int j = 0; j < toDeletes[i].size(); ++j)
    {
        Clause & c = ca[toDeletes[i][j]]; 
        if (!c.can_be_deleted())
        {
            c.set_delete(true);
            data.removedClause(toDeletes[i][j]);
            if (doStatistics)
            {
                ++localStats[i].subsumedClauses;
                localStats[i].subsumedLiterals += c.size();
            }
        }
    } 
  // Setting non-learnt information
  for (int i = 0; i < nonLearnts.size(); ++i)
    for (int j = 0; j < nonLearnts[i].size(); ++j)
    {
        Clause & c = ca[nonLearnts[i][j]];
        if (!c.can_be_deleted() && c.learnt())
            c.set_learnt(false);
    }
}

void* Subsumption::runParallelSubsume(void* arg)
{
  SubsumeWorkData* workData = (SubsumeWorkData*) arg;
  workData->subsumption->par_subsumption_worker(workData->start,workData->end, *(workData->to_delete), *(workData->set_non_learnt), *(workData->stats));
  return 0;
}

void Subsumption::parallelStrengthening(Heap<VarOrderBVEHeapLt> * heap, const bool doStatistics)
{
  //fullStrengthening(data);
  cerr << "c parallel strengthening with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  //vector< struct SubsumeStatsData > localStats (controller.size());
  vector<Job> jobs( controller.size() );
  vector< SpinLock > var_locks (data.nVars() + 1); // 1 extra SpinLock for data
  vector< vector < OccUpdate > > occ_updates(controller.size());
  unsigned int queueSize = data.getStrengthClauses().size();
  unsigned int partitionSize = data.getStrengthClauses().size() / controller.size();
  
  for ( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    cerr << "c p s thread " << i << " running from " << workData[i].start << " to " << workData[i].end << endl;
    workData[i].data  = &data; 
    workData[i].var_locks = & var_locks;
/*    localStats[i].removedLiterals = 0;
    localStats[i].strengthSteps = 0;
    localStats[i].strengthTime = 0;
    localStats[i].lockTime = 0;*/
    workData[i].stats = & localStats[i];
    workData[i].occ_updates = & occ_updates[i];
    workData[i].heap = heap;
    jobs[i].function  = Subsumption::runParallelStrengthening;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );

  // update Occurrences
  for (int i = 0; i < controller.size(); ++i)
    updateOccurrences(occ_updates[i], heap);

  //propagate units
  propagation.propagate(data, true, heap);
}

void* Subsumption::runParallelStrengthening(void* arg)
{
    SubsumeWorkData* workData = (SubsumeWorkData*) arg;
    if ( opt_naivStrength ) workData->subsumption->par_strengthening_worker(workData->start,workData->end, *(workData->var_locks), *(workData->stats), *(workData->occ_updates), workData->heap);
    else workData->subsumption->par_nn_strengthening_worker(workData->start,workData->end, *(workData->var_locks), *(workData->stats), *(workData->occ_updates), workData->heap);
    return 0;
}
