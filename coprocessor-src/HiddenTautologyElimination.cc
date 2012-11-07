/*******************************************************************[HiddenTautologyElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/HiddenTautologyElimination.h"

static const char* _cat = "COPROCESSOR 3 - HTE";

static IntOption opt_steps  (_cat, "cp3_hte_steps",  "Number of steps that are allowed per iteration", -1, IntRange(-1, INT32_MAX));

using namespace Coprocessor;

HiddenTautologyElimination::HiddenTautologyElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller )
: Technique( _ca, _controller )
, steps(opt_steps)
, activeFlag(0)
{
}

void HiddenTautologyElimination::eliminate(CoprocessorData& data)
{
  // get active flags
  activeFlag.resize( data.nVars(), 0 );
  BIG big;
  
  // TODO: collect flags from clauses that are binary and not modified?
  
  // here, use only clauses of the formula handling learnt clauses is more complicated!
  big.create(ca,data,data.getClauses() );
  // get active variables
  data.getActiveVariables( this->lastDeleteTime() ,activeVariables);
  // TODO: define an order?
  
  // run subsumption for the whole queue
  if( controller.size() > 0 ) {
    parallelElimination(data, big); // use parallel, is some conditions have been met
    data.correctCounters();
  } else {
    elimination_worker(data, 0, activeVariables.size(), big);
  }
  
  // get delete timer
  updateDeleteTime( data.getMyDeleteTimer() );
  
  // clear queue afterwards
  activeVariables.clear();
}



bool HiddenTautologyElimination::hasToEliminate()
{
  return activeVariables.size() > 0; 
}

void HiddenTautologyElimination::elimination_worker (CoprocessorData& data, uint32_t start, uint32_t end, BIG& big, bool doStatistics, bool doLock)
{
  
  if( data.randomized() ) { // shake order, if randomized is wanted
    const uint32_t diff = end - start;
    for (uint32_t v = start; v < end; ++v) {
      int newPos = start + rand() % diff;
      const uint32_t tmp = activeVariables [v]; activeVariables [v] = activeVariables [newPos]; activeVariables [newPos] = tmp;
    }
  }
  
  // create markArrays!
  MarkArray posArray;
  MarkArray negArray;
  posArray.create( data.nVars() * 2);
  negArray.create( data.nVars() * 2);
  Lit* litQueue = (Lit*) malloc( data.nVars() * 2 );
  MethodFree litQueueFree(litQueue); // will automatically free the resources at a return!
  
  for (uint32_t index = start; index < end; ++index)
  {
    if( steps == 0 && !data.unlimited() ) break; // stop if number of iterations has been reached
    const Var v = activeVariables[v];
    
    // fill hlaArrays, check for failed literals
    Lit unit = fillHlaArrays(v,big,posArray,negArray,litQueue,doLock);
    if( unit != lit_Undef ) {
      if( doLock ) data.lock();
      lbool result = data.enqueue(unit);
      if( doLock ) data.unlock();
      if( result == l_False ) return;
      continue; // no need to check a variable that is unit!
    }

    hiddenTautologyElimination(v,data,big,posArray,negArray,doStatistics,doLock);
  }
}

void HiddenTautologyElimination::initClause( const CRef cr )
{
  const Clause& c = ca[cr];
  if (!c.can_be_deleted()) // TODO: && c.modified()
  {
    for( int i = 0 ; i < c.size(); ++ i )
      activeVariables.push_back( var(c[i]));
  }
    
}


void HiddenTautologyElimination::parallelElimination(CoprocessorData& data, BIG& big)
{
  cerr << "c parallel subsumptoin with " << controller.size() << " threads" << endl;
  EliminationData workData[ controller.size() ];
  vector<Job> jobs( controller.size() );
  
  unsigned int queueSize = activeVariables.size();
  unsigned int partitionSize = activeVariables.size() / controller.size();
  // setup data for workers
  for( int i = 0 ; i < controller.size(); ++ i ) {
    workData[i].hte   = this; 
    workData[i].data  = &data; 
    workData[i].big   = &big;
    workData[i].start = i * partitionSize; 
    workData[i].end   = (i + 1 == controller.size()) ? queueSize : (i+1) * partitionSize; // last element is not processed!
    jobs[i].function  = HiddenTautologyElimination::runParallelElimination;
    jobs[i].argument  = &(workData[i]);
  }
  controller.runJobs( jobs );
}

void* HiddenTautologyElimination::runParallelElimination(void* arg)
{
  EliminationData* workData = (EliminationData*) arg;
  // run without statistics and with locking
  workData->hte->elimination_worker( *(workData->data),workData->start,workData->end,*(workData->big),false,true);
  return 0;
}

bool HiddenTautologyElimination::hiddenTautologyElimination(Var v, CoprocessorData& data, BIG& big, MarkArray& hlaPositive, MarkArray& hlaNegative, bool statistic, bool doLock)
{
  const bool talkMuch = true;
  bool didSomething = false;
  // run for both polarities
  for( uint32_t pol = 0; pol < 2; ++ pol )
  {
    // fill for positive variable
    const Lit i = mkLit(v,pol);
    MarkArray& hlaArray = (pol == 0 ) ? hlaPositive : hlaNegative;
    hlaArray.nextStep();
    if( talkMuch ) cerr << "c [HTE] hla for " << toInt(i) << endl;
    
    // iterate over binary clauses with occurences of the literal i
    const vector<CRef>& iList = data.list(i);

	// transitive reduction of BIG
        for ( uint32_t k = 0; k < iList.size(); k++ )
        {  
            const CRef clsidx = iList[k];
            Clause& cl = ca[ clsidx ];
            if ( cl.can_be_deleted() ) continue; // todo have another flag ignored!
            if ( cl.size() == 2) {
                bool remClause = false;
                for ( uint32_t j = 0; j < 2; j++ ) {
		    // check for tautology
		    const Lit clLit = cl[j];
                    if ( hlaArray.isCurrentStep( toInt(~clLit) ) ) {
			const Lit literal0 = cl[0];
			const Lit literal1 = cl[1];

			// execute only if not in parallel mode!
			if( !doLock ) big.removeEdge( literal0, literal1);
                       cl.set_delete(true);
			didSomething = true;
                       remClause = true;
                       break;
                    }
                }
// TODO enable for parallel?		steps = (steps>0) ? steps - 1 : 0; // limit
                // if clause has been removed from its lists, update!
                if ( remClause ) {
		    // TODO: statistics removed clause
		    if( statistic ) data.removedClause(clsidx);
                    cl.set_delete(true);
                    k--;
                }
            }
        }
        // apply HTE to formula
        // set hla array for all the binary clauses
	Lit* binaryI = big.getArray( ~i );
	const uint32_t binaryIsize = big.getSize(~i);
        for ( uint32_t j = 0; j < binaryIsize; j++ ) {
	  if( talkMuch ) cerr << "c mark " << toInt( ~binaryI[j] ) << " with " << toInt(i) << endl;
	  hlaArray.setCurrentStep( toInt( ~binaryI[j] ) );
	}
   // TODO: port other code     
            for ( uint32_t k = 0; k < iList.size(); k++ )
            {
                CRef clsidx = iList[k];
                Clause& cl = ca[ clsidx ];
                if ( cl.is_ignored() ) continue;
                bool ignClause = false;
		bool changed = false;
                if ( cl.size() > 2 ) {
// TODO enable for parallel?		    steps = (steps>0) ? steps - 1 : 0; // limit

                    for ( uint32_t j = 0; j < cl.size(); j++ ) {
		      const Lit clauseLiteral = cl[j];
                        if ( hlaArray.isCurrentStep( toInt( ~clauseLiteral) ) ) {
			    didSomething = true;
                            ignClause = true;
                            cl.set_delete(true); // TODO remove from occurence lists?
			    if( statistic ) data.removedClause(clsidx);
                            break;
                        }
                        else if ( clauseLiteral != i && hlaArray.isCurrentStep(  toInt(clauseLiteral) ) ) {
                          didSomething = true;  
			  // remove the clause from the occurence list of the literal that is removed
			  if( doLock ) data.lock();
			  data.removeClauseFrom( clsidx, clauseLiteral );
			  if( doLock ) data.unlock();

                            // remove the literal
                          changed = true;
			  if( statistic ) data[ clauseLiteral ] --;
			    cl.removePositionUnsorted(j);
                            // update the index
                            j--;
			    
                            if ( cl.size() == 1 ) {
			      if( doLock ) data.lock(); 
                                if ( l_False == data.enqueue(cl[0]) ) {
				   if( doLock ) data.unlock();
                                  return didSomething;
                                }
                             if( doLock ) data.unlock();    
                            } else if ( cl.size() == 0 ) {
			      if( doLock ) data.lock(); 
			      data.setFailed();
			      if( doLock ) data.unlock(); 
			      return didSomething;
                            }
                        }
                    }
                }

                // delete the clause, update the index
                if (ignClause) {
		    didSomething = true;
                    k--;
                } else if( changed ) {
		  // update information about the final clause (EE,BIG,SUSI)
		  assert( false && "Modified flag for clauses is not implemented yet!" );
		}
            }
  }
  return didSomething;
}

Lit HiddenTautologyElimination::fillHlaArrays(Var v, BIG& big, MarkArray& hlaPositive, MarkArray& hlaNegative, Lit* litQueue, bool doLock)
{
  const bool talkMuch = true;
  Lit *head, *tail; // maintain the hla queue

  // create both the positive and negative array!
  for( uint32_t pol = 0; pol < 2; ++ pol )
  {
    // fill for positive variable
    const Lit i = mkLit(v,pol);
    MarkArray& hlaArray = (pol == 0 ) ? hlaPositive : hlaNegative;
    hlaArray.nextStep();
    if( talkMuch ) cerr << "c [HTE] fill hla for " << toInt(i) << endl;
    
    hlaArray.setCurrentStep( toInt(i) );
    // process all literals in list (inverse BIG!)
    const Lit* posList = big.getArray(~i);
    const uint32_t posSize = big.getSize(~i);
    for( uint32_t j = 0 ; j < posSize; ++ j )
    {
      const Lit imp = ~(posList[j]);
      if( talkMuch ) cerr << "c [HTE] look at literal " << toInt(imp) << endl;
      if ( hlaArray.isCurrentStep( toInt(imp) ) ) continue;
      
      head = litQueue; tail = litQueue;
      *(head++) = imp;
      hlaArray.setCurrentStep( toInt(imp ) );
      if( talkMuch ) cerr << "c [HTE] add to array: " << toInt(imp) << endl;
      // process queue
      while( tail < head ) {
	const Lit lit = *(tail++);
	steps = ( steps > 0 ) ? steps - 1 : 0;
	const Lit* kList = big.getArray(~lit);
	const uint32_t kListSize = big.getSize(~lit);
	for( uint32_t k = 0 ; k < kListSize; ++k ) {
	  const Lit kLit = ~kList[k];
	  if( ! hlaArray.isCurrentStep( toInt(kLit) ) ) {
	    if ( hlaArray.isCurrentStep( toInt( ~kLit) ) ) {
	      if( talkMuch ) cerr << "c [HTE] failed literal: " << toInt(i) << endl;
	      return i; // return the failed literal
	    }
	    
	    hlaArray.setCurrentStep( toInt(kLit) );
	    if( talkMuch ) { cerr << "c [HTE] add to array " << toInt(i) << " for " << toInt(kLit) << endl;
	    }
	    *(head++) = kLit;
	  }
	}
      }
      if( talkMuch ) cerr << "c [HTE] remove from array: " << toInt(imp) << endl;
      hlaArray.reset( toInt(imp) );
    } // end for pos list
  }

  return lit_Undef;
}
