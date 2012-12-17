/**********************************************************************************[Subsumption.cc]
Copyright (c) 2012, Norbert Manthey, Kilian Gebhardt, Max Löwen, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"

using namespace Coprocessor;

static const char* _cat = "CP3 SUBSUMPTION";
// options
static BoolOption  opt_naivStrength    (_cat, "cp3_naiveStength", "use naive strengthening", false);

Subsumption::Subsumption( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation )
: Technique( _ca, _controller )
, propagation( _propagation )
{
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
      fullStrengthening(data);
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
                if (c.size() == ca[list[i]].size() && cr > list[i])
                {
                    c.set_delete(true);
                    if (!c.learnt() && ca[list[i]].learnt())
                        ca[list[i]].set_learnt(false);
                    continue;
                }
                ca[list[i]].set_delete(true); 
                if( doStatistics ) data.removedClause(list[i]);  // tell statistic about clause removal
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
 */
void Subsumption :: par_subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, vector<CRef> & to_delete, vector< CRef > & set_non_learnt)
{
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
	        } else  if (c.ordered_subsumes(ca[list[i]])) {
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
                        c.set_delete(true);
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
            }
        }
        //set can_subsume to false
        c.set_subsume(false);
        
        // add Clause to non-learnt-queue
        if (learnt_subsumed_non_learnt)
            set_non_learnt.push_back(cr);
    }    
}

bool Subsumption::hasToStrengthen() const
{
  return strengthening_queue.size() > 0;
}

/** runs a fullstrengthening on strengthening_queue, needs occurrencelists (naive strengthening seems to be faster, TODO: strengthening on literals with minimal occurrences)
 * @param data occurrenceslists
 * @return 
 */
lbool Subsumption::fullStrengthening(CoprocessorData& data)
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
    //for every clause:
    for (int i = 0; i < strengthening_queue.size(); ++i)
    {
        Clause &c = ca[strengthening_queue[i]];
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
                if (ca[list[k]].can_be_deleted())     // dont check if this clause can be deleted
                    continue;
                if (c.ordered_subsumes(ca[list[k]]))    // check for subsumption
                {
                    ca[list[k]].remove_lit(neg_lit);     // strengthen clause //TODO are UNIT-Clauses consistent?
                    if(ca[list[k]].size() == 1)
                    {
                      // propagate if clause is unit
                      data.enqueue(ca[list[k]][0]);
                      propagation.propagate(data, true);
                    }
                    else 
                    {
                        // add clause since it got smaller and could subsume to subsumption_queue
                        clause_processing_queue.push_back(list[k]);
                    
                        // update occurrences
                        list[k] = list[list.size() - 1];
                        list.pop_back();    // shrink vector
                    }
                }
            }
            c[j] = ~neg_lit;    // change the negated lit back
        }
    }
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
  int si, so;           // indices used for "can be strengthened"-testing
  int negated_lit_pos;  // index of negative lit, if we find one
  for (; end > start;)
  {
    --end;
    CRef cr = strengthening_queue[end];
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
        // if neglit found and we went through all lits of strengthener, then the other clause can be strengthened
        if(!other.can_subsume())
        {
          // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
          other.set_subsume(true);
          clause_processing_queue.push_back(list[j]);
        }
        // update occurrence-list for this lit (this must be done after pushing to subsumptionqueue)
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
        other.remove_lit(other[negated_lit_pos]);   // remove lit from clause (this must be done after updating occurrences)
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
        // other can be strengthened
        if(!other.can_subsume())
        {
          // if the flag was set, this clause is allready in the subsumptionqueue, if not, we need to add this clause as it could subsume again
          other.set_subsume(true);
          clause_processing_queue.push_back(list_neg[j]);
        }
        // update occurrence-list for this lit (this must be done after pushing to subsumptionqueue)
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
        other.remove_lit(other[negated_lit_pos]);   // remove lit from clause (this must be done after updating occurrences)
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


void Subsumption::parallelSubsumption(CoprocessorData& data)
{
  cerr << "c parallel subsumption with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  vector< vector < CRef > > toDeletes  (controller.size());
  vector< vector < CRef > > nonLearnts (controller.size());
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
            c.set_delete(true);
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
  workData->subsumption->par_subsumption_worker(*(workData->data),workData->start,workData->end, *(workData->to_delete), *(workData->set_non_learnt));
  return 0;
}

void Subsumption::parallelStrengthening(CoprocessorData& data)
{
  //fullStrengthening(data);
  cerr << "c parallel strengthening with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  //TODO partition work

  for ( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].data  = &data; 
    workData[i].start = 0; //TODO set i-th partition limits
    workData[i].end   = 0; 
    jobs[i].function  = Subsumption::runParallelStrengthening;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );
  
}

void* Subsumption::runParallelStrengthening(void* arg)
{
    SubsumeWorkData* workData = (SubsumeWorkData*) arg;
    workData->subsumption->strengthening_worker(*(workData->data),workData->start,workData->end, false);
    return 0;
}
