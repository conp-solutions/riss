/*******************************************************************************[FourierMotzkin.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/FourierMotzkin.h"
#include "mtl/Sort.h"


static const char* _cat = "COPROCESSOR 3 - FOURIERMOTZKIN";

static IntOption  opt_fmLimit        (_cat, "cp3_fm_limit" ,"number of steps allowed for FM", 12000000, IntRange(0, INT32_MAX));
static IntOption  opt_fmGrow         (_cat, "cp3_fm_grow"  ,"max. grow of number of constraints per step", 10, IntRange(0, INT32_MAX));

static BoolOption opt_atMostTwo      (_cat, "cp3_fm_amt"   ,"extract at-most-two", false);
static BoolOption opt_findUnit       (_cat, "cp3_fm_unit"  ,"check for units first", true);
static BoolOption opt_merge          (_cat, "cp3_fm_merge" ,"perform AMO merge", true);
static BoolOption opt_duplicates     (_cat, "cp3_fm_dups"  ,"avoid finding the same AMO multiple times", true);
static BoolOption opt_cutOff         (_cat, "cp3_fm_cut"   ,"avoid eliminating too expensive variables (>10,10 or >5,15)", true);
  

#if defined CP3VERSION 
static const int debug_out = 0;
#else
static IntOption debug_out                 (_cat, "fm-debug",       "Debug Output of Fourier Motzkin", 0, IntRange(0, 4));
#endif

using namespace Coprocessor;

FourierMotzkin::FourierMotzkin( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation )
: Technique(_ca,_controller)
, data(_data)
, propagation(_propagation)
, processTime(0)
, amoTime(0)
, fmTime(0)
, steps(0)
, fmLimit(opt_fmLimit)
, foundAmos(0)
, sameUnits(0)
, deducedUnits(0)
, propUnits(0)
, addDuplicates(0)
, irregular(0)
, usedClauses(0)
, discardedCards(0)
, removedCards(0)
, newCards(0)
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
  data.ma.resize( data.nVars() );
  
  MarkArray inAmo;
  inAmo.create( 2 * data.nVars() );

  // setup own structures
  Heap<LitOrderHeapLt> heap(data); // heap that stores the literals according to their frequency
  heap.addNewElement(data.nVars() * 2);
  
  vector< CardC > cards; // all amos that are collected
  
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
  big.create( ca,data,data.getClauses(),data.getLEarnts() );
  
  cards.clear();
  
  if( debug_out > 0) cerr << "c run with " << heap.size() << " elements" << endl;
  
  // for l in F
  while (heap.size() > 0 && (data.unlimited() || fmLimit > steps) && !data.isInterupted() ) 
  {
    /** garbage collection */
    data.checkGarbage();
    
    const Lit right = toLit(heap[0]);
    assert( heap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    heap.removeMin();
    
    if( data[ right ] < 2 ) continue; // no enough occurrences -> skip!
    const uint32_t size = big.getSize( ~right );
    if( debug_out > 2) cerr << "c check " << right << " with " << data[right] << " cls, and " << size << " implieds" << endl;
    if( size < 2 ) continue; // cannot result in a AMO of required size -> skip!
    Lit* list = big.getArray( ~right );

    // create first list right -> X == -right \lor X, ==
    inAmo.nextStep(); // new AMO
    data.lits.clear(); // new AMO
    data.lits.push_back(right); // contains list of negated AMO!
    for( int i = 0 ; i < size; ++ i ) {
      const Lit& l = list[i];
      if( data[ l ] < 2 ) continue; // cannot become part of AMO!
      if( big.getSize( ~l ) < 2 ) continue; // cannot become part of AMO!
      if( inAmo.isCurrentStep( toInt(l) ) ) continue; // avoid duplicates!
      inAmo.setCurrentStep( toInt(l ) );
      data.lits.push_back(l); // l is implied by ~right -> canidate for AMO(right,l, ... )
    }
    if( debug_out > 2) cerr << "c implieds: " << data.lits.size() << endl;
    if( data.lits.size() < 3 ) continue; // do not consider this variable, because it does not hit enough literals
    
    // TODO: should sort list according to frequency in binary clauses - ascending, so that small literals are removed first, increasing the chance for this more frequent ones to stay!
    
    // check whether each literal can be inside the AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i ) { // keep the very first, since it created the list!
      const Lit l = data.lits[i];
      if( l == lit_Undef ) continue; // do not handle removed literals!
      const uint32_t size2 = big.getSize( ~l );
      Lit* list2 = big.getArray( ~l );
      // if not all, disable this literal, remove it from data.lits
      inAmo.nextStep(); // new AMO
      for( int j = 0 ; j < size2; ++ j ) inAmo.setCurrentStep( toInt(list2[j]) );
      int j = 0;
      for( ; j<data.lits.size(); ++ j ) 
	if( i!=j 
	  && data.lits[j] != lit_Undef 
	  && !inAmo.isCurrentStep( toInt( data.lits[j] ) ) 
	) break;
      if( j != data.lits.size() ) {
	if( debug_out > 1) cerr << "c reject [" <<i<< "]" << data.lits[i] << ", because failed with [" << j << "]" << data.lits[j] << endl;
	data.lits[i] = lit_Undef; // if not all literals are covered, disable this literal!
      } else if( debug_out > 1) cerr << "c keep [" <<i<< "]" << data.lits[i] << " which hits [" << j << "] literas"  << endl;
    }
    
    // found an AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i )
      if ( data.lits[i] == lit_Undef ) { data.lits[i] = data.lits[ data.lits.size() - 1 ]; data.lits.pop_back(); --i; }
    
    for( int i = 0 ; i < data.lits.size(); ++ i ){
      if( opt_duplicates ) { // remove the edges in the big that represent this AMO
	for( int j = i+1; j < data.lits.size(); ++ j ) big.removeEdge(data.lits[i], data.lits[j] );
      }
      data.lits[i] = ~ data.lits[i]; // need to negate all!
      data.ma.setCurrentStep( var(data.lits[i]) ); // memorize that this variable is part of an AMO
    }

    // remember that these literals have been used in an amo already!
    sort(data.lits);
    cards.push_back( data.lits );

    if( debug_out > 0 ) cerr << "c found AMO (negated, == AllExceptOne): " << data.lits << endl;
  }
 
  amoTime = cpuTime() - amoTime;
  foundAmos = cards.size();
  
  if( debug_out > 0 ) cerr << "c finished search AMO --- process ... " << endl;
  
  // perform AMT extraction
  if( opt_atMostTwo ) {
    
  }
  
  
  fmTime = cpuTime() - fmTime;
  // setup all data structures
  
  vector< vector<int> > leftHands(data.nVars() * 2 ),rightHands(data.nVars() * 2 ); // indexes into the cards list
  for( int i = 0 ; i < cards.size(); ++ i ) {
    for( int j = 0; j < cards[i].ll.size(); ++ j )  leftHands[ toInt( cards[i].ll[j] ) ].push_back( i );
    for( int j = 0; j < cards[i].lr.size(); ++ j ) rightHands[ toInt( cards[i].lr[j] ) ].push_back( i );
  }
  
  vector<Lit> unitQueue;
  
  // perform FindUnit
  if( opt_findUnit ) {
    inAmo.nextStep(); // check for each literal whether its already in the unit queue
    if( debug_out > 0 ) cerr << "c find units ... " << endl;
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
	      if( ! inAmo.isCurrentStep( toInt(v1[n1]) ) ) { // store each literal only once in the queue
		sameUnits ++;
		inAmo.setCurrentStep( toInt(v1[n1]) );
		unitQueue.push_back(v1[n1]);
		if( l_False == data.enqueue( v1[n1] ) ) goto finishedFM; // enqueing this literal failed -> finish!
	      }
	      // TODO: enqueue, later remove from all cards!
	      if( debug_out > 1 ) cerr << "c AMOs entail unit literal " << ~ v1[n1] << endl;
	      n1++;n2++;
	    } else if( v1[n1] < v2[n2] ) {n1 ++;}
	    else { n2 ++; }
	  }
	}
      }
    }
    
    // propagate method
    while( unitQueue.size() > 0 ) {
      Lit p = unitQueue.back(); unitQueue.pop_back();
      for( int p1 = 0; p1 < 2; ++ p1 ) {
	if( p1 == 1 ) p = ~p;
	for( int p2 = 0 ; p2 < 2; ++ p2 ) {
	  vector<int>& indexes = (p2==0 ? leftHands[ toInt(p) ] : rightHands[ toInt(p) ]);
	  for( int i = 0 ; i < indexes.size(); ++ i ) {
	    CardC& c = cards[ indexes[i] ];
	    steps ++;
	    if( c.invalid() ) continue; // do not consider this constraint any more!
	    if( debug_out > 2 ) cerr << "c propagate " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    // remove all assigned literals and either reduce the ll vector, or "k"
	    int kc = 0;
	    for( int j = 0 ; j < c.ll.size(); ++ j ) {
	      if( data.value(c.ll[j]) == l_True ) c.k --;
	      else if( data.value(c.ll[j]) == l_Undef ) {
		c.ll[kc++] = c.ll[j];
	      }
	    }
	    c.ll.resize(kc);
	    if( debug_out > 2 ) cerr << "c        to " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    if(c.isUnit() ) {  // propagate AMOs only!
	      for( int j = 0 ; j < c.ll.size(); ++ j ) {
		if( ! inAmo.isCurrentStep( toInt(c.ll[j]) ) ) { // store each literal only once in the queue
		  propUnits ++;
		  inAmo.setCurrentStep( toInt(c.ll[j]) );
		  unitQueue.push_back(c.ll[j]);
		  if( data.enqueue( c.ll[j] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << c.ll[j] << " failed" << endl;
		    goto finishedFM; // enqueing this literal failed -> finish!
		  }
		}
	      }
	      c.invalidate();
	    }
	    if( c.taut() ) c.invalidate();
	    else if ( c.failed() ) {
	      if( debug_out > 1 ) cerr << "c resulting constraint cannot be satisfied!" << endl;
	      data.setFailed(); goto finishedFM;
	    }
	  }
	}
      }
      // clear all occurrence lists!
      leftHands[toInt(p)].clear();leftHands[toInt(~p)].clear();
      rightHands[toInt(p)].clear();rightHands[toInt(~p)].clear();
    }
    
  }
  
  // perform merge
  if( opt_merge ) {
    if( debug_out > 0 ) cerr << "c merge AMOs ... " << endl;
    for( Var v = 0 ; v < data.nVars(); ++v  ) {
      const Lit pl = mkLit(v,false), nl = mkLit(v,true);
      if( leftHands[ toInt(pl) ] .size() > 2 || leftHands[ toInt(nl) ] .size() > 2 ) continue; // only if the variable has very few occurrences
      for( int i = 0 ; i < leftHands[toInt(pl)].size() && steps < fmLimit; ++ i ) { // since all cards are compared, be careful!
	const CardC& card1 = cards[leftHands[toInt(pl)][i]];
	steps ++;
	if( card1.invalid() ) continue;
	if( !card1.amo() ) continue; // can do this on AMO only
	for( int j = 0 ; j < leftHands[toInt(nl)].size(); ++ j ) {
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
	      if( ! inAmo.isCurrentStep( toInt(v1[n1]) ) ) { // store each literal only once in the queue
		  sameUnits ++;
		  inAmo.setCurrentStep( toInt(v1[n1]) );
		  unitQueue.push_back(v1[n1]);
		  if( data.enqueue( v1[n1] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << v1[n1] << " failed" << endl;
		    goto finishedFM; // enqueing this literal failed -> finish!
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
	  if( debug_out > 0 ) cerr << "c deduced AMO " << data.lits << " from AMO " << card1.ll << " and AMO " << card2.ll << endl;
	  
	  const int index = cards.size();
	  cards.push_back( CardC(data.lits) ); // create AMO
	  for( int k = 0 ; k < data.lits.size(); ++ k ) leftHands[ toInt( data.lits[k] ) ].push_back(index);
	}
      }

    }
  }
  
  // propagate found units - if failure, skip next steps
  if( data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); goto finishedFM; }
  
  // perform actual Fourier Motzkin method 
  // use all clauses that could be useful
  heap.clear();
  for( int i = 0 ; i < data.getClauses().size(); ++ i ) {
    const Clause& c = ca[ data.getClauses()[i] ];
    if( c.can_be_deleted() || c.size() < 3 ) continue; // only larger clauses!
    for( int j = 0 ; j < c.size(); ++ j ) {
      if( data.ma.isCurrentStep( var(c[j])) ) { // a literal of this clause took part in an AMO
	const int index = cards.size();
	cards.push_back( CardC( c ) );
	if( debug_out > 1 ) cerr << "c clause " << c << " leads to card " << cards[cards.size() -1 ].ll << " <= " << cards[cards.size() -1 ].k << " + " << cards[cards.size() -1 ].lr << endl;
	for( int j = 0 ; j < c.size(); ++ j ) {
	  rightHands[toInt(c[j])].push_back(index);
	  if( !heap.inHeap(toInt(c[j])) ) heap.insert( toInt(c[j]) );
	}
	usedClauses ++;
	break;
      }
    }
  }
  
  // for l in F
  while (heap.size() > 0 && (data.unlimited() || fmLimit > steps) && !data.isInterupted() ) 
  {
    /** garbage collection */
    data.checkGarbage();
    
    const Lit toEliminate = toLit(heap[0]);
    assert( heap.inHeap( toInt(toEliminate) ) && "item from the heap has to be on the heap");

    heap.removeMin();

    if( debug_out > 1 ) cerr << "c eliminate literal " << toEliminate << endl;
    
    int oldSize = cards.size();
    
    // TODO: apply heuristics from BVE (do not increase number of constraints! - first create, afterwards count!
      if( leftHands[ toInt(toEliminate) ] .size() == 0  || rightHands[ toInt(toEliminate) ] .size() == 0 ) continue; // only if the variable has very few occurrences
      if( opt_cutOff && 
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
	    if( debug_out > 2 ) cerr << "c irregular literal " << toEliminate << " with constraints (" << rightHands[toInt(toEliminate)][j] << " == " << leftHands[toInt(toEliminate)][i] << "): " << endl
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

	  CardC& thisCard = cards[ index ];
	  for( int p = 0 ; p < 2; ++ p ) {
	    // first half
	    const vector<Lit>& v1 = p == 0 ? card1.ll : card1.lr;
	    const vector<Lit>& v2 = p == 0 ? card2.ll : card2.lr;
	    int n1=0,n2=0;
	    data.lits.clear();
	    while( n1 < v1.size() && n2 < v2.size() ) {
	      if( v1[n1] == v2[n2] ) { // same literal in two amos with a different literal -> have to be unit!
		addDuplicates ++;
		// TODO: enqueue, later remove from all cards!
		n1++;n2++;
	      } else if( v1[n1] < v2[n2] ) {
		if( v1[n1] != toEliminate ) data.lits.push_back(v1[n1]);
		n1 ++;
	      } else { 
		if( v2[n2] != toEliminate ) data.lits.push_back(v2[n2]);
		n2 ++;
	      }
	    }
	    for(; n1 < v1.size(); ++ n1 ) if( v1[n1] != toEliminate ) data.lits.push_back( v1[n1] );
	    for(; n2 < v2.size(); ++ n2 ) if( v2[n2] != toEliminate ) data.lits.push_back( v2[n2] );
	    if( p == 0 ) thisCard.ll = data.lits;
	    else thisCard.lr = data.lits;
	  }
	  // TODO: remove duplicate literals from lr and ll!
	  int nl = 0,nr = 0,kl=0,kr=0;
	  while( nl < thisCard.ll.size() && nr < thisCard.lr.size() ) {
	    if( thisCard.ll[nl] == thisCard.lr[nr] ) { // do not keep the same literal!
	       nr ++; nl ++;
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
	  
	  thisCard.k = card1.k + card2.k;
	  if( debug_out > 1 ) cerr << "c resolved new card " << thisCard.ll << " <= " << thisCard.k << " + " << thisCard.lr << "  (unit: " << thisCard.isUnit() << " taut: " << thisCard.taut() << ", failed: " << thisCard.failed() << "," << (int)thisCard.lr.size() + thisCard.k << ")" << endl
				    << " from " << card1.ll << " <= " << card1.k << " + " << card1.lr << endl
				    << " and  " << card2.ll << " <= " << card2.k << " + " << card2.lr << endl;
	  if( thisCard.taut() ) {
	     if( debug_out > 1 ) cerr << "c new card is taut! - drop it" << endl;
	     cards.pop_back(); // remove last card again
	     continue;
	  } else if( thisCard.failed() ) {
	    if( debug_out > 1 ) cerr << "c new card is failed! - stop procedure!" << endl;
	    data.setFailed();
	    goto finishedFM;
	  }else if( thisCard.isUnit() ) {
	    deducedUnits += thisCard.ll.size(); 
	    for( int k = 0 ; k < thisCard.ll.size(); ++ k ) {
	      
	    }
	  }
	}
      }
      
      if( cards.size() + leftHands[ toInt(toEliminate) ] .size() + rightHands[ toInt(toEliminate) ].size() > oldSize + opt_fmGrow ) {
	discardedCards += (cards.size() - oldSize);
	// remove!
	cards.resize( oldSize );
	continue;
      } 
      
      // remove all old constraints, add all new constraints!
      for( int p = 0 ; p < 2; ++ p ) {
	vector<int>& indexes = p==0 ? leftHands[ toInt(toEliminate) ] : rightHands[ toInt(toEliminate) ];
	removedCards += indexes.size();
	while( indexes.size() > 0 ) {
	  const CardC& card = cards[ indexes[0] ];
	  for( int j = 0 ; j < card.ll.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.ll[j];
	    if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    for( int k = 0 ; k < leftHands[toInt(l)].size(); ++ k ) if( leftHands[toInt(l)][k] == indexes[0] ) {
	      leftHands[toInt(l)][k] = leftHands[toInt(l)][ leftHands[toInt(l)].size() - 1]; leftHands[toInt(l)].pop_back(); break;
	    }
	  }
	  for( int j = 0 ; j < card.lr.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.lr[j];
	    if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    for( int k = 0 ; k < rightHands[toInt(l)].size(); ++ k ) if( rightHands[toInt(l)][k] == indexes[0] ) {
	      rightHands[toInt(l)][k] = rightHands[toInt(l)][ rightHands[toInt(l)].size() - 1]; rightHands[toInt(l)].pop_back(); break;
	    }
	  }
	  if( debug_out > 2 ) cerr << "c removed  " << card.ll << " <= " << card.k << " + " << card.lr << endl;
	}
      }
      
      // add new constraints
      for( int i = oldSize; i < cards.size(); ++ i ) {
	const CardC& card = cards[i];
	for( int j = 0 ; j < card.ll.size(); ++ j )  leftHands[ toInt(card.ll[j]) ].push_back(i);
	for( int j = 0 ; j < card.lr.size(); ++ j ) rightHands[ toInt(card.lr[j]) ].push_back(i);
      }
      newCards += cards.size() - oldSize;
    
  }
  
  // propagate found units - if failure, skip next steps
  if( data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); goto finishedFM; }
  
  finishedFM:;
  
  
  fmTime = cpuTime() - fmTime;
  
  return false;
}
    
void FourierMotzkin::printStatistics(ostream& stream)
{
  stream << "c [STAT] FM " << processTime << " s, " 
  << amoTime << " amo-s, " 
  << fmTime << " fm-s, "
  << foundAmos << " amos, " 
  << sameUnits << " sameUnits, " 
  << deducedUnits << " deducedUnits, "
  << propUnits << " propUnits, " 
  << usedClauses << " cls, " 
  << steps << " steps, " 
  << addDuplicates << " addDups, " 
  << endl
  << "c [STAT] FM(2) "
  << irregular << " irregulars, " 
  << discardedCards << " discards, "
  << newCards << " addedCs, "
  << removedCards << " removedCs, "
  << endl;
}

void FourierMotzkin::giveMoreSteps()
{
  
}
  
void FourierMotzkin::destroy()
{
  
}