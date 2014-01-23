/*******************************************************************************[FourierMotzkin.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/FourierMotzkin.h"
#include "mtl/Sort.h"
#include <bits/algorithmfwd.h>


using namespace Coprocessor;

FourierMotzkin::FourierMotzkin( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation )
: Technique(_config, _ca,_controller)
, data(_data)
, propagation(_propagation)
, processTime(0)
, amoTime(0)
, amtTime(0)
, fmTime(0)
, twoPrTime(0)
, deduceAloTime(0)
, steps(0)
, searchSteps(0)
, fmLimit(config.opt_fmLimit)
, foundAmos(0)
, foundAmts(0)
, newAmos(0)
, newAlos(0)
, newAlks(0)
, sameUnits(0)
, deducedUnits(0)
, propUnits(0)
, addDuplicates(0)
, irregular(0)
, pureAmoLits(0)
, usedClauses(0)
, cardDiff(0)
, discardedCards(0)
, discardedNewAmos(0)
, removedCards(0)
, newCards(0)
, addedBinaryClauses(0)
, addedClauses(0)
, detectedDuplicates(0)
, twoPrAmos(0)
, twoPrAmoLits(0)
, dedAlos(0)
{
  
}

void FourierMotzkin::reset()
{
  
}
  
bool FourierMotzkin::process()
{
  MethodTimer mt(&processTime);
  bool didSomething = false;

  // have a slot per variable
  data.ma.resize( data.nVars() * 2 );
  
  MarkArray inAmo;
  inAmo.create( 2 * data.nVars() );

  // setup own structures
  Heap<LitOrderHeapLt> heap(data); // heap that stores the literals according to their frequency
  heap.addNewElement(data.nVars() * 2);
  
  vector< CardC > cards,rejectedNewAmos,rejectedNewAlos,rejectedNewAlks; // storage for constraints
  
  heap.clear();
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( !heap.inHeap(toInt(mkLit(v,false))) )  heap.insert( toInt(mkLit(v,false)) );
    if( !heap.inHeap(toInt(mkLit(v,true))) )   heap.insert( toInt(mkLit(v,true))  );
  }

  amoTime = cpuTime() - amoTime;
  data.ma.nextStep();
  inAmo.nextStep();
  
  // create full BIG, also rewrite learned clauses!!
  BIG big;
  big.create( ca,data.nVars(),data.getClauses(),data.getLEarnts() );
  
  cards.clear();
  
  if( config.fm_debug_out > 0) cerr << "c run with " << heap.size() << " elements" << endl;
  
  // for l in F, and only if not interrupted, and not limits are reached or active
  while (heap.size() > 0 && (data.unlimited() || (fmLimit > steps && config.opt_fmSearchLimit > searchSteps ) ) && !data.isInterupted() ) 
  {
    const Lit right = toLit(heap[0]);
    assert( heap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    heap.removeMin();
    
    if( data[ right ] < 2 ) continue; // no enough occurrences -> skip!
    const uint32_t size = big.getSize( ~right );
    if( config.fm_debug_out > 2) cerr << "c check " << right << " with " << data[right] << " cls, and " << size << " implieds" << endl;
    if( size < 2 ) continue; // cannot result in a AMO of required size -> skip!
    const Lit* list = big.getArray( ~right );

    // create first list right -> X == -right \lor X, ==
    inAmo.nextStep(); // new AMO
    data.lits.clear(); // new AMO
    data.lits.push_back(right); // contains list of negated AMO!
    for( int i = 0 ; i < size; ++ i ) {
      searchSteps ++;
      const Lit& l = list[i];
      if( data[ l ] < 2 ) continue; // cannot become part of AMO!
      if( big.getSize( ~l ) < 2 ) continue; // cannot become part of AMO!
      if( inAmo.isCurrentStep( toInt(l) ) ) continue; // avoid duplicates!
      inAmo.setCurrentStep( toInt(l ) );
      data.lits.push_back(l); // l is implied by ~right -> canidate for AMO(right,l, ... )
      if( data.lits.size() > config.opt_fmMaxAMO ) break; // do not collect more literals for the AMO (heuristic cut off...)
    }
    if( config.fm_debug_out > 2) cerr << "c implieds: " << data.lits.size() << ", namely " << data.lits << endl;
    if( data.lits.size() < 3 || config.opt_fmSearchLimit <= searchSteps ) continue; // do not consider this variable, because it does not hit enough literals
    
    // TODO: should sort list according to frequency in binary clauses - ascending, so that small literals are removed first, increasing the chance for this more frequent ones to stay!
    
    // check whether each literal can be inside the AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i ) { // keep the very first, since it created the list!
      const Lit l = data.lits[i];
      if( l == lit_Undef ) continue; // do not handle removed literals!

      if( config.opt_fmSearchLimit <= searchSteps ) { // if limit is reached, invalidate current AMO candidate
	data.lits.clear();
	break;
      }

      const uint32_t size2 = big.getSize( ~l );
      const Lit* list2 = big.getArray( ~l );
      // if not all, disable this literal, remove it from data.lits
      inAmo.nextStep(); // new AMO
      
      if( config.opt_rem_first ) {
	for( int j = 0 ; j < size2; ++ j ) {inAmo.setCurrentStep( toInt(list2[j]) );searchSteps++;}
	int j = 0;
	for( ; j<data.lits.size(); ++ j ) {
	  if( i!=j 
	    && data.lits[j] != lit_Undef 
	    && !inAmo.isCurrentStep( toInt( data.lits[j] ) ) 
	  ) break;
	  searchSteps++;
	}
	if( j != data.lits.size() ) {
	  if( config.fm_debug_out > 0) cerr << "c reject [" <<i<< "]" << data.lits[i] << ", because failed with [" << j << "]" << data.lits[j] << endl;
	  data.lits[i] = lit_Undef; // if not all literals are covered, disable this literal!
	} else if( config.fm_debug_out > 0) cerr << "c keep [" <<i<< "]" << data.lits[i] << " which hits [" << j << "] literas"  << endl;
      } else {
	for( int j = 0 ; j < size2; ++ j ) {
	  if( config.fm_debug_out > 2 ) cerr << "c literal " << l << " hits literal " << list2[j] << endl;
	  inAmo.setCurrentStep( toInt(list2[j]) );
	  searchSteps++;
	}
	inAmo.setCurrentStep( toInt(l) ); // set literal itself!
	int j = i+1; // previous literals have been tested already!
	for( ; j < data.lits.size(); ++ j ) {
	  if( data.lits[j] == lit_Undef ) continue; // do not process this literal!
	  searchSteps++;
	  if( config.fm_debug_out > 2 ) cerr << "c check literal " << data.lits[j] << "[" << j << "]" << endl;
	  if( !inAmo.isCurrentStep( toInt( data.lits[j] ) ) // not in AMO with current literal
	  ) {
	    if( config.fm_debug_out > 0) cerr << "c reject [" <<j<< "]" << data.lits[j] << ", because failed with [" << i << "]" << data.lits[i] << endl;
	    data.lits[j] = lit_Undef; // if not all literals are covered, disable this literal!
	  } else {
	    if( config.fm_debug_out > 0) cerr << "c keep [" <<j<< "]" << data.lits[j] << " which is hit by literal " << data.lits[i] << "[" << i << "] "  << endl;    
	  }
	}
      }
      
      
    }
    
    // found an AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i )
      if ( data.lits[i] == lit_Undef ) { data.lits[i] = data.lits[ data.lits.size() - 1 ]; data.lits.pop_back(); --i; }
    
    for( int i = 0 ; i < data.lits.size(); ++ i ){
      if( config.opt_fm_avoid_duplicates ) { // remove the edges in the big that represent this AMO
	for( int j = i+1; j < data.lits.size(); ++ j ) {
	  searchSteps+=big.getSize(data.lits[i]); // approximate the number of memory hits here
	  big.removeEdge(data.lits[i], data.lits[j] );
	}
      }
      data.lits[i] = ~ data.lits[i]; // need to negate all!
      data.ma.setCurrentStep( var(data.lits[i]) ); // memorize that this variable is part of an AMO
    }

    // remember that these literals have been used in an amo already!
    sort(data.lits);
    cards.push_back( data.lits );

    if( config.opt_fm_multiVarAMO && data.lits.size() > 1 && config.opt_fm_avoid_duplicates && big.getSize( right ) > 0 ) heap.insert( toInt( right ) ); // this literal might have more cliques
    
    if( config.fm_debug_out > 0 ) cerr << "c found AMO: " << data.lits << endl;
  }
  
  if( config.opt_fm_avoid_duplicates && cards.size() > 0 )
    big.recreate( ca,data.nVars(),data.getClauses(),data.getLEarnts() );
 
  amoTime = cpuTime() - amoTime;
  foundAmos = cards.size();
  
  if( config.fm_debug_out > 0 ) cerr << "c finished search AMO --- process ... " << endl;
  
  // perform AMT extraction
  amtTime = cpuTime() - amtTime;
  if( config.opt_atMostTwo ) {
    for( Var v = 0 ; v < data.nVars(); ++ v ) {
      if( config.opt_fmSearchLimit <= searchSteps ) { // if limit is reached, invalidate current AMO candidate
	data.lits.clear();
	break;
      }
      for( int p = 0 ; p < 1; ++ p ) { // for both polarities
	if( config.opt_fmSearchLimit <= searchSteps ) { // if limit is reached, invalidate current AMO candidate
	  data.lits.clear();
	  break;
	}
        const Lit l = mkLit(v,p==1);
	if( config.fm_debug_out > 1 ) cerr << "c consider literal " << l << " with " << data.list(l).size() << " clauses" << endl;
        data.lits.clear(); data.ma.nextStep(); // prepare set!
	data.lits.push_back(l); data.ma.setCurrentStep( toInt(l) ); // add current literal!
	
	for( int i = 0 ; i < data.list(l).size(); ++ i ) { // collect all literals that occur with l in a ternary clause!
	  const Clause& c = ca[data.list(l)[i]];
	  searchSteps++;
	  if( c.can_be_deleted() || c.size() != 3 ) {
	    if( c.can_be_deleted() ) {data.list(l)[ i ] = data.list(l)[ data.list(l).size() -1 ]; data.list(l).pop_back(); --i; } // remove can-be-deleted clause!
	    continue; // look for ternary clauses only!
	  }
	  if( c[0] != l ) continue; // consider only clauses, where l is the smallest literal?! (all other AMT's would have been found before!)
	  for( int j = 1; j < 3; ++ j ) {
	    searchSteps++;
	    if( c[j] != l && ! data.ma.isCurrentStep( toInt(c[j] ) ) ) { 
	      data.lits.push_back(c[j]); data.ma.setCurrentStep(toInt(c[j]));
	    }
	  }
	}
	sort(data.lits); // sort lits in list!
	assert( data.lits[0] == l && "current literal has to be the smallest literal!" );
	
	for( int i = 0 ; i < data.lits.size(); ++ i ) { // check whether each literal can be found with each pair of other literals!
	  // setup the map
	  const Lit l0 = data.lits[i];
	  for( int j = i+1 ; j < data.lits.size(); ++ j ) {
	    const Lit l1 = data.lits[j];
	    for( int k = j+1 ; k < data.lits.size(); ++ k ) { 
	      searchSteps++;
	      const Lit l2 = data.lits[k];
	      if( config.fm_debug_out > 2 ) cerr << "c check triple " << l0 << " - " << l1 << " - " << l2 << endl;
	      int m = 0;
	      for(  ; m < data.list(l0).size(); ++ m ) { // collect all literals that occur with l in a ternary clause!
		const Clause& c = ca[data.list(l0)[m]];
		if( c.can_be_deleted() || c.size() != 3 ) continue; // do not use this clause!
		if( c[1] == l1 && c[2] == l2 ) break; // found corresponding clause! - l0 < l1 -> needs to be c[0]
	      }
	      if( m == data.list(l0).size() ) { // did not find triple l0,l1,l2
		for( j = i; j + 1 < data.lits.size(); ++ j ) data.lits[j] = data.lits[j+1]; // move all remaining literals one position to front
		data.lits.pop_back(); // remove last literal => deleted current literal, list still sorted!
		--i; // reduce pointer in list
		goto checkNextLiteral;
	      }
	    } 
	  } 
	  checkNextLiteral:; // jump here, if checking the current literal failed
	}
	
	if( data.lits.size() > 3 ) {
	  for( int i = 0 ; i < data.lits.size(); ++ i ) data.lits[i] = ~data.lits[i];
	  foundAmts ++;
	  if( config.fm_debug_out > 1 ) cerr << "c found AM2["<<foundAmts<<"]: " << data.lits << endl;
	  cards.push_back( CardC( data.lits ) ); // use default AMO constructor
	  cards.back().k = 2; // set k to be 2, since its an at-most-two constraint!
	}
	
      }
    }
  }
  amtTime = cpuTime() - amtTime;
  
  MethodTimer fmt( &fmTime );
  // setup all data structures
  
  vector< vector<int> > leftHands(data.nVars() * 2 ),rightHands(data.nVars() * 2 ); // indexes into the cards list
  for( int i = 0 ; i < cards.size(); ++ i ) {
    for( int j = 0; j < cards[i].ll.size(); ++ j )  leftHands[ toInt( cards[i].ll[j] ) ].push_back( i );
    for( int j = 0; j < cards[i].lr.size(); ++ j ) rightHands[ toInt( cards[i].lr[j] ) ].push_back( i );
  }
  
  vec<Lit> unitQueue;
  vector<int> newAMOs,newALOs,newALKs;
  
  // perform FindUnit
  if( config.opt_findUnit ) {
    inAmo.nextStep(); // check for each literal whether its already in the unit queue
    if( config.fm_debug_out > 0 ) cerr << "c find units ... " << endl;
    for( Var v = 0 ; v < data.nVars(); ++v  ) {
      const Lit pl = mkLit(v,false), nl = mkLit(v,true);
      for( int i = 0 ; i < leftHands[toInt(pl)].size() && steps < fmLimit; ++ i ) { // since all cards are compared, be careful!
	const CardC& card1 = cards[leftHands[toInt(pl)][i]];
	steps ++;
	if( !card1.amo() ) continue; // can do this on AMO only
	
	for( int j = 0 ; j < leftHands[toInt(nl)].size(); ++ j ) {
	  const CardC& card2 = cards[leftHands[toInt(nl)][j]];
	  steps ++;
	  if( !card2.amo() ) continue; // can do this on AMO only
	  const vector<Lit>& v1 = card1.ll, &v2 = card2.ll; // get references
	  int n1=0,n2=0;
	  while( n1 < v1.size() && n2 < v2.size() ) {
	    if( v1[n1] == v2[n2] ) { // same literal in two amos with a different literal -> have to be unit!
	      if( ! inAmo.isCurrentStep( toInt(~v1[n1]) ) ) { // store each literal only once in the queue
		sameUnits ++;
		inAmo.setCurrentStep( toInt(~v1[n1]) );
		unitQueue.push(~v1[n1]);
		didSomething = true;
		if( l_False == data.enqueue( ~v1[n1] ) ) return didSomething; // enqueing this literal failed -> finish!
	      }
	      // TODO: enqueue, later remove from all cards!
	      if( config.fm_debug_out > 1 ) cerr << "c AMOs entail unit literal " << ~ v1[n1] << endl;
	      n1++;n2++;
	    } else if( v1[n1] < v2[n2] ) {n1 ++;}
	    else { n2 ++; }
	  }
	}
      }
      // if found something, propagate!
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) return didSomething;
    }
    
  }
  
   
  // perform merge
  if( config.opt_merge ) {
    if( config.fm_debug_out > 0 ) cerr << "c merge AMOs ... " << endl;
    for( Var v = 0 ; v < data.nVars(); ++v  ) {
      const Lit pl = mkLit(v,false), nl = mkLit(v,true);
      if( leftHands[ toInt(pl) ] .size() > 2 || leftHands[ toInt(nl) ] .size() > 2 ) continue; // only if the variable has very few occurrences
      for( int i = 0 ; i < leftHands[toInt(pl)].size() && steps < fmLimit; ++ i ) { // since all cards are compared, be careful!
	steps ++;
	if( cards[leftHands[toInt(pl)][i]].invalid() ) continue;
	if( !cards[leftHands[toInt(pl)][i]].amo() ) continue; // can do this on AMO only
	for( int j = 0 ; j < leftHands[toInt(nl)].size(); ++ j ) {
	  const CardC& card1 = cards[leftHands[toInt(pl)][i]]; // get both references here, because otherwise they will become invalid!
	  const CardC& card2 = cards[leftHands[toInt(nl)][j]];
	  steps ++;
	  if( card2.invalid() ) continue;
	  if( !card2.amo() ) continue; // can do this on AMO only
	  
	  // merge the two cardinality constraints card1 and card2
	  // check for duplicate literals (count them, treat them special!)
	  // otherwise combine the two left hands!
	  // add the new cards to the structure!
	  const vector<Lit>& v1 = card1.ll,& v2 = card2.ll; // get references
	  int n1=0,n2=0;
	  data.lits.clear();
	  while( n1 < v1.size() && n2 < v2.size() ) {
	    if( v1[n1] == v2[n2] ) { // same literal in two amos with a different literal -> have to be unit!
	      if( ! inAmo.isCurrentStep( toInt(~v1[n1]) ) ) { // store each literal only once in the queue
		  sameUnits ++;
		  inAmo.setCurrentStep( toInt(~v1[n1]) );
		  unitQueue.push(~v1[n1]);
		  didSomething = true;
		  if( data.enqueue( ~v1[n1] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << ~v1[n1] << " failed" << endl;
		    return didSomething; // enqueing this literal failed -> finish!
		  } else {
		    cerr << "c found unit, enqued " << ~v1[n1] << "" << endl;
		  }
	      }
	      n1++;n2++;
	    } else if( v1[n1] < v2[n2] ) {
	      if( v1[n1] != pl ) data.lits.push_back(v1[n1]);
	      n1 ++;
	    } else { 
	      if( v2[n2] != nl ) data.lits.push_back(v2[n2]);
	      n2 ++;
	    }
	  }
	  for(; n1 < v1.size(); ++ n1 ) if( v1[n1] != pl ) data.lits.push_back( v1[n1] );
	  for(; n2 < v2.size(); ++ n2 ) if( v2[n2] != nl ) data.lits.push_back( v2[n2] );
	  if( data.lits.size() == 0 ) continue; // no AMO, if there are no literals inside!
	  if( config.fm_debug_out > 0 ) cerr << "c deduced AMO " << data.lits << " from AMO " << card1.ll << " and AMO " << card2.ll << endl;
	  
	  // check whether there are similar variables inside the constraint, if so - drop it!
	  bool hasComplements = false;
	  for( int k = 0 ; k + 1 < data.lits.size(); ++k ) if( data.lits[k] == ~ data.lits[k+1] ) { 
	    if( config.fm_debug_out > 2 ) cerr << "c deduced AMO contains complementary literals - set all others to false!" << endl;
	    Var uv = var(data.lits[k]);
	    for( int k = 0 ; k < data.lits.size(); ++ k ) {
	      if( var(data.lits[k]) == uv ) continue; // do not enqueue complementary literal!
	      if( ! inAmo.isCurrentStep( toInt(~data.lits[k]) ) ) { // store each literal only once in the queue
		  sameUnits ++;
		  inAmo.setCurrentStep( toInt(~data.lits[k]) );
		  unitQueue.push(~data.lits[k]);
		  didSomething = true;
		  if( data.enqueue( ~data.lits[k] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << ~data.lits[k] << " failed" << endl;
		    return didSomething; // enqueing this literal failed -> finish!
		  }
	      }
	    }
	    hasComplements = true; break;
	  }
	  if(hasComplements) continue;
	  
	  const int index = cards.size();
	  cards.push_back( CardC(data.lits) ); // create AMO
	  for( int k = 0 ; k < data.lits.size(); ++ k ) leftHands[ toInt( data.lits[k] ) ].push_back(index);
	}
      }
      // if found something, propagate!
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) return didSomething;
    }
  }
  
  // propagate found units - if failure, skip next steps
  if( data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); return didSomething; }

  // perform find 2product encoding
  findTwoProduct( cards, big, leftHands );
  
  // remove duplicate or subsumed AMOs!
  removeSubsumedAMOs( cards, leftHands );
  
  // try to deduce ALO constraints from a AMO-ALO matrix
  deduceALOfromAmoAloMatrix(cards, leftHands);
  
  // set all variables that appeard in cardinality constraints, to collect clauses next
  data.ma.nextStep();
  for( int i = 0 ; i < cards.size(); ++ i ) {
    const CardC & c = cards[i];
    for( int j = 0; j < c.ll.size(); ++j ) data.ma.setCurrentStep( var(c.ll[j] ) );
    for( int j = 0; j < c.lr.size(); ++j ) data.ma.setCurrentStep( var(c.lr[j] ) );
  }
  // use all clauses that could be useful
  heap.clear();
  for( int i = 0 ; i < data.getClauses().size(); ++ i ) {
    const Clause& c = ca[ data.getClauses()[i] ];
    if( c.can_be_deleted() || c.size() < 3 ) continue; // only larger clauses!
    for( int j = 0 ; j < c.size(); ++ j ) {
      if( data.ma.isCurrentStep( var(c[j])) ) { // a literal of this clause took part in an AMO
	const int index = cards.size();
	cards.push_back( CardC( c ) );
	if( config.fm_debug_out > 1 ) cerr << "c clause " << c << " leads to card " << cards[cards.size() -1 ].ll << " <= " << cards[cards.size() -1 ].k << " + " << cards[cards.size() -1 ].lr << endl;
	for( int j = 0 ; j < c.size(); ++ j ) {
	  rightHands[toInt(c[j])].push_back(index);
	  if( !heap.inHeap(toInt(c[j])) ) heap.insert( toInt(c[j]) );
	}
	usedClauses ++;
	break;
      }
    }
  }

  // perform actual Fourier Motzkin method 
  if( config.fm_debug_out > 0 ) cerr << "c apply FM ..." << endl;
  // for l in F
  while (heap.size() > 0 && (data.unlimited() || fmLimit > steps) && !data.isInterupted() ) 
  {
    /** garbage collection */
    data.checkGarbage();
    
    const Lit toEliminate = toLit(heap[0]);
    assert( heap.inHeap( toInt(toEliminate) ) && "item from the heap has to be on the heap");

    heap.removeMin();

    if( config.fm_debug_out > 1 ) cerr << endl << "c eliminate literal " << toEliminate << endl;
    
    int oldSize = cards.size(),oldNewSize = newAMOs.size(), oldAloSize = newALOs.size(), oldAlkSize = newALKs.size();
    
    // TODO: apply heuristics from BVE (do not increase number of constraints! - first create, afterwards count!
      if( leftHands[ toInt(toEliminate) ] .size() == 0  || rightHands[ toInt(toEliminate) ] .size() == 0 ) {
	pureAmoLits ++;
	continue; // only if the variable has very few occurrences. cannot handle pure here, because its also inside the formula at other places!
      }
      if( config.opt_cutOff && 
	 ((leftHands[ toInt(toEliminate) ] .size() > 5 && rightHands[ toInt(toEliminate) ] .size() > 15)
	 || (leftHands[ toInt(toEliminate) ] .size() > 15 && rightHands[ toInt(toEliminate) ] .size() > 5)
	 || (leftHands[ toInt(toEliminate) ] .size() > 10 && rightHands[ toInt(toEliminate) ] .size() > 10))
      ) continue; // do not eliminate this variable, if it looks too expensive

      for( int i = 0 ; i < leftHands[toInt(toEliminate)].size() && steps < fmLimit; ++ i ) { // since all cards are compared, be careful!
	steps ++;
	if( cards[leftHands[toInt(toEliminate)][i]].invalid() ) { // drop invalid constraints from list!
	  leftHands[toInt(toEliminate)][i] = leftHands[toInt(toEliminate)][ leftHands[toInt(toEliminate)].size() -1 ];
	  leftHands[toInt(toEliminate)].pop_back();
	  --i;
	  continue; // do not work with invalid constraints!
	}
	
	for( int j = 0 ; j < rightHands[toInt(toEliminate)].size(); ++ j ) {
	  if( rightHands[toInt(toEliminate)][j] == leftHands[toInt(toEliminate)][i] ){ 
	    if( config.fm_debug_out > 2 ) cerr << "c irregular literal " << toEliminate << " with constraints (" << rightHands[toInt(toEliminate)][j] << " == " << leftHands[toInt(toEliminate)][i] << "): " << endl
				      << " and  " << cards[rightHands[toInt(toEliminate)][j]].ll << " <= " << cards[rightHands[toInt(toEliminate)][j]].k << " + " << cards[rightHands[toInt(toEliminate)][j]].lr << endl;
	    irregular++; 
	    continue;
	  } // avoid merging a constraint with itself
	  steps ++;
	  
	  if( cards[rightHands[toInt(toEliminate)][j]].invalid() ) { // drop invalid constraints from list!
	    rightHands[toInt(toEliminate)][j] = rightHands[toInt(toEliminate)][ rightHands[toInt(toEliminate)].size() -1 ];
	    rightHands[toInt(toEliminate)].pop_back();
	    --j;
	    continue; // do not work with invalid constraints!
	  }
	  
	  const int index = cards.size();
	  cards.push_back( CardC() );
	  // get references here, because push could change memory locations!
	  const CardC& card1 = cards[leftHands[toInt(toEliminate)][i]];
	  const CardC& card2 = cards[rightHands[toInt(toEliminate)][j]];
	  assert( !card1.invalid() && !card2.invalid() && "both constraints must not be invalid" );

	  bool hasDuplicates = false; // if true, then the resulting constraint is invalid!
	  CardC& thisCard = cards[ index ];
	  int extraK = 0; // if during reasoning the K value needs to be adopted
	  for( int p = 0 ; p < 2; ++ p ) {
	    // first half
	    const vector<Lit>& v1 = p == 0 ? card1.ll : card1.lr;
	    const vector<Lit>& v2 = p == 0 ? card2.ll : card2.lr;
	    int n1=0,n2=0;
	    data.lits.clear();
	    while( n1 < v1.size() && n2 < v2.size() ) {
	      if( config.fm_debug_out ) cerr  << "c [FM] compare " << v1[n1] << " with " << v2[n2] << endl;
	      if( v1[n1] == v2[n2] ) { // same literal in two amos with a different literal -> have to be unit!
		addDuplicates ++;
		// TODO: enqueue, later remove from all cards!
		data.lits.push_back( v1[n1] ); // keep them
		hasDuplicates = true;
		n1++;n2++;
		break;
	      } else if (v1[n1] == ~v2[n2] ) { // complementary literal, remove both, increase counting variable
		extraK = (p == 0) ? extraK - 1: extraK + 1; // depending on whether the complementary pair is removed on the left or the right side of the <= operator, decrease or increase!
		n1++;n2++;
	      } else if( v1[n1] < v2[n2] ) {
		if( v1[n1] != toEliminate ) data.lits.push_back(v1[n1]);
		n1 ++;
	      } else { 
		if( v2[n2] != toEliminate ) data.lits.push_back(v2[n2]);
		n2 ++;
	      }
	    }
	    if(!hasDuplicates) { // add the remaining elements of one of the vectors!
	      for(; n1 < v1.size(); ++ n1 ) if( v1[n1] != toEliminate ) data.lits.push_back( v1[n1] );
	      for(; n2 < v2.size(); ++ n2 ) if( v2[n2] != toEliminate ) data.lits.push_back( v2[n2] );
	    }
	    if( p == 0 ) thisCard.ll = data.lits;
	    else thisCard.lr = data.lits;
	  }
	  
	  if( hasDuplicates ) {
	    if( config.fm_debug_out > 1 ) cerr << "c new card would have duplicates - drop it (can be fixed with full FM algorithm which uses weights)" << endl
	         << "c  from " << card1.ll << " <= " << card1.k << " + " << card1.lr << endl
		 << "c   and " << card2.ll << " <= " << card2.k << " + " << card2.lr << endl << endl;
	     cards.pop_back(); // remove last card again
	     continue;
	  }
	  
	  int nl = 0,nr = 0,kl=0,kr=0;
	  while( nl < thisCard.ll.size() && nr < thisCard.lr.size() ) {
	    if( thisCard.ll[nl] == thisCard.lr[nr] ) { // do not keep the same literal!
	       nr ++; nl ++;
	    } else if( thisCard.ll[nl] == ~thisCard.lr[nr] ) { // approximate, keep only the literal on the left side of the operator!
	      thisCard.ll[kl++] = thisCard.ll[nl]; nl ++; // keep literal on the left side
	    } else if( thisCard.ll[nl] < thisCard.lr[nr] ) {
	      thisCard.ll[kl++] = thisCard.ll[nl]; nl ++; // keep this literal on the left side!
	    } else {
	      thisCard.lr[kr++] = thisCard.lr[nr]; nr ++; // keep this literal on the right side!
	    }
	  }
	  for( ; nr < thisCard.lr.size() ; ++ nr ) thisCard.lr[kr++] = thisCard.lr[nr];
	  for( ; nl < thisCard.ll.size() ; ++ nl ) thisCard.ll[kl++] = thisCard.ll[nl];
	  thisCard.lr.resize(kr);
	  thisCard.ll.resize(kl);
	  
	  thisCard.k = card1.k + card2.k + extraK; // do not forget the extra cardinality value!	  
	  
	  if( config.fm_debug_out > 1 ) cerr << "c resolved new CARD " << thisCard.ll << " <= " << thisCard.k << " + " << thisCard.lr << "  (unit: " << thisCard.isUnit() << " taut: " << thisCard.taut() << ", failed: " << thisCard.failed() << "," << (int)thisCard.lr.size() + thisCard.k << ")" << endl
				    << "c from " << card1.ll << " <= " << card1.k << " + " << card1.lr << endl
				    << "c and  " << card2.ll << " <= " << card2.k << " + " << card2.lr << endl << endl;
	  if( thisCard.taut() ) {
	     if( config.fm_debug_out > 1 ) cerr << "c new card is taut! - drop it" << endl;
	     cards.pop_back(); // remove last card again
	     continue;
	  } else if( thisCard.failed() ) {
	    if( config.fm_debug_out > 1 ) cerr << "c new card is failed! - stop procedure!" << endl;
	    didSomething = true;
	    data.setFailed();
	    return didSomething;
	  }else if( thisCard.isUnit() ) { // all literals in ll have to be set to false!
	    if( config.fm_debug_out > 1 ) cerr << "c new card is unit - enqueue all literals" << endl;
	    deducedUnits += thisCard.ll.size() + thisCard.lr.size(); 
	    for( int k = 0 ; k < thisCard.ll.size(); ++ k ) {
		if( ! inAmo.isCurrentStep( toInt(~thisCard.ll[k]) ) ) { // store each literal only once in the queue
		  inAmo.setCurrentStep( toInt(~thisCard.ll[k]) );
		  unitQueue.push(~thisCard.ll[k]);
		  didSomething = true;
		  if( data.enqueue( ~thisCard.ll[k] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << ~thisCard.ll[k] << " failed" << endl;
		    return didSomething;
		  }
		}
	    }
	    for( int k = 0 ; k < thisCard.lr.size(); ++ k ) { // all literals in lr have to be set to true!
		if( ! inAmo.isCurrentStep( toInt(thisCard.lr[k]) ) ) { // store each literal only once in the queue
		  inAmo.setCurrentStep( toInt(thisCard.lr[k]) );
		  unitQueue.push(thisCard.lr[k]);
		  didSomething = true;
		  if( data.enqueue( thisCard.lr[k] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << thisCard.lr[k] << " failed" << endl;
		    return didSomething;
		  }
		}
	    }
	    cards.pop_back(); // remove last card again, because all its information has been used already!
	    continue;
	  } else if (thisCard.amo() ) {
	    if( config.fm_debug_out > 1 ) cerr << "c new card is NEW amo - memorize it!" << endl;
	    if(config.opt_newAmo) newAMOs.push_back( index );
	    newAmos ++;
	  } else if (thisCard.alo() && config.opt_newAlo > 0) {
	    if( config.fm_debug_out > 1 ) cerr << "c new card is NEW alo - memorize it!" << endl;
	    newAlos ++; 
	    if(config.opt_newAmo) newALOs.push_back( index );
	  }  else if (thisCard.alo() && config.opt_newAlk > 0) {
	    if( config.fm_debug_out > 1 ) cerr << "c new card is NEW alk - memorize it!" << endl;
	    newAlks ++; 
	    if(config.opt_newAmo) newALKs.push_back( index );
	  }
	}
      }
      
      // new constraints > removed constraints + grow -> drop the elimination again!
      if( (cards.size() - oldSize > leftHands[ toInt(toEliminate) ] .size() + rightHands[ toInt(toEliminate) ].size()  + config.opt_fmGrow ) // local increase to large
	|| (cardDiff > config.opt_fmGrowT ) // global limit hit
      ) {
	discardedCards += (cards.size() - oldSize);
	// if( rejectedNewAmos
	if( config.opt_keepAllNew ) { // copy the new AMOs that should be rejected
	  for( int p = oldNewSize; p < newAMOs.size(); ++ p ) {
	    rejectedNewAmos.push_back( cards[ newAMOs[p] ] );
	  }
	} else discardedNewAmos += (newAMOs.size() - oldNewSize );
	if( config.opt_newAlo > 1  ) { // copy the new AMOs that should be rejected
	  for( int p = oldAloSize; p < newAMOs.size(); ++ p ) {
	    rejectedNewAlos.push_back( cards[ newAMOs[p] ] );
	  }
	} 
	if( config.opt_newAlk > 1 ) { // copy the new AMOs that should be rejected
	  for( int p = oldAlkSize; p < newAMOs.size(); ++ p ) {
	    rejectedNewAlks.push_back( cards[ newAMOs[p] ] );
	  }
	} 
	
	// remove and shrink pointer lists!
	cards.resize( oldSize );
	newAMOs.resize( oldNewSize );
	newALOs.resize( oldAloSize );
	newALKs.resize( oldAlkSize );
	assert( (newAMOs.size() == 0 || newAMOs.back() < cards.size()) && "after shrink, no pointer to invalid constraints can be left!" );
	continue;
      } 
      
      cardDiff += ((int)cards.size() - (int)oldSize - (int)leftHands[ toInt(toEliminate) ] .size() + (int)rightHands[ toInt(toEliminate) ].size());
      
      // remove all old constraints, add all new constraints!
      for( int p = 0 ; p < 2; ++ p ) {
	vector<int>& indexes = p==0 ? leftHands[ toInt(toEliminate) ] : rightHands[ toInt(toEliminate) ];
	removedCards += indexes.size();
	while( indexes.size() > 0 ) {
	  int removeIndex = indexes[0]; // copy, because reference of vector will be changed!
	  const CardC& card = cards[ removeIndex ];
	  if( config.fm_debug_out > 2 ) cerr << "c remove  " << card.ll << " <= " << card.k << " + " << card.lr << " ... " << endl;
	  for( int j = 0 ; j < card.ll.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.ll[j];
	    // if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    if( config.fm_debug_out > 3 ) cerr << "c check " << removeIndex << " in " << leftHands[toInt(l)] << endl;
	    for( int k = 0 ; k < leftHands[toInt(l)].size(); ++ k ) {
	      if( leftHands[toInt(l)][k] == removeIndex ) {
		if( config.fm_debug_out > 3 ) cerr << "c from leftHand " << l << "( " << j << "/" << card.ll.size() << "," << k << "/" << leftHands[toInt(l)].size()  << ")" << endl;
		leftHands[toInt(l)][k] = leftHands[toInt(l)][ leftHands[toInt(l)].size() - 1]; leftHands[toInt(l)].pop_back(); 
		break;
	      }
	    }
	    if( config.fm_debug_out > 3 ) cerr << "c finished literal " << l << "( " << j << "/" << card.ll.size() << ")" << endl;
	  }
	  for( int j = 0 ; j < card.lr.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.lr[j];
	    // if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    if( config.fm_debug_out > 3 ) cerr << "c check " << removeIndex << " in " << rightHands[toInt(l)] << endl;
	    for( int k = 0 ; k < rightHands[toInt(l)].size(); ++ k ) {
	      if( rightHands[toInt(l)][k] == removeIndex ) {
		if( config.fm_debug_out > 3 ) cerr << "c from rightHands " << l << "( " << j << "/" << card.lr.size() << "," << k << "/" << rightHands[toInt(l)].size()  << ")" << endl;
		rightHands[toInt(l)][k] = rightHands[toInt(l)][ rightHands[toInt(l)].size() - 1]; rightHands[toInt(l)].pop_back(); break;
	      }
	    }
	    if( config.fm_debug_out > 3 ) cerr << "c finished literal " << l << "( " << j << "/" << card.ll.size() << ")" << endl;
	  }
	  
	}
      }
      
      // add new constraints
      for( int i = oldSize; i < cards.size(); ++ i ) {
	const CardC& card = cards[i];
	for( int j = 0 ; j < card.ll.size(); ++ j )  leftHands[ toInt(card.ll[j]) ].push_back(i);
	for( int j = 0 ; j < card.lr.size(); ++ j ) rightHands[ toInt(card.lr[j]) ].push_back(i);
      }
      newCards += cards.size() - oldSize;
    
      // if found something, propagate!
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) return didSomething;
  }
  
  // propagate found units - if failure, skip next steps
  if( data.ok() && data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); return didSomething; }
  
  if( data.ok() && config.opt_newAmo > 0 && (newAMOs.size() > 0 || rejectedNewAmos.size() > 0) ) {
    big.recreate( ca, data.nVars(), data.getClauses() ); 
    if(config.opt_newAmo > 1) big.generateImplied(data); // try to avoid adding redundant clauses!
    for( int p = 0; p<2; ++ p ) {
      // use both the list of newAMOs, and the rejected new amos!
      for( int i = 0 ; i < (p == 0 ? newAMOs.size() : rejectedNewAmos.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newAMOs[i]] : rejectedNewAmos[i];
	if( c.invalid() || !c.amo() ) {
	  if( config.fm_debug_out > 1 ) cerr << "c new AMO " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	for( int j = 0 ; j < c.ll.size(); ++ j ) {
	  for( int k = j+1; k < c.ll.size(); ++ k ) {
	    bool present = false;
	    if( config.opt_newAmo == 2 ) present = big.implies(c.ll[j],~c.ll[k]) || big.implies(~c.ll[k],c.ll[j]); // fast stamp check
	    if(!present) present = big.isChild(c.ll[j],~c.ll[k]) || big.isChild(c.ll[k],~c.ll[j]); // slower list check
	    if( !present ) { // if the information is not part of the formula yet, add the clause!
	      if( config.fm_debug_out > 2 ) cerr << "c create new binary clause " <<  ~c.ll[k] << " , " << ~c.ll[j] << endl;
	      addedBinaryClauses ++;
	      didSomething = true;
	      unitQueue.clear();
	      unitQueue.push( ~c.ll[k] < ~c.ll[j] ? ~c.ll[k] : ~c.ll[j] );
	      unitQueue.push( ~c.ll[k] < ~c.ll[j] ? ~c.ll[j] : ~c.ll[k] );
	      CRef tmpRef = ca.alloc(unitQueue, false); // no learnt clause!
	      assert( ca[tmpRef][0] < ca[tmpRef][1] && "the clause has to be sorted!" );
	      data.addClause( tmpRef );
	      data.getClauses().push( tmpRef );
	    }
	  }
	}
	unitQueue.clear();
      }
    }
  }
  
  if( data.ok() && config.opt_newAlo > 0 && (newALOs.size() > 0 || rejectedNewAlos.size() > 0) ) {
    for( int p = 0; p<2; ++ p ) {
      for( int i = 0 ; i < (p == 0 ? newALOs.size() : rejectedNewAlos.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newALOs[i]] : rejectedNewAlos[i];
	if( c.invalid() || !c.alo() ) {
	  if( config.fm_debug_out > 1 ) cerr << "c new ALO " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	didSomething = true;
	if( c.lr.size() == 1 ) {
	  if( data.enqueue(c.lr[0]) == l_False ) return didSomething;
	} else if( ! hasDuplicate(c.lr) ) {
	  CRef tmpRef = ca.alloc(c.lr, false); // no learnt clause!
	  ca[tmpRef].sort();
	  assert( ca[tmpRef][0] < ca[tmpRef][1] && "the clause has to be sorted!" );
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  addedClauses++;
	}
      }
    }
  }
  
  // propagate found units - if failure, skip next steps
  if( data.ok() && data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); return didSomething; }
  
  if( data.ok() && config.opt_newAlk > 0 && (newALKs.size() > 0 || rejectedNewAlks.size() > 0) ) {
    for( int p = 0; p<2; ++ p ) {
      for( int i = 0 ; i < (p == 0 ? newALKs.size() : rejectedNewAlks.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newALKs[i]] : rejectedNewAlks[i];
	if( c.invalid() || !c.alk() ) {
	  if( config.fm_debug_out > 1 ) cerr << "c new ALK " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	didSomething = true;
	if( c.lr.size() == 1 ) {
	  if( data.enqueue(c.ll[0]) == l_False ) return didSomething;
	} else if( ! hasDuplicate(c.lr) ) {
	  assert( c.lr.size() > 1 && "empty and unit should have been handled before!" );
	  CRef tmpRef = ca.alloc(c.lr, false); // no learnt clause!
	  ca[tmpRef].sort();
	  assert( ca[tmpRef][0] < ca[tmpRef][1] && "the clause has to be sorted!" );
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  addedClauses++;
	}
      }
    }
  }
  
  // propagate found units - if failure, skip next steps
  if( data.ok() && data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); return didSomething; }
  
  finishedFM:;
  
  fmTime = cpuTime() - fmTime;
  
  return didSomething;
}


void FourierMotzkin::removeSubsumedAMOs(vector< FourierMotzkin::CardC >& cards, vector< std::vector< int > >& leftHands)
{
  for( int i = 0 ; i < cards.size(); ++ i )
  {
    CardC& c = cards[ i ];
    if( !c.amo() ) continue; // do only for AMOs
    
    Lit min = c.ll[0];
    for( int j = 1; j < c.ll.size(); ++ j )
      if( leftHands[ toInt( c.ll[j] ) ] < leftHands[ toInt( min ) ] ) min = c.ll[j];
    
    // check whether an AMO, or a bigger AMO can be found in the list of min
    for( int m = 0 ; m < leftHands[ toInt(min) ].size(); ++ m ) {
      const CardC& ref = cards[ leftHands[ toInt(min) ] [m] ];
      if( !ref.amo() || leftHands[ toInt(min) ] [m] == i ) continue; // look only for amos, and do not compare itself with the current AMO
      
      int j = 0, k = 0; // loop over both amos!
      const vector<Lit>& rl = ref.ll;
      while( j < c.ll.size() && k < rl.size() )
      {
	if( c.ll[j] < rl[k] ) break; // the new AMO is new
	else if ( rl[k] < c.ll[j] ) k ++; // simply jump over lits that are in ref, but not in lits
	else { ++j; ++k; }
      }
      if( j == c.ll.size() ) c.invalidate(); // invalidate each AMO that has been found to be redundant
    }
    
  }
}


void FourierMotzkin::deduceALOfromAmoAloMatrix(vector< FourierMotzkin::CardC >& cards, vector< std::vector< int > >& leftHands)
{
  MethodTimer mt( &deduceAloTime );
  if( config.fm_debug_out > 0 ) cerr << "c run deduceAlo method" << endl;
  
  vector<int> amoCands;
  vector<int> amoCandsLocal;
  for( int i = 0 ; i < cards.size(); ++ i ) 
  {
    const CardC& A = cards[i]; 	// cardinality constraints are sorted
    if( !cards[i].amo() ) continue;	// not an AMO, test next AMO, use only AMOs with size > 2 (heuristic!)
    const Lit a = cards[i].ll[0];	// since sorted, the first literal in ll is the smallest literal
    searchSteps ++;
    if( config.fm_debug_out > 2 ) cerr << "c work on AMO " << A.ll << endl;
   
    const int listAsize = data.list( a ).size();
    for( int j = 0 ; j < listAsize; ++ j ) // for B in ALO_l
    {
      const Clause& B = ca[ data.list(a)[j] ];
      searchSteps ++;
      if( B.size() > A.ll.size() ) continue; // |B| <= |A| !

      if( config.fm_debug_out > 2 ) cerr << "c test with clause  " << B << endl;

      bool isMin = true;
      for( int k = 0 ; k < B.size(); ++ k )
	if( B[k] < a ) { isMin = false; break; }
      if( !isMin ) continue;	// do not use the current clause, because it contains a smaller literal!

      // check whether all ALOs for the current B are present
      data.clss.clear();	// ALO candidates for matrix!
      data.ma.nextStep();
      for( int k = 0 ; k < A.ll.size(); ++ k ) {	// for kLit in A, each lit in A should hit an ALO that hits als AMOs
	const Lit kLit = A.ll[k];
	bool foundAlo = true;
	for( int m = 0 ; m < data.list( kLit ).size(); ++ m ) {
	  searchSteps ++;
	  const Clause& ALOk = ca[ data.list( kLit )[m] ];
	  if( ALOk.size() != B.size() ) continue;
	  bool hitAlready = false;
	  for( int n = 0 ; n < ALOk.size(); ++ n ) {
	    if (data.ma.isCurrentStep( toInt( ALOk[n] ) ) ) { hitAlready = true; break; }
	  }
	  if( hitAlready ) continue;
	  data.clss.push_back( data.list( kLit )[m] );
	  for( int n = 0 ; n < ALOk.size(); ++ n ) // avoid duplicates!
	    data.ma.setCurrentStep( toInt( ALOk[n] ) );
	  break;	// sufficient to find one!
	}
      }
      if( config.fm_debug_out > 2 ) cerr << "c collected " << data.clss.size() << " ALOs" << endl;
      if( searchSteps > config.opt_fmSearchLimit ) break;
      if( data.clss.size() < A.ll.size() ) continue;
      
      // check whether sufficiently many AMOs are present (size of AMO might be larger than required!)
      amoCands.clear();
      data.ma.nextStep();
      bool nextB = false;
      for( int k = 0 ; k < B.size(); ++ k ) {
	const Lit bLit = B[k];
	for( int m = 0; m < leftHands[ toInt( bLit )] .size(); ++ m ) {
	  searchSteps ++;
	  const CardC& C = cards[ leftHands[ toInt( bLit )][m] ];
	  if( !C.amo() ) continue; // not this card!
	  if( C.ll.size() != A.ll.size() ) continue; // not a good size!
	  bool hitAlready = false;
	  for( int n = 0 ; n < C.ll.size(); ++ n ) {
	    if (data.ma.isCurrentStep( toInt( C.ll[n] ) ) ) { hitAlready = true; break; }
	  }
	  if( hitAlready ) continue;
	  // use this AMO! mark its lits to avoid duplicates in AMO!
	  amoCands.push_back( leftHands[ toInt( bLit )][m] );
	  for( int n = 0 ; n < C.ll.size(); ++ n )
	    data.ma.setCurrentStep( toInt( C.ll[n] ) );
	  break;
	}
      }
      if( config.fm_debug_out > 2 ) cerr << "c collected " << amoCands.size() << " AMOs" << endl;
      if( nextB || amoCands.size() != B.size() ) continue; // has to find enough AMOs
      if( searchSteps > config.opt_fmSearchLimit ) break;
      
      // have the two sets for AMOs and ALOs to try to construct the matrix!
      data.ma.nextStep();
      for( int k = 0 ; k < data.clss.size(); ++ k ) 
      {
	const Clause& C = ca[ data.clss[k] ];
	if( config.fm_debug_out > 2 ) cerr << "c hit check with clause " << C << endl;
	amoCandsLocal =  amoCands; // add all these literals to the vector
	for( int m = 0 ; m < C.size(); ++ m ) {
	  searchSteps ++;
	  const Lit mLit = C[m];
	  if( data.ma.isCurrentStep( toInt(mLit) ) ) { 
	    if( config.fm_debug_out > 2 ) cerr << "c found lit that was hit already: " << mLit << endl;
	    nextB = true; break;
	  }
	  data.ma.setCurrentStep( toInt( mLit) );
	  for( int n = 0; n < amoCandsLocal.size(); ++n ) {
	    bool containsMLit = false;
	    const CardC& D = cards[ amoCandsLocal[n] ];	// current AMO D
	    for( int o = 0 ; o < D.ll.size(); ++o ){
	      if( D.ll[o] == mLit ) { 
		if( config.fm_debug_out > 2 ) cerr << "c lit " << mLit << " from ALO hits lit " << D.ll[o] << " from AMO" << endl;
		containsMLit = true; 
		break; // hit exactly one literal in each constraint!
	      }
	    }
	    if( containsMLit ) {	// if m \in D
	      amoCandsLocal[n] = amoCandsLocal[ amoCandsLocal.size() - 1 ];
	      amoCandsLocal.pop_back();
	      break;
	    }
	  }
	}
	if( config.fm_debug_out > 3 ) cerr << "c non-hit amos: " << amoCandsLocal.size() << endl;
	if( amoCandsLocal.size() != 0 || nextB ) break; // if S != 0, goto next B
      }
      if( nextB || amoCandsLocal.size() != 0) continue; // did not hit all lits in last iteration
      
      // here, the new AMOs can be added!
      if( config.fm_debug_out > 2 ) {
	cerr << "c can add the following ALOs, due to" << endl;
	for( int k = 0 ; k < data.clss.size(); ++ k ) cerr << "c ALO " << ca[ data.clss[k] ] << endl;
	for( int k = 0 ; k < amoCands.size(); ++ k ) cerr << "c AMO " << cards[ amoCands[k] ].ll << endl;
      }
      for( int k = 0 ; k < amoCands.size(); ++ k ) {
	if( config.fm_debug_out > 0 ) cerr << "c can add ALO " << cards[amoCands[k]].ll << endl;
	
	if( ! hasDuplicate( cards[amoCands[k]].ll ) ) { // if there is not already a clause that is subsumed by the current clause
	  CRef cr = ca.alloc( cards[amoCands[k]].ll , false); 
	  data.addClause(cr);
	  data.getClauses().push(cr);
	  dedAlos ++; // stats
	}
      }
      
    }
   
   
  }
}

void FourierMotzkin::findTwoProduct(vector< FourierMotzkin::CardC >& cards, BIG& big, vector< std::vector< int > >& leftHands)
{
  if( config.opt_fmSearchLimit <= searchSteps ) return; // if limit is reached, invalidate current AMO candidate
  MethodTimer mt( &twoPrTime );
  if( config.fm_debug_out > 0 ) cerr << "c run find two product" << endl;
  MarkArray newAmoLits;
  newAmoLits.create(2*data.nVars());
  
  for( int i = 0 ; i < cards.size(); ++ i ) 
  {
    if( config.opt_fmSearchLimit <= searchSteps ) { // if limit is reached, invalidate current AMO candidate
      break;
    }
    CardC& A = cards[i]; 	// cardinality constraints are sorted
    if( !cards[i].amo() || cards[i].invalid() ) continue;	// not an AMO, test next AMO, use only AMOs with size > 2 (heuristic!)
    const Lit a = cards[i].ll[0];	// since sorted, the first literal in ll is the smallest literal
    
    if( config.fm_debug_out > 2 ) cerr << "c work on AMO " << cards[i].ll << " with smallest lit " << a << endl;
    
    Lit* aList = big.getArray( ~a);
    if( big.getSize( ~a) == 0 ) continue; // not this AMO, because the smallest literal does not imply other literals
    Lit l = ~ aList[0];
    for( int j = 1 ; j < big.getSize( ~a); ++ j ){ // get the smallest literal, but negated!
      if( ~aList[j] < l ) l = ~aList[j];
      searchSteps++;
    }
      
    if( config.fm_debug_out > 2 ) cerr << "c min lit = " << l << " that implies " << big.getSize(l) << endl;
    for( int j = 0 ; j < big.getSize( l ); ++ j ) // for b in BIG(l), b!=a
    {
      if( config.opt_fmSearchLimit <= searchSteps ) break; // if limit is reached, invalidate current AMO candidate
      const Lit b = big.getArray(l)[j];	// literal hit by the literal l.
      if( b == a ) continue;
      if( config.fm_debug_out > 2 ) cerr << "c current implied: " << b << endl;
      
      for( int k = 0 ; k < leftHands[ toInt(b) ].size(); ++k ) // for B in AMOs_b, A != B
      {
	if( leftHands[ toInt(b) ][k] <= i ) {
	  if( config.fm_debug_out > 3 ) cerr << "c reject AMO " << cards[ leftHands[ toInt(b) ][k] ].ll << " because of position " << endl;
	  continue;		// A != B, and check each pair only once!
	}
	searchSteps++;
	CardC& B = cards[ leftHands[ toInt(b) ][k] ];		// turn index into CardC
	if( ! B.amo() ) continue; // work only on pairs of amo!
	// if( b != B.ll[0] ) continue; // use this AMO B only, if b is its smallest literal (avoid duplicates)
	// TODO: check sizes of A and B, use them only, if they differ with max 1
	data.lits.clear();	// global AMO P, sufficient since there won't be local ones!
	newAmoLits.nextStep();	// new set of literals
	if( config.fm_debug_out > 3 ) cerr << "c check AMOs " << cards[i].ll << " and " << B.ll << endl;
	for( int m = 0 ; m < B.ll.size(); ++m ) // for literal k in B
	{
	  if( config.opt_fmSearchLimit <= searchSteps ) { data.lits.clear(); break; } // reached limit?
	  const Lit bLit = B.ll[m]; // each literal in B should imply a set of literals, where each of these literals hits a different lit in A
	  searchSteps++;
	  if( config.fm_debug_out > 3 ) cerr << "c current lit in B: " << bLit << endl;
	  
	  data.ma.nextStep();	// prepare for count different hits
	  for( int n = 0 ; n < cards[i].ll.size(); ++ n )  // mark all literals from AMO A for becoming hit in this iteration
	    data.ma.setCurrentStep( toInt( cards[i].ll[n] ) );
	  
	  for( int n = 0 ; n < big.getSize( ~bLit ); ++ n ) // check whether this literal implies a set of literal, which hit all different literals from A
	  {
	    searchSteps++;
	    const Lit hitLit = ~ big.getArray( ~bLit )[n];
	    if( newAmoLits.isCurrentStep( toInt(hitLit) ) ) continue; // do not collect such a literal twice!
	    
	    if( config.fm_debug_out > 3 ) cerr << "c test hitLit " << hitLit << " which implies " << big.getSize( hitLit ) << " lits"  << endl;

	    for( int o = 0 ; o < big.getSize( hitLit ); ++o ) { // check whether this literal hits a literal from A
	      searchSteps++;
	      const Lit targetLit = big.getArray( hitLit )[o];
	      if( data.ma.isCurrentStep( toInt( targetLit ) ) ) {
		if( config.fm_debug_out > 3 ) cerr << "c target lit " << targetLit << endl;
		data.ma.reset( toInt( targetLit ) );	// count hit, reset current variable
		data.lits.push_back( hitLit );
		newAmoLits.setCurrentStep( toInt(hitLit) );	// do not add this lit twice to the current set
		break;					// take the first best literal that is hit! NOTE approximation!
	      } else {
		if( config.fm_debug_out > 3 ) cerr << "c re-hit target lit " << targetLit << endl;
	      }
	    }
	  }  // end collecting literals that hit lits in A
	  
	} // end for lits in B
	// add the global set of lits as new AMO
	if( data.lits.size() > 0 ) { // if there is a global AMO
	  sort( data.lits );
	  searchSteps+= data.lits.size(); // approximate sorting
	  // TODO check for complementary literals! or being unit
	  if( !amoExistsAlready(data.lits, leftHands, cards ) ) {
	    if( config.fm_debug_out > 0 ) cerr << "c found new AMO " << data.lits << endl;
	    if( config.fm_debug_out > 2 ) cerr << "c   based on " << cards[i].ll << " and AMO " << B.ll << endl; // here, the reference is still working
	    for( int n = 0 ; n < data.lits.size(); ++ n )
	      leftHands[ toInt( data.lits[n] ) ].push_back( cards.size() );	// register new AMO in data structures
	    cards.push_back( CardC( data.lits ) ); // actually add new AMO
	    twoPrAmos ++;
	    twoPrAmoLits += data.lits.size();
	  }
	}
      } // select next B

    } // end looping over implied lits from A's smallest literal
    
  } // end looping over cardinality constraints
  
}

bool FourierMotzkin::amoExistsAlready(const vector< Lit >& lits, vector< std::vector< int > >& leftHands, vector< FourierMotzkin::CardC >& cards)
{
  // check whether the sorted set of literals is already present as AMO (or larger AMO!)
  // implement with while loop, running over two constraints at the same time, for all constraints with the least frequent literal
  
  if ( lits.size() == 0 ) return true; // amo() is always true!

  Lit min = lits[0];
  for( int i = 1; i < lits.size(); ++ i )
    if( leftHands[ toInt( lits[i] ) ] < leftHands[ toInt( min ) ] ) min = lits[i];
  
  // check whether an AMO, or a bigger AMO can be found in the list of min
  for( int i = 0 ; i < leftHands[ toInt(min) ].size(); ++ i ) {
    const CardC& ref = cards[ leftHands[ toInt(min) ] [i] ];
    if( !ref.amo() ) continue; // look only for amos
    steps ++;
    int j = 0, k = 0; // loop over both amos!
    const vector<Lit>& rl = ref.ll;
    while( j < lits.size() && k < rl.size() )
    {
      if( lits[j] < rl[k] ) break; // the new AMO is new
      else if ( rl[k] < lits[j] ) k ++; // simply jump over lits that are in ref, but not in lits
      else { ++j; ++k; }
    }
    if( j == lits.size() ) return true; // found a AMO that is at least as strong as the current one (might contain more lits!)
  }
  
  return false;
}


    
template <class S, class T>
static bool ordered_subsumes (const S & c, const T& other)
{
    int i = 0, j = 0;
    while (i < c.size() && j < other.size())
    {
        if (c[i] == other[j])
        {
            ++i;
            ++j;
        }
        // D does not contain c[i]
        else if (c[i] < other[j])
            return false;
        else
            ++j;
    }
    if (i == c.size())
        return true;
    else
        return false;
}
    
bool FourierMotzkin::hasDuplicate(const vector<Lit>& c)
{
  if( c.size() == 0 ) return false;
  Lit min = c[0];
  for( int i = 1; i < c.size(); ++ i ) if( data[min] < data[c[i]] ) min = c[i];
  
  vector<CRef>& list = data.list(min); 
  for( int i = 0 ; i < list.size(); ++ i ) {
    Clause& d = ca[list[i]];
    if( d.can_be_deleted() ) continue;
    int j = 0 ;
    if( d.size() == c.size() ) { // do not remove itself - cannot happen, because one clause is not added yet
      while( j < c.size() && c[j] == d[j] ) ++j ;
      if( j == c.size() ) { 
	if( config.fm_debug_out > 1 ) cerr << "c clause is equal to [" << list[i] << "] : " << d << endl;
	detectedDuplicates ++;
	return true;
      }
    }
    if( config.opt_checkSub ) { // check each clause for being subsumed -> kick subsumed clauses!
      if( d.size() < c.size() ) {
	detectedDuplicates ++;
	if( ordered_subsumes(d,c) ) {
	  if( config.fm_debug_out > 1 ) cerr << "c clause " << c << " is subsumed by [" << list[i] << "] : " << d << endl;
	  return true; // the other clause subsumes the current clause!
	}
      } if( d.size() > c.size() ) { // if size is equal, then either removed before, or not removed at all!
	if( ordered_subsumes(c,d) ) { 
	  d.set_delete(true);
	  data.removedClause(list[i]);
	  detectedDuplicates++;
	}
      }
    }
  }
  return false;
}
    
bool FourierMotzkin::propagateCards(vec< Lit >& unitQueue, vector< std::vector< int > >& leftHands, vector< std::vector< int > >& rightHands, vector< FourierMotzkin::CardC >& cards, MarkArray& inAmo)
{
    // propagate method
    while( unitQueue.size() > 0 ) {
      Lit p = unitQueue.last(); unitQueue.pop();
      for( int p1 = 0; p1 < 2; ++ p1 ) {
	if( p1 == 1 ) p = ~p;
	for( int p2 = 0 ; p2 < 2; ++ p2 ) {
	  vector<int>& indexes = (p2==0 ? leftHands[ toInt(p) ] : rightHands[ toInt(p) ]);
	  for( int i = 0 ; i < indexes.size(); ++ i ) {
	    CardC& c = cards[ indexes[i] ];
	    steps ++;
	    if( c.invalid() ) continue; // do not consider this constraint any more!
	    if( config.fm_debug_out > 2 ) cerr << "c propagate " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    // remove all assigned literals and either reduce the ll vector, or "k"
	    int kc = 0;
	    for( int j = 0 ; j < c.ll.size(); ++ j ) {
	      if( data.value(c.ll[j]) == l_True ) {
		if( config.fm_debug_out > 2 ) cerr << "c since " << c.ll[j] << " == top, reduce k from " << c.k << " to " << c.k - 1 << endl;
		c.k --;
	      } else if( data.value(c.ll[j]) == l_Undef ) {
		if( config.fm_debug_out > 2 ) cerr << "c keep literal " << c.ll[j] << endl;
		c.ll[kc++] = c.ll[j];
	      } else if( config.fm_debug_out > 2 ) cerr << "c drop unsatisfied literal " << c.ll[j] << endl;
	    }
	    c.ll.resize(kc);
	    if( config.fm_debug_out > 2 ) cerr << "c        to " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    if(c.isUnit() ) {  // propagate AMOs only!
	      for( int j = 0 ; j < c.ll.size(); ++ j ) { // all literals in ll have to be set to false
		if( ! inAmo.isCurrentStep( toInt(~c.ll[j]) ) ) { // store each literal only once in the queue
		  propUnits ++;
		  inAmo.setCurrentStep( toInt(~c.ll[j]) );
		  unitQueue.push(~c.ll[j]);
		  if( data.enqueue( ~c.ll[j] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << ~c.ll[j] << " failed" << endl;
		     return false; // enqueing this literal failed -> finish!
		  }
		}
	      }
	      for( int j = 0 ; j < c.lr.size(); ++ j ) { // al literals in lr have to be set to true
		if( ! inAmo.isCurrentStep( toInt(c.lr[j]) ) ) { // store each literal only once in the queue
		  propUnits ++;
		  inAmo.setCurrentStep( toInt(c.lr[j]) );
		  unitQueue.push(c.lr[j]);
		  if( data.enqueue( c.lr[j] ) == l_False ) {
		    if( config.fm_debug_out > 1 ) cerr << "c enquing " << c.lr[j] << " failed" << endl;
		     return false; // enqueing this literal failed -> finish!
		  }
		}
	      }
	      c.invalidate();
	    }
	    if( c.taut() ) c.invalidate();
	    else if ( c.failed() ) {
	      if( config.fm_debug_out > 1 ) cerr << "c resulting constraint cannot be satisfied!" << endl;
	      data.setFailed(); return false;
	    }
	  }
	}
      }
      // clear all occurrence lists!
      leftHands[toInt(p)].clear();leftHands[toInt(~p)].clear();
      rightHands[toInt(p)].clear();rightHands[toInt(~p)].clear();
    }
    return true;
}
    
    
void FourierMotzkin::printStatistics(ostream& stream)
{
  stream << "c [STAT] FM " << processTime << " s, " 
  << amoTime << " amo-s, " 
  << fmTime << " fm-s, "
  << amtTime << " amt-s, "
  << foundAmos << " amos, " 
  << foundAmts << " amts, "
  << sameUnits << " sameUnits, " 
  << deducedUnits << " deducedUnits, "
  << propUnits << " propUnits, " 
  << usedClauses << " cls, " 
  << steps << " steps, " 
  << searchSteps << " searchSteps, "
  << addDuplicates << " addDups, " 
  << endl
  << "c [STAT] FM(2) "
  << irregular << " irregulars, " 
  << pureAmoLits << " pureAmoLits, "
  << cardDiff << " diff, " 
  << discardedCards << " discards, "
  << newCards << " addedCs, "
  << removedCards << " removedCs, "
  << newAmos << " newAmos, " 
  << discardedNewAmos << " discardedNewAmos, " 
  << endl
  << "c [STAT] FM(3) "
  << addedBinaryClauses << " newAmoBinCls, "
  << addedClauses << " newCls, "
  << newAlos << " newAlos, "
  << newAlks << " newAlks, "
  << detectedDuplicates << " duplicates, " 
  << endl
  << "c [STAT] FM(4) "
  << twoPrTime << " 2PrdTime, "
  << twoPrAmos << " 2PrAmos, "
  << twoPrAmoLits << " 2PrLits, "
  << deduceAloTime << " dedAloTime, "
  << dedAlos << " dedAlos, "
  << endl;
}

void FourierMotzkin::giveMoreSteps()
{
  
}
  
void FourierMotzkin::destroy()
{
  
}