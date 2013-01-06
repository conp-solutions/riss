/**********************************************************************************[Subsumption.cc]
Copyright (c) 2012, Norbert Manthey, Kilian Gebhardt, Max Löwen, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"

using namespace Coprocessor;

static const char* _cat = "CP3 SUBSUMPTION";
// options
static BoolOption  opt_naivStrength    (_cat, "cp3_naive_strength", "use naive strengthening", false);
static BoolOption  opt_par_strength    (_cat, "cp3_par_strength", "use par strengthening", false);
    
Subsumption::Subsumption( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation )
: Technique( _ca, _controller )
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
}


void Subsumption::subsumeStrength(CoprocessorData& data)
{
  while( hasToSubsume() || hasToStrengthen() )
  {
    if( hasToSubsume() ){
      fullSubsumption(data);
      // clear queue afterwards
      clause_processing_queue.clear();
    }
    if( hasToStrengthen() ) {
      if (opt_par_strength && controller.size() > 0)
      {
          parallelStrengthening(data);
          data.correctCounters();
      }
      else {
          fullStrengthening(data);
      }
      // clear queue afterwards
      strengthening_queue.clear();
    }
  }
}



bool Subsumption::hasToSubsume() const
{
  return clause_processing_queue.size() > 0; 
}

lbool Subsumption::fullSubsumption(CoprocessorData& data)
{
  // run subsumption for the whole queue
  //if( controller.size() > 0 && (clause_processing_queue.size() > 100000 || ( clause_processing_queue.size() > 50000 && 10*data.nCls() > 22*data.nVars() ) ) ) {
  if (controller.size() > 0){ //TODO for Development only
    parallelSubsumption(data); // use parallel, is some conditions have been met
    data.correctCounters();    // 
  } else {
    subsumption_worker(data,0,clause_processing_queue.size());
  }
  clause_processing_queue.clear();
  // no result to tell to the outside
  return l_Undef; 
}

bool Subsumption::hasWork() const
{
  return hasToStrengthen() || hasToSubsume();
}


void Subsumption :: subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, const bool doStatistics)
{
    for (; end > start;)
    {
        --end;
	const CRef cr = clause_processing_queue[end];
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
                if (c.size() == ca[list[i]].size() && cr > list[i]) //TODO disable this in non-parallel worker
                {
                    c.set_delete(true);
                    if (!c.learnt() && ca[list[i]].learnt())
                        ca[list[i]].set_learnt(false);
                    continue;
                }
                ca[list[i]].set_delete(true); 
                if( doStatistics ) data.removedClause(list[i]);  // tell statistic about clause removal
		        if( global_debug_out ) cerr << "c clause " << ca[list[i]] << " is deleted by " << c << endl;
                if (!ca[list[i]].learnt() && c.learnt())
                {
                    c.set_learnt(false);
                }
            }
        }
        //set can_subsume to false
        c.set_subsume(false);
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
void Subsumption :: par_subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, vector<CRef> & to_delete, vector< CRef > & set_non_learnt, struct SubsumeStatsData & stats, const bool doStatistics)
{
    if (doStatistics)
    {
        stats.processTime = cpuTime() - stats.processTime;   
    }
    while (end > start)
    {
        --end;
        const CRef cr = clause_processing_queue[end];
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
                        c.set_delete(true);
                        if (doStatistics)
                        {
                            ++stats.subsumedClauses;
                            stats.subsumedLiterals += c.size();
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
                ca[list[i]].set_delete(true);
                if (doStatistics)
                {
                    ++stats.subsumedClauses;
                    stats.subsumedLiterals += ca[list[i]].size();
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
        stats.processTime = cpuTime() - stats.processTime;
}


/**
 * locking based strengthening, based on naive subsumption check with negated literal
 * 
 * @param var_lock vector of locks for each variable
 *
 */
void Subsumption::par_strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int stop, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, const bool doStatistics) 
{
    assert(start <= stop && stop <= strengthening_queue.size() && "invalid indices");
    assert(data.nVars() <= var_lock.size() && "var_lock vector to small");
    
    if (doStatistics)
        stats.strengthTime = cpuTime() - stats.strengthTime;
    
    deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
    SpinLock & data_lock = var_lock[data.nVars()];
    while (stop > start)
    {    
        CRef cr = CRef_Undef;
        if( localQueue.size() == 0 ) {
            --stop;
            cr = strengthening_queue[stop];
        } else {
            // TODO: have a counter for this situation!
            cr = localQueue.back();
            localQueue.pop_back();
        }
 
        Clause & c = ca[strengthening_queue[stop]];
        lock_strengthener:
        if (c.can_be_deleted() || c.size() == 0)
            continue;
        Var fst = var(c[0]);
        // lock 1st var
        var_lock[fst].lock();
        
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

        for (int l = 0; l < c.size(); ++l)
        {
            Lit neg = ~ c[l];
            c[l] = neg;
            vector< CRef> & list = data.list(neg);
            for (int l_cr = 0; l_cr < list.size(); ++l_cr)
            {
                assert(list[l_cr] != strengthening_queue[stop] && "expect no tautologies here");
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
                   var_lock[d_fst].lock();

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
                    assert(pos != -1 && "Position invalid");
                    if (doStatistics) ++stats.removedLiterals;
                    // unit found
                    if (d.size() == 2)
                    {
                        d.set_delete(true);
                        //data.lock();
                        data_lock.lock();
                        lbool state = data.enqueue(d[(pos + 1) % 2]);
                        data.removedClause(list[l_cr]);
                        data_lock.unlock();   
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
                        d.removePositionSortedThreadSafe(pos);
                        // TODO to much overhead? 
                        data_lock.lock();
                        data.removedLiteral(neg, 1);
                        if ( ! d.can_subsume()) 
                        {
                            d.set_subsume(true);
                            clause_processing_queue.push_back(list[l_cr]);
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
    if (doStatistics) stats.strengthTime = cpuTime() - stats.strengthTime;
}

/** lock-based parallel non-naive strengthening-methode
 * @param data object
 * @param start where to start strengthening in strengtheningqueue
 * @param end where to stop strengthening
 * @param var_lock vector of locks for each variable
 */
void Subsumption::par_nn_strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int end, vector< SpinLock > & var_lock, struct SubsumeStatsData & stats, const bool doStatistics)
{ 
  assert(start <= end && end <= strengthening_queue.size() && "invalid indices");
  assert(data.nVars() <= var_lock.size() && "var_lock vector to small");
  if (doStatistics)
    stats.strengthTime = cpuTime() - stats.strengthTime;
 
  int si, so;           // indices used for "can be strengthened"-testing
  int negated_lit_pos;  // index of negative lit, if we find one
  deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
  SpinLock & data_lock = var_lock[data.nVars()];

  for (; end > start;)
  {
    CRef cr = CRef_Undef;
    if( localQueue.size() == 0 ) {
      --end;
      cr = strengthening_queue[end];
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
     
    //find Lit with least occurrences and its occurrences
    Lit min = strengthener[0];
    
    vector<CRef>& list = data.list(min);        // occurrences of minlit from strengthener
    vector<CRef>& list_neg = data.list(~min);   // occurrences of negated minlit from strengthener
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
      negated_lit_pos = 0;  //the position for neglit cant be 0, so we will use this as "neglit not found"
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
          if (negated_lit_pos == 0)
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
      if (negated_lit_pos > 0 && si == strengthener.size())
      {
          
          if (doStatistics) ++stats.removedLiterals;
          // unit found
          if (other.size() == 2)
          {
              other.set_delete(true);
              data_lock.lock();
              lbool state = data.enqueue(other[0]);
              data.removedClause(list[j]);
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
                localQueue.push_back( list[j] );
                other.set_strengthen(true);
              }
              Lit neg = other[negated_lit_pos];
              //no need for threadsafe implementation of rPS, since 1st lit could not be removed here
              other.removePositionSorted(negated_lit_pos);
              // TODO to much overhead? 
              data_lock.lock();
              data.removedLiteral(neg, 1);
              if ( ! other.can_subsume()) 
              {
                  other.set_subsume(true);
                  clause_processing_queue.push_back(list[j]);
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
                  clause_processing_queue.push_back(list_neg[j]);
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
    strengthener.set_strengthen(false);

    // free lock of first variable
    var_lock[fst].unlock();
  }
  if (doStatistics) stats.strengthTime = cpuTime() - stats.strengthTime;
}

bool Subsumption::hasToStrengthen() const
{
  return strengthening_queue.size() > 0;
}

/** runs a fullstrengthening on strengthening_queue, needs occurrencelists (naive strengthening seems to be faster, TODO: strengthening on literals with minimal occurrences)
 * @param data occurrenceslists
 * @return 
 */
lbool Subsumption::fullStrengthening(CoprocessorData& data, const bool doStatistics)
{
    /*
     * TODO strengthening with min oppurances-lits
    for( int pos = 0 ; pos < 2; ++ pos )
    {
       //find lit with minimal occurrences
       vector<CRef>& list = pos == 0 ? data.list(min) :  data.list(~min);
    }
     */
  if( !opt_naivStrength ) {
    strengthening_worker(data, 0, strengthening_queue.size());
    return l_Undef;
  }
    if (doStatistics) strengthTime = cpuTime() - strengthTime;
    deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
    //for every clause:
    for (int i = 0; i < strengthening_queue.size();)
    {
        CRef cr = CRef_Undef;
        if( localQueue.size() == 0 ) {
            cr = strengthening_queue[i];
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
        for (int j = 0; j < c.size(); ++j)
        {
            // negate this literal and check for subsumptions for every occurrence of its negation:
            Lit neg_lit = ~c[j];
            c[j] = neg_lit;     // temporarily change lit for subsumptiontest
            vector<CRef> & list = data.list(neg_lit);  // get occurrences of this lit
            for (unsigned int k = 0; k < list.size(); ++k)
            {
                Clause & other = ca[list[k]];
                if (other.can_be_deleted())     // dont check if this clause can be deleted
                    continue;
                if (doStatistics) ++strengthSteps;
                if (c.ordered_subsumes(other))    // check for subsumption
                {
                    if (doStatistics) ++removedLiterals;
                    other.remove_lit(neg_lit);     // strengthen clause
		            if( global_debug_out ) cerr << "c remove " << neg_lit << " from clause " << other << endl;
                    if(other.size() == 1)
                    {
                      // propagate if clause is unit
                      data.enqueue(other[0]);
                      propagation.propagate(data, true);
                    } 
                    else
                    {
                        // add clause since it got smaler and could subsume to subsumption_queue
                        if(!other.can_subsume())
                        {
                            // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
                            other.set_subsume(true);
                            clause_processing_queue.push_back(list[k]);
                        }
                        // keep track of this clause for further strengthening!
                        if( !other.can_strengthen() ) {
	                        localQueue.push_back( list[k] );
	                        other.set_strengthen(true);
	                    }

                        // update occurances
                        list[k] = list[list.size() - 1];
                        list.pop_back();    // shrink vector
                        
                        k--; // since k++ in for-loop
                    }
                }
            }
            c[j] = ~neg_lit;    // change the negated lit back
        }
        c.set_strengthen(false);
    }
  if (doStatistics) strengthTime = cpuTime() - strengthTime;
  // no result to tell to the outside
  return l_Undef;   
}

/** the strengthening-methode, which runs through the strengtheningqueue from start to end and tries to strengthen with the clause from str-queue on the clauses from data
 * @param data vector of occurrences of clauses
 * @param start where to start strengthening in strengtheningqueue
 * @param end where to stop strengthening
 */
void Subsumption::strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics)
{
  if (doStatistics)
      strengthTime = cpuTime() - strengthTime;
  int si, so;           // indices used for "can be strengthened"-testing
  int negated_lit_pos;  // index of negative lit, if we find one
  deque<CRef> localQueue; // keep track of all clauses that have been added back to the strengthening queue because they have been strengthened
  for (; end > start;)
  {
    CRef cr = CRef_Undef;
    if( localQueue.size() == 0 ) {
      --end;
      cr = strengthening_queue[end];
    } else {
      // TODO: have a counter for this situation!
      cr = localQueue.back();
      localQueue.pop_back();
    }
    Clause& strengthener = ca[cr];
    if (strengthener.can_be_deleted() || !strengthener.can_strengthen())
      continue;
    //find Lit with least occurrences and its occurrences
    Lit min = strengthener[0];
    
    vector<CRef>& list = data.list(min);        // occurrences of minlit from strengthener
    vector<CRef>& list_neg = data.list(~min);   // occurrences of negated minlit from strengthener
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      Clause& other = ca[list[j]];
      if (other.can_be_deleted() || list[j] == cr)
        continue;
      // test if we can strengthen other clause
      if (doStatistics) ++strengthSteps;
      si = 0;
      so = 0;
      negated_lit_pos = 0;  //the position for neglit cant be 0, so we will use this as "neglit not found"
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
          if (negated_lit_pos == 0)
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
      if (negated_lit_pos > 0 && si == strengthener.size())
      {
        if (doStatistics) ++ removedLiterals;
        // if neglit found and we went through all lits of strengthener, then the other clause can be strengthened
        if(!other.can_subsume())
        {
          // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
          other.set_subsume(true);
          clause_processing_queue.push_back(list[j]);
        }
        // keep track of this clause for further strengthening!
        if( !other.can_strengthen() ) {
	        localQueue.push_back( list[j] );
	        other.set_strengthen(true);
	    }
        // update occurance-list for this lit (this must be done after pushing to subsumptionqueue)
        // first find the list, which has to be updated
        for (int l = 0; l < data.list(other[negated_lit_pos]).size(); ++l)
        {
          if(list[j] == data.list(other[negated_lit_pos])[l])
          {
            data.list(other[negated_lit_pos])[l] = data.list(other[negated_lit_pos])[data.list(other[negated_lit_pos]).size() - 1];
            data.list(other[negated_lit_pos]).pop_back();
            break;
          }
        }
        if( global_debug_out ) cerr << "c remove " << negated_lit_pos << " from clause " << other << endl;
        other.remove_lit(other[negated_lit_pos]);   // remove lit from clause (this must be done after updating occurances)
        if(other.size() == 1)
        {
          // propagate if clause is only 1 lit big
          data.enqueue(other[0]);
          propagation.propagate(data, true);
        }
      }
    }
    // now test for the occurrences of negated min, we only need to test, if all lits after min in strengthener are also in other
    for (unsigned int j = 0; j < list_neg.size(); ++j)
    {
      Clause& other = ca[list_neg[j]];
      if (other.can_be_deleted())
        continue;
      if (doStatistics) ++ strengthSteps;
      si = 1;
      so = 0;
      negated_lit_pos = 0;
      // find neg_lit_pos (it should be here, because other is from the occurrences of ~min)
      while (other[so] != ~min)
      {
          ++so;
      }
      negated_lit_pos = so;
      // we found the position of neglit, now test if all lits from strengthener besides neglit are in other
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
        if (doStatistics) ++removedLiterals;
        // other can be strengthened
        if(!other.can_subsume())
        {
          // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
          other.set_subsume(true);
          clause_processing_queue.push_back(list_neg[j]);
        }
	// keep track of this clause for further strengthening!
        if( !other.can_strengthen() ) {
	   localQueue.push_back( list_neg[j] );
	   other.set_strengthen(true);
	}
        // update occurance-list for this lit (this must be done after pushing to subsumptionqueue)
        // first find the list, which has to be updated
        for (int l = 0; l < data.list(other[negated_lit_pos]).size(); ++l)
        {
          if(list_neg[j] == data.list(other[negated_lit_pos])[l])
          {
            data.list(other[negated_lit_pos])[l] = data.list(other[negated_lit_pos])[data.list(other[negated_lit_pos]).size() - 1];
            data.list(other[negated_lit_pos]).pop_back();
            break;
          }
        }
        if( global_debug_out ) cerr << "c remove " << negated_lit_pos << " from clause " << other << endl;
        other.remove_lit(other[negated_lit_pos]);   // remove lit from clause (this must be done after updating occurances)
        if(other.size() == 1)
        {
          // propagate if clause is only 1 lit big
          data.enqueue(other[0]);
          propagation.propagate(data, true);
        }
      }
    }
    strengthener.set_strengthen(false);
  }
  if (doStatistics) strengthTime = cpuTime() - strengthTime;
}

void Subsumption::addClause(const Minisat::CRef cr)
{
  const Clause& c = ca[cr];
  if (c.can_subsume() && !c.can_be_deleted())
    clause_processing_queue.push_back(cr);
  if (c.can_strengthen() && !c.can_be_deleted())
    strengthening_queue.push_back(cr);
}

void Subsumption::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if (c.can_subsume() && !c.can_be_deleted())
    clause_processing_queue.push_back(cr);
  if (c.can_strengthen() && !c.can_be_deleted())
    strengthening_queue.push_back(cr);
}


void Subsumption::parallelSubsumption(CoprocessorData& data, const bool doStatistics)
{
  cerr << "c parallel subsumption with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  vector< vector < CRef > > toDeletes  (controller.size());
  vector< vector < CRef > > nonLearnts (controller.size());
  vector< struct SubsumeStatsData > localStats (controller.size());
  unsigned int queueSize = clause_processing_queue.size();
  unsigned int partitionSize = clause_processing_queue.size() / controller.size();
  // setup data for workers
  for( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].data  = &data;
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    workData[i].to_delete = & toDeletes[i];
    workData[i].set_non_learnt = & nonLearnts[i];
    localStats[i].subsumedClauses = 0;
    localStats[i].subsumedLiterals = 0;
    localStats[i].subsumeSteps = 0;
    localStats[i].processTime = 0;
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
  workData->subsumption->par_subsumption_worker(*(workData->data),workData->start,workData->end, *(workData->to_delete), *(workData->set_non_learnt), *(workData->stats));
  return 0;
}

void Subsumption::parallelStrengthening(CoprocessorData& data)
{
  //fullStrengthening(data);
  cerr << "c parallel strengthening with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector< struct SubsumeStatsData > localStats (controller.size());
  vector<Job> jobs( controller.size() );
  vector< SpinLock > var_locks (data.nVars() + 1); // 1 extra SpinLock for data
  unsigned int queueSize = strengthening_queue.size();
  unsigned int partitionSize = strengthening_queue.size() / controller.size();
  
  for ( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    cerr << "c p s thread " << i << " running from " << workData[i].start << " to " << workData[i].end << endl;
    workData[i].data  = &data; 
    workData[i].var_locks = & var_locks;
    localStats[i].removedLiterals = 0;
    localStats[i].strengthSteps = 0;
    localStats[i].strengthTime = 0;
    workData[i].stats = & localStats[i];
    jobs[i].function  = Subsumption::runParallelStrengthening;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );

  //propagate units
  propagation.propagate(data, true);
}

void* Subsumption::runParallelStrengthening(void* arg)
{
    SubsumeWorkData* workData = (SubsumeWorkData*) arg;
    workData->subsumption->par_strengthening_worker(*(workData->data),workData->start,workData->end, *(workData->var_locks), *(workData->stats));
    return 0;
}
