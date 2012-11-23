/*******************************************************************[HiddenTautologyElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/HiddenTautologyElimination.h"

static const char* _cat = "COPROCESSOR 3 - HTE";

static IntOption opt_steps  (_cat, "cp3_hte_steps",  "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX));

using namespace Coprocessor;

HiddenTautologyElimination::HiddenTautologyElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller )
: Technique( _ca, _controller )
, steps(opt_steps)
, activeFlag(0)
{
}

void HiddenTautologyElimination::eliminate(CoprocessorData& data)
{
  if( ! isInitializedTechnique() ) {
    initializedTechnique(); 
  }
  
  // get active flags
  activeFlag.resize( data.nVars(), 0 );
  BIG big;
  
  // TODO: collect flags from clauses that are binary and not modified?
  
  // here, use only clauses of the formula handling learnt clauses is more complicated!
  big.create(ca,data,data.getClauses() );

  if( false ) {
  fprintf(stderr, "implications:\n");
  // debug output - print big
  for( Var v = 0 ; v < data.nVars(); ++v )
  {
    Lit l         = mkLit(v,false);
    Lit* list     = big.getArray(l);
    uint32_t size = big.getSize(l);
    printLit(l);
    fprintf(stderr, " -> ");
    for( int i = 0 ; i < size; ++ i )
    {
      printLit( list[i] );
      fprintf(stderr, ", ");   
    }
    l    = mkLit(v,true);
    list = big.getArray(l);
    size = big.getSize(l);
    fprintf(stderr, "\n");
    printLit(l);
    fprintf(stderr, " -> ");
    for( int i = 0 ; i < size; ++ i )
    {
      printLit( list[i] );
      fprintf(stderr, ", ");   
    }
  }
  fprintf(stderr, "\n");  
  }
  
  // get active variables
  if( lastDeleteTime() == 0 ) 
    for( Var v = 0; v < data.nVars(); ++ v ) activeVariables.push_back(v);
  else data.getActiveVariables( lastDeleteTime(), activeVariables);
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
  const bool talk = false;
  
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
  
  if( talk ) fprintf(stderr, "c HTE from %d to %d out of %d\n", start, end, activeVariables.size());
  
  for (uint32_t index = start; index < end; ++index)
  {
    if( steps == 0 && !data.unlimited() ) break; // stop if number of iterations has been reached
    const Var v = activeVariables[index];
    if( talk )  fprintf(stderr, "c HTE on variable %d\n", v+1);
    
    // fill hlaArrays, check for failed literals
    if( true ) {
    Lit unit = fillHlaArrays(v,big,posArray,negArray,litQueue,doLock);
    if( unit != lit_Undef ) {
      if( doLock ) data.lock();
      lbool result = data.enqueue(unit);
      if( doLock ) data.unlock();
      if( result == l_False ) return;
      continue; // no need to check a variable that is unit!
    }
    }
    hiddenTautologyElimination(v,data,big,posArray,negArray,doStatistics,doLock);
  }
}

void HiddenTautologyElimination::initClause( const CRef cr )
{
  return;
  /*
  const Clause& c = ca[cr];
  if (!c.is_ignored()) // TODO: && c.modified()
  {
    for( int i = 0 ; i < c.size(); ++ i )
      activeVariables.push_back( var(c[i]));
  }
  */
}


void HiddenTautologyElimination::parallelElimination(CoprocessorData& data, BIG& big)
{
  cerr << "c parallel HTE with " << controller.size() << " threads" << endl;
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
  const bool talkMuch = false;
  bool didSomething = false;
  // run for both polarities
  for( uint32_t pol = 0; pol < 2; ++ pol )
  {
    // fill for positive variable
    const Lit i = mkLit(v,pol == 0 ? false : true );
    MarkArray& hlaArray = (pol == 0 ) ? hlaPositive : hlaNegative;
    
    if( false ) {
    fprintf(stderr, "before HTE step (filled hla arrays): " );
    printLit(i);
    fprintf(stderr, " tagged: ");
    for( Var v = 0 ; v < data.nVars(); ++v )
      for( int p = 0 ; p < 2; ++ p )
	if( hlaArray.isCurrentStep( toInt(mkLit(v,p)) ) ) { printLit( mkLit(v,p)); fprintf(stderr, " "); }
    fprintf(stderr, "\n");
    }
    
    // hlaArray.nextStep(); // TODO: this check seems to be not necessary!
    
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
        
        if( false ) {
    fprintf(stderr, "during HTE step (created hla arrays): " );
    printLit(i);
    fprintf(stderr, " tagged: " );
    for( Var v = 0 ; v < data.nVars(); ++v )
      for( int p = 0 ; p < 2; ++ p )
	if( hlaArray.isCurrentStep( toInt(mkLit(v,p)) ) ) { printLit( mkLit(v,p)); fprintf(stderr, " "); }
    fprintf(stderr, "\n");
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
                if ( cl.can_be_deleted() ) continue;
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
		  // FIXME TODO have modified flag here! assert( false && "Modified flag for clauses is not implemented yet!" );
		}
            }
  }
  return didSomething;
}

Lit HiddenTautologyElimination::fillHlaArrays(Var v, BIG& big, MarkArray& hlaPositive, MarkArray& hlaNegative, Lit* litQueue, bool doLock)
{
  const bool talkMuch = false;
  Lit *head, *tail; // maintain the hla queue

  // create both the positive and negative array!
  for( uint32_t pol = 0; pol < 2; ++ pol )
  {
    // fill for positive variable
    const Lit i = mkLit(v, pol == 0 ? false : true);
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

bool HiddenTautologyElimination::hlaMarkClause(const Minisat::CRef cr, BIG& big, MarkArray& markArray, Lit* litQueue)
{
  const Clause& clause = ca[cr]; 
  if( clause.size() < 3 ) return false; // do not work on binary and smaller clauses!
  
  Lit *head, *tail; // indicators for the hla queue
  head = litQueue; tail = litQueue;
  // markArray with all literals of the clause
  for( uint32_t j = 0 ; j < clause.size(); ++ j ) {
    const Lit clLit = clause[j];
    markArray.setCurrentStep( toInt(clLit));	// mark current literal
    *(head++) = clLit;				// add literal to the queue
  }

      while( tail < head ) {
	const Lit lit = *(tail++);
	const Lit* jList = big.getArray(~lit);
	const uint32_t jListSize = big.getSize(~lit);
	
	for( uint32_t j = 0 ; j < jListSize; ++j ) {
	  const Lit jLit = ~jList[j];
	  if( clause.size() == 2 ) {
	    // do not remove the binary clause that is responsible for the current edge
	    if( lit == clause[0] && ~jLit == clause[1] ) continue;
	    if( lit == clause[1] && ~jLit == clause[0] ) continue;
	  }
	  
	  if( ! markArray.isCurrentStep( toInt(jLit) ) ) {
	    if( markArray.isCurrentStep( toInt(~jLit) ) ) {
	      if( clause.size() == 2 )
                big.removeEdge ( clause[0], clause[1] );
	      return true;
	    }
	    markArray.setCurrentStep( toInt(jLit) );
	    *(head++) = jLit;
	  }
	}
      }
 
  return false;
}

bool HiddenTautologyElimination::hlaMarkClause(vec< Lit >& clause, BIG& big, MarkArray& markArray, Lit* litQueue, bool addLits)
{
  if( clause.size() < 3 ) return false; // do not work on binary and smaller clauses!
  
  Lit *head, *tail; // indicators for the hla queue
  head = litQueue; tail = litQueue;
  // markArray with all literals of the clause
  for( uint32_t j = 0 ; j < clause.size(); ++ j ) {
    const Lit clLit = clause[j];
    markArray.setCurrentStep( toInt(clLit));	// mark current literal
    *(head++) = clLit;				// add literal to the queue
  }

      while( tail < head ) {
	const Lit lit = *(tail++);
	const Lit* jList = big.getArray(~lit);
	const uint32_t jListSize = big.getSize(~lit);
	
	for( uint32_t j = 0 ; j < jListSize; ++j ) {
	  const Lit jLit = ~jList[j];
	  if( clause.size() == 2 ) {
	    // do not remove the binary clause that is responsible for the current edge
	    if( lit == clause[0] && ~jLit == clause[1] ) continue;
	    if( lit == clause[1] && ~jLit == clause[0] ) continue;
	  }
	  
	  if( ! markArray.isCurrentStep( toInt(jLit) ) ) {
	    if( markArray.isCurrentStep( toInt(~jLit) ) ) {
	      if( clause.size() == 2 )
                big.removeEdge ( clause[0], clause[1] );
	      return true;
	    }
	    markArray.setCurrentStep( toInt(jLit) );
	    if( addLits ) clause.push(jLit); // add literals to the vector?
	    *(head++) = jLit;
	  }
	}
      }
 
  return false;
}


bool HiddenTautologyElimination::alaMarkClause(const CRef cr, CoprocessorData& data, MarkArray& markArray, MarkArray& helpArray)
{
  vec<Lit> lits;
  const Clause& c = ca[cr];
  for (int i = 0 ; i < c.size(); ++ i ) lits.push(c[i]);
  return alaMarkClause(lits,data,markArray,helpArray,false);
}

bool HiddenTautologyElimination::alaMarkClause(vec<Lit>& clause, CoprocessorData& data, MarkArray& markArray, MarkArray& helpArray, bool addLits)
{
  helpArray.nextStep();
  deque <Lit> queue; // TODO: build heap here!
  for ( int i = 0 ; i < clause.size(); ++ i ) {
    helpArray.setCurrentStep(toInt(clause[i]));
    markArray.setCurrentStep(toInt(clause[i]));
    queue.push_back(clause[i]);
  }
  
  while( !queue.empty() ) { // do not use a queue, but a heap!!
    const Lit l = queue.front(); queue.pop_front(); 
    helpArray.reset(toInt(l));
    
    vector<CRef>& list = data.list(l);
    for( int i = 0 ; i < list.size(); ++ i ) {
      const Clause& c = ca[ list[i] ];
      Lit l1 = lit_Undef;
      for( int j = 0 ; j < c.size(); ++ j ) {
	const Lit l2 = c[j];
	if( l2 == l ) continue;
	if( ! markArray.isCurrentStep(toInt(l2)) ) {
	 if( l1 == lit_Undef ) l1 = l2;  // remember missing literal!
	 else {l1 = lit_Error; break;}     // there is more than one literal that does not fit!
	}
      }
      if( l1 == lit_Undef ) return true; // ATE, since all literals are inside of the analyzed clause clause (is subsumed!)
      if( l1 != lit_Undef && l1 != lit_Error ) {
	if( markArray.isCurrentStep( toInt(l1) ) ) return true; // found ATE, the clause can be removed from the formula!
	if( ! markArray.isCurrentStep(toInt(~l1)) ) {
	  if( addLits ) clause.push(~l1); // add literal to array and to vector
	   markArray.setCurrentStep( toInt(~l1) );
	}
	if( !helpArray.isCurrentStep( toInt(~l1) ) ) {
	  queue.push_back(~l1); 
	  helpArray.setCurrentStep( toInt(~l1) );
	}
	
      }
    }
  }
  
}

