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
  return false;  
}

lbool Subsumption::fullStrengthening(CoprocessorData& data)
{
  return l_Undef;   
}

void Subsumption::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if (c.can_subsume() && !c.can_be_deleted())
    clause_processing_queue.push_back(cr);
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
