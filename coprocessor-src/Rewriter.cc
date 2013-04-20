/*************************************************************************************[Rewriter.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Rewriter.h"

#include "mtl/Sort.h"

#include <algorithm>

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - REWRITE";

static IntOption  opt_min             (_cat, "cp3_rew_min"  ,"min occurrence to be considered", 7, IntRange(0, INT32_MAX));
static IntOption  opt_iter            (_cat, "cp3_rew_iter" ,"number of iterations", 1, IntRange(0, INT32_MAX));
static IntOption  opt_minAMO          (_cat, "cp3_rew_minA" ,"min size of altered AMOs", 8, IntRange(0, INT32_MAX));
static IntOption  opt_rewlimit        (_cat, "cp3_rew_limit","number of steps allowed for REW", 1200000, IntRange(0, INT32_MAX));
static IntOption  opt_Varlimit        (_cat, "cp3_rew_Vlimit","max number of variables to still perform REW", 1000000, IntRange(0, INT32_MAX));
static IntOption  opt_Addlimit        (_cat, "cp3_rew_Addlimit","number of new variables being allowed", 100000, IntRange(0, INT32_MAX));

static BoolOption opt_rew_avg         (_cat, "cp3_rew_avg"   ,"use AMOs above equal average only?", true);
static BoolOption opt_rew_ratio       (_cat, "cp3_rew_ratio" ,"allow literals in AMO only, if their complement is not more frequent", true);
static BoolOption opt_rew_once        (_cat, "cp3_rew_once"  ,"rewrite each variable at most once! (currently: yes only!)", true);

#if defined CP3VERSION 
static const int debug_out = 0;
#else
static IntOption debug_out                 (_cat, "rew-debug",       "Debug Output of Rewriter", 0, IntRange(0, 4));
#endif
	
Rewriter::Rewriter(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Coprocessor::Subsumption& _subsumption)
: Technique( _ca, _controller )
, data( _data )
, processTime(0)
, amoTime(0)
, rewTime(0)
, rewLimit(opt_rewlimit)
, steps(0)
, detectedDuplicates(0)
, createdClauses(0)
, droppedClauses(0)
, enlargedClauses(0)
, sortCalls(0)
, reuses(0)
, processedAmos(0)
, foundAmos(0)
, maxAmo(0)
, addedVariables(0)
, removedVars(0)
, removedViaSubsubption(0)
, subsumption( _subsumption )
, rewHeap( data )
{
}


bool Rewriter::process()
{
  MethodTimer mt(&processTime);
  bool didSomething = false;

  assert( opt_rew_once && "other parameter setting that true is not supported at the moment" );
  
  if( data.nVars() > opt_Varlimit ) return false; // do nothing, because too many variables!
  
  // have a slot per variable
  data.ma.resize( data.nVars() );
  
  
  MarkArray inAmo;
  inAmo.create( 2 * data.nVars() );

  // setup own structures
  rewHeap.addNewElement(data.nVars() * 2);
  
  
  int complementDropCount = 0;
  vec<Lit> clsLits; // vector for literals of the rewritten clauses
  
  //
  // have multiple iterations, if very large AMOs occur?
  //
  vector< vector<Lit> > amos; // all amos that are collected
  for( int algoIters = 0; algoIters < opt_iter && !data.isInterupted() && (data.unlimited() || steps < rewLimit); ++ algoIters ) {
    
  rewHeap.clear();
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( data[  mkLit(v,false) ] >= opt_min ) if( !rewHeap.inHeap(toInt(mkLit(v,false))) )  rewHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] >= opt_min ) if( !rewHeap.inHeap(toInt(mkLit(v,true))) )   rewHeap.insert( toInt(mkLit(v,true))  );
  }

  amoTime = cpuTime() - amoTime;
  data.ma.nextStep();
  inAmo.nextStep();
  
  // create full BIG, also rewrite learned clauses!!
  BIG big;
  big.create( ca,data,data.getClauses(),data.getLEarnts() );
  
  amos.clear();
  
  if( debug_out > 0) cerr << "c run with " << rewHeap.size() << " elements" << endl;
  
  // for l in F
  while (rewHeap.size() > 0 && (data.unlimited() || rewLimit > steps) && !data.isInterupted() ) 
  {
    /** garbage collection */
    data.checkGarbage();
    
    const Lit right = toLit(rewHeap[0]);
    assert( rewHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    if( debug_out > 2 && rewHeap.size() > 0 ) cerr << "c [REW] new first item: " << rewHeap[0] << " which is " << right << endl;
    rewHeap.removeMin();
    
    if( opt_rew_ratio && data[right] < data[~right] ) continue; // if ratio, do not consider literals with the wrong ratio
    if( opt_rew_once && data.ma.isCurrentStep( var(right) ) ) continue; // do not touch variable twice!
    if( data[ right ] < opt_minAMO ) continue; // no enough occurrences -> skip!
    const uint32_t size = big.getSize( ~right );
    if( debug_out > 2) cerr << "c check " << right << " with " << data[right] << " cls, and " << size << " implieds" << endl;
    if( size < opt_minAMO ) continue; // cannot result in a AMO of required size -> skip!
    Lit* list = big.getArray( ~right );

    // create first list right -> X == -right \lor X, ==
    inAmo.nextStep(); // new AMO
    data.lits.clear(); // new AMO
    data.lits.push_back(right); // contains list of negated AMO!
    for( int i = 0 ; i < size; ++ i ) {
      const Lit& l = list[i];
      if( data[ l ] < opt_minAMO ) continue; // cannot become part of AMO!
      if( big.getSize( ~l ) < opt_minAMO ) continue; // cannot become part of AMO!
      if( opt_rew_once && data.ma.isCurrentStep( var(l) ) ) continue; // has been used previously
      if( opt_rew_ratio && data[l] < data[~l] ) continue; // if ratio, do not consider literals with the wrong ratio
      if( inAmo.isCurrentStep( toInt(l) ) ) continue; // avoid duplicates!
      inAmo.setCurrentStep( toInt(l ) );
      data.lits.push_back(l); // l is implied by ~right -> canidate for AMO(right,l, ... )
    }
    if( debug_out > 2) cerr << "c implieds: " << data.lits.size() << endl;
    
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
	if( debug_out > 0) cerr << "c reject [" <<i<< "]" << data.lits[i] << ", because failed with [" << j << "]" << data.lits[j] << endl;
	data.lits[i] = lit_Undef; // if not all literals are covered, disable this literal!
      } else if( debug_out > 0) cerr << "c keep [" <<i<< "]" << data.lits[i] << " which hits [" << j << "] literas"  << endl;
    }
    
    // found an AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i )
      if ( data.lits[i] == lit_Undef ) { data.lits[i] = data.lits[ data.lits.size() - 1 ]; data.lits.pop_back(); --i; }
    
    // use only even sized AMOs -> drop last literal!
    if( data.lits.size() % 2 == 1 ) data.lits.pop_back();
    
    if( data.lits.size() < opt_minAMO ) continue; // AMO not big enough -> continue!
    
    // remember that these literals have been used in an amo already!
    amos.push_back( data.lits );
    for( int i = 0 ; i < data.lits.size(); ++ i )
      amos[amos.size() -1][i] = ~ (amos[amos.size() -1][i]); // need to negate all!
      
    // check AMO!
    if( true ) {
	// find all AMO binary clauses, and replace them with smaller variables!
	inAmo.nextStep();
	for( int j = 0 ; j < amos[amos.size() -1].size(); ++ j ) {
	  if( debug_out > 0 ) cerr << "c set " << ~amos[amos.size() -1][j] << endl;
	  inAmo.setCurrentStep( toInt( ~amos[amos.size() -1][j] ) );
	}
	
	int count = 0;
	for( int j = 0 ; j < amos[amos.size() -1].size(); ++ j ){
	  const Lit l = ~amos[amos.size() -1][j];
	  if( debug_out > 1 ) cerr << "c check literal " << l << endl;
	  vector<CRef>& ll = data.list(l);
	  for( int k = 0 ; k < ll.size(); ++ k ) {
	    Clause& c= ca[ ll[k] ];
	    if( c.can_be_deleted() || c.size() != 2 ) continue; // to not care about these clauses!
	    if( inAmo.isCurrentStep( toInt(c[0]) ) && inAmo.isCurrentStep( toInt(c[1]) ) ) { count ++;  }
	    else if( debug_out > 2 ) cerr << "c not matching binary clause: " << c << endl;
	  }
	}
	if( debug_out > 0 )  cerr << "c found " << count << " binary clauses, out of " << (amos[amos.size() -1].size()*(amos[amos.size() -1].size()-1)) / 2 << endl;
	assert( count >= ((amos[amos.size() -1].size()*(amos[amos.size() -1].size()-1)) / 2) && "not all clauses have been found" );
    } // end of check!
    
    for( int i = 0 ; i < data.lits.size(); ++ i )
      data.ma.setCurrentStep( var(data.lits[i]) );
    
    if( debug_out > 0 ) cerr << "c found AMO (negated, == AllExceptOne): " << data.lits << endl;
  }
 
  amoTime = cpuTime() - amoTime;
  foundAmos = amos.size();
  
  cerr << "c finished search AMO --- process ... " << endl;
  
  rewTime = cpuTime() - rewTime;
 
 // actual rewriting method
  data.ma.nextStep();
  for( int i = 0 ; i < amos.size() && !data.isInterupted() && (data.unlimited() || (steps < rewLimit && addedVariables < opt_Addlimit ) ); ++ i )
  {
    vector<Lit>& amo = amos[i];
    processedAmos++;
    maxAmo = maxAmo >= amo.size() ? maxAmo : amo.size();
    
    int size = amo.size();
    removedVars +=size;
    assert( size % 2 == 0 && "AMO has to be even!" );
    int rSize = size / 2;
    assert( rSize * 2 == size && "AMO has to be even!" );
    
    if( debug_out > 0 ) cerr << "c process amo " << i << "/" << amos.size() << " with size= " << size << " and half= " << rSize << endl;
    
    for( int j = 0 ; j < amo.size(); ++ j ) {
      assert( !data.ma.isCurrentStep( var(amo[j] ) ) && "touch variable only once during one iteration!" ); 
      data.ma.setCurrentStep( var(amo[j]) ); // for debug only!
    }
    
    const Var newX = nextVariable('s');addedVariables ++;
    const Lit newXp = mkLit(newX,false); const Lit newXn = mkLit(newX,true); // literals
    data.lits.clear(); // collect all replace literals!
    for(  int j = 0 ; j < rSize; ++ j ) {
      Var newRj = nextVariable('r');addedVariables ++;
      data.lits.push_back(mkLit(newRj,false) );
    }
    
    // find all AMO binary clauses, and replace them with smaller variables!
    inAmo.nextStep();
    for( int j = 0 ; j < amo.size(); ++ j ) {
      if( debug_out > 0 ) cerr << "c set " << ~amo[j] << endl;
      inAmo.setCurrentStep( toInt( ~amo[j] ) );
    }
    
    int count = 0;
    for( int j = 0 ; j < amo.size(); ++ j ){
      const Lit l = ~amo[j];
      vector<CRef>& ll = data.list(l);
      if( debug_out > 0 ) cerr << "c check literal " << l << "[" << ll.size() << "]" << endl;
      for( int k = 0 ; k < ll.size(); ++ k ) {
	Clause& c= ca[ ll[k] ];
	if( c.can_be_deleted() || c.size() != 2 ) continue; // to not care about these clauses!
	if( inAmo.isCurrentStep( toInt(c[0]) ) && inAmo.isCurrentStep( toInt(c[1]) ) ) { count ++; c.set_delete(true); data.removedClause(ll[k]); }
	else if( debug_out > 2 ) cerr << "c not matching binary clause: " << c << endl;
      }
    }
    if( debug_out > 0 )  cerr << "c found " << count << " binary clauses, out of " << (size*(size-1)) / 2 << endl;
    assert( count >= ((size*(size-1)) / 2) && "not all clauses have been found" );
    
    // replace smaller half of variables
    for( int half = 0; half < 2; ++ half ) {
    
      //
      // do both halfs based on the same code!!
      //
      
      for( int j = 0 ; j < rSize; ++ j ) {
	
	{ // to make sure all variables are valid only for positive!
	// if a clause of l=amo[j] contains the literal l' == ~amo[j+rSize], then in the long run the clause will be dropped!
	const Lit l =  amo[j + (half==0 ? 0 : rSize) ]; // positive occurrence[amo contains negative occurrences!]:  l is replaced by (newl \land newX); ~l is replaced by (~newL \lor ~newX)
	const Lit r1 = half==0 ? newXp : newXn;		// replace with this literal
	const Lit r2 = data.lits[j];	// replace with this literal
	//
	// store undo info! l <-> (newl \land newX)
	// 
	clsLits.clear(); // represent and gate with 3 clauses, push them!
	clsLits.push(lit_Undef);clsLits.push(l);clsLits.push(~r1);clsLits.push(~r2);
	clsLits.push(lit_Undef);clsLits.push(~l);clsLits.push(r1);
	clsLits.push(lit_Undef);clsLits.push(~l);clsLits.push(r2);
	data.addExtensionToExtension(clsLits);
	//
	if( debug_out > 1 ) cerr << endl << endl << "c replace " << l << " with (" << r1 << " and " << r2 << ")" << endl;
	vector<CRef>& ll = data.list(l);
	for( int k = 0 ; k < ll.size(); ++ k ) {
	  Clause& c= ca[ ll[k] ];
	  assert(c.size() > 1 && "there should not be unit clauses!" );
	  if( c.can_be_deleted() ) continue; // to not care about these clauses!
	  if( debug_out > 1 ) cerr << endl << "c rewrite POS [" << ll[k] << "] : " << c << endl;
	  clsLits.clear();
	  int hitPos = -1;
	  Lit minL = c[0];
	  bool foundNR1 = false, foundNR2 = false, foundNR1comp=false, foundNR2comp = false;
	  for( hitPos = 0 ; hitPos < c.size(); ++ hitPos ){
	    assert( var(c[hitPos]) < newX && "newly added variables are always greater" );
	    if( c[hitPos] == l ) {
	      break; // look for literal l
	    }
	    const Lit cl = c[hitPos];
	    clsLits.push(cl);
	    minL =  data[cl] < data[minL] ? c[hitPos] : minL;
	    foundNR1 = foundNR1 || (cl == r1); foundNR2 = foundNR2 || (cl == r2);
	    foundNR1comp = foundNR1comp || (cl == ~r1); foundNR2comp = foundNR2comp || (cl == ~r2);
	  }
	  for( ; hitPos+1 < c.size(); ++hitPos ) {
	    c[hitPos] = c[hitPos+1]; // remove literal l
	    const Lit cl = c[hitPos];
	    clsLits.push( c[hitPos] );
	    minL =  data[c[hitPos]] < data[minL] ? c[hitPos] : minL;
	    foundNR1 = foundNR1 || (cl == r1); foundNR2 = foundNR2 || (cl == r2);
	    foundNR1comp = foundNR1comp || (cl == ~r1); foundNR2comp = foundNR2comp || (cl == ~r2);
	  }
	  assert( hitPos + 1 == c.size() );
	  if( debug_out > 4 ) cerr << "c hit at " << hitPos << " intermediate clause: " << c << endl;
	  
	  if( debug_out > 3 ) cerr << "c found r1: " << foundNR1 << "  r2: " << foundNR2 << " r1c: " <<  foundNR1comp << " rc2: " << foundNR2comp << " " << endl;
	  
	  Lit minL1 =  data[r1] < data[minL] ? r1 : minL;
	  Lit minL2 =  data[r2] < data[minL] ? r2 : minL;
	  
	  assert( (!foundNR1 || !foundNR1comp ) && "cannot have both!" );
	  assert( (!foundNR2 || !foundNR2comp ) && "cannot have both!" );
	  
	  //
	  // TODO: handle foundNR1 and foundNR2 here
	  //
	  if( foundNR1 && foundNR2 ) { c.shrink(1); if( debug_out > 2 ) cerr << "c into2 pos new [" << ll[k] << "] : " << c << endl; continue; }
	  if( debug_out > 4 )cerr << "c r1" << endl;
	  bool reuseClause = false;
	  // have altered the clause, and its not a tautology now, and its not subsumed
	  if( !foundNR1comp && !foundNR1 ) {c[hitPos] = r1;c.sort();sortCalls++;} // in this clause, have the new literal, if not present already
	  if( !foundNR1comp && !hasDuplicate( data.list(minL1) , c ) ) { // not there or subsumed -> add clause and test other via vector!
	    if( foundNR1 ) c.shrink(1); // already sorted!
	    else {
	      // add clause c to other lists if not there yet
	      data[r1] ++; data.list(r1).push_back( ll[k] ); // add the clause to the right list! update stats!
	      data.addSubStrengthClause(ll[k]); // afterwards, check for subsumption!
	    }
	    if( debug_out > 1 ) cerr << "c into1 pos [" << ll[k] << "] : " << c << endl;
	  } else {
	    // TODO could reuse clause here!
	    reuseClause = true;
	  }
	  
	  if( debug_out > 4 )cerr << "c r2 (reuse=" << reuseClause << ")" << endl;
	  if( reuseClause ) {
	    reuses ++;
	    for( hitPos = 0 ; hitPos + 1 < c.size(); ++ hitPos ) if( c[hitPos] == r1 ) break; // replace r1 with r2 (or overwrite last position!)
	    if( debug_out > 3 )cerr << "c found " << r1 << " at " << hitPos << endl;
	    if( !foundNR2comp && !foundNR2 ) {c[hitPos] = r2;c.sort();sortCalls++;} // in this clause, have the new literal, if not present already
	    if( !foundNR2comp && !hasDuplicate( data.list(minL1) , c ) ) { // not there or subsumed -> add clause and test other via vector!
	      if( foundNR2 ) c.shrink(1); // already sorted!
	      else {
		// add clause c to other lists if not there yet
		data[r2] ++; data.list(r2).push_back( ll[k] ); // add the clause to the right list! update stats!
		data.addSubStrengthClause(ll[k]); // afterwards, check for subsumption!
	      }
	      if( debug_out > 1 ) cerr << "c into2 reuse [" << ll[k] << "] : " << c << endl;
	    } else {
	      droppedClauses ++;
	      c.set_delete(true);
	      data.removedClause( ll[k] );
	    }
	  } else { // do not reuse clause!
	    if( !foundNR2comp && !foundNR2) { clsLits.push( r2 ); sort(clsLits); sortCalls++;}
	    if( !foundNR2comp && !hasDuplicate( data.list(minL2) , clsLits ) ) {
	      // add clause with clsLits
	      CRef tmpRef = ca.alloc(clsLits, c.learnt() ); // use learnt flag of other !
	      data.addSubStrengthClause(tmpRef); // afterwards, check for subsumption!
	      if( debug_out > 1 ) cerr << "c into2 pos new [" << tmpRef << "] : " << ca[tmpRef] << endl;
	      createdClauses ++;
	      // clause is sorted already!
	      data.addClause( tmpRef ); // add to all literal lists in the clause
	      data.getClauses().push( tmpRef );
	    }
	  }
	} // end positive clauses
	ll.clear(); data[l] = 0; // clear list of positive clauses, update counter of l!
	} // end environment!
	
	// rewrite negative clauses
	const Lit nl = ~amo[j + (half==0 ? 0 : rSize) ]; 		// negative occurrence [amo contains negative occurrences!]:  ~l is replaced by (~newL \lor ~newX)
	const Lit nr1 = half==0 ? newXn : newXp;		// replace with this literal
	const Lit nr2 = ~data.lits[j];	// replace with this literal, always nr2 > nr1!!
	if( debug_out > 1 ) cerr << endl << endl << "c replace " << nl << " with (" << nr1 << " lor " << nr2 << ")" << endl;
	vector<CRef>& nll = data.list(nl);
	for( int k = 0 ; k < nll.size(); ++ k ) {
	  Clause& c= ca[ nll[k] ];
	  assert(c.size() > 1 && "there should not be unit clauses!" );
	  if( c.can_be_deleted() ) continue; // to not care about these clauses!
	  if( debug_out > 1 ) cerr << endl << "c rewrite NEG [" << nll[k] << "] : " << c << endl;
	  clsLits.clear();
	  int hitPos = -1;
	  Lit minL = c[0];
	  bool foundNR1 = false, foundNR2 = false, foundNR1comp=false, foundNR2comp = false;
	  for( hitPos = 0 ; hitPos < c.size(); ++ hitPos ){
	    assert( var(c[hitPos]) < newX && "newly added variables are always greater" );
	    if( c[hitPos] == nl ) break; // look for literal nl
	    const Lit cl = c[hitPos];	    
	    clsLits.push(cl);
	    minL =  data[cl] < data[minL] ? cl : minL;
	    foundNR1 = foundNR1 || (cl == nr1); foundNR2 = foundNR2 || (cl == nr2);
	    foundNR1comp = foundNR1comp || (cl == ~nr1); foundNR2comp = foundNR2comp || (cl == ~nr2);
	  }
	  for( ; hitPos+1 < c.size(); ++hitPos ) {
	    c[hitPos] = c[hitPos+1]; // remove literal nl
	    const Lit cl = c[hitPos];
	    clsLits.push( cl );
	    minL =  data[cl] < data[minL] ? cl : minL;
	    foundNR1 = foundNR1 || (cl == nr1); foundNR2 = foundNR2 || (cl == nr2);
	    foundNR1comp = foundNR1comp || (cl == ~nr1); foundNR2comp = foundNR2comp || (cl == ~nr2);
	  }
	  assert( clsLits.size() + 1 == c.size() && "the literal itself should be missing!" );
	  
	  if( debug_out > 3 ) cerr << "c found nr1: " << foundNR1 << "  nr2: " << foundNR2 << " nr1c: " <<  foundNR1comp << " nrc2: " << foundNR2comp << " " << endl;
	  
	  assert( (!foundNR1 || !foundNR1comp ) && "cannot have both!" );
	  assert( (!foundNR2 || !foundNR2comp ) && "cannot have both!" );
	  
	  if( foundNR1comp || foundNR2comp ) { // found complementary literals -> drop clause!
	    c.set_delete(true);
	    data.removedClause(nll[k]);
	    if( debug_out > 1 ) cerr << "c into tautology " << endl;
	    continue; // next clause!
	  } else if (foundNR1 && foundNR2 ) {
	    c.shrink(1); // sorted already! remove only the one literal!
	    data.addSubStrengthClause(nll[k]);
	    if( debug_out > 1 ) cerr << "c into reduced [" << nll[k] << "] : " << c << endl;
	    continue;
	  } else if( foundNR1 ) {
	    c[hitPos] = nr2; data.list(nr2).push_back( nll[k] );
	    data.addSubStrengthClause(nll[k]); c.sort() ;sortCalls++;
	    if( debug_out > 1 ) cerr << "c into equal1 [" << nll[k] << "] : " << c << endl;
	  } else if ( foundNR2 ) {
	    c[hitPos] = nr1; data.list(nr1).push_back( nll[k] );
	    data.addSubStrengthClause(nll[k]); c.sort() ;sortCalls++;
	    if( debug_out > 1 ) cerr << "c into equal2 [" << nll[k] << "] : " << c << endl;
	  }
	  
	  // general case: nothings inside -> enlarge clause!
	  // check whether nr1 is already inside
	  clsLits.push( nr1 );
	  clsLits.push( nr2 );
	  sort(clsLits);sortCalls++;
	  if( hitPos > 0 && nr1 < c[hitPos-1]  ) c.sort() ;
	  
	  minL =  data[nr1] < data[minL] ? nr1 : minL;
	  minL =  data[nr2] < data[minL] ? nr2 : minL;
	  
	  if( !hasDuplicate( data.list(minL), clsLits ) ) {
	    enlargedClauses ++;
	    c.set_delete(true);
	    data.removedClause( nll[k] );
	    CRef tmpRef = ca.alloc(clsLits, c.learnt() ); // no learnt clause!
	    data.addSubStrengthClause(tmpRef); // afterwards, check for subsumption!
	    if( debug_out > 1 ) cerr << "c into4 neg new [" << tmpRef << "] : " << ca[tmpRef] << endl;
	    // clause is sorted already!
	    data.addClause( tmpRef ); // add to all literal lists in the clause
	    data.getClauses().push( tmpRef );
	  } else {
	    droppedClauses ++;
	    c.set_delete(true);
	    data.removedClause( nll[k] );
	    if( debug_out > 1 ) cerr << "c into5 redundant dropped clause" << endl;
	  }
	  
	}
	nll.clear(); data[nl] = 0; // clear list of positive clauses, update counter of l!
	  
      } // end this half!
    
    } // finished this AMO
    // ADD new AMO of first half (first most easy!!) + add to subsumption! 
    clsLits.clear();
    clsLits.push(lit_Undef);clsLits.push(lit_Undef);
    for( int j = 0 ; j < rSize; ++ j ) {
      for( int k = j+1 ; k < rSize; ++k ) {
	clsLits[0] = data.lits[j] < data.lits[k] ? ~data.lits[j] : ~data.lits[k];
	clsLits[1] = data.lits[j] < data.lits[k] ? ~data.lits[k] : ~data.lits[j];
	CRef tmpRef = ca.alloc(clsLits, false ); // no learnt clause!
	data.addSubStrengthClause(tmpRef); // afterwards, check for subsumption!
	if( debug_out > 2 ) cerr << "c add new AMO [" << tmpRef << "] : " << ca[tmpRef] << endl;
	// clause is sorted already!
	data.addClause( tmpRef ); // add to all literal lists in the clause
	data.getClauses().push( tmpRef );
      }
    }

    
    // do subsumption per iteration!
    subsumption.process();
    
    // do garbage collection, since we do a lot with clauses!
    // actually, variable densing should be done here as well ...
    data.checkGarbage();
  }
 
  } // end of iteration
  rewTime = cpuTime() - rewTime;
  
  inAmo.destroy();
}

Var Rewriter::nextVariable(char type)
{
  Var nextVar = data.nextFreshVariable(type);
  
  // enlarge own structures
  rewHeap.addNewElement(data.nVars());
  
  return nextVar;
}

void Rewriter::printStatistics(ostream& stream)
{
  stream << "c [STAT] REW " << processTime << "s, "
  << amoTime << " s-amo, "
  << rewTime << " rewTime, "
  << steps << " steps, " << endl;
  stream << "c [STAT] REW(2) " 
  << detectedDuplicates << " duplicates, "
  << createdClauses << " created, "
  << enlargedClauses << " enlarged, "
  << reuses << " reused, "
  << droppedClauses << " dropped, "
  << removedViaSubsubption << " droped(sub), "<< endl;
 
  stream << "c [STAT] REW(2) " 
  << sortCalls << " sorts, "
  << processedAmos << " amos, "
  << foundAmos << " foundAmos, "
  << maxAmo << " maxAmo, " 
  << addedVariables << " addedVars, "
  << removedVars << " removedVars, "
  << endl;
}


void Rewriter::destroy()
{
  rewHeap.clear(true);
}

bool Rewriter::checkPush(vec<Lit> & ps, const Lit l)
{
    if (ps.size() > 0)
    {
        if (ps.last() == l)
         return false;
        if (ps.last() == ~l)
         return true;
    }
    ps.push(l);
    return false;
}
  
// TODO: have a template here!
bool Rewriter::ordered_subsumes (const Clause& c, const Clause & other) const
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
  
bool Rewriter::ordered_subsumes (const vec<Lit>& c, const Clause & other) const
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

bool Rewriter::ordered_subsumes (const Clause & c, const vec<Lit>& other) const
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
  
// TODO: have a template here!
bool Rewriter::hasDuplicate(vector<CRef>& list, const Clause& c)
{
  for( int i = 0 ; i < list.size(); ++ i ) {
    Clause& d = ca[list[i]];
    if( d.can_be_deleted() ) continue;
    int j = 0 ;
    if( d.size() == c.size() && (&c != &d) ) { // do not remove itself!
      while( j < c.size() && c[j] == d[j] ) ++j ;
      if( j == c.size() ) { 
	if( debug_out > 1 ) cerr << "c clause is equal to [" << list[i] << "] : " << d << endl;
	detectedDuplicates ++;
	return true;
      }
    }
    if( true ) { // check each clause for being subsumed -> kick subsumed clauses!
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
	  removedViaSubsubption ++;
	}
      }
    }
  }
  return false;
}
  
bool Rewriter::hasDuplicate(vector<CRef>& list, const vec<Lit>& c)
{
  for( int i = 0 ; i < list.size(); ++ i ) {
    Clause& d = ca[list[i]];
    if( d.can_be_deleted() ) continue;
    int j = 0 ;
    if( d.size() == c.size() ) {
      while( j < c.size() && c[j] == d[j] ) ++j ;
      if( j == c.size() ) { 
	if( debug_out > 1 ) cerr << "c clause is equal to [" << list[i] << "] : " << d << endl;
	detectedDuplicates ++;
	return true;
      }
    }
    if( true ) { // check each clause for being subsumed -> kick subsumed clauses!
      if( d.size() < c.size() ) {
	detectedDuplicates ++;
	if( debug_out > 1 ) cerr << "c clause " << c << " is subsumed by [" << list[i] << "] : " << d << endl;
	if( ordered_subsumes(d,c) ) return true; // the other clause subsumes the current clause!
      } if( d.size() > c.size() ) { // if size is equal, then either removed before, or not removed at all!
	if( ordered_subsumes(c,d) ) { 
	  d.set_delete(true);
	  data.removedClause(list[i]);
	  removedViaSubsubption ++;
	}
      }
    }
  }
  return false;
}

