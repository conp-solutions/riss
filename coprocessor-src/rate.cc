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
, ratmSteps(0)
, rateCandidates(0)
, remRAT(0)
, remAT(0)
, remHRAT(0)
, remBCE(0)
, remBRAT(0)
, blockCheckOnSameClause(0)

, bcaCandidates(0)
, bcaResolutionChecks(0)
, bcaSubstitue(0)
, bcaSubstitueLits(0)
, bcaFullMatch(0)
, bcaATs(0)
, bcaStrenghening(0)
{
  
}


void RATElimination::reset()
{
  
}

bool RATElimination::process()
{
  if( !config.opt_rate_rate && !config.opt_rate_bcs && !config.opt_rate_ratm && !config.opt_rate_ratm_extended ) return false;
  
  if( ! performSimplification() ) return false; // do not do anything?!
  modifiedFormula = false;
  
  // do not simplify, if the formula is considered to be too large!
  if( !data.unlimited() && ( data.nVars() > config.opt_rate_vars && data.getClauses().size() + data.getLEarnts().size() > config.opt_rate_cls && data.nTotLits() <= config.opt_rate_lits) ) return false;

  // make sure that all clauses in the formula do not contain assigned literals
  if( l_False == propagation.process(data,true) ) {
    return true;
  }

  // re-setup solver! (now, the clause swill not be sorted any longer, due to two-watched literal propagation and move to front strategy)
  reSetupSolver();
  
  if( config.opt_rate_bcs ) { 
    modifiedFormula = blockedSubstitution() || modifiedFormula;
  }

  if( config.opt_rate_rate ) { 
    modifiedFormula = eliminateRAT() || modifiedFormula;
  }
  
  if( config.opt_rate_ratm || config.opt_rate_ratm_extended ) { 
    modifiedFormula = minimizeRAT() || modifiedFormula;
  }
  // clean solver!
  cleanSolver();
  
    // make sure that all clauses in the formula do not contain assigned literals
  if( l_False == propagation.process(data,true) ) {
    return true;
  }
  
  
  return modifiedFormula;
}


bool RATElimination::eliminateRAT()
{
  MethodClock mc( rateTime );
  bool didSomething = false;
  
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
      if( ! nextRound.isCurrentStep( toInt(l) ) ) continue; // has been processed before already
      assert( !rateHeap.inHeap(toInt(l)) && "literal should not be in the heap already!" );
      rateHeap.insert( toInt(l) );
    }
    nextRoundLits.clear();
    nextRound.nextStep();
              
    // do RAT Elimination on all the literals of the heap
    while (rateHeap.size() > 0 && (data.unlimited() || config.rate_Limit > rateSteps) && !data.isInterupted() )
    {
      // interupted ?

      if( data.isInterupted() ) break;
      const Lit right = toLit(rateHeap[0]);
      assert( rateHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
      rateHeap.removeMin();
      
      if( data.doNotTouch(var(right)) ) {continue;}// do not consider variables that have to stay fixed!

      // check whether a clause is a tautology wrt. the other clauses
      const Lit left = ~right; // complement
      DOUT( if( config.opt_rate_debug > 0 ) cerr << endl << "c RATE work on literal " << right << " with " << data.list(right).size() << " clauses " << endl;);
      DOUT( if( config.opt_rate_debug > 3 ) cerr << "current trail: " << data.getTrail() << endl;);
      
      data.lits.clear(); // used for covered literal elimination
      
      for( int i = 0 ; i < data.list(right).size(); ++ i ) 
      {

	Clause& c = ca[ data.list(right)[i] ];
	if ( c.can_be_deleted() || c.learnt() ) continue; // TODO: yet we do not work with learned clauses, because its expensive
	if ( c.size() < config.rate_minSize ) continue; // ignore "small" clauses // TODO have a value for the parameter to disable this limit (e.g. are ther binaray RAT clauses)
	
	rateCandidates ++;
	DOUT( if( config.opt_rate_debug > 0 ) cerr << endl << "c test clause [" << data.list(right)[i] << "] " << c << endl;);
	
	DOUT( if( config.opt_rate_debug > 3 ) {
	  cerr << "c current formula: " << endl;
	  for( int t = 0 ; t < data.getClauses().size(); ++ t ) {
	    if( ! ca[data.getClauses()[t]].can_be_deleted() ) cerr << "[" << data.getClauses()[t] << "] "<< ca[data.getClauses()[t]] << endl;
	  }
	  for( Var v = 0 ; v < data.nVars(); ++v ) {
	    for ( int p = 0 ; p < 2; ++ p ) {
	      const Lit l = mkLit(v,p==0);
	      if( data.list(l).size() > 0 ) {
		cerr << "c list(" << l << "): " << endl;
		for( int t = 0 ; t < data.list(l).size(); ++t ) if( !ca[ data.list(l)[t] ].can_be_deleted() ) cerr << "[" << data.list(l)[t] << "] " << ca[ data.list(l)[t] ] << endl;
		cerr << endl;
	      }
	    }
	  }
	});
	
	// literals to propagate
	data.lits.clear();
	
	for( int j = 0 ; j < c.size(); ++ j) data.lits.push_back( c[j] );
	const int defaultLits = data.lits.size(); // number of lits that can be kept for each resolvent

	solver.detachClause( data.list(right)[i], true ); // detach the clause eagerly
	assert( solver.decisionLevel() == 0 && "check can only be done on level 0" );
	
	/** check whether the clause C is asymmetric tautology
	 */
	
	solver.newDecisionLevel(); // to be able to backtrack
	
	DOUT( if( config.opt_rate_debug > 2 ) cerr << "c enqueue complements in " << data.lits << endl;);
	
	for( int j = 0 ; j < data.lits.size(); ++ j ) solver.uncheckedEnqueue( ~data.lits[j] ); // enqueue all complements

	bool confl = CRef_Undef != solver.propagate(); // check whether unit propagation finds a conflict for (F \ C) \land \ngt{C}, and hence C would be AT
   
	assert( solver.decisionLevel() == 1 && "has to be 1" );
	rateSteps += solver.trail.size(); // approximate effort for propagation

	if( confl == true) { // found a conflict during propagating ~C
	  remAT++;
	  ca[ data.list(right)[i]].sort();		// necessary to ensure a general result -->  RATE is not confluent! (only for benchnmarking!)
	  solver.cancelUntil(0); // backtrack
	 
	  // reference c might not be valid any more (because propagate could have created new clauses via LHBR), hence, use ca[ data.list(right)[i]]
	  for( int j = 0 ; j < ca[ data.list(right)[i]].size(); ++ j ) { // all complementary literals can be tested again
	    if( ! nextRound.isCurrentStep(toInt(~ca[ data.list(right)[i]][j]) ) ) { // if literal not in set for next round, put it there!
	      nextRoundLits.push_back( ~ca[ data.list(right)[i]][j] );
	      nextRound.setCurrentStep( toInt(~ca[ data.list(right)[i]][j]) );
	    }
	  }

	  DOUT( if( config.opt_rate_debug > 1 ) cerr << "c RATE eliminate AT clause [" << data.list(right)[i] << "] " << ca[data.list(right)[i]] << endl;);
	  ca[ data.list(right)[i]] .set_delete(true);
	  data.removedClause( data.list(right)[i] );
	  data.addCommentToProof("AT clause during RATE");
	  data.addToProof(c,true);
	  didSomething = true;
	  
	  continue; // consider next clause
	}
	
	// data.lits contains the complements of the clause C, except literal right.
	data.ma.nextStep();
	for( int j = 0 ; j < data.lits.size(); ++ j ) {
	  data.ma.setCurrentStep( toInt( data.lits[j] ) ); // mark all literals of c except right, for fast resolution checks
	}
	
	DOUT( if( config.opt_rate_debug > 3 ) {
	  cerr << "c current formula: " << endl;
	  for( int t = 0 ; t < data.getClauses().size(); ++ t ) {
	    if( ! ca[data.getClauses()[t]].can_be_deleted() ) cerr << "[" << data.getClauses()[t] << "] "<< ca[data.getClauses()[t]] << endl;
	  }
	});


	DOUT( if( config.opt_rate_debug > 2 ) cerr << "c RATE resolve with " << data.list(left).size() << " clauses" << endl;);
	bool allResolventsRedundant = true;
	bool allTaut = true;
	bool usedBratClause = false;
	for( int j = 0 ; allResolventsRedundant && j < data.list(left).size(); ++ j ) // for all clauses D \in F_{\ngt{l}}
	{
	  Clause& d = ca[ data.list(left)[j] ];
	  if( d.can_be_deleted() ) continue; // no resolvent required
	  rateSteps ++;
	  DOUT( if( config.opt_rate_debug > 2 ) cerr << "c RATE resolve with clause [" << data.list(left)[j] << "]" << d << endl;);
	  bool isTaut = resolveUnsortedStamped( right, d, data.ma, data.lits ); // data.lits contains the resolvent
	  rateSteps += d.size(); // approximate effort for resolution
	  DOUT( if( config.opt_rate_debug > 2 ) cerr << "c RATE resolvent (taut=" << isTaut << ") : " << data.lits << endl;);
	  
	  if( isTaut ) { // if the resolvent is a tautology, then the resolvent is redundant wrt. formula, and we do not need to perform propagation
	    data.lits.resize( defaultLits ); 
	    continue;
	  } // if resolvent is a tautology, then the its also AT (simulates BCE). remove the literals from D from the resolvent again
	  else allTaut = false;

	  // test whether the resolvent is AT
	  DOUT( if( config.opt_rate_debug > 2 ){ 
		  cerr << "c enqueue complements in " << data.lits << endl;
		  cerr << "the propagated trail is " << solver.trail << endl;
	  });
	  
	  confl = false; // confl can be true at this point!
           
      solver.newDecisionLevel(); // to be able to backtrack   
        
      for( int k = defaultLits; k < data.lits.size(); ++ k ){
        if ( solver.value(data.lits[k]) == l_Undef ) solver.uncheckedEnqueue( ~data.lits[k] );// check whether the literal is not propagated 
        else if ( solver.value(data.lits[k]) == l_True) { // check whether the negated literal is already propagated --> conflict!  
             confl = true;
             break;
	     }
      }
      
      if ( confl == false ) confl = CRef_Undef != solver.propagate();// check whether unit propagation finds a conflict for (F \ C) \land \ngt{C}, and hence C would be AT
      solver.cancelUntil(1);    // backtrack just to level 1 to keep first part of the resolvent propagated
      rateSteps += solver.trail.size()-defaultLits; // approximate effort for propagation with respect to the level! 
      
      DOUT( if( config.opt_rate_debug > 2 ) cerr << "c propagate with conflict " << (confl != CRef_Undef ? "yes" : " no") << endl;);

	  if( confl == false ) {
	    // the resolvent is not AT, hence, check whether the resolvent is blocked
	    if( config.opt_rate_brat ) {
	      MethodClock bratMC( bratTime ); // measure time
	      // check whether resolvent is blocked on one of its literals
	      bool isblocked = false;
// 	      bratResolve.nextStep(); //TODO these two lines are not used
// 	      for( int k = 0 ; k < data.lits.size(); ++k ) bratResolve.setCurrentStep( toInt(data.lits[k] ) ); // create markArray for redundant clause
	      for( int k = 0 ; k < data.lits.size(); ++k )
	      {
		const Lit resL = data.lits[k];
		if( data.doNotTouch( var(resL) ) ) continue; // do not perform blocked clause addition on doNotTouch variables
		isblocked = true;

		for( int m = 0 ; m < data.list(~resL).size(); ++ m ) { // resolve with all candidates
		  const Clause& e = ca[ data.list(~resL)[m] ]; 
		  if( e.can_be_deleted() ) continue;
		  if( data.list(right)[i] == data.list(~resL)[m] ) blockCheckOnSameClause ++;
		  
		  bool hasComplement = false; // check resolvent for being tautologic
		  for( int n = 0 ; n < e.size(); ++ n ) {
		    if( e[n] == ~resL ) continue;
		    if( data.ma.isCurrentStep( toInt(~e[n]) ) ) { hasComplement = true; break; }
		  }
		  
		  if( !hasComplement ) { isblocked = false; break; }
		}
		
		if( isblocked == true ) {
		  usedBratClause = true;
		  break; // found a blocking literal
		}
	      }
	      allResolventsRedundant = isblocked; // overloading ... not really AT, but the resolvent is redundant
	      
	    } else {
			
	      allResolventsRedundant = false;	// not AT 
	    }
	  } 
	  else

	  data.lits.resize(defaultLits);	// remove the literals from D again
	  
      if( !data.unlimited() && config.rate_Limit <= rateSteps) {	// check step limits
	    allResolventsRedundant = false; 
	    break; // if limit reached but not all clauses tested, then the clause C is not known to be redundant
	  }
	}
		  
	solver.cancelUntil(0);	// backtrack

	if( allResolventsRedundant ) {	// clause C is RAT, remove it!
	  data.addToExtension(data.list(right)[i], right); 
	  DOUT( if( config.opt_rate_debug > 1 ) cerr << "c RATE eliminate RAT clause [" << data.list(right)[i] << "] " << c << endl;);
	  c.sort();		// necessary to ensure a general result -->  RATE is not confluent! (just for benchmarking)
	  c.set_delete(true);
	  remBRAT = usedBratClause ? remBRAT + 1 : usedBratClause;
	  remRAT = (!allTaut && !usedBratClause) ? remRAT+1 : remRAT;
	  remBCE = ( allTaut) ? remBCE+1 : remBCE;
	  for( int k = 0 ; k < c.size(); ++ k ) {	// all complementary literals can be tested again
	    if( ! nextRound.isCurrentStep(toInt(~c[k]) ) ) {
	      nextRoundLits.push_back( ~c[k] );
	      nextRound.setCurrentStep( toInt(~c[k]) );
	    }
	  }
	  data.addCommentToProof("rat clause during RATE");
	  data.addToProof(c,true);
	  data.removedClause( data.list(right)[i] );
	  didSomething = true;
	} else {
	  solver.attachClause( data.list(right)[i] ); // if clause cannot be removed by RAT Elimination, attach it again!
	}
      } // end iterating over all clauses that contain (right)


 }
  } while ( nextRoundLits.size() > 0 && (data.unlimited() || config.rate_Limit > rateSteps) && !data.isInterupted() ); // repeat until all literals have been seen until a fixed point is reached!

  return didSomething;
}


bool RATElimination::propagateUnit(const Lit& unit, int& trailPosition)
{
  assert( solver.value(unit) == l_Undef && "we only propagate units we did not see before" ); // FIXME is the if-statement below necessary?
  if(  solver.value(unit) == l_Undef ) {
    unitRATM ++;
    const int oldTrailsize = solver.trail.size();

    data.addCommentToProof("remove literals by ATM -- created unit clause");
    data.addUnitToProof(unit);  // add the shrinked unit clause to the proof

    solver.uncheckedEnqueue(unit);
    
    if (CRef_Undef != solver.propagate()) { 
      data.setFailed(); 
      return true;
    }
    
// 	    const CRef conflCl = solver.propagate();
// 	    if (CRef_Undef != conflCl) { 
//  	      data.addToProof(ca[conflCl]);
// 	      cout << "Here comes the ConflCl: " << ca[conflCl] << endl;
// 	      data.setFailed();
// 	    }

    ratmSteps +=  solver.trail.size() - oldTrailsize;

      for (int g=trailPosition; g< solver.trail.size(); ++g){
	
	data.addCommentToProof("implied by previous RATM/ATM unit");
	data.addUnitToProof( solver.trail[g] );  
    
	for (int p=0; p < data.list(solver.trail[g]).size(); ++p){
	  Clause& gpclause = ca[data.list(solver.trail[g])[p]];
	  if (! gpclause.can_be_deleted() ) {
	    gpclause.set_delete(true);
	    data.addToProof( gpclause, true );  // remove the clause from the proof (its subsumed by a unit clause)
	    data.removedClause( data.list(solver.trail[g])[p] );
	  }
	}	   
	data.list(solver.trail[g]).clear();
      }
      trailPosition = solver.trail.size();
      return true;
  }  
  return false;
	
}

bool RATElimination::shortATM(const CRef clause, const Lit left, int& trailPosition, vector<Lit>& atlits)
{	  
  Clause& cl = ca[ clause ];

  if( !config.opt_rate_ratm_extended ) {

    solver.newDecisionLevel();

    for( int k = 0 ; k < cl.size(); ++ k){
      if ( cl[k] != left ){ 
	assert( solver.value(cl[k]) == l_Undef && "all units should be propagated properly before already" );
	 solver.uncheckedEnqueue( ~cl[k] ); 
      }
    }

    ratmSteps += solver.trail.size();

    if (CRef_Undef != solver.propagate()) {
      solver.cancelUntil(0);
      return true;
    }
  }
  else
  {

  bool confl = false;
  int lastKeptLiteral = 0;
  int delPrefix=0;	 

  solver.newDecisionLevel(); // to be able to backtrack

  atlits.clear();

  for( int j = 0 ; j < cl.size(); ++ j ) {  if (cl[j] != left) atlits.push_back(cl[j]);}
  
  for( int j = 0 ; j < atlits.size(); ++ j ){
    if ( solver.value(atlits[j]) == l_Undef ) { 
      const int oldTrailsize = solver.trail.size();
      solver.uncheckedEnqueue( ~atlits[j] );
      confl = CRef_Undef != solver.propagate();  
      ratmSteps += solver.trail.size() - oldTrailsize;
    } 
    else confl = (solver.value(atlits[j]) == l_True);
    if (confl) {

      if (j == 0 ) { // first tested literal is conflicting --> top-level-conflict! --> propagate!

	solver.cancelUntil(0); // backtrack before check the value of the lit!
	assert ( solver.value(atlits[j])  == l_Undef && " It should be deleted or l_undef at level 0! ");
	propagateUnit(atlits[j], trailPosition);

	return true;
      }
      lastKeptLiteral = j;
      break;
    }
  }

  assert ( (lastKeptLiteral > 0 || !confl) && "Cannot shink to a Unit!"); 

  if (confl){

    confl = false;
    solver.cancelUntil(0); // backtrack
    solver.newDecisionLevel(); // to be able to backtrack	

    assert (lastKeptLiteral > 0 && "Cannot shink to a Unit!");

    for( int j = lastKeptLiteral ; j >= 0; -- j ){

      if ( solver.value(atlits[j]) == l_Undef ) {
	const int oldTrailsize = solver.trail.size();
	solver.uncheckedEnqueue( ~atlits[j] );  
	confl = CRef_Undef != solver.propagate();
	ratmSteps += solver.trail.size() - oldTrailsize;
      }
      else  confl = solver.value(atlits[j]) == l_True;

      if (confl) {

	if (j == lastKeptLiteral) { // first tested literal is conflicting --> top-level-conflict! --> propagate!

	  solver.cancelUntil(0); // backtrack before check the value of the lit!
	  assert ( solver.value(atlits[j])  == l_Undef && " It should be deleted or l_undef at level 0! ");
	  propagateUnit(atlits[j], trailPosition);

	  return true;
	}
	delPrefix = j; 
	break;
      }
    }
  
    if (delPrefix > 0 || lastKeptLiteral < atlits.size()-1 ){ // skip clauses, where all negated literals are needed to cause the conflict

      int j = 0, k = 0;

      cerr << "c detach clause to remove literals" << endl;
      solver.detachClause(clause, true);

      for ( j = 0; j < delPrefix; ++j ) {
	data.removeClauseFrom( clause, atlits[j] ); // remove the clause from the corresponding list
	data.removedLiteral(atlits[j]);
      }

      for ( ; j <= lastKeptLiteral; ++j ) {
	cl[k++] = atlits[j];
      }
      cl[k++] = left;
    
      for ( ; j < atlits.size(); ++j ) {
	data.removeClauseFrom( clause, atlits[j] ); // remove the clause from the corresponding list
	data.removedLiteral(atlits[j]);
      }
      minATM +=  (cl.size() - k); 

      const int a = cl.size();
      cl.shrink(cl.size() - (k));
      assert (a != cl.size() &&  "Clause is not shrinked! Infinite loop may cause!" ); // FIXME int a needed to check this, delete/comment after fuzzing
      data.addCommentToProof("remove literals by ATM");
      data.addToProof(cl);                 // add the shrinked clause to the proof
      data.addToProof(atlits, true, left); // remove the original clause from the proof (all literals from atlits as well as the literal left)

      cerr << "c attach clause after removing " << k << "literals" << endl;
      solver.attachClause(clause);
    }

    solver.cancelUntil(0);
    return true;
  }

  if ( solver.value(left) == l_Undef ) {
   // solver.propagate();
    
    cerr << "c trail: " << solver.trail << endl;
    cerr << "c current clause[" << clause << "]: " << cl << endl;
    cerr << "c delete: " << cl.can_be_deleted() << endl;
    cerr << "c left: " << left << endl;
    
    
    assert (false);   // Why left hasn't a value? It should be! FIXME
    const int oldTrailsize = solver.trail.size();
    solver.uncheckedEnqueue( left );
    if ( CRef_Undef != solver.propagate() ) {
      ratmSteps += solver.trail.size() - oldTrailsize;
      solver.cancelUntil(0);
      return true;
    }
  }
  else
    if ( solver.value(left) == l_False ) {
      solver.cancelUntil(0);
      return true;
    }
  }
  return false; 
    
}

bool RATElimination::minimizeRAT()
{
  MethodClock mc( ratmTime );
  bool didSomething = false;
  
  LitOrderRATEHeapLt comp(data, config.rate_orderComplements); // use this sort criteria!
  Heap<LitOrderRATEHeapLt> rateHeap(comp);  // heap that stores the variables according to their frequency (dedicated for BCE)
  
  // setup own structures
  rateHeap.addNewElement(data.nVars() * 2); // set up storage, does not add the element
  rateHeap.clear();
  
  // structures to have inner and outer round
  
  MarkArray nextRound;
  vector<Lit> nextRoundLits;
  nextRound.create(2*data.nVars());
  
  MarkArray reduct;
  reduct.create(2*data.nVars());
  
  int trailPosition = 0;
  vector<Lit> atlits;
  
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
      if( ! nextRound.isCurrentStep( toInt(l) ) ) continue; // has been processed before already
      assert( !rateHeap.inHeap(toInt(l)) && "literal should not be in the heap already!" );
      rateHeap.insert( toInt(l) );
    }
    nextRoundLits.clear();
    nextRound.nextStep();
              
    // do RAT Minimization on all the literals of the heap
    while (rateHeap.size() > 0 && (data.unlimited() || config.ratm_Limit > ratmSteps) && !data.isInterupted() && data.ok() )
    {
      
      // interupted ?

      if( data.isInterupted() ) break;
      const Lit right = toLit(rateHeap[0]);
      assert( rateHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
      rateHeap.removeMin();   
     
      
      if( data.doNotTouch(var(right)) ) continue;  // do not consider variables that have to stay fixed or which are already propagated!
      if( solver.value(right) != l_Undef ) continue;
      const Lit left = ~right; // complement

      cerr << "c select literal right=" << right << " trail: " << solver.trail << endl;
      
      DOUT( if( config.opt_rate_debug > 0 ) cerr << endl << "c RATE work on literal " << right << " with " << data.list(right).size() << " clauses " << endl;);
      DOUT( if( config.opt_rate_debug > 3 ) cerr << "current trail: " << data.getTrail() << endl;   );
      
      for( int i = 0 ; i < data.list(right).size() && data.ok(); ++ i ) 
      {
	const CRef cr = data.list(right)[i];
	Clause& c = ca[ cr ];
	data.lits.clear();
	
	if ( c.can_be_deleted() || c.learnt() ) continue; // TODO: yet we do not work with learned clauses, because its expensive
	if ( c.size() < config.rate_minSize ) continue; // ignore "small" clauses // TODO have a value for the parameter to disable this limit (e.g. are ther binaray RAT clauses)
	
	for ( int k = 0; k < c.size(); ++k ) if ( c[k] != right ) data.lits.push_back( c[k] );
		
	reduct.nextStep();
	
	assert( solver.decisionLevel() == 0 && "check can only be done on level 0" );

	int lastKeptLiteral = 0, toBeKeptLiterals = 0;
	bool confl = true; // has to be true --> exit condition for the for-loop!
	for( int j = 0; j < data.list(left).size() && data.ok() && confl; ++j )
	{
	  const CRef clause = data.list(left)[j];
	  if ( ca[ clause ].can_be_deleted() ) continue;
	  
	 if ( shortATM(clause, left, trailPosition, atlits) ) {  // calls the ATM routine, returns true, if clause has AT
	   conflATM++;
	   confl = true;
	   assert( solver.decisionLevel() == 0 && "after conflict the decision level has to be set to 0 again" );
	   if (!data.ok()) return true;
	   continue;
	 }
	 else {
	   cerr << "c detach clause to check RAT (has not AT)" << endl;
	   solver.detachClause(cr,true);
	 }
	 
	 if ( c.can_be_deleted() ) { // the clause might be removed by unit propagation from the ATM method
	   solver.cancelUntil(0);
	   break;
	 }

	  assert( solver.decisionLevel() == 1 && "has to be 1" );	  
	  
	  solver.newDecisionLevel();
	  
	  confl = false;
	  
	  for( int k = 0 ; k < data.lits.size(); ++ k ){
	      if ( solver.value(data.lits[k]) == l_Undef ) { 
		const int oldTrailsize = solver.trail.size();
		solver.uncheckedEnqueue( ~data.lits[k] );
		confl = CRef_Undef != solver.propagate();    
		ratmSteps += solver.trail.size() - oldTrailsize;
	      }
	      else  confl = (solver.value( data.lits[k] ) == l_True);
	    
	      if ( confl ) {
		if ( k >= lastKeptLiteral ) lastKeptLiteral = k+1;
		ratmSteps += k+1;
		break;
	      }
	      
	  }
	
	  assert ( (lastKeptLiteral > 0 || !confl) && "Cannot shink to a Unit!"); 
	
	  if ( confl ){
	    conflRATM++;
	    confl = false;
	    solver.cancelUntil(1); // backtrack
	    solver.newDecisionLevel(); // to be able to backtrack	
	    for( int k = (lastKeptLiteral-1) ; k >= 0; -- k ){
	        if ( solver.value(data.lits[k]) == l_Undef ) { 
		  const int oldTrailsize = solver.trail.size();
		  solver.uncheckedEnqueue( ~data.lits[k]);  
		  confl = CRef_Undef != solver.propagate();
		  ratmSteps += solver.trail.size() - oldTrailsize;
	        }
	        else  if ( solver.value(data.lits[k]) == l_True ){
		  confl = true;		  
		  }   
		if (!reduct.isCurrentStep( toInt(data.lits[k]) )){
		    reduct.setCurrentStep(toInt(data.lits[k]));
		    toBeKeptLiterals ++;
		} 
	        if (confl) {
		  
		  if (toBeKeptLiterals == data.lits.size()){   // then stop with the current clause!
		    confl = false;
		  }
		  break;
	        }
	     }
        }
        // has to be attached for the ATM routine in the next cycle
        cerr << "c with right=" << right << " and left=" << left << " attach at RATM 720" << endl;
	solver.attachClause(cr);
	solver.cancelUntil(0);	
     } // END for loop over left-clauses
     
     if ( c.can_be_deleted() ) // happens if c is satisfied by a Unit-Propagation in the ATM routine
       if ( solver.value(right) == l_Undef ) continue;
       else break;
    
     if ( !confl ) continue; // resolvent didn't fulfill the RAT requirement
     
     if ( lastKeptLiteral == 0 ) {   // all clauses in data.lits(left) have AT -- kept literals have never been seen
        cerr << "c with right=" << right << " and left=" << left << " detach at RATM732" << endl;
	solver.detachClause(cr,true);
	solver.cancelUntil(0); 
	
	if(  solver.value(right) == l_Undef ) { // necassary, because unit's propagated in the ATM prozedure may implied right
	  propagateUnit(right, trailPosition);
	}
	else {
	  cerr << "c with right=" << right << " and left=" << left << " attach at RATM739" << endl;
	  solver.attachClause( cr );
	}
	continue;
      }
      
  if ( lastKeptLiteral > 0 ) {
	cerr << "c with right=" << right << " and left=" << left << " detach at RATM747" << endl;
	solver.detachClause( cr, true ); 
	int n=0;
	reduct.setCurrentStep(toInt(right));
	for ( int k = 0; k < c.size(); ++k ) {
	  if (reduct.isCurrentStep(toInt(c[k]))) c[n++] = c[k]; 
	  else {
	    data.removeClauseFrom( cr, c[k] ); // remove the clause from the corresponding list
	    data.removedLiteral(c[k]);	 
	  }
	}
	
	const int a = c.size();
	minRATM +=  (c.size() - n);
	c.shrink(c.size() - n);
	assert ( n == c.size() && "Clause is not shrinked! Infinite loop may cause!" ); // FIXME int a needed to check this, delete/comment after fuzzing
	data.addCommentToProof("minimize by RAT minimization");
	data.addToProof( c , false, right); // add minimized version to proof
	data.addToProof( data.lits, true, right ); // delete all literals from data.lits and additionally right (right will be printed first, because its the RAT literal)

	if( config.opt_rate_ratm_rounds ){
	  for( int k = 0 ; k < c.size(); ++ k ) {	// all complementary literals can be tested again
	    if( ! nextRound.isCurrentStep(toInt(~c[k]) ) ) {
	      nextRoundLits.push_back( ~c[k] );
	      nextRound.setCurrentStep( toInt(~c[k]) );
	    }
	  }
	}
	cerr << "c with right=" << right << " and left=" << left << " attach at RATM774" << endl;
	solver.attachClause( cr );
      } 
    } // end of for c in F_right
  }
  } while ( nextRoundLits.size() > 0 && (data.unlimited() || config.ratm_Limit > ratmSteps) && !data.isInterupted() ); // repeat until all literals have been seen until a fixed point is reached!

  if ( minRATM !=0 || minATM !=0 || unitRATM !=0 ) didSomething = true;
  return didSomething;
}


	
bool RATElimination::minimizeAT()
{
  MethodClock mc( rateTime );
  bool didSomething = false;
  
  LitOrderRATEHeapLt comp(data, config.rate_orderComplements); // use this sort criteria!
  Heap<LitOrderRATEHeapLt> rateHeap(comp);  // heap that stores the variables according to their frequency (dedicated for BCE)
  
  // setup own structures
  rateHeap.addNewElement(data.nVars() * 2); // set up storage, does not add the element
  rateHeap.clear();
  
    
  // init
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    if( data.doNotTouch(v) ) continue; // do not consider variables that have to stay fixed!
    if( data[  mkLit(v,false) ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,false))) )  rateHeap.insert(toInt( mkLit(v,false) ));
    if( data[  mkLit(v,true)  ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,true))) )   rateHeap.insert(toInt( mkLit(v,true) ));
  }
  
  int shrinked = 0;
  int shrinkedToUnit = 0;
  vector<CRef> clauses;
  int count=0;
  
do{ 
  
  while (rateHeap.size() > 0 && (data.unlimited() || config.rate_Limit > ratmSteps) && !data.isInterupted() && solver.okay())
    {
      
      const Lit right = toLit(rateHeap[0]);
      assert( rateHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
      rateHeap.removeMin();
      
      if( data.doNotTouch(var(right)) ) continue; // do not consider variables that have to stay fixed!
      
      for( int i = 0 ; i < data.list(right).size(); ++ i ) 
      {
	const CRef cr = data.list(right)[i];
	Clause& c = ca[ cr ];
	if ( c.can_be_deleted() || c.learnt() ) continue; // TODO: yet we do not work with learned clauses, because its expensive
	if ( c.size() < config.rate_minSize ) continue; // ignore "small" clauses // TODO have a value for the parameter to disable this limit (e.g. are ther binaray RAT clauses)
	clauses.push_back( cr );     // collect clauses in the order of the heap but no clause twice
	c.set_delete(true); // undo after collection finished
      }
    }

    for( int i=0 ; i < clauses.size(); ++i ) {
	ca[ clauses[i] ].set_delete(false); //undo
    }
    
    for( int i=0 ; i < clauses.size() && (data.unlimited() || config.rate_Limit > ratmSteps) ; ++i ) {
	const CRef cr = clauses[i];
	Clause& c = ca[ cr ];
	if ( c.can_be_deleted() || c.learnt() ) continue; // TODO: yet we do not work with learned clauses, because its expensive
	DOUT( if( config.opt_rate_debug > 0 ) cerr << endl << "c test clause [" << cr << "] " << c << endl;);
	
	DOUT( if( config.opt_rate_debug > 3 ) {
	  cerr << "c current formula: " << endl;
	  for( int t = 0 ; t < data.getClauses().size(); ++ t ) {
	    if( ! ca[data.getClauses()[t]].can_be_deleted() ) cerr << "[" << data.getClauses()[t] << "] "<< ca[data.getClauses()[t]] << endl;
	  }
	  for( Var v = 0 ; v < data.nVars(); ++v ) {
	    for ( int p = 0 ; p < 2; ++ p ) {
	      const Lit l = mkLit(v,p==0);
	      if( data.list(l).size() > 0 ) {
		cerr << "c list(" << l << "): " << endl;
		for( int t = 0 ; t < data.list(l).size(); ++t ) if( !ca[ data.list(l)[t] ].can_be_deleted() ) cerr << "[" << data.list(l)[t] << "] " << ca[ data.list(l)[t] ] << endl;
		cerr << endl;
	      }
	    }
	  }
	});
	
	// literals to propagate
	data.lits.clear();

	for( int j = 0 ; j < c.size(); ++ j) data.lits.push_back( c[j] );
	
	cerr << "c with right=" << right << " and left=" << left << " detach at RATM868" << endl;
	solver.detachClause( cr, true ); // detach the clause eagerly
	assert( solver.decisionLevel() == 0 && "check can only be done on level 0" );
	
	DOUT( if( config.opt_rate_debug > 2 ) cerr << "c enqueue complements in " << data.lits << endl;);
	
	bool confl = false;
	int lastKeptLiteral = 0;
	int delPrefix=0;
	solver.newDecisionLevel(); // to be able to backtrack	
	
	for( int j = 0 ; j < data.lits.size(); ++ j ){
	  if ( solver.value(data.lits[j]) == l_Undef ) { 
	    const int oldTrailsize = solver.trail.size();
	    solver.uncheckedEnqueue( ~data.lits[j] );
	    confl = CRef_Undef != solver.propagate();    
	    ratmSteps += solver.trail.size() - oldTrailsize;
	  }
	  else  confl = solver.value(data.lits[j]) == l_True;
	  
	  if (confl) {
	    if (j == 0) { // first tested literal is conflicting --> top-level-conflict! --> propagate!
	      solver.cancelUntil(0);
	      const int oldTrailsize = solver.trail.size();
	      if(  l_True == data.enqueue( data.lits[j])  ) {
		  shrinkedToUnit ++;
		  solver.propagate();
		  ratmSteps +=  solver.trail.size() - oldTrailsize;
	      }
	      confl = false;
	      break;
	    }
	    lastKeptLiteral = j+1;
	    break;
	  }
	}
	
	assert ( (lastKeptLiteral > 1 || !confl) && "Cannot shink to a Unit!"); 
	
	if (confl){
	  
	  confl = false;
	  solver.cancelUntil(0); // backtrack
	  solver.newDecisionLevel(); // to be able to backtrack	
	  
	  for( int j = lastKeptLiteral-1 ; j >= 0; -- j ){
	    
	    if ( solver.value(data.lits[j]) == l_Undef ) { 
	      const int oldTrailsize = solver.trail.size();
	      solver.uncheckedEnqueue( ~data.lits[j] );  
	      confl = CRef_Undef != solver.propagate();
	      ratmSteps += solver.trail.size() - oldTrailsize;
	      
	    }
	    else  confl = solver.value(data.lits[j]) == l_True;
	    
	    if (confl) {
	      if (j == lastKeptLiteral-1) { // first tested literal is conflicting --> top-level-conflict! --> propagate!
		solver.cancelUntil(0); 
		const int oldTrailsize = solver.trail.size();
		if(  l_True == data.enqueue( data.lits[j])  ) {
		  shrinkedToUnit ++;	  
		  solver.propagate();
		  ratmSteps +=  solver.trail.size() - oldTrailsize;
		}
		confl = false;
		break;
	      }
	      delPrefix = j; 
	      break;
	    }
	  }
	}

	if (confl && (delPrefix > 0 || lastKeptLiteral < data.lits.size())){ // skip clauses, where all negated literals are needed to cause the conflict
	  
	  int j = 0, k = 0;
	  
	  for ( j = 0; j < delPrefix; ++j ) {
	    data.removeClauseFrom( cr, data.lits[j] ); // remove the clause from the corresponding list
	    data.removedLiteral(data.lits[j]);
	  }
	
	  for ( ; j < lastKeptLiteral; ++j ) {
	     c[k++] = c[j];
	  }
	  
	  for ( ; j < c.size(); ++j ) {
	    data.removeClauseFrom( cr, data.lits[j] ); // remove the clause from the corresponding list
	    data.removedLiteral(data.lits[j]);
	  }
	  
	  shrinked +=  (c.size() - k);
	  
	  c.shrink(c.size() - k);   	
	  
	  for ( int j = 0; j < data.lits.size() ; ++ j ) {
	    rateHeap.update(toInt( data.lits[j] ));
	    rateHeap.update(toInt( ~data.lits[j] ));
	  }

	  didSomething = true;
	}	
	
	cerr << "c with right=" << right << " and left=" << left << " attach at RATM970" << endl;
	solver.attachClause( cr ); 
	solver.cancelUntil(0); // backtrack	
    }
  clauses.clear();  
} while (rateHeap.size()>0 && (data.unlimited() || config.rate_Limit > ratmSteps) && !data.isInterupted());
  cout << "c |  " << shrinked << " deleted literals during ATM/RAM. " << shrinkedToUnit << " propagated literals." << endl;
  return didSomething;
}

bool RATElimination::blockedSubstitution()
{
  MethodClock mc (bcaTime);
  bool didSomething = false;
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
    if( data[  mkLit(v,false) ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,false))) )  nextRoundLits.push_back( mkLit(v,false) );
    if( data[  mkLit(v,true)  ] > 0 ) if( !rateHeap.inHeap(toInt(mkLit(v,true))) )   nextRoundLits.push_back( mkLit(v,true) );
  }
  data.ma.resize(2*data.nVars());
  data.ma.nextStep();
  
  do {   
    // re-init heap
    for( int i = 0 ; i < nextRoundLits.size(); ++ i ) {
      const Lit l = nextRoundLits[i];
      if( ! nextRound.isCurrentStep( toInt(l) ) ) continue; // has been processed before already
      assert( !rateHeap.inHeap(toInt(l)) && "literal should not be in the heap already!" );
      rateHeap.insert( toInt(l) );
    }
    nextRoundLits.clear();
    nextRound.nextStep();
    
    
    // do BCA
    while (rateHeap.size() > 0 && (data.unlimited() || config.bceLimit > rateSteps) && !data.isInterupted() )
    {
      // interupted ?
      if( data.isInterupted() ) break;
      
      const Lit right = toLit(rateHeap[0]);
      assert( rateHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
      rateHeap.removeMin();

//       cerr << "c BCE with " << right << endl;
      
      // check whether a clause is a tautology wrt. the other clauses
      const Lit left = ~right; // complement

      data.lits.clear(); // used for covered literal elimination
      for( int i = 0 ; i < data.list(right).size(); ++ i ) 
      {
	if(  ca[ data.list(right)[i] ].can_be_deleted() 
	  || ca[ data.list(right)[i] ].size() == 2 ) continue; // do not use binary clauses
	
	
	for( int j = 0 ; j < data.list(right).size(); ++ j )
	{
	  if( i == j ) continue; // do not replace the clause with itself!
	  
	  Clause& c = ca[ data.list(right)[i] ];
	  Clause& d = ca[ data.list(right)[j] ];
	  if( d.can_be_deleted() || c.size() > d.size() ) continue; // do not work on uninteresting clauses!

	  int pc = 0, pd = 0;
	  Lit extraClit = lit_Undef;
	  
	  data.ma.nextStep();
// 	  cerr << "c mark nextstep "  << endl;
	  for( int k = 0 ; k < d.size(); ++k ) {
// 	    cerr << "c mark " << d[k] << endl;
	    data.ma.setCurrentStep( toInt( d[k] ) ); // mark all Lits from D
	  }

	  // check lits of C whether they match
	  for( int k = 0; k < c.size(); ++ k ) {
	    if( ! data.ma.isCurrentStep( toInt( c[k] ) ) ) {
// 	      cerr << "c diff " << c[k] << endl;
	      if( extraClit == lit_Undef ) extraClit = c[k];
	      else { extraClit = lit_Error; break; }
	    } else {
// 	      cerr << "c reset " << c[k] << endl;
	      data.ma.reset( toInt( c[k] ) );
	    }
	  }
	  if ( extraClit == lit_Error ) continue; // use next clause!

	  // collect lits for new clause (the ones only present in D)
	  data.lits.clear();
	  for( int k = 0 ; k < d.size(); ++k ) {
	    if( data.ma.isCurrentStep( toInt(d[k]) ) ) data.lits.push_back( d[k] );
	  }
	  
	  if( extraClit == lit_Undef ) { // found duplicate clauses
	    didSomething = true;
	    bcaSubstitue ++; bcaSubstitueLits += c.size();
	    // delete the old clause
	    ca[ data.list(right)[j] ] .set_delete(true);		// d can be deleted, because it can be produced by reslution with c and the new clause!
	    data.removedClause( data.list(right)[j] );
	    solver.detachClause( data.list(right)[j] );	// remove clause from unit propagation
	    continue;
	  }
	  
	  // replace common literals with complement of different literal
// 	  cerr << "c check " << ~extraClit << endl;
	  if( ! data.ma.isCurrentStep( toInt( ~extraClit ) ) ) {
	    data.lits.push_back( ~extraClit );
	    data.ma.setCurrentStep( toInt( ~extraClit ) ); // now all lits of data.lits are marked
	    bcaFullMatch ++;
	  }
	  bcaCandidates ++;

	  if( data.lits.size() <= 1 ) {
	    bcaStrenghening ++;
	    // TODO: could implement strengthening here!
	    continue;
	  }
	  
	  bool isRedundant = false;
	  Lit redundantLit = lit_Undef;
	  
	  if( !isRedundant ) { // is clause AT?
	    // test whether the resolvent is AT
	    solver.newDecisionLevel();
	    DOUT( if( config.opt_rate_debug > 2 ) cerr << "c enqueue complements in " << data.lits << endl;);
	    for( int k = 0 ; k < data.lits.size(); ++ k ) {
	      if( solver.value( ~data.lits[k] ) == l_False ) { isRedundant = true; break; }
	      else if ( solver.value( ~data.lits[k] ) == l_Undef ) solver.uncheckedEnqueue( ~data.lits[k] );	// enqueue all complements
	    }
	    if( ! isRedundant ) {
	      CRef confl = solver.propagate();	// check whether unit propagation finds a conflict for (F \ C) \land \ngt{C}, and hence C would be AT
	      DOUT( if( config.opt_rate_debug > 2 ) cerr << "c propagate with conflict " << (confl != CRef_Undef ? "yes" : " no") << endl;);
	      solver.cancelUntil(0);	// backtrack
	      if( confl != CRef_Undef ) isRedundant = true;	// clause is AT
	    }
	    if( isRedundant ) bcaATs ++; // stats
	  }
	  
	  if( !isRedundant ) { // is clause blocked?
	    bool isblocked = false;
	    for( int k = 0 ; k < data.lits.size() - 1; ++k )
	    {
	      const Lit resL = data.lits[k];
	      redundantLit = resL;
	      if( data.doNotTouch( var(resL) ) ) continue; // do not perform blocked clause addition on doNotTouch variables
	      isblocked = true;

	      for( int m = 0 ; m < data.list(~resL).size(); ++ m ) { // resolve with all candidates
		const Clause& e = ca[ data.list(~resL)[m] ]; 
		if( e.can_be_deleted() ) continue;
		bcaResolutionChecks ++;
		
		bool hasComplement = false; // check resolvent for being tautologic
		for( int n = 0 ; n < e.size(); ++ n ) {
		  if( e[n] == ~resL ) continue;
		  if( data.ma.isCurrentStep( toInt(~e[n]) ) ) {hasComplement = true; break; }
		}
		
		if( !hasComplement ) { isblocked = false; break; }
		
	      }
	      
	      if( isblocked == true ) break; // found a blocking literal
	    }
	    isRedundant = isblocked;
	  }
	  
	  if( isRedundant ) {
	    // stats
	    didSomething = true;
	    bcaSubstitue ++; bcaSubstitueLits += c.size() - 1;
	    
	    // cerr << "c substitute " << d << " with " << data.lits << " via " << c << endl;
	    
	    // write according proof
	    data.addCommentToProof("add a blocked clause for blocked substitution");
	    data.addToProof(data.lits,false,redundantLit); // add new clause
	    data.addToProof(d,true);	// delete the clause D
	    
	    // add the new clause
	    // all clauses have to be sorted during simplification
	    CRef tmpRef = ca.alloc(data.lits, d.learnt() );
	    data.addClause( tmpRef );		//
	    if( ca[ data.list(right)[j] ].learnt() ) data.getLEarnts().push( tmpRef );
	    else data.getClauses().push( tmpRef );
	    solver.attachClause( tmpRef );	// add clause for unit propagation
	    
	    // delete the old clause
	    ca[ data.list(right)[j] ] .set_delete(true);		// d can be deleted, because it can be produced by reslution with c and the new clause!
	    data.removedClause( data.list(right)[j] );
	    solver.detachClause( data.list(right)[j] );	// remove clause from unit propagation
    
	    // add literals for the next round!
	    for ( int k = 0 ; k < data.lits.size(); ++ k ) {
	      if( ! nextRound.isCurrentStep(toInt( data.lits[k] ) ) ) {
		nextRoundLits.push_back( data.lits[k] );
		nextRound.setCurrentStep( toInt( data.lits[k] ) );
	      }
	    }
	  }
	}
	

	
      } // end iterating over all clauses that contain (right)
    }
  
  } while ( nextRoundLits.size() > 0 && (data.unlimited() || config.bceLimit > rateSteps) && !data.isInterupted() ); // repeat until all literals have been seen until a fixed point is reached!
  return didSomething;
}

bool RATElimination::resolveUnsortedStamped( const Lit l, const Clause& d, MarkArray& ma, vector<Lit>& resolvent )
{
    for( int i = 0 ; i < d.size(); ++ i ) {
      const Lit& dl = d[i];
      if( dl == ~l) continue; // on this literal we currently resolve
      if( ma.isCurrentStep( toInt( ~dl ) ) ) return true; // complementary literals in the resolvent
      if( ! ma.isCurrentStep( toInt(dl) ) ) {
	resolvent.push_back( dl );	// literal not yet present in resolvent, add it
      }
    }
    return false;
}

void RATElimination::printStatistics(ostream& stream)
{
  cerr << "c [STAT] RATM "  << ratmTime.getCpuTime() << " seconds, " << ratmSteps << " steps, "
  << minRATM  << " min-RATM, "
  << minATM  << " min-ATM, "
  << unitRATM   << " unit-ATM/RATM, "
  << conflRATM << " confl-RATM, "
  << conflATM << " confl-ATM," << endl;
  cerr << "c [STAT] RATE "  << rateTime.getCpuTime() << " seconds, " << rateSteps << " steps, "
  << rateCandidates << " checked, "
  << remRAT  << " rem-RAT, "
  << remBCE  << " rem-BCE, "
  << remAT   << " rem-AT, "
  << remHRAT << " rem-HRAT," << endl;
  cerr << "c [STAT] RATE-BCS "  << bcaTime.getCpuTime() << " seconds, " 
  << bcaCandidates << " cands, "
  << bcaResolutionChecks << " resChecks, "
  << bcaSubstitue << " substituted, "
  << bcaATs << " addedATs, "
  << bcaSubstitueLits << " subLits, "
  << bcaFullMatch << " fullMatches, "
  << bcaStrenghening << " strengthenings, "
  << endl;
  cerr << "c [STAT] RATE-BRAT "  << bratTime.getCpuTime() << " seconds, " << remBRAT << " remBRAT, " << blockCheckOnSameClause << " sameClauseChecks, "
  << endl;
}

void RATElimination::giveMoreSteps()
{
  rateSteps = rateSteps < config.opt_bceInpStepInc ? 0 : rateSteps - config.opt_bceInpStepInc;
}
  
void RATElimination::destroy()
{
  
}

void RATElimination::cleanSolver()
{
  // clear all watches!
  solver.watches.cleanAll();
  
  // clear all watches!
  for (int v = 0; v < solver.nVars(); v++)
    for (int s = 0; s < 2; s++)
      solver.watches[ mkLit(v, s) ].clear();

  solver.learnts_literals = 0;
  solver.clauses_literals = 0;
  solver.watches.cleanAll();
  
  for( int i = 0 ; i < solver.learnts.size(); ++ i ) 
    ca[ solver.learnts[i] ].sort();
  for( int i = 0 ; i < solver.clauses.size(); ++ i ) 
    ca[ solver.clauses[i] ].sort(); 
}

void RATElimination::reSetupSolver()
{
  assert( solver.decisionLevel() == 0 && "solver can be re-setup only at level 0!" );
    // check whether reasons of top level literals are marked as deleted. in this case, set reason to CRef_Undef!
    if( solver.trail_lim.size() > 0 )
      for( int i = 0 ; i < solver.trail_lim[0]; ++ i )
        if( solver.reason( var(solver.trail[i]) ) != CRef_Undef )
          if( ca[ solver.reason( var(solver.trail[i]) ) ].can_be_deleted() )
            solver.vardata[ var(solver.trail[i]) ].reason = CRef_Undef;

    // give back into solver
    for( int p = 0 ; p < 1; ++ p ) { // do not use learned clauses, because they might be dropped without any notice later again
      vec<CRef>& clauses = (p == 0 ? solver.clauses : solver.learnts );
      for (int i = 0; i < clauses.size(); ++i)
      {
	  const CRef cr = clauses[i];
	  Clause & c = ca[cr];
	  assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
	  if ( !c.can_be_deleted() ) // all clauses are neccesary for re-setup!
	  {
	      assert( c.mark() == 0 && "only clauses without a mark should be passed back to the solver!" );
	      if (c.size() > 1)
	      {
		  // do not watch literals that are false!
		  int j = 1;
		  for ( int k = 0 ; k < 2; ++ k ) { // ensure that the first two literals are undefined!
		    if( solver.value( c[k] ) == l_False ) {
		      for( ; j < c.size() ; ++j )
			if( solver.value( c[j] ) != l_False ) 
			  { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
		    }
		  }
		  // assert( (solver.value( c[0] ) != l_False || solver.value( c[1] ) != l_False) && "Cannot watch falsified literals" );
		  
		  // reduct of clause is empty, or unit
		  if( solver.value( c[0] ) == l_False ) { data.setFailed(); return; }
		  else if( solver.value( c[1] ) == l_False ) {
		    if( data.enqueue(c[0]) == l_False ) { return; }
		    else { 
		      c.set_delete(true);
		    }
		    if( solver.propagate() != CRef_Undef ) { data.setFailed(); return; }
		    c.set_delete(true);
		  } else solver.attachClause(cr);
	      }
	      else if (solver.value(c[0]) == l_Undef)
		  if( data.enqueue(c[0]) == l_False ) { return; }
	      else if (solver.value(c[0]) == l_False )
	      {
		// assert( false && "This UNSAT case should be recognized before re-setup" );
		data.setFailed();
	      }
	  }
      }
    }
}




