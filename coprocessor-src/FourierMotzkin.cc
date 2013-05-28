/*******************************************************************************[FourierMotzkin.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/FourierMotzkin.h"
#include "mtl/Sort.h"


static const char* _cat = "COPROCESSOR 3 - FOURIERMOTZKIN";

static IntOption  opt_fmLimit        (_cat, "cp3_fm_limit"  ,"number of steps allowed for FM", 12000000, IntRange(0, INT32_MAX));
static IntOption  opt_fmGrow         (_cat, "cp3_fm_grow"   ,"max. grow of number of constraints per step", 40, IntRange(0, INT32_MAX));
static IntOption  opt_fmGrowT        (_cat, "cp3_fm_growT"  ,"total grow of number of constraints", 100000, IntRange(0, INT32_MAX));

static BoolOption opt_atMostTwo      (_cat, "cp3_fm_amt"     ,"extract at-most-two", false);
static BoolOption opt_findUnit       (_cat, "cp3_fm_unit"    ,"check for units first", true);
static BoolOption opt_merge          (_cat, "cp3_fm_merge"   ,"perform AMO merge", true);
static BoolOption opt_duplicates     (_cat, "cp3_fm_dups"    ,"avoid finding the same AMO multiple times", true);
static BoolOption opt_cutOff         (_cat, "cp3_fm_cut"     ,"avoid eliminating too expensive variables (>10,10 or >5,15)", true);
static IntOption opt_newAmo          (_cat, "cp3_fm_newAmo"  ,"encode the newly produced AMOs (with pairwise encoding) 0=no,1=yes,2=try to avoid redundant clauses",  2, IntRange(0, 2));
static BoolOption opt_keepAllNew     (_cat, "cp3_fm_keepM"   ,"keep all new AMOs (also rejected ones)", true);
static IntOption opt_newAlo          (_cat, "cp3_fm_newAlo"  ,"create clauses from deduced ALO constraints 0=no,1=from kept,2=keep all ",  2, IntRange(0, 2));
static IntOption opt_newAlk          (_cat, "cp3_fm_newAlk"  ,"create clauses from deduced ALK constraints 0=no,1=from kept,2=keep all (possibly redundant!)",  2, IntRange(0, 2));
static BoolOption opt_checkSub       (_cat, "cp3_fm_newSub"  ,"check whether new ALO and ALK subsume other clauses (only if newALO or newALK)", true);
  

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
  big.create( ca,data,data.getClauses(),data.getLEarnts() );
  
  cards.clear();
  
  if( debug_out > 0) cerr << "c run with " << heap.size() << " elements" << endl;
  
  // for l in F
  while (heap.size() > 0 && (data.unlimited() || fmLimit > steps) && !data.isInterupted() ) 
  {
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

    if( debug_out > 0 ) cerr << "c found AMO: " << data.lits << endl;
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
  
  vec<Lit> unitQueue;
  vector<int> newAMOs,newALOs,newALKs;
  
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
	      if( ! inAmo.isCurrentStep( toInt(~v1[n1]) ) ) { // store each literal only once in the queue
		sameUnits ++;
		inAmo.setCurrentStep( toInt(~v1[n1]) );
		unitQueue.push(~v1[n1]);
		if( l_False == data.enqueue( ~v1[n1] ) ) goto finishedFM; // enqueing this literal failed -> finish!
	      }
	      // TODO: enqueue, later remove from all cards!
	      if( debug_out > 1 ) cerr << "c AMOs entail unit literal " << ~ v1[n1] << endl;
	      n1++;n2++;
	    } else if( v1[n1] < v2[n2] ) {n1 ++;}
	    else { n2 ++; }
	  }
	}
      }
      // if found something, propagate!
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) goto finishedFM;
    }
    
  }
  
   
  // perform merge
  if( opt_merge ) {
    if( debug_out > 0 ) cerr << "c merge AMOs ... " << endl;
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
		  if( data.enqueue( ~v1[n1] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << ~v1[n1] << " failed" << endl;
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
	  
	  // check whether there are similar variables inside the constraint, if so - drop it!
	  bool hasComplements = false;
	  for( int k = 0 ; k + 1 < data.lits.size(); ++k ) if( data.lits[k] == ~ data.lits[k+1] ) { 
	    if( debug_out > 2 ) cerr << "c deduced AMO contains complementary literals - set all others to false!" << endl;
	    Var uv = var(data.lits[k]);
	    for( int k = 0 ; k < data.lits.size(); ++ k ) {
	      if( var(data.lits[k]) == uv ) continue; // do not enqueue complementary literal!
	      if( ! inAmo.isCurrentStep( toInt(~data.lits[k]) ) ) { // store each literal only once in the queue
		  sameUnits ++;
		  inAmo.setCurrentStep( toInt(~data.lits[k]) );
		  unitQueue.push(~data.lits[k]);
		  if( data.enqueue( ~data.lits[k] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << ~data.lits[k] << " failed" << endl;
		    goto finishedFM; // enqueing this literal failed -> finish!
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
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) goto finishedFM;
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

  if( debug_out > 0 ) cerr << "c apply FM ..." << endl;
  // for l in F
  while (heap.size() > 0 && (data.unlimited() || fmLimit > steps) && !data.isInterupted() ) 
  {
    /** garbage collection */
    data.checkGarbage();
    
    const Lit toEliminate = toLit(heap[0]);
    assert( heap.inHeap( toInt(toEliminate) ) && "item from the heap has to be on the heap");

    heap.removeMin();

    if( debug_out > 1 ) cerr << endl << "c eliminate literal " << toEliminate << endl;
    
    int oldSize = cards.size(),oldNewSize = newAMOs.size(), oldAloSize = newALOs.size(), oldAlkSize = newALKs.size();
    
    // TODO: apply heuristics from BVE (do not increase number of constraints! - first create, afterwards count!
      if( leftHands[ toInt(toEliminate) ] .size() == 0  || rightHands[ toInt(toEliminate) ] .size() == 0 ) {
	pureAmoLits ++;
	continue; // only if the variable has very few occurrences. cannot handle pure here, because its also inside the formula at other places!
      }
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

	  bool hasDuplicates = false; // if true, then the resulting constraint is invalid!
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
		data.lits.push_back( v1[n1] ); // keep them
		hasDuplicates = true;
		n1++;n2++;
		break;
	      } else if( v1[n1] < v2[n2] ) {
		if( v1[n1] != toEliminate ) data.lits.push_back(v1[n1]);
		n1 ++;
	      } else { 
		if( v2[n2] != toEliminate ) data.lits.push_back(v2[n2]);
		n2 ++;
	      }
	    }
	    if(!hasDuplicates) {
	      for(; n1 < v1.size(); ++ n1 ) if( v1[n1] != toEliminate ) data.lits.push_back( v1[n1] );
	      for(; n2 < v2.size(); ++ n2 ) if( v2[n2] != toEliminate ) data.lits.push_back( v2[n2] );
	    }
	    if( p == 0 ) thisCard.ll = data.lits;
	    else thisCard.lr = data.lits;
	  }
	  
	  if( hasDuplicates ) {
	    if( debug_out > 1 ) cerr << "c new card would have duplicates - drop it (can be fixed with full FM algorithm which uses weights)" << endl
	         << "c  from " << card1.ll << " <= " << card1.k << " + " << card1.lr << endl
		 << "c   and " << card2.ll << " <= " << card2.k << " + " << card2.lr << endl << endl;
	     cards.pop_back(); // remove last card again
	     continue;
	  }
	  
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
	  if( debug_out > 1 ) cerr << "c resolved new CARD " << thisCard.ll << " <= " << thisCard.k << " + " << thisCard.lr << "  (unit: " << thisCard.isUnit() << " taut: " << thisCard.taut() << ", failed: " << thisCard.failed() << "," << (int)thisCard.lr.size() + thisCard.k << ")" << endl
				    << "c from " << card1.ll << " <= " << card1.k << " + " << card1.lr << endl
				    << "c and  " << card2.ll << " <= " << card2.k << " + " << card2.lr << endl << endl;
	  if( thisCard.taut() ) {
	     if( debug_out > 1 ) cerr << "c new card is taut! - drop it" << endl;
	     cards.pop_back(); // remove last card again
	     continue;
	  } else if( thisCard.failed() ) {
	    if( debug_out > 1 ) cerr << "c new card is failed! - stop procedure!" << endl;
	    data.setFailed();
	    goto finishedFM;
	  }else if( thisCard.isUnit() ) { // all literals in ll have to be set to false!
	    if( debug_out > 1 ) cerr << "c new card is unit - enqueue all literals" << endl;
	    deducedUnits += thisCard.ll.size() + thisCard.lr.size(); 
	    for( int k = 0 ; k < thisCard.ll.size(); ++ k ) {
		if( ! inAmo.isCurrentStep( toInt(~thisCard.ll[k]) ) ) { // store each literal only once in the queue
		  inAmo.setCurrentStep( toInt(~thisCard.ll[k]) );
		  unitQueue.push(~thisCard.ll[k]);
		  if( data.enqueue( ~thisCard.ll[k] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << ~thisCard.ll[k] << " failed" << endl;
		    goto finishedFM;
		  }
		}
	    }
	    for( int k = 0 ; k < thisCard.lr.size(); ++ k ) { // all literals in lr have to be set to true!
		if( ! inAmo.isCurrentStep( toInt(thisCard.lr[k]) ) ) { // store each literal only once in the queue
		  inAmo.setCurrentStep( toInt(thisCard.lr[k]) );
		  unitQueue.push(thisCard.lr[k]);
		  if( data.enqueue( thisCard.lr[k] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << thisCard.lr[k] << " failed" << endl;
		    goto finishedFM;
		  }
		}
	    }
	    cards.pop_back(); // remove last card again, because all its information has been used already!
	    continue;
	  } else if (thisCard.amo() ) {
	    if( debug_out > 1 ) cerr << "c new card is NEW amo - memorize it!" << endl;
	    if(opt_newAmo) newAMOs.push_back( index );
	    newAmos ++;
	  } else if (thisCard.alo() && opt_newAlo > 0) {
	    if( debug_out > 1 ) cerr << "c new card is NEW alo - memorize it!" << endl;
	    newAlos ++; 
	    if(opt_newAmo) newALOs.push_back( index );
	  }  else if (thisCard.alo() && opt_newAlk > 0) {
	    if( debug_out > 1 ) cerr << "c new card is NEW alk - memorize it!" << endl;
	    newAlks ++; 
	    if(opt_newAmo) newALKs.push_back( index );
	  }
	}
      }
      
      // new constraints > removed constraints + grow -> drop the elimination again!
      if( (cards.size() - oldSize > leftHands[ toInt(toEliminate) ] .size() + rightHands[ toInt(toEliminate) ].size()  + opt_fmGrow ) // local increase to large
	|| (cardDiff > opt_fmGrowT ) // global limit hit
      ) {
	discardedCards += (cards.size() - oldSize);
	// if( rejectedNewAmos
	if( opt_keepAllNew ) { // copy the new AMOs that should be rejected
	  for( int p = oldNewSize; p < newAMOs.size(); ++ p ) {
	    rejectedNewAmos.push_back( cards[ newAMOs[p] ] );
	  }
	} else discardedNewAmos += (newAMOs.size() - oldNewSize );
	if( opt_newAlo > 1  ) { // copy the new AMOs that should be rejected
	  for( int p = oldAloSize; p < newAMOs.size(); ++ p ) {
	    rejectedNewAlos.push_back( cards[ newAMOs[p] ] );
	  }
	} 
	if( opt_newAlk > 1 ) { // copy the new AMOs that should be rejected
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
	  if( debug_out > 2 ) cerr << "c remove  " << card.ll << " <= " << card.k << " + " << card.lr << " ... " << endl;
	  for( int j = 0 ; j < card.ll.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.ll[j];
	    // if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    if( debug_out > 3 ) cerr << "c check " << removeIndex << " in " << leftHands[toInt(l)] << endl;
	    for( int k = 0 ; k < leftHands[toInt(l)].size(); ++ k ) {
	      if( leftHands[toInt(l)][k] == removeIndex ) {
		if( debug_out > 3 ) cerr << "c from leftHand " << l << "( " << j << "/" << card.ll.size() << "," << k << "/" << leftHands[toInt(l)].size()  << ")" << endl;
		leftHands[toInt(l)][k] = leftHands[toInt(l)][ leftHands[toInt(l)].size() - 1]; leftHands[toInt(l)].pop_back(); 
		break;
	      }
	    }
	    if( debug_out > 3 ) cerr << "c finished literal " << l << "( " << j << "/" << card.ll.size() << ")" << endl;
	  }
	  for( int j = 0 ; j < card.lr.size(); ++ j ) { // remove all lls from left hand!
	    const Lit l = card.lr[j];
	    // if( !heap.inHeap( toInt(l) ) ) heap.insert( toInt(l) ); // add literal of modified cards to heap again
	    if( debug_out > 3 ) cerr << "c check " << removeIndex << " in " << rightHands[toInt(l)] << endl;
	    for( int k = 0 ; k < rightHands[toInt(l)].size(); ++ k ) {
	      if( rightHands[toInt(l)][k] == removeIndex ) {
		if( debug_out > 3 ) cerr << "c from rightHands " << l << "( " << j << "/" << card.lr.size() << "," << k << "/" << rightHands[toInt(l)].size()  << ")" << endl;
		rightHands[toInt(l)][k] = rightHands[toInt(l)][ rightHands[toInt(l)].size() - 1]; rightHands[toInt(l)].pop_back(); break;
	      }
	    }
	    if( debug_out > 3 ) cerr << "c finished literal " << l << "( " << j << "/" << card.ll.size() << ")" << endl;
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
      if(!propagateCards( unitQueue, leftHands, rightHands, cards,inAmo) ) goto finishedFM;
  }
  
  // propagate found units - if failure, skip next steps
  if( data.ok() && data.hasToPropagate() )
    if( propagation.process(data,true) == l_False ) {data.setFailed(); goto finishedFM; }
  
  if( data.ok() && opt_newAmo > 0 && (newAMOs.size() > 0 || rejectedNewAmos.size() > 0) ) {
    big.recreate( ca, data, data.getClauses() ); 
    if(opt_newAmo > 1) big.generateImplied(data); // try to avoid adding redundant clauses!
    for( int p = 0; p<2; ++ p ) {
      // use both the list of newAMOs, and the rejected new amos!
      for( int i = 0 ; i < (p == 0 ? newAMOs.size() : rejectedNewAmos.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newAMOs[i]] : rejectedNewAmos[i];
	if( c.invalid() || !c.amo() ) {
	  if( debug_out > 1 ) cerr << "c new AMO " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	for( int j = 0 ; j < c.ll.size(); ++ j ) {
	  for( int k = j+1; k < c.ll.size(); ++ k ) {
	    bool present = false;
	    if( opt_newAmo == 2 ) present = big.implies(c.ll[j],~c.ll[k]) || big.implies(~c.ll[k],c.ll[j]); // fast stamp check
	    if(!present) present = big.isChild(c.ll[j],~c.ll[k]) || big.isChild(c.ll[k],~c.ll[j]); // slower list check
	    if( !present ) { // if the information is not part of the formula yet, add the clause!
	      if( debug_out > 2 ) cerr << "c create new binary clause " <<  ~c.ll[k] << " , " << ~c.ll[j] << endl;
	      addedBinaryClauses ++;
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
  
  if( data.ok() && opt_newAlo > 0 && (newALOs.size() > 0 || rejectedNewAlos.size() > 0) ) {
    for( int p = 0; p<2; ++ p ) {
      for( int i = 0 ; i < (p == 0 ? newALOs.size() : rejectedNewAlos.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newALOs[i]] : rejectedNewAlos[i];
	if( c.invalid() || !c.alo() ) {
	  if( debug_out > 1 ) cerr << "c new ALO " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	if( c.lr.size() == 1 ) {
	  if( data.enqueue(c.lr[0]) == l_False ) goto finishedFM;
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
    if( propagation.process(data,true) == l_False ) {data.setFailed(); goto finishedFM; }
  
  if( data.ok() && opt_newAlk > 0 && (newALKs.size() > 0 || rejectedNewAlks.size() > 0) ) {
    for( int p = 0; p<2; ++ p ) {
      for( int i = 0 ; i < (p == 0 ? newALKs.size() : rejectedNewAlks.size()) ; ++ i ) {
	CardC& c = p == 0 ? cards[newALKs[i]] : rejectedNewAlks[i];
	if( c.invalid() || !c.alk() ) {
	  if( debug_out > 1 ) cerr << "c new ALK " << c.ll << " <= " << c.k << " + " << c.lr << " is dropped!" << endl;
	  continue;
	}
	if( c.lr.size() == 1 ) {
	  if( data.enqueue(c.ll[0]) == l_False ) goto finishedFM;
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
    if( propagation.process(data,true) == l_False ) {data.setFailed(); goto finishedFM; }
  
  finishedFM:;
  
  fmTime = cpuTime() - fmTime;
  
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
	if( debug_out > 1 ) cerr << "c clause is equal to [" << list[i] << "] : " << d << endl;
	detectedDuplicates ++;
	return true;
      }
    }
    if( opt_checkSub ) { // check each clause for being subsumed -> kick subsumed clauses!
      if( d.size() < c.size() ) {
	detectedDuplicates ++;
	if( ordered_subsumes(d,c) ) {
	  if( debug_out > 1 ) cerr << "c clause " << c << " is subsumed by [" << list[i] << "] : " << d << endl;
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
	    if( debug_out > 2 ) cerr << "c propagate " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    // remove all assigned literals and either reduce the ll vector, or "k"
	    int kc = 0;
	    for( int j = 0 ; j < c.ll.size(); ++ j ) {
	      if( data.value(c.ll[j]) == l_True ) {
		if( debug_out > 2 ) cerr << "c since " << c.ll[j] << " == top, reduce k from " << c.k << " to " << c.k - 1 << endl;
		c.k --;
	      } else if( data.value(c.ll[j]) == l_Undef ) {
		if( debug_out > 2 ) cerr << "c keep literal " << c.ll[j] << endl;
		c.ll[kc++] = c.ll[j];
	      } else if( debug_out > 2 ) cerr << "c drop unsatisfied literal " << c.ll[j] << endl;
	    }
	    c.ll.resize(kc);
	    if( debug_out > 2 ) cerr << "c        to " << c.ll << " <= " << c.k << " + " << c.lr << endl;
	    if(c.isUnit() ) {  // propagate AMOs only!
	      for( int j = 0 ; j < c.ll.size(); ++ j ) { // all literals in ll have to be set to false
		if( ! inAmo.isCurrentStep( toInt(~c.ll[j]) ) ) { // store each literal only once in the queue
		  propUnits ++;
		  inAmo.setCurrentStep( toInt(~c.ll[j]) );
		  unitQueue.push(~c.ll[j]);
		  if( data.enqueue( ~c.ll[j] ) == l_False ) {
		    if( debug_out > 1 ) cerr << "c enquing " << ~c.ll[j] << " failed" << endl;
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
		    if( debug_out > 1 ) cerr << "c enquing " << c.lr[j] << " failed" << endl;
		     return false; // enqueing this literal failed -> finish!
		  }
		}
	      }
	      c.invalidate();
	    }
	    if( c.taut() ) c.invalidate();
	    else if ( c.failed() ) {
	      if( debug_out > 1 ) cerr << "c resulting constraint cannot be satisfied!" << endl;
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
  << endl;
}

void FourierMotzkin::giveMoreSteps()
{
  
}
  
void FourierMotzkin::destroy()
{
  
}