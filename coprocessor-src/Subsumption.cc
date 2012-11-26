/**********************************************************************************[Subsumption.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Subsumption.h"

using namespace Coprocessor;

Subsumption::Subsumption( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller )
: Technique( _ca, _controller )
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



bool Subsumption::hasToSubsume()
{
  return clause_processing_queue.size() > 0; 
}

lbool Subsumption::fullSubsumption(CoprocessorData& data)
{
  // run subsumption for the whole queue
  if( controller.size() > 0 && (clause_processing_queue.size() > 100000 || ( clause_processing_queue.size() > 50000 && 10*data.nCls() > 22*data.nVars() ) ) ) {
    parallelSubsumption(data); // use parallel, is some conditions have been met
    data.correctCounters();    // 
  } else {
    subsumption_worker(data,0,clause_processing_queue.size());
  }

  // no result to tell to the outside
  return l_Undef; 
}

void Subsumption :: subsumption_worker (CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics)
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

bool Subsumption::hasToStrengthen()
{
  return strengthening_queue.size() > 0;
}

lbool Subsumption::fullStrengthening(CoprocessorData& data)
{
  strengthening_worker(data, 0, strengthening_queue.size());
    //for every clause:
/*    for (int i = 0; i < strengthening_queue.size(); ++i)
    {
        Clause &c = ca[strengthening_queue[i]];
        if (c.can_be_deleted())
            continue;
        // for every literal in this clause:
        for (int j = 0; j < c.size(); ++j)
        {
            // negate this literal and check for subsumptions for every occurance of his negation:
            Lit neg_lit = ~c[j];
            c[j] = neg_lit;     // change temporaly lit for subsumptiontest
            // get ocurances of this lit
            vector<CRef> & list = data.list(neg_lit);

            for (unsigned int k = 0; k < list.size(); ++k)
            {
                if (ca[list[k]].can_be_deleted())     // dont check if this clause can be deleted
                    continue;
                if (c.ordered_subsumes(ca[list[k]]))    // check for subsumption
                {
//printf(".");
                    ca[list[k]].remove_lit(neg_lit);     // strenghten clause
                    // add clause since it got smaler and could subsume to subsumption_queue
                    clause_processing_queue.push_back(list[k]);
                    // update occurances
                    list[k] = list[list.size() - 1];
                    list.pop_back();    // shrink vector
                }
            }
            c[j] = ~neg_lit;    // change the negated lit back
        }
    }
  // no result to tell to the outside
//printf("\n");*/
  return l_Undef;   
}

void Subsumption::strengthening_worker(CoprocessorData& data, unsigned int start, unsigned int end, bool doStatistics)
{
  int si, so;
  int negated_lit_pos;  //the position for neglit cant be 0, so we will use this as "neglit not found"
  for (; end > start;)
  {
    --end;
    CRef cr = strengthening_queue[end];
    Clause& strengthener = ca[cr];
    if (strengthener.can_be_deleted())
      continue;
    //find Lit with least occurrences and his occurances
    Lit min = strengthener[0];
    vector<CRef>& list = data.list(min);        // occurances of minlit from strengthener
    vector<CRef>& list_neg = data.list(~min);   // occurances of negated minlit from strengthener
    // test every clause, where the minimum is, if it can be strenghtened
    for (unsigned int j = 0; j < list.size(); ++j)
    {
      Clause& other = ca[list[j]];
      if (other.can_be_deleted())
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
            // if it's the first neglit found, save his position
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
//printf(".");
        other.remove_lit(other[negated_lit_pos]);
        // add clause to subsumptionqueue since it got smaler and could subsume
        clause_processing_queue.push_back(list[j]);
        // update occurance-list for this lit
        list[j] = list[list.size() - 1];            // get the latest clause from list and put it here
        list.pop_back();                            // shrink vector
      }
    }
    // now test for the occurances of negated min, we only need to test, if all lits after min in strengthener are also in other
    for (unsigned int j = 0; j < list_neg.size(); ++j)
    {
      Clause& other = ca[list_neg[j]];
      if (other.can_be_deleted())
        continue;
      si = 1;
      so = 0;
      negated_lit_pos = 0;
      // find neg_lit_pos (it should be here, because other is from the occurances of ~min)
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
//printf(".");
        other.remove_lit(other[negated_lit_pos]);
        // add clause to subsumptionqueue since it got smaler and could subsume
        clause_processing_queue.push_back(list_neg[j]);
        // update occurance-list for this lit
        list_neg[j] = list_neg[list_neg.size() - 1];            // get the latest clause from list and put it here
        list_neg.pop_back();                            // shrink vector
      }
    }
  }
//printf("\n");
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
  cerr << "c parallel subsumptoin with " << controller.size() << " threads" << endl;
  SubsumeWorkData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  
  unsigned int queueSize = clause_processing_queue.size();
  unsigned int partitionSize = clause_processing_queue.size() / controller.size();
  // setup data for workers
  for( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].subsumption = this; 
    workData[i].data  = &data; 
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    jobs[i].function  = Subsumption::runParallelSubsume;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );
}

void* Subsumption::runParallelSubsume(void* arg)
{
  SubsumeWorkData* workData = (SubsumeWorkData*) arg;
  workData->subsumption->subsumption_worker(*(workData->data),workData->start,workData->end, false);
  return 0;
}
