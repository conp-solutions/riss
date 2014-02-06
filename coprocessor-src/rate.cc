/*****************************************************************************************[rate.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/rate.h"

using namespace Coprocessor;

RATElimination::RATElimination( CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver, Propagation& _propagation )
: Technique(_config, _ca,_controller)
, data(_data)
, solver( _solver)
, propagation( _propagation )

, rateSteps(0)
, rateCandidates(0)
, remRAT(0)
, remAT(0)
, remHRAT(0)
{
  
}


void RATElimination::reset()
{
  
}

bool RATElimination::process()
{
  MethodClock mc( rateTime );
  
  LitOrderRATEHeapLt comp(data, config.rate_orderComplements); // use this sort criteria!
  Heap<LitOrderRATEHeapLt> rateHeap(comp);  // heap that stores the variables according to their frequency (dedicated for BCE)
  
  // setup own structures
  rateHeap.addNewElement(data.nVars() * 2); // set up storage, does not add the element
  rateHeap.clear();

  // structures to have inner and outer round
  MarkArray nextRound;
  vector<Lit> nextRoundLits;
  nextRound.create(2*data.nVars());
  // init
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    if( data.doNotTouch(v) ) continue; // do not consider variables that have to stay fixed!
    if( data[  mkLit(v,false) ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,false))) )  nextRoundLits.push_back( mkLit(v,false) );
    if( data[  mkLit(v,true)  ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,true))) )   nextRoundLits.push_back( mkLit(v,true) );
  }
  data.ma.resize(2*data.nVars());
  data.ma.nextStep();
  
  do {
    
    // re-init heap
    for( int i = 0 ; i < nextRoundLits.size(); ++ i ) {
      const Lit l = nextRoundLits[i];
      assert( !rateHeap.inHeap(toInt(l)) && "literal should not be in the heap already!" );
      rateHeap.insert( toInt(l) );
    }
    nextRoundLits.clear();
    nextRound.nextStep();
    
    
    // do BCE on all the literals of the heap
    while (rateHeap.size() > 0 && (data.unlimited() || config.bceLimit > rateSteps) && !data.isInterupted() )
    {
      // interupted ?
      if( data.isInterupted() ) break;
      
      const Lit right = toLit(rateHeap[0]);
      assert( rateHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
      rateHeap.removeMin();
      
      if( data.doNotTouch(var(right)) ) continue; // do not consider variables that have to stay fixed!

      // check whether a clause is a tautology wrt. the other clauses
      const Lit left = ~right; // complement
      if( config.opt_bce_debug ) cerr << "c BCE work on literal " << right << " with " << data.list(right).size() << " clauses " << endl;
      data.lits.clear(); // used for covered literal elimination
      for( int i = 0 ; i < data.list(right).size(); ++ i ) 
      {
	Clause& c = ca[ data.list(right)[i] ];
	if( c.can_be_deleted() || (!config.bceBinary && c.size() == 2 && !config.opt_bce_cle ) ) continue; // do not work on uninteresting clauses!
	
	if( config.opt_bce_cle ) {
	  data.lits.clear(); // collect the literals that could be removed by CLE
	  for( int k = 0 ; k < c.size(); ++k ) if( right != c[k] ) data.lits.push_back( c[k] );
	}
	
	bool canCle = false; // yet, no resolvent has been produced -> cannot perform CLE
	bool isBlocked = (c.size() > 2 || config.bceBinary); // yet, no resolvent has been produced -> has to be blocked
	
	if(! isBlocked && !config.opt_bce_cle ) break; // early abort
	
	for( int j = 0 ; j < data.list(left).size(); ++ j )
	{
	  Clause& d = ca[ data.list(left)[j] ];
	  if( d.can_be_deleted() ) continue; // do not work on uninteresting clauses!
	  rateSteps ++;
	  
	}
	
	// if cle took place, there might be something to be propagated
	if( data.hasToPropagate() ) {
	  int prevSize = data.list(right).size();
	  propagation.process(data, true); // propagate, if there's something to be propagated
	  modifiedFormula = modifiedFormula  || propagation.appliedSomething();
	  if( !data.ok() ) return modifiedFormula;
	  if( prevSize != data.list( right ).size() ) i = -1; // start the current list over, if propagation did something to the list size (do not check all clauses of the list!)!
	} 
	
      } // end iterating over all clauses that contain (right)
    }
  
    // perform garbage collection
    data.checkGarbage();  
  
  } while ( nextRoundLits.size() > 0 && (data.unlimited() || config.bceLimit > rateSteps) && !data.isInterupted() ); // repeat until all literals have been seen until a fixed point is reached!
  
  return modifiedFormula;
}

  
void RATElimination::printStatistics(ostream& stream)
{
  cerr << "c [STAT] RATE "  << rateTime.getCpuTime() << " seconds, " << rateSteps << " steps, "
  << rateCandidates << " checked, "
  << remRAT << " rem-RAT, "
  << remAT << "rem-AT, "
  << remHRAT << " rem-HRAT," << endl;
}

void RATElimination::giveMoreSteps()
{
  rateSteps = rateSteps < config.opt_bceInpStepInc ? 0 : rateSteps - config.opt_bceInpStepInc;
}
  
void RATElimination::destroy()
{
  
}