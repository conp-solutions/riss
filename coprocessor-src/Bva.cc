/******************************************************************************************[BVA.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Bva.h"

#include <algorithm>

using namespace Coprocessor;

// potential options
// 	CONST_PARAM bool bvaComplement;		/// treat found complements special?
// 	CONST_PARAM uint32_t bvaPush;		/// which literals to push to queue again (0=none,1=original,2=all)
// 	CONST_PARAM bool bvaRewEE;		/// run rewEE after BVA found new gates?
// 	uint32_t bvaLimit;			/// number of checks until bva is aborted
// 	CONST_PARAM bool bvaRemoveDubplicates;	/// remove duplicate clauses from occurrence lists
// 	CONST_PARAM bool bvaSubstituteOr;	/// when c = (a AND b) is found, also replace (-a OR -b) by -c

static const char* _cat = "COPROCESSOR 3 - BVA";

static IntOption  opt_bva_push             (_cat, "cp3_bva_push",    "push variables back to queue (0=none,1=original,2=all)", 2, IntRange(0, 2));
static IntOption  opt_bva_VarLimit         (_cat, "cp3_bva_Vlimit",  "use BVA only, if number of variables is below threshold", 3000000, IntRange(-1, INT32_MAX));
static IntOption  opt_bva_Alimit           (_cat, "cp3_bva_limit",   "number of steps allowed for AND-BVA", 1200000, IntRange(0, INT32_MAX));
static BoolOption opt_Abva                 (_cat, "cp3_Abva",        "perform AND-bva", true);
static IntOption  opt_inpStepInc           (_cat, "cp3_bva_incInp",  "increases of number of steps per inprocessing", 80000, IntRange(0, INT32_MAX));

static BoolOption opt_bvaComplement        (_cat, "cp3_bva_compl",   "treat complementary literals special", true);
static BoolOption opt_bvaRemoveDubplicates (_cat, "cp3_bva_dupli",   "remove duplicate clauses", true);
static BoolOption opt_bvaSubstituteOr      (_cat, "cp3_bva_subOr",   "try to also substitus disjunctions", false);



#if defined CP3VERSION 
static const int bva_debug = 0;
static const bool opt_bvaAnalysis = false;
static const bool opt_Xbva = 0;
static const bool opt_Ibva = 0;
static const int opt_bvaAnalysisDebug = 0;
static const int opt_bva_Xlimit = 100000000;
static const int opt_bva_Ilimit = 100000000;
#else
static IntOption  bva_debug                (_cat, "bva-debug",       "Debug Output of BVA", 0, IntRange(0, 4));
static IntOption  opt_bvaAnalysisDebug     (_cat, "cp3_bva_ad",      "experimental analysis", 0, IntRange(0, 4));

static IntOption  opt_bva_Xlimit            (_cat, "cp3_bva_Xlimit",   "number of steps allowed for XOR-BVA", 100000000, IntRange(0, INT32_MAX));
static IntOption  opt_bva_Ilimit            (_cat, "cp3_bva_Ilimit",   "number of steps allowed for ITE-BVA", 100000000, IntRange(0, INT32_MAX));

static IntOption  opt_Xbva          (_cat, "cp3_Xbva",       "perform XOR-bva (1=half gates,2=full gates)", 0, IntRange(0, 2));
static IntOption  opt_Ibva          (_cat, "cp3_Ibva",       "perform ITE-bva (1=half gates,2=full gates)", 0, IntRange(0, 2));
#endif
	
BoundedVariableAddition::BoundedVariableAddition(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data)
: Technique( _ca, _controller )
, data( _data )
, doSort( true )
, processTime(0)
, andTime(0)
, iteTime(0)
, xorTime(0)
, andDuplicates(0)
, andComplementCount(0)
, andReplacements(0)
, andTotalReduction(0)
, andReplacedOrs(0)
, andReplacedMultipleOrs(0)
, andMatchChecks(0)
, xorfoundMatchings(0)
, xorMultiMatchings(0)
, xorMatchSize(0)
, xorMaxPairs(0)
, xorFullMatches(0)
, xorTotalReduction(0)
, xorMatchChecks(0)
, iteFoundMatchings(0)
, iteMultiMatchings(0)
, iteMatchSize(0)
, iteMaxPairs (0)
, iteTotalReduction(0)
, iteMatchChecks(0)

, bvaHeap( data )
, bvaComplement(opt_bvaComplement)
, bvaPush(opt_bva_push)
, bvaALimit(opt_bva_Alimit)
, bvaXLimit(opt_bva_Xlimit)
, bvaILimit(opt_bva_Ilimit)
, bvaRemoveDubplicates(opt_bvaRemoveDubplicates)
, bvaSubstituteOr(opt_bvaSubstituteOr)
{

}

void BoundedVariableAddition::giveMoreSteps()
{
  andMatchChecks = andMatchChecks < opt_inpStepInc ? 0 : andMatchChecks - opt_inpStepInc;
  xorMatchChecks = xorMatchChecks < opt_inpStepInc ? 0 : xorMatchChecks - opt_inpStepInc;
  iteMatchChecks = iteMatchChecks < opt_inpStepInc ? 0 : iteMatchChecks - opt_inpStepInc;
}

bool BoundedVariableAddition::process()
{
  processTime = cpuTime() - processTime;
  bool didSomething = false;

  // use BVA only, if number of variables is not too large 
  if( data.nVars() < opt_bva_VarLimit || !data.unlimited() ) {
    // run all three types of bva - could even re-run?
    if( opt_Abva ) didSomething = andBVA();
    if( opt_Xbva == 1) didSomething = xorBVAhalf() || didSomething;
    else if( opt_Xbva == 2) didSomething = xorBVAfull() || didSomething;
    if( opt_Ibva == 1) didSomething = iteBVAhalf() || didSomething;
    else if( opt_Ibva == 2) didSomething = iteBVAfull() || didSomething;
  }
  
  processTime = cpuTime() - processTime;
}


bool BoundedVariableAddition::andBVA() {
  
  bool didSomething=false;

  MethodTimer processTimer( &andTime );
  doSort = true;

  // setup own structures
  bvaHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( data[  mkLit(v,false) ] > 2 ) if( !bvaHeap.inHeap(toInt(mkLit(v,false))) )  bvaHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] > 2 ) if( !bvaHeap.inHeap(toInt(mkLit(v,true))) )   bvaHeap.insert( toInt(mkLit(v,true))  );
  }
  data.ma.resize(2*data.nVars());
  bvaCountMark.resize( data.nVars() * 2, lit_Undef);
  bvaCountCount.resize( data.nVars() * 2);
  bvaCountSize.resize( data.nVars() * 2);
  
	vector<Lit> stack;
	
	int32_t currentReduction = 0;
	
	bool addedNewAndGate = false;
	
	// for l in F
	while (bvaHeap.size() > 0 && (data.unlimited() || bvaALimit > andMatchChecks) && !data.isInterupted() ) {
	  
	  if( bva_debug > 1 ) cerr << "c next major loop iteration with heapSize " << bvaHeap.size() << endl;
	  
	  if( bva_debug > 2 )
	    if( checkLists("check data structure integrity") )
	      assert( false && "there cannot be duplicate clause entries in clause lists!" );
	  
	  // interupted ?
	  if( data.isInterupted() ) break;
	  
	  /** garbage collection */
	  data.checkGarbage();
	  
	  const Lit right = toLit(bvaHeap[0]);
	  assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

	  if( bva_debug > 2 && bvaHeap.size() > 0 ) cerr << "c [BVA] new first item: " << bvaHeap[0] << " which is " << right << endl;
	  bvaHeap.removeMin();
	  
	  // TODO: check for literals that are frozen
	  // if( search.eliminated.get( right.toVar() ) ) continue;
	  
	  if( data.value( right ) != l_Undef ) continue;
	  
	  bvaMatchingLiterals.clear();
	  if( data.list(right).size() < 3 ) continue;
	  // removeDuplicateClauses(right);
	  // if( data.list(right).size() < 3 ) continue;
	  // cerr << "c BVA work on " << right << " with " << data[right] << endl;
	  
	  uint32_t index = 0;
	  while ( bvaMatchingClauses.size() < index+1 ) bvaMatchingClauses.push_back( vector<CRef>() );
	  // fill matrix (Mlit in pseudo code) with first column, corresponding to Mcls
	  bvaMatchingClauses[0].clear();
	  for( uint32_t i = 0 ; i < data.list( right).size(); ++i )
	  {
	    
	    bvaMatchingClauses[0].push_back( data.list(right)[i] );
	  }
	  
	  bvaMatchingLiterals.push_back(right);	// setup vector of literals that match
	  currentReduction = 0;			// set value of correction
	  
	  if( bva_debug > 2 ) cerr << "c BVA test right: " << right << endl;
	  // while literals can be found that increase the reduction:
	  while( true )
	  {
	  
	  stack.clear();
	  index ++; // count column of literal that will be added
	  
	  // create the stack with all matching literals
	  for( uint32_t i = 0 ; i <  bvaMatchingClauses[0].size() && ( data.unlimited() || bvaALimit > andMatchChecks ) && !data.isInterupted() ; ++i )
	  {
	    if( bva_debug > 2 ) cerr << "c next reduce literal loop iteration with matching clauses: " << bvaMatchingClauses[0].size() << endl;
	    // reserve space
	    while ( bvaMatchingClauses.size() < index+1 ) bvaMatchingClauses.push_back( vector<CRef>() );
	    
	    // continue if clause is empty
	    CRef C =  bvaMatchingClauses[0][i];
	    if( C == CRef_Undef ) continue;
	    Clause& clauseC = ca[ C];
	    if( clauseC.can_be_deleted() ) continue;
	    // no unit clauses!
	    if( clauseC.size() == 1 ) continue;
	    data.ma.nextStep();	// mark current step
	    
	    // cerr << "c check clause[" << C << "]= " << clauseC << " whether lit " << right << " is part of it" << endl;
	    
	    // try to find a minimum!
	    Lit l1 = lit_Undef;
	    uint32_t min = 0;
	    bool rightInFlag=false;
	    for( uint32_t j = 0 ; j < clauseC.size(); ++ j ) {
	      const Lit& lt = clauseC[j];
	      data.ma.setCurrentStep(toInt(lt));
	      if( lt == right ) { data.ma.reset(toInt(lt)); rightInFlag=true; continue; }
	      
	      if ( l1 == lit_Undef ) {
		l1 = lt; min = data.list( lt).size();
	      } else {
		if( min > data.list(lt).size() ) {
		  min = data.list(lt).size();
		  l1 = lt;
		}
	      }
	    }
	    // cerr << "c selected literal " << l1 << endl;
	    
	    if( ! rightInFlag && bva_debug>3) { cerr << "literal " << right << " is not part of the clause " << clauseC <<  " , which is learned(" << clauseC.learnt() << ") and canBeDeleted(" << clauseC.can_be_deleted() << ")" << endl;}
	    
	    assert ( rightInFlag && "literal does not occur in match clause" );
	    assert( l1 != right  && "minimal literal is equal to right" );
	    assert( l1 != lit_Undef && "there is no minimal literal" );

	    if( l1 == lit_Undef ) continue;
	    if( data.list( l1).size() < 2 ) continue; // there has to be something to be joined
	    
	    // for D in Fl1
	    for( uint32_t j = 0 ; j < data.list( l1).size(); ++j ) {
	      const CRef D = data.list( l1)[j];
	      andMatchChecks ++;
	      if( D == C ) continue;
	      const Clause& clauseD = ca[ D];
	      if( clauseD.can_be_deleted() ) continue;
	      if( clauseC.size() != clauseD.size() ) continue;	// TODO: relax later

	      // for l2 in D
	      Lit match = lit_Undef;
	      assert ( data.ma.isCurrentStep( toInt(l1) ) );
	      // find match
	      for( uint32_t k =0; k < clauseD.size(); ++k ) {
		const Lit l2 = clauseD[k];
		
		if( l2 == right ) goto nextD;
		
		if( !data.ma.isCurrentStep( toInt(l2) ) ) {
		  if( match == lit_Undef ) match = l2; // found matching literal?
		  else goto nextD;
		}
	      }
	      
	      // cerr << "c found match: " << match << endl;
	      
	      if( match == lit_Undef ) { andDuplicates++; continue; } // found a matching clause
	      assert ( match != right && "matching literal is the right literal");
	
	      if( bva_debug > 3 ) cerr << "c check count for literal " << match << " where countSize is mark: " << bvaCountMark.size() << " count:" << bvaCountCount.size() << " index: " << toInt(match) << endl;
	      // count found match
	      if (bvaCountMark[ toInt( match ) ] == right ) {
		bvaCountCount[ toInt( match ) ] ++;
		bvaCountSize[ toInt( match ) ] += clauseD.size() - 1;
	      } else {
		bvaCountMark[ toInt( match ) ] = right;
		bvaCountCount[ toInt( match ) ] = 1;
		bvaCountSize[ toInt( match ) ] = clauseD.size() - 1;
		stack.push_back( match );
	      }
	      nextD:;
	    }
	    
	  }
	  // cerr << "c check match-stack for " << right << " of size " << stack.size()  << endl;
	  
	  // found all partner literals for l, if there is none, continue with next literal
	  if( stack.size() == 0 ) break;
	  
	  uint32_t max = 0;
	  Lit left = lit_Undef; 
	  bool foundRightComplement = false;
	  // find maximum element in stack
	  for( uint32_t i = 0 ; i < stack.size(); ++i ){
	    assert( bvaCountMark[ toInt(stack[i]) ] == right );
	    const Lit stackLit = stack[i];
	    // remove literals from the candidates, that already match the literal
	    for(  uint32_t j = 0; j < bvaMatchingLiterals.size(); ++ j ) {
	      if( bvaMatchingLiterals[j] == stackLit ) {
		// cerr << "c remove stack lit because it matches already: " << bvaMatchingLiterals[j] << endl;
		goto nextStackLiteral;
	      }
	    }
	    
	    if( stack[i] == ~right ) foundRightComplement = true;
	    // reset bvaMark
	    bvaCountMark[ toInt(stack[i]) ] = lit_Undef;
	    
	    if( bvaCountCount[ toInt(stack[i]) ] > max ){
	      left = stack[i];
	      assert(left != right );
	      max = bvaCountCount[ toInt(left) ];
	    }
	    nextStackLiteral:;
	  }
	  // cerr << "c picked left = " << left << endl;
	  
	  for( int i = 0 ; i < stack.size() ; ++ i ) {
	    bvaCountMark[ toInt( stack[i] ) ] = lit_Error;
	    bvaCountCount[ toInt( stack[i] ) ] = 0;
	    bvaCountSize[ toInt( stack[i] ) ] = 0;
	  }
	  
	  
	  if( left == lit_Undef ) {
	    static bool didIt = false;
	    if( !didIt ) {
	      // cerr << "c second BVA literal became litUndef - check how!" << endl; didIt = true; 
	      // assert(false && "remove this bug?!" );
	    }
	    // cerr << "c break" << endl;
	    break;
	  }
	  
	  if(  bva_debug > 2 ) cerr << "c BVA selected left: " << left << " foundRC=" << foundRightComplement << ", bvaCmpl=" << bvaComplement << endl;
	  // tackle complement specially, if found
	  if( bvaComplement && foundRightComplement ) left = ~right;
	  
	  // found clause(s) [right, l,...], [-right,l,...]
	  if( left == ~right && bvaComplement ) {
	    didSomething = true;
	    if(  bva_debug > 1 ) cerr << "c [BVA] found complement " << left << endl;
	    if( !bvaHandleComplement( right ) ) {
	      data.setFailed();
	      return didSomething;
	    }
	    andComplementCount ++;
	    if( !bvaHeap.inHeap( toInt(right) ) ) bvaHeap.insert( toInt(right) ) ;
	    else bvaHeap.update( toInt(right) );
	    if( !bvaHeap.inHeap( toInt(left) ) ) bvaHeap.insert( toInt(left) ) ;
	    else bvaHeap.update( toInt(left) );
	    // if complement has been found, this complement is treated and the next literal is picked
	    if( bva_debug > 1 && bvaHeap.size() > 0 ) cerr << "c [BVA] heap(" << bvaHeap.size() << ") with at 0: " << toLit(bvaHeap[0]) << endl;
	    break;
	  }
	  
	  // if nothing has been selected, because not enough there - use next literal!
	  if( left == lit_Undef ) {
	    // cerr << "c continue with no match?" << endl;
	    continue;
	  }
	  
	  // do not continue, if there won't be a reduction
	  // dows not hold in the multi case!!
	  if( index == 1 && max < 3 ) {
	    if(  bva_debug > 2 ) cerr << "c [BVA] interrupt because of too few matchings " << right << " @" << index << " with " << max << endl;
	    break;
	  }
	  if(  bva_debug > 0 ) cerr << "c [BVA] index=" << index << " max=" << max << " rightList= " << data.list(right).size() << " leftList= " << data.list(left).size() << endl;

	  // heuristically remove duplicates!
	  if( var(left) >= data.nVars() ) cerr << "c [BVA] working on too large variable " << var(left) << " vs " << data.nVars() 
	      << " literal L: " << left << " int: " << toInt(left)
	      << " literal R: " << right << " int: " << toInt(right)  
	      << endl;
	  if( max > data.list(left).size() || max > data.list(right).size() ) {
	    if(  bva_debug > 1 ) cerr << "c remove duplicate clauses from left list " << left << " with index= " << index << endl;
	    uint32_t os = data.list(left).size();
	    removeDuplicateClauses(left);
	    if(  bva_debug > 1 ) {
	      cerr << "c reduced list by " << os - data.list(left).size() << " to " << data.list(left).size() << endl;
	      for( uint32_t j = 0 ; j<data.list(left).size(); ++j ) { cerr <<  data.list(left)[j] << " : " << ca[ data.list(left)[j] ]  << endl;}
	    }
	    index --; continue; // redo the current literal!
	  }
	  
	  // calculate new reduction and compare!
	  int32_t newReduction = ( (int)(bvaMatchingLiterals.size() + 1) * (int)max ) - ( (int)max + (int)(bvaMatchingLiterals.size() + 1) ); 
	  if( bva_debug > 1 ) cerr << "c BVA new reduction: " << newReduction << " old: " << currentReduction << endl;
	  
	  // check whether new reduction is better, if so, update structures and proceed with adding matching clauses to structure
	  if( newReduction > currentReduction )
	  {
	    if( bva_debug > 2 ) cerr << "c [BVA] keep literal " << left << endl;
	    currentReduction = newReduction;
	    bvaMatchingLiterals.push_back( left );
	  } else break;
	  
	  const uint32_t matches = bvaCountCount[ toInt(left) ];

	  // get vector for the literal that will be added now
	  while ( bvaMatchingClauses.size() < bvaMatchingLiterals.size() ) bvaMatchingClauses.push_back( vector<CRef>() );
	  
	  // if there has been already the space, simply re-use the old space
	  bvaMatchingClauses[index].clear();
	  bvaMatchingClauses[index].assign( bvaMatchingClauses[0].size(),CRef_Undef);
	  
	  assert (max <= data.list( left).size() );
	  const vector<CRef>& rightList = bvaMatchingClauses[ 0 ];
	  vector<CRef>& leftList = data.list( left); // will be sorted below
	  uint32_t foundCurrentMatches = 0;
	  // for each clause in Mcls, find matching clause in list of clauses with literal "left"
	  for( uint32_t i = 0 ; i < rightList.size(); ++i ) {
	    if( rightList[i] == CRef_Undef ) {
	      continue; // to skip clauses that have been de-selected
	    }
	    const CRef C = rightList[i];
	    const Clause& clauseC = ca[ C ];
	    if( clauseC.can_be_deleted() ) continue;
	    if( bva_debug > 2 && clauseC.size() < 2 ) cerr << "c [BVA] unit clause in right clauses (" << clauseC.size() << " " << endl;
	    if( clauseC.size() < 2 ) continue;
	    data.ma.nextStep();
	    for( uint32_t j = 0; j < clauseC.size(); j ++ ) {
	      if( clauseC[j] == right ) continue;
	      data.ma.setCurrentStep( toInt(clauseC[j]) );
	    }
	    // so far, left and right cannot be in the same clause for matching
	    if ( data.ma.isCurrentStep( toInt(left) ) ) { 
	      bvaMatchingClauses[ 0 ][i] = CRef_Undef;	// FIXME: check whether excluding this clause is necessary (assumption: yes)
	      continue;
	    }
	    data.ma.reset( toInt(right) ); data.ma.setCurrentStep( toInt(left) );
	    unsigned oldFoundCurrentMatches = foundCurrentMatches;
	    for( uint32_t j = foundCurrentMatches ; j < leftList.size(); ++j ) {
	      CRef D = leftList[j];
	      const Clause& clauseD = ca[ D];
	      if( clauseD.can_be_deleted() ) continue;

	      if( bva_debug > 2 && clauseD.size() < 1 ) cerr << "c [BVA] found unit clause in list" << left << endl;
	      
	      CRef tmp2 = CRef_Undef;
	      if( clauseD.size() != clauseC.size() ) continue; // TODO: relax later!
	      for( uint32_t k = 0 ; k < clauseD.size() ; ++ k ){
		if( !data.ma.isCurrentStep( toInt(clauseD[k]) ) ) goto nextD2;
	      }
	      bvaMatchingClauses[index][i] = D;
	      // swap positions in the list
	      tmp2 = leftList[ foundCurrentMatches ];
	      leftList[ foundCurrentMatches ] = D;
	      leftList[ j ] = tmp2;
	      
	      foundCurrentMatches++;
	      break;
	      nextD2:;
	    }
	    data.ma.reset( toInt(left) );
	    // deselect clause, because it does not match with any clause of the current "left" literal
	    if( foundCurrentMatches == oldFoundCurrentMatches ) bvaMatchingClauses[0][i] = CRef_Undef;
	  }
	  // return number of matching clauses
	  
	  } // end while improving a literal
	  
	  // continue with next literal, if there has not been any reduction
	  if( bvaMatchingLiterals.size() < 2 ) {
	    if( bvaHeap.size() > 0 )
	    if( bva_debug > 2 ) cerr << "c [BVA] continue because not two matching literals (" << right << "), bvaHeap[" << bvaHeap.size() << "],0=" << toLit(bvaHeap[0]) << endl;
	    continue;
	  }
	  
	  // create the new clauses!
	  const Var newX = nextVariable('a');
	  andReplacements++; andTotalReduction += currentReduction;
	  didSomething = true;
	  assert( !bvaHeap.inHeap( toInt(right) ) && "by adding new variable, heap should not contain the currently removed variable" );
	  
	  if( bva_debug > 1 ) cerr << "c [BVA] heap size " << bvaHeap.size() << endl;
	  Lit replaceLit = mkLit( newX, false );
	  // for now, use the known 2!
	  assert( bvaMatchingLiterals.size() <= bvaMatchingClauses.size() ); // could be one smaller, because stack has only one element
	  
	  if( bva_debug > 1 ) {
	    cerr << "c [BVA] matching literals:";
	    for( uint32_t i = 0 ; i < bvaMatchingLiterals.size(); ++ i ) cerr << " " << bvaMatchingLiterals[i];
	    cerr << endl;
	  }
	  
	  for( uint32_t i = 0 ; i < bvaMatchingLiterals.size(); ++ i ) {
	    vector<CRef>& iClauses = bvaMatchingClauses[i];
	    if( i == 0 ) {	// clauses that are in the occurrenceList list of right

		if( bva_debug > 3 ) {
		  cerr << "c PRE occurrence list of right - lit: " << right << " : " << endl;
		  for( int i = 0 ; i < data.list(right).size(); ++ i )
		    cerr << "c [" << i << "] clause[ " << data.list(right)[i] << " ]= " << ca[data.list(right)[i]] << endl;
		}

	      // clauses of right literal, alter, so that they will be kept
	      for( uint32_t j = 0 ; j < iClauses.size(); ++ j )
	      {
		if( iClauses[j] == CRef_Undef ) continue; // don't bvaHeap with 0-pointers
		bool replaceFlag = false;
		Clause& clauseI = ca[ iClauses[j] ];
		if( clauseI.can_be_deleted() ) continue;
		if( bva_debug > 2 && clauseI.size() < 2) cerr << "c [BVA] unit clause for replacement (" << clauseI.size() << ")" << endl;
		assert( ! clauseI.can_be_deleted() && "matching clause is ignored" );
		
		if( bva_debug > 2 )cerr << "c rewrite clause[ " << iClauses[j] << " ]= " << clauseI << endl;
		
		// take care of learned clauses (if the replace clause is learned and any matching clause is original, keep this clause!)
		for( uint32_t k = 1; k< bvaMatchingLiterals.size(); ++ k ) {
		  if( !ca[ bvaMatchingClauses[k][j] ].learnt() ) { clauseI.set_learnt(false); break; }
		}
		for( uint32_t k = 0 ; k< clauseI.size(); ++k ) {
		  if( clauseI[k] == right ) { 
		    if( bva_debug > 2 ) cerr << "c replace literal " << clauseI[k] << " with literal " << replaceLit << endl;
		    clauseI[k] = replaceLit;
		    data.removedLiteral( right );
		    data.addedLiteral( replaceLit );
		    replaceFlag = true;
		    break;
		  }
		}
		if( clauseI.size() == 1 ) {
		  if( l_False == data.enqueue( replaceLit ) ) {
		    return didSomething;
		  }
		  static bool didIt = true;
		  if( didIt ) { cerr << "c BVA clause with replaced literal is unit" << endl; didIt = false; }
		  clauseI.set_delete(true);
		}
		// ensure that the clauses are sorted
		clauseI.sort();
		assert( replaceFlag && "could not replace literal right");
	      
		// remove clause from the occurrenceList list
		if( !data.removeClauseFrom(iClauses[j], right ) ) {
		 cerr << "c was not able to find clause ref " << iClauses[j] << " (" << clauseI << ") in list of literal " << right << " (was rewritten)" << endl; 
		}
		
		// add clause into occurrenceList list of new variable
		data.list( replaceLit ). push_back(iClauses[j]);
		if( bva_debug > 2 )cerr << "c into pre-sort clause[ " << iClauses[j] << " ]= " << clauseI << endl;
	        clauseI.sort();
		if( bva_debug > 2 )cerr << "c into clause[ " << iClauses[j] << " ]= " << clauseI << endl;
		
		if( bva_debug > 3 ) {
		  cerr << "c POST occurrence list of right - lit: " << right << " : " << endl;
		  for( int i = 0 ; i < data.list(right).size(); ++ i )
		    cerr << "c [" << i << "] clause[ " << data.list(right)[i] << " ]= " << ca[data.list(right)[i]] << endl;
		}
	      }
	    } else {
	      assert( iClauses.size() == bvaMatchingClauses[0].size() );
	      // remove remaining clauses completely from formula
	      for( uint32_t j = 0 ; j < iClauses.size(); ++ j ) {
		if( bvaMatchingClauses[0][j] != CRef_Undef && iClauses[j] != CRef_Undef ) {
		  Clause& clauseI = ca[ iClauses[j] ];
		  // cerr << "c delete clause[" << iClauses[j] << "]= " << clauseI << endl;
		  clauseI.set_delete(true);
		  data.removedClause( iClauses[j] );
		}
	      }
	    }
	  }
	  assert( bvaMatchingLiterals.size() > 1 && "too few matching literals");
	  
	  // and-gate needs to be written explicitely -> add the one missing clause, if something has been found
	  if( bva_debug > 2 ) cerr << "c [BVA] current reduction: " << currentReduction << endl;
	  if( bvaSubstituteOr && currentReduction > 1 ) // -newX = (a AND b), thus newX = (-a OR -b). replace all occurrences!
	  {
	    data.ma.nextStep();
	    // mark all complements
	    assert( bvaMatchingLiterals.size() > 0 && "there have to be bva literals" );
	    Lit min = ~bvaMatchingLiterals[0];
	    uint32_t minO = data[min];
	    for( uint32_t i = 0 ; i < bvaMatchingLiterals.size(); ++ i ) {
	      const Lit& literal = ~bvaMatchingLiterals[i];
	      data.ma.setCurrentStep( toInt(literal) );
	      if( data[literal] < minO ) {
		min = literal; minO = data[ min ];
	      }
	    }
	    bool addedAnd = false;
	    bool isNotFirst = false; // for statistics
	    for( uint32_t i = data.list(min).size() ; i > 0; --i ) {
	      const CRef clRef = data.list(min)[i-1];
	      Clause& clause = ca[ clRef ];
	      // clause cannot contain all matching literals
	      if( clause.can_be_deleted() || clause.size() < bvaMatchingLiterals.size() ) continue;
	      // check whether all literals are matched!
	      uint32_t count = 0;
	      for( uint32_t j = 0 ; j < clause.size(); ++ j ) {
		if( data.ma.isCurrentStep( toInt( clause[j] ) ) ) count ++;
	      }
	      // reject current clause
	      if( count != bvaMatchingLiterals.size() ) continue;
	      // rewrite current clause
	      {
		if( bva_debug > 2 ) cerr << "c rewrite clause[ " << clRef << " ]= " << clause << endl;
		andReplacedOrs ++;
		andReplacedMultipleOrs = (isNotFirst ? andReplacedMultipleOrs + 1 : andReplacedMultipleOrs );
		isNotFirst = true; // each next clause is not the first clause
		uint32_t j = 0;
		for(  ; j < clause.size(); ++ j ) {
		  const Lit literal = clause[j];
		  if( data.ma.isCurrentStep( toInt(literal) ) ) {
		    if( bva_debug > 2 ) cerr << "c replace literal " << clause[j] << " with " << ~replaceLit << endl;
		    data.removeClauseFrom(clRef, literal);
		    clause[j] =  ~replaceLit ;
		    data.removedLiteral(literal);
		    data.addedLiteral( ~replaceLit );
		    data.list( ~replaceLit ).push_back( clRef );
		    count --;
		    break;
		  }
		}
		if( bva_debug > 2 ) cerr << "c found first literal at " << j << "/" << clause.size() << endl;
		for( ; j < clause.size(); ++ j ) { 
		  const Lit literal = clause[j];
		  if( bva_debug > 2 ) cerr << "c check literal " << clause[j] << endl;
		  if( data.ma.isCurrentStep( toInt(literal) ) ) {
		    if( bva_debug > 2 ) cerr << "c remove literal " << clause[j] << endl;
		    data.removeClauseFrom(clRef, literal);
		    if( doSort ) clause.removePositionUnsorted(j);
		    else clause.removePositionSorted(j);
		    data.removedLiteral(literal);
		    
		    data.updateClauseAfterDelLit(clause);
		    if( clause.size() == 1 ) {
		      if( l_False == data.enqueue(clause[0] ) ) {
			return false;
		      }
		      clause.set_delete(true); // forget about this clause, it's in the trail now
		    }
		    --j;
		    count --;
		  }
		}
		clause.sort();
		assert( count == 0 && "all matching literals have to be replaced/removed!" );
		if( bva_debug > 2 ) cerr << "c into clause[ " << clRef << " ]= " << clause << endl;
	      }
	      
	      // add and-clause only once
	      if( !addedAnd ) {
		addedAnd = true;
		addedNewAndGate = true;
		clauseLits.clear();
		for( uint32_t j = 0 ; j < bvaMatchingLiterals.size(); ++j )
		  clauseLits.push( ~bvaMatchingLiterals[j] );
		clauseLits.push( replaceLit );
		CRef tmpRef = ca.alloc(clauseLits, false); // no learnt clause!
		if( doSort ) ca[tmpRef].sort();
		data.addClause( tmpRef );
		data.getClauses().push( tmpRef );
		if( bva_debug > 2 ) cerr << "add new ANDclause[ " << tmpRef << " ]= " << ca[tmpRef] << endl;
	      }
	    }
	    
	  }

	  clauseLits.clear();
	  clauseLits.push( ~replaceLit );
	  clauseLits.push( lit_Undef );
	  
	  for( uint32_t i = 0; i < bvaMatchingLiterals.size(); ++i ) {
	    clauseLits[1] = bvaMatchingLiterals[i];
	    CRef newClause = ca.alloc(clauseLits, false); // no learnt clause!
	    if( doSort ) ca[newClause].sort();
	    data.addClause( newClause );
	    data.getClauses().push( newClause );
	    if( bva_debug > 2 ) cerr << "add BVA clause " << ca[newClause] << endl;
	  }
	  
	  if( bvaPush > 0 ) {
	    if( bva_debug > 1 &&  bvaHeap.inHeap(toInt(right)) ) cerr << "c [BVA] add item that is not removed: " << right << endl;
	    if( bvaHeap.inHeap(toInt(right)) ) bvaHeap.update(toInt(right));
	    else bvaHeap.insert( toInt(right) );
	  }
	  if( bvaPush > 1 ) {
	    if( bvaMatchingLiterals.size() > 2 ) {
	      if( bvaHeap.inHeap( toInt(mkLit(newX, false)) )) bvaHeap.update( toInt(mkLit(newX, false)));
	      else bvaHeap.insert( toInt(mkLit( newX, false ) ));
	    }
	  }
	  if( bva_debug > 1 && bvaHeap.size() > 0 ) cerr << "c [BVA] heap(" << bvaHeap.size() << ") with at 0: " << toLit(bvaHeap[0]) << endl;
	  
	} // end for l in F
  endBVA:;
  
// 	  if( bva_debug > 2 )
// 	  {
// 	    for( Var v = 1 ; v <= search.var_cnt; ++v ) {
// 	      Lit r = mkLit(v,false);
// 	      cerr << "c " << r << " list: " << endl;
// 	      for( uint32_t ti = 0 ; ti < data.list( r ).size(); ti ++ ) { printClause( search, data.list( r )[ti] ); cerr  << " i:" << search.gsa.get(data.list( r )[ti]).can_be_deleted() << endl; }
// 	      cerr << "c " << ~r << " list: " << endl;
// 	      for( uint32_t ti = 0 ; ti < data.list( ~r ).size(); ti ++ ) { printClause( search, data.list( ~r )[ti] ); cerr << " i:" << search.gsa.get(data.list(~r)[ti]).can_be_deleted() << endl; }
// 	    }
// 	  }
  
  
  return didSomething;
}

bool BoundedVariableAddition::xorBVAhalf()
{
  xorTime = cpuTime() - xorTime ;
  
  // setup parameters
  const int replacePairs = 3;
  const int smallestSize = 3;
  
  // data structures
  bvaHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( data[  mkLit(v,false) ] >= replacePairs ) if( !bvaHeap.inHeap(toInt(mkLit(v,false))) )  bvaHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] >= replacePairs ) if( !bvaHeap.inHeap(toInt(mkLit(v,true))) )   bvaHeap.insert( toInt(mkLit(v,true))  );
  }
  data.ma.resize(2*data.nVars());
  
  vector<xorHalfPair> xorPairs;
  
  bool didSomething = false;
  
  while (bvaHeap.size() > 0 && (data.unlimited() || bvaXLimit > xorMatchChecks) && !data.isInterupted() ) {
    
    /** garbage collection */
    data.checkGarbage();
    
    const Lit right = toLit(bvaHeap[0]);
    assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    bvaHeap.removeMin();
    if( data.value( right ) != l_Undef ) continue;

    if( opt_bvaAnalysisDebug > 2 ) cerr << "c analysis on " << right << endl;
    for( uint32_t j = 0 ; j < data.list( right ).size() && !data.isInterupted(); ++j ) { // iterate over all candidates for C
      const Clause & c = ca[ data.list(right)[j] ] ;
      if( c.can_be_deleted() || c.size() < smallestSize ) continue;
    
      if( opt_bvaAnalysisDebug  > 3 ) cerr << "c work on clause " << c << endl;
      data.ma.nextStep();
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	data.ma.setCurrentStep( toInt( c[k] ) ); // mark all lits in C to check "C == D" fast
      }
      data.ma.reset( toInt(right) );
      
      Lit min = lit_Undef;
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	if( toInt(l1) <= toInt(right) ) continue; // only bigger literals!
	data.ma.reset( toInt(l1) );

	// here, look only for the interesting case for XOR!
	bool doesMatch = true;
	for( uint32_t m = 0 ; m < data.list( ~l1 ).size() && (data.unlimited() || bvaXLimit > xorMatchChecks) ; ++m ) {
	  if( data.list( ~l1 )[m] == data.list(right)[j] ) continue; // C != D
	  if( data.list(right)[j] > data.list( ~l1 )[m] ) continue; // find each case only once!

	  const Clause & d = ca[ data.list( ~l1 )[m] ] ;
	  xorMatchChecks ++;
	  if( d.can_be_deleted() || d.size() != c.size() ) continue; // |D| == |C|

	  doesMatch = true;	  
	  for( int r = 0 ; r < d.size(); ++ r ) {
	    const Lit dl = d[r];
	    if( var(dl) == var(l1) || var(dl) == var(right) ) continue;
	    if( ! data.ma.isCurrentStep  ( toInt(dl) ) ) { doesMatch = false; break; }
	  }
	  
	  if( !doesMatch ) continue; // check next candidate for D!
	  if( opt_bvaAnalysisDebug > 3 ) cerr << "c data.ma with clause " << d << endl;
	  // cerr << "c match for (" << right << "," << l1 << ") -- (" << ~right << "," << ~l1 << "): " << c << " and " << d << endl;
	  xorPairs.push_back( xorHalfPair(right,l1, data.list(right)[j], data.list( ~l1 )[m]) );
	  break; // do not try to find more clauses that match C!
	}

	data.ma.setCurrentStep( toInt(l1) ); // add back to match set!
	if( doesMatch ) break; // do not collect all pairs of this clause!
      }
      
    }
    // evaluate matches here!
    // sort based on second literal -- TODO: use improved sort!
    
    // sort( xorPairs.begin(), xorPairs.end() );
    if( xorPairs.size() > 20 ) 
      mergesort( &(xorPairs[0]), xorPairs.size());
    else {
      for( int i = 0 ; i < xorPairs.size(); ++ i ) {
	for( int j = i+1; j < xorPairs.size(); ++ j ) {
	  if ( xorPairs[i] > xorPairs[j] ) {
	    const xorHalfPair tmp =  xorPairs[i];
	    xorPairs[i] = xorPairs[j];
	    xorPairs[j] = tmp;
	  }
	}
      }
    }
    
    
    // check whether one literal matches multiple clauses
    int maxR = 0; int maxI = 0; int maxJ = 0;
    bool multipleMatches = false;
    for( int i = 0 ; i < xorPairs.size(); ++ i ) {
      int j = i;
      while ( j < xorPairs.size() && toInt(xorPairs[i].l2) == toInt(xorPairs[j].l2) ) ++j ;
      assert(j>=i);
      if( j - i >= replacePairs ) {
	int thisR = j-i;
	multipleMatches = maxR > 0; // set to true, if multiple matchings could be found
	if( thisR > maxR ) {
	  maxI = i; maxJ = j; maxR = thisR; 
	}
      }
      i = j - 1; // jump to next matching
    }

    if( maxR >= replacePairs ) {
    // apply rewriting for the biggest matching!
	  xorMaxPairs = xorMaxPairs > maxJ-maxI ? xorMaxPairs : maxJ - maxI ;
	  if( opt_bvaAnalysisDebug > 0) {
	    if( opt_bvaAnalysisDebug ) cerr << "c found XOR matching with " << maxJ-maxI << " pairs for (" << xorPairs[maxI].l1 << " -- " << xorPairs[maxI].l2 << ")" << endl;
	    if( opt_bvaAnalysisDebug > 1) {
	      for( int k = maxI; k < maxJ; ++ k ) cerr << "c p " << k - maxI << " : " << ca[ xorPairs[k].c1 ] << " and " << ca[ xorPairs[k].c2 ] << endl;
	    }
	  }
	  // TODO: check for implicit full gate
	  
	  // apply replacing/rewriting here (right,l1) -> (x); add clauses (-x,right,l1),(-x,-right,-l1)
	  const Var newX = nextVariable('x'); // done by procedure! bvaHeap.addNewElement();
	  if( opt_bvaAnalysisDebug ) cerr << "c introduce new variable " << newX + 1 << endl;
	  
	  didSomething = true;
	  for( int k = maxI; k < maxJ; ++ k ) {
	    ca[ xorPairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c XOR-BVA deleted " << ca[xorPairs[k].c2] << endl;
	    data.removedClause( xorPairs[k].c2 );
	    Clause& c = ca[ xorPairs[k].c1 ];
	    if( !ca[ xorPairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == xorPairs[k].l1 ) c[ci] = mkLit(newX,false);
	      else if (c[ci] == xorPairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA into " << c << endl;
	    data.removeClauseFrom(xorPairs[k].c1,xorPairs[k].l1);
	    data.removeClauseFrom(xorPairs[k].c1,xorPairs[k].l2);
	    data.removedLiteral(xorPairs[k].l1); data.removedLiteral(xorPairs[k].l2); data.addedLiteral(mkLit(newX,false));
	    data.list(mkLit(newX,false)).push_back( xorPairs[k].c1 );
	  }
	  // add new clauses
	  data.lits.clear();
	  data.lits.push_back( mkLit(newX,true) );
	  data.lits.push_back( xorPairs[maxI].l1 );
	  data.lits.push_back( xorPairs[maxI].l2 );
	  CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	  ca[tmpRef].sort();
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	  data.lits[1] = ~data.lits[1];data.lits[2] = ~data.lits[2];
	  tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	  ca[tmpRef].sort();
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	  
	  xorTotalReduction += (maxR - 2);
	  
	  // occurrences of l1 and l2 are only reduced; do not check!
	  if( opt_bva_push > 1 && data[mkLit(newX,false)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,false))) ) bvaHeap.insert( toInt(mkLit(newX,false)) );
	  }
	  // stats
	  xorfoundMatchings ++;
	  xorMatchSize += (maxJ-maxI);
	  // TODO: look for the other half of the gate definition!
    }
    
    if( multipleMatches ) xorMultiMatchings ++;
    if( opt_bva_push > 0 && multipleMatches && data[right] > replacePairs ) { // readd only, if we missed something!
      assert( !bvaHeap.inHeap( toInt(right)) && "literal representing right hand side has to be removed from heap already" ) ;
      if( !bvaHeap.inHeap( toInt(right)) ) bvaHeap.insert( toInt(right) );
    }
    
    if(opt_bvaAnalysisDebug && checkLists("XOR: check data structure integrity") ){
      assert( false && "integrity of data structures has to be ensured" ); 
    }
    
    xorPairs.clear();
  }

  xorTime = cpuTime() - xorTime;
  

  
  return didSomething;
}


bool BoundedVariableAddition::xorBVAfull()
{
  xorTime = cpuTime() - xorTime ;
  
  // setup parameters
  const int replacePairs = 5; // number of clauses to result in a reduction
  const int smallestSize = 3; // clause size
  
  // data structures
  bvaHeap.addNewElement(data.nVars() * 2); // keeps old values?! - drawback: cannot pre-filter
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( !bvaHeap.inHeap(toInt(mkLit(v,false))) )  bvaHeap.insert( toInt(mkLit(v,false)) );
    if( !bvaHeap.inHeap(toInt(mkLit(v,true))) )   bvaHeap.insert( toInt(mkLit(v,true))  );
  }
  data.ma.resize(2*data.nVars());
  
  vector<xorHalfPair> xorPairs;		// positive occurrences
  vector<xorHalfPair> nxorPairs;	// negative occurrences
  
  vector<int> nCount; // hit count per positive literal
  
  bool didSomething = false;
  
  while (bvaHeap.size() > 0 && (data.unlimited() || bvaXLimit > xorMatchChecks) && !data.isInterupted() ) {
    
    /** garbage collection */
    data.checkGarbage();
    
    const Lit right = toLit(bvaHeap[0]);
    assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    bvaHeap.removeMin();
    if( data.value( right ) != l_Undef ) continue;
    // TODO: how to do symmetry breaking; avoid redundant work?

    nCount.assign(data.nVars() * 2, 0 ); // reset literal counts!

    if( opt_bvaAnalysisDebug > 2 ) cerr << "c analysis on " << right << endl;
    for( uint32_t j = 0 ; j < data.list( right ).size() && !data.isInterupted() && (data.unlimited() || bvaXLimit > xorMatchChecks); ++j ) { // iterate over all candidates for C
      const Clause & c = ca[ data.list(right)[j] ] ;
      if( c.can_be_deleted() || c.size() < smallestSize ) continue;
    
      if( opt_bvaAnalysisDebug  > 3 ) cerr << "c work on clause " << c << endl;
      data.ma.nextStep();
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	data.ma.setCurrentStep( toInt( c[k] ) ); // mark all lits in C to check "C == D" fast
      }
      data.ma.reset( toInt(right) );
      
      Lit min = lit_Undef;
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	if( toInt(l1) <= toInt(right) ) continue; // only bigger literals!
	data.ma.reset( toInt(l1) );

	// here, look only for the interesting case for XOR!
	bool doesMatch = true;
	for( uint32_t m = 0 ; m < data.list( ~l1 ).size() && (data.unlimited() || bvaXLimit > xorMatchChecks) ; ++m ) {
	  if( data.list( ~l1 )[m] == data.list(right)[j] ) continue; // C != D
	  if( data.list(right)[j] > data.list( ~l1 )[m] ) continue; // find each case only once!

	  const Clause & d = ca[ data.list( ~l1 )[m] ] ;
	  xorMatchChecks ++;
	  if( d.can_be_deleted() || d.size() != c.size() ) continue; // |D| == |C|

	  doesMatch = true;	  
	  for( int r = 0 ; r < d.size(); ++ r ) {
	    const Lit dl = d[r];
	    if( var(dl) == var(l1) || var(dl) == var(right) ) continue;
	    if( ! data.ma.isCurrentStep  ( toInt(dl) ) ) { doesMatch = false; break; }
	  }
	  
	  if( !doesMatch ) continue; // check next candidate for D!
	  if( opt_bvaAnalysisDebug > 3 ) cerr << "c data.ma with clause " << d << endl;
	  // cerr << "c match for (" << right << "," << l1 << ") -- (" << ~right << "," << ~l1 << "): " << c << " and " << d << endl;
	  xorPairs.push_back( xorHalfPair(right,l1, data.list(right)[j], data.list( ~l1 )[m]) );
	  break; // do not try to find more clauses that match C!
	}

	data.ma.setCurrentStep( toInt(l1) ); // add back to match set!
	if( doesMatch ) break; // do not collect all pairs of this clause!
      }
      
    }
    // evaluate matches here!
    // sort based on second literal -- TODO: use improved sort!
    // sort( xorPairs.begin(), xorPairs.end() );
    if( xorPairs.size() > 20 ) 
      mergesort( &(xorPairs[0]), xorPairs.size());
    else {
      for( int i = 0 ; i < xorPairs.size(); ++ i ) {
	for( int j = i+1; j < xorPairs.size(); ++ j ) {
	  if ( xorPairs[i] > xorPairs[j] ) {
	    const xorHalfPair tmp =  xorPairs[i];
	    xorPairs[i] = xorPairs[j];
	    xorPairs[j] = tmp;
	  }
	}
      }
    }
    
    const Lit nRight = ~right;
    for( uint32_t j = 0 ; j < data.list( nRight ).size() && !data.isInterupted() && (data.unlimited() || bvaXLimit > xorMatchChecks); ++j ) { // iterate over all candidates for C
      const Clause & c = ca[ data.list(nRight)[j] ] ;
      if( c.can_be_deleted() || c.size() < smallestSize ) continue;
    
      if( opt_bvaAnalysisDebug  > 3 ) cerr << "c work on clause " << c << endl;
      data.ma.nextStep();
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	data.ma.setCurrentStep( toInt( c[k] ) ); // mark all lits in C to check "C == D" fast
      }
      data.ma.reset( toInt(nRight) );
      
      Lit min = lit_Undef;
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k];
	if( toInt(l1) <= toInt(nRight) ) continue; // only bigger literals!
	data.ma.reset( toInt(l1) );

	// here, look only for the interesting case for XOR!
	bool doesMatch = true;
	for( uint32_t m = 0 ; m < data.list( ~l1 ).size() && (data.unlimited() || bvaXLimit > xorMatchChecks) ; ++m ) {
	  if( data.list( ~l1 )[m] == data.list(nRight)[j] ) continue; // C != D
	  if( data.list(nRight)[j] > data.list( ~l1 )[m] ) continue; // find each case only once!

	  const Clause & d = ca[ data.list( ~l1 )[m] ] ;
	  xorMatchChecks ++;
	  if( d.can_be_deleted() || d.size() != c.size() ) continue; // |D| == |C|

	  doesMatch = true;	  
	  for( int r = 0 ; r < d.size(); ++ r ) {
	    const Lit dl = d[r];
	    if( var(dl) == var(l1) || var(dl) == var(nRight) ) continue;
	    if( ! data.ma.isCurrentStep  ( toInt(dl) ) ) { doesMatch = false; break; }
	  }
	  
	  if( !doesMatch ) continue; // check next candidate for D!
	  if( opt_bvaAnalysisDebug > 3 ) cerr << "c data.ma with clause " << d << endl;
	  // cerr << "c match for (" << nRight << "," << l1 << ") -- (" << ~nRight << "," << ~l1 << "): " << c << " and " << d << endl;
	  nxorPairs.push_back( xorHalfPair(nRight,l1, data.list(nRight)[j], data.list( ~l1 )[m]) );
	  break; // do not try to find more clauses that match C!
	}

	data.ma.setCurrentStep( toInt(l1) ); // add back to match set!
	if( doesMatch ) break; // do not collect all pairs of this clause!
      }
      
    }
    // evaluate matches here!
    // sort based on second literal -- TODO: use improved sort!
    for( int i = 0 ; i < nxorPairs.size(); ++ i ) {
      for( int j = i+1; j < nxorPairs.size(); ++ j ) {
	if ( toInt(nxorPairs[i].l2) > toInt(nxorPairs[j].l2) ) {
	  const xorHalfPair tmp =  nxorPairs[i];
	  nxorPairs[i] = nxorPairs[j];
	  nxorPairs[j] = tmp;
	}
      }
    }
    
    // generate negative counts!
    // positive: check whether one literal matches multiple clauses; get counters per literal l2!
    int nmaxR = 0; // to check whether this literal could be removed
    for( int i = 0 ; i < nxorPairs.size(); ++ i ) {
      int j = i;
      while ( j < nxorPairs.size() && toInt(nxorPairs[i].l2) == toInt(nxorPairs[j].l2) ) ++j ;
      assert(j>=i);
      nCount[ toInt(nxorPairs[i].l2) ] += (j-i); // store the value!
      nmaxR = nmaxR > (j-i) ? nmaxR : j-i;
      i = j - 1; // jump to next matching
    }
    
    // check whether one literal matches multiple clauses for positive
    int maxR = 0; int maxI = 0; int maxJ = 0;
    bool multipleMatches = false;
    for( int i = 0 ; i < xorPairs.size(); ++ i ) {
      int j = i;
      while ( j < xorPairs.size() && toInt(xorPairs[i].l2) == toInt(xorPairs[j].l2) ) ++j ;
      assert(j>=i);
      int thisR = j-i + nCount[ toInt(~xorPairs[i].l2) ]; // this matching plus the clauses of the opposite polarity!
      if( j - i >= replacePairs ) {
	multipleMatches = maxR > 0; // set to true, if multiple matchings could be found
	if( thisR > maxR ) {
	  maxI = i; maxJ = j; maxR = thisR; 
	}
      }
      i = j - 1; // jump to next matching
    }

    if( maxR >= replacePairs ) {
      
      // find bounds in negative list!
      int nmaxI = 0;
      const Lit l2 = xorPairs[maxI].l2;
      for( ; nmaxI < nxorPairs.size(); ++ nmaxI ) if( nxorPairs[nmaxI].l2 == ~l2 ) break;
      int nmaxJ = nmaxI + nCount[ toInt(~l2) ];
      
    // apply rewriting for the biggest matching!
	  xorMaxPairs = xorMaxPairs > maxJ-maxI + nCount[ toInt(~l2)] ? xorMaxPairs : maxJ - maxI  + nCount[ toInt(~l2) ];
	  if( opt_bvaAnalysisDebug > 0) {
	    cerr << "c found XOR matching with " << maxJ-maxI  << " pairs for (" << xorPairs[maxI].l1 << " -- " << xorPairs[maxI].l2 << ")" << endl;
	    if( opt_bvaAnalysisDebug > 1) {
	      for( int k = maxI; k < maxJ; ++ k ) cerr << "c p " << k - maxI << " : " << ca[ xorPairs[k].c1 ] << " and " << ca[ xorPairs[k].c2 ] << endl;
	    }
	    cerr << "c found XOR matching with " << nCount[ toInt(~l2)] << " pairs for (" << nxorPairs[nmaxI].l1 << " -- " << nxorPairs[nmaxI].l2 << ")" << endl;
	    if( opt_bvaAnalysisDebug > 1) {
	      for( int k = maxI; k < maxJ; ++ k ) cerr << "c p " << k - maxI << " : " << ca[ nxorPairs[k].c1 ] << " and " << ca[ nxorPairs[k].c2 ] << endl;
	    }
	  }
	  // apply replacing/rewriting here (right,l1) -> (x); add clauses (-x,right,l1),(-x,-right,-l1)
	  const Var newX = nextVariable('x'); // done by procedure! bvaHeap.addNewElement();
	  if( opt_bvaAnalysisDebug ) cerr << "c introduce new variable " << newX + 1 << endl;
	  
	  didSomething = true;
	  for( int k = maxI; k < maxJ; ++ k ) {
	    ca[ xorPairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c XOR-BVA deleted " << ca[xorPairs[k].c2] << endl;
	    data.removedClause( xorPairs[k].c2 );
	    Clause& c = ca[ xorPairs[k].c1 ];
	    if( !ca[ xorPairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == xorPairs[k].l1 ) c[ci] = mkLit(newX,false);
	      else if (c[ci] == xorPairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA into " << c << endl;
	    data.removeClauseFrom(xorPairs[k].c1,xorPairs[k].l1);
	    data.removeClauseFrom(xorPairs[k].c1,xorPairs[k].l2);
	    data.removedLiteral(xorPairs[k].l1); data.removedLiteral(xorPairs[k].l2); data.addedLiteral(mkLit(newX,false));
	    data.list(mkLit(newX,false)).push_back( xorPairs[k].c1 );
	  }
	  
	  for( int k = nmaxI; k < nmaxJ; ++ k ) {
	    ca[ nxorPairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c XOR-BVA deleted " << ca[nxorPairs[k].c2] << endl;
	    data.removedClause( nxorPairs[k].c2 );
	    Clause& c = ca[ nxorPairs[k].c1 ];
	    if( !ca[ nxorPairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == nxorPairs[k].l1 ) c[ci] = mkLit(newX,true);
	      else if (c[ci] == nxorPairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA into " << c << endl;
	    data.removeClauseFrom(nxorPairs[k].c1,nxorPairs[k].l1);
	    data.removeClauseFrom(nxorPairs[k].c1,nxorPairs[k].l2);
	    data.removedLiteral(nxorPairs[k].l1); data.removedLiteral(nxorPairs[k].l2); data.addedLiteral(mkLit(newX,true));
	    data.list(mkLit(newX,true)).push_back( nxorPairs[k].c1 );
	  }
	  
	  // add new clauses (only the ones needed!)
	  if( maxI < maxJ ) { // there are clauses for the positive to be introduced
	    data.lits.clear();
	    data.lits.push_back( mkLit(newX,true) );
	    data.lits.push_back( xorPairs[maxI].l1 );
	    data.lits.push_back( xorPairs[maxI].l2 );
	    CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	    data.lits[1] = ~data.lits[1];data.lits[2] = ~data.lits[2];
	    tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	  }
	  if( nCount[toInt(~l2)] > 0 ) { // there are clauses for the negative to be introduced
	    data.lits.clear();
	    data.lits.push_back( mkLit(newX,false) );
	    data.lits.push_back( nxorPairs[maxI].l1 );
	    data.lits.push_back( nxorPairs[maxI].l2 );
	    CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	    data.lits[1] = ~data.lits[1];data.lits[2] = ~data.lits[2];
	    tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c XOR-BVA added " << ca[tmpRef] << endl;
	  }	  
	  
	  // depending on which clauses have been created
	  xorTotalReduction += (maxR ) - (maxI < maxJ ? 2 : 0 ) - (nCount[toInt(~l2)] > 0 ? 2 : 0 );
	  
	  // occurrences of l1 and l2 are only reduced; do not check!
	  if( opt_bva_push > 1 && data[mkLit(newX,false)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,false))) ) bvaHeap.insert( toInt(mkLit(newX,false)) );
	  }
	  if( opt_bva_push > 1 && data[mkLit(newX,true)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,true))) ) bvaHeap.insert( toInt(mkLit(newX,true)) );
	  }
	  // stats
	  xorfoundMatchings ++;
	  xorMatchSize += (maxJ-maxI) + nCount[ toInt(~l2)];
    }
    
    if( multipleMatches ) xorMultiMatchings ++;
    
    if( opt_bva_push > 0 && multipleMatches && data[right] > replacePairs ) { // re-add only, if we missed something!
      assert( !bvaHeap.inHeap( toInt(right)) && "literal representing right hand side has to be removed from heap already" ) ;
      if( !bvaHeap.inHeap( toInt(right)) ) bvaHeap.insert( toInt(right) );
    }
    
    if(opt_bvaAnalysisDebug && checkLists("XOR: check data structure integrity") ){
      assert( false && "integrity of data structures has to be ensured" ); 
    }
    
    xorPairs.clear();
    nxorPairs.clear();
  }

  xorTime = cpuTime() - xorTime;
  
  // cerr << "c check when other literal can be removed!" << endl;

  
  return didSomething;
}


bool BoundedVariableAddition::iteBVAhalf()
{
  iteTime = cpuTime() - iteTime;
  
  // setup parameters
  const int replacePairs = 3;
  const int smallestSize = 3;
  
  bool didSomething = false;;
  
  // data structures
  bvaHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( data[  mkLit(v,false) ] >= replacePairs
      && data[  mkLit(v,true) ] >= replacePairs // add only, if both polarities occur frequently enough!
    ) {
      if( !bvaHeap.inHeap(toInt(mkLit(v,false))) ) bvaHeap.insert( toInt(mkLit(v,false)) );
      if( !bvaHeap.inHeap(toInt(mkLit(v,true))) ) bvaHeap.insert( toInt(mkLit(v,true))  );
    }
  }
  data.ma.resize(2*data.nVars());
  
  vector<iteHalfPair> itePairs;
  
  while (bvaHeap.size() > 0 && (data.unlimited() || bvaILimit > iteMatchChecks) && !data.isInterupted() ) {
    
    data.checkGarbage();
    
    const Lit right = toLit(bvaHeap[0]);
    assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    bvaHeap.removeMin();
    if( data.value( right ) != l_Undef ) continue;

    // x <-> ITE(s,t,f) in cls:
    // s  and  t ->  x  == -s,-t,x
    // -s and  f ->  x  == s,-f,x
    // s  and -t -> -x  == -s,t,-x
    // -s and -f -> -x  == s,f,-x
    
    // look for pairs, where right = s:
    // C :=  s,f,C'
    // D := -s,t,C'
    
    if( opt_bvaAnalysisDebug > 2 ) cerr << "c analysis on " << right << endl;
    for( uint32_t j = 0 ; j < data.list( right ).size() && !data.isInterupted(); ++j ) { // iterate over all candidates for C
      const Clause & c = ca[ data.list(right)[j] ] ;
      if( c.can_be_deleted() || c.size() < smallestSize ) continue;
    
      if( opt_bvaAnalysisDebug  > 3 ) cerr << "c work on clause " << c << endl;
      data.ma.nextStep();
      for( int k = 0 ; k < c.size(); ++ k ) {
	data.ma.setCurrentStep( toInt( c[k] ) ); // mark all lits in C to check "C == D" fast
      }
      data.ma.reset( toInt(right) );
      data.ma.setCurrentStep( toInt(~right) ); // array to be hit fully: C \setminus r \cup ~r, have exactly one miss!
      
      Lit min = lit_Undef;
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k]; 
	if( l1 == right ) continue;  // TODO can symmetry breaking be applied?
	
	data.ma.reset( toInt(l1) ); // should have all literals except right and l1, but at least ~right, and some other literal! remaining literals have to be equal!

	// here, look only for the interesting case for ITE!
	bool doesMatch = true;
	for( uint32_t m = 0 ; m < data.list( ~right ).size() && (data.unlimited() || bvaILimit > iteMatchChecks); ++m ) {
	  if( data.list( ~right )[m] == data.list(right)[j] ) continue; // C != D
	  if( data.list(right)[j] > data.list( ~right )[m] ) continue; // find each case only once!

	  const Clause & d = ca[ data.list( ~right )[m] ] ;
	  iteMatchChecks++;
	  if( d.can_be_deleted() || d.size() != c.size() ) continue; // |D| == |C|

	  doesMatch = true;	  
	  Lit matchLit = lit_Undef;
	  for( int r = 0 ; r < d.size(); ++ r ) {
	    const Lit dl = d[r];
	    if( dl == ~right ) continue; // first literal, which does not hit!
	    if( ! data.ma.isCurrentStep  ( toInt(dl) ) ) { 
	      if( matchLit == lit_Undef && var(dl) != var(l1)  ) matchLit = dl; // ensure that f and t have different variables
	      else { // only one literal is allowed to miss the hit
		doesMatch = false;
		break;
	      }
	    }
	  }
	  
	  if( !doesMatch ) continue; // check next candidate for D!
	  assert( matchLit != lit_Undef && "cannot have clause with duplicate literals, or tautological clause" );
	  if( opt_bvaAnalysisDebug > 3 ) cerr << "c match with clause " << d << endl;
	  // cerr << "c match for (" << right << "," << l1 << ") -- (" << ~right << "," << ~l1 << "): " << c << " and " << d << endl;
	  itePairs.push_back( iteHalfPair(right,l1,matchLit, data.list(right)[j], data.list( ~right )[m]) );
	  break; // do not try to find more clauses that match C on the selected literal!
	}

	data.ma.setCurrentStep( toInt(l1) );
	// for ITE, try to find all matches!
	
	//if( doesMatch ) break; // do not collect all pairs of this clause!
      }
      
    }
    // evaluate matches here!
    // sort based on second literal -- TODO: use improved sort!
    if( itePairs.size() > 20 ) 
      mergesort( &(itePairs[0]), itePairs.size());
    else {
      for( int i = 0 ; i < itePairs.size(); ++ i ) {
	for( int j = i+1; j < itePairs.size(); ++ j ) {
	  if ( itePairs[i] > itePairs[j]) {
	    const iteHalfPair tmp =  itePairs[i];
	    itePairs[i] = itePairs[j];
	    itePairs[j] = tmp;
	  }
	}
      }
    }
    
    // check whether one literal matches multiple clauses
    int maxR = 0; int maxI = 0; int maxJ = 0;
    bool multipleMatches = false;
    for( int i = 0 ; i < itePairs.size(); ++ i ) {
      int j = i;
      while ( j < itePairs.size() 
	&& ( // if both literals match // TODO: have a more symmetry-breaking one here? there are 4 kinds of ITEs that can be represented with 4 variables, two work on the same output literal, combine them!
	    ( toInt(itePairs[i].l2) == toInt(itePairs[j].l2 ) && toInt(itePairs[i].l3) == toInt(itePairs[j].l3 ) )
	)
      ) ++j ;
      assert(j>=i);
      if( j - i  >= replacePairs ) {
	int thisR = j-i ;
	multipleMatches = maxR > 0; // set to true, if multiple matchings could be found
	if( thisR > maxR ) {
	  maxI = i; maxJ = j; maxR = thisR; 
	}
      }
      i = j - 1; // jump to next matching
    }

    if( maxR >= replacePairs ) {
    // apply rewriting for the biggest matching!
	  iteMaxPairs = iteMaxPairs > maxJ-maxI ? iteMaxPairs : maxJ - maxI ;

	  // TODO: check for implicit full gate

	  // apply replacing/rewriting here (right,l1) -> (x); add clauses (-x,right,l1),(-x,-right,-l1)
	  const Var newX = nextVariable('i'); // done by procedure! bvaHeap.addNewElement();
	  if( opt_bvaAnalysisDebug ) cerr << "c introduce new variable " << newX + 1 << endl;
	  
	  for( int k = maxI; k < maxJ; ++ k ) {
	    ca[ itePairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c ITE-BVA deleted " << ca[itePairs[k].c2] << endl;
	    data.removedClause( itePairs[k].c2 );
	    Clause& c = ca[ itePairs[k].c1 ];
	    if( !ca[ itePairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == itePairs[k].l1 ) c[ci] = mkLit(newX,false);
	      else if (c[ci] == itePairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA into " << c << endl;
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l1);
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l2);
	    data.removedLiteral(itePairs[k].l1); data.removedLiteral(itePairs[k].l2); data.addedLiteral(mkLit(newX,false));
	    data.list(mkLit(newX,false)).push_back( itePairs[k].c1 );
	  }
	  // add new clauses
	  data.lits.clear();
	  data.lits.push_back( mkLit(newX,true) );
	  data.lits.push_back( itePairs[maxI].l1 );
	  data.lits.push_back( itePairs[maxI].l2 );
	  CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	  ca[tmpRef].sort();
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	  data.lits[1] = ~data.lits[1];data.lits[2] = itePairs[maxI].l3;
	  tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	  ca[tmpRef].sort();
	  data.addClause( tmpRef );
	  data.getClauses().push( tmpRef );
	  if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	  
	  if( opt_bva_push > 1 && data[mkLit(newX,false)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,false))) ) bvaHeap.insert( toInt(mkLit(newX,false)) );
	  }
	  // stats
	  didSomething = true;
	  iteFoundMatchings ++;
	  iteMatchSize += (maxJ-maxI);
	  iteTotalReduction += (maxR - 2);
	  // TODO: look for the other half of the gate definition!
    }
    
    if( multipleMatches ) iteMultiMatchings ++;
    if( ( opt_bva_push > 0 && multipleMatches ) && data[right] > replacePairs ) { // readd only, if something might have been missed!
      assert( !bvaHeap.inHeap( toInt(right)) && "literal representing right hand side has to be removed from heap already" ) ;
      if( !bvaHeap.inHeap( toInt(right)) ) bvaHeap.insert( toInt(right) );
    }
    
    if(opt_bvaAnalysisDebug && checkLists("ITE: check data structure integrity") ) {
      assert( false && "integrity of data structures has to be ensured" ); 
    }
    
    itePairs.clear();
  }
  
  iteTime = cpuTime() - iteTime;
  return didSomething;
}


bool BoundedVariableAddition::iteBVAfull()
{
  iteTime = cpuTime() - iteTime;
  
  // setup parameters
  const int replacePairs = 5; // number of clauses
  const int smallestSize = 3; // clause size
  
  bool didSomething = false;;
  
  // cerr << "c full ITE bva" << endl;
  
  // data structures
  bvaHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    { // TODO: have some way of filtering?
      if( !bvaHeap.inHeap(toInt(mkLit(v,false))) ) bvaHeap.insert( toInt(mkLit(v,false)) );
      if( !bvaHeap.inHeap(toInt(mkLit(v,true))) ) bvaHeap.insert( toInt(mkLit(v,true))  );
    }
  }
  data.ma.resize(2*data.nVars());
  
  vector<iteHalfPair> itePairs;
  
  while (bvaHeap.size() > 0 && (data.unlimited() || bvaILimit > iteMatchChecks) && !data.isInterupted() ) {
    
    data.checkGarbage();
    
    const Lit right = toLit(bvaHeap[0]);
    assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    bvaHeap.removeMin();
    if( data.value( right ) != l_Undef ) continue;

    // x <-> ITE(s,t,f) in cls:
    // s  and  t ->  x  == -s,-t,x
    // -s and  f ->  x  == s,-f,x
    // s  and -t -> -x  == -s,t,-x
    // -s and -f -> -x  == s,f,-x
    
    // look for pairs, where right = s:
    // C :=  s,f,C'
    // D := -s,t,C'
    
    if( opt_bvaAnalysisDebug > 2 ) cerr << "c analysis on " << right << endl;
    for( uint32_t j = 0 ; j < data.list( right ).size() && !data.isInterupted(); ++j ) { // iterate over all candidates for C
      const Clause & c = ca[ data.list(right)[j] ] ;
      if( c.can_be_deleted() || c.size() < smallestSize ) continue;
    
      if( opt_bvaAnalysisDebug  > 3 ) cerr << "c work on clause " << c << endl;
      data.ma.nextStep();
      for( int k = 0 ; k < c.size(); ++ k ) {
	data.ma.setCurrentStep( toInt( c[k] ) ); // mark all lits in C to check "C == D" fast
      }
      data.ma.reset( toInt(right) );
      data.ma.setCurrentStep( toInt(~right) ); // array to be hit fully: C \setminus r \cup ~r, have exactly one miss!
      
      Lit min = lit_Undef;
      for( int k = 0 ; k < c.size(); ++ k ) {
	const Lit l1 = c[k]; 
	if( l1 == right ) continue;  // TODO can symmetry breaking be applied?
	
	data.ma.reset( toInt(l1) ); // should have all literals except right and l1, but at least ~right, and some other literal! remaining literals have to be equal!

	// here, look only for the interesting case for ITE!
	bool doesMatch = true;
	for( uint32_t m = 0 ; m < data.list( ~right ).size() && (data.unlimited() || bvaILimit > iteMatchChecks); ++m ) {
	  if( data.list( ~right )[m] == data.list(right)[j] ) continue; // C != D
	  if( data.list(right)[j] > data.list( ~right )[m] ) continue; // symmetry breaking: find each case only once!

	  const Clause & d = ca[ data.list( ~right )[m] ] ;
	  iteMatchChecks++;
	  if( d.can_be_deleted() || d.size() != c.size() ) continue; // |D| == |C|

	  doesMatch = true;	  
	  Lit matchLit = lit_Undef;
	  for( int r = 0 ; r < d.size(); ++ r ) {
	    const Lit dl = d[r];
	    if( dl == ~right ) continue; // first literal, which does not hit!
	    if( ! data.ma.isCurrentStep  ( toInt(dl) ) ) { 
	      if( matchLit == lit_Undef && var(dl) != var(l1)  ) matchLit = dl; // ensure that f and t have different variables
	      else { // only one literal is allowed to miss the hit
		doesMatch = false;
		break;
	      }
	    }
	  }
	  
	  if( !doesMatch ) continue; // check next candidate for D!
	  assert( matchLit != lit_Undef && "cannot have clause with duplicate literals, or tautological clause" );
	  if( opt_bvaAnalysisDebug > 3 ) cerr << "c match with clause " << d << endl;
	  // cerr << "c match for (" << right << "," << l1 << ") -- (" << ~right << "," << ~l1 << "): " << c << " and " << d << endl;
	  itePairs.push_back( iteHalfPair(right,l1,matchLit, data.list(right)[j], data.list( ~right )[m]) );
	  break; // do not try to find more clauses that match C on the selected literal!
	}

	data.ma.setCurrentStep( toInt(l1) );
	// for ITE, try to find all matches!
	
	//if( doesMatch ) break; // do not collect all pairs of this clause!
      }
      
    }
    // evaluate matches here!
    // sort based on second literal -- TODO: use improved sort!
    if( itePairs.size() > 20 ) 
      mergesort( &(itePairs[0]), itePairs.size());
    else {
      for( int i = 0 ; i < itePairs.size(); ++ i ) {
	for( int j = i+1; j < itePairs.size(); ++ j ) {
	  if ( itePairs[i] > itePairs[j]) {
	    const iteHalfPair tmp =  itePairs[i];
	    itePairs[i] = itePairs[j];
	    itePairs[j] = tmp;
	  }
	}
      }
    }
    
    /*
    cerr << "c final matches: " << endl;
    for( int i = 0 ; i < itePairs.size(); ++ i ) {
      cerr << "c ITE[" << i << "] s=" << itePairs[i].l1 << " f=" << itePairs[i].l2 << " t=" << itePairs[i].l3 << endl; ;
    }
    */
    // check whether one literal matches multiple clauses
    int maxR = 0; int maxI = 0; int maxJ = 0; int maxK = 0;
    bool multipleMatches = false;
    for( int i = 0 ; i < itePairs.size(); ++ i ) {
      int j = i;
      while ( j < itePairs.size() 
	&& ( // if both literals match // TODO: have a more symmetry-breaking one here? there are 4 kinds of ITEs that can be represented with 4 variables, two work on the same output literal, combine them!
	    ( toInt(itePairs[i].l2) == toInt(itePairs[j].l2 ) && toInt(itePairs[i].l3) == toInt(itePairs[j].l3 ) )
	)
      ) ++j ; // j exatly points behind the last hitting tuple
      assert(j>=i);

	// if( j + 1 < itePairs.size() ) cerr << "c following ITE s=" << itePairs[j+1].l1 << " f=" << itePairs[j+1].l2 << " t=" << itePairs[j+1].l3 << endl;
	int k = j;
	if( j + 1 < itePairs.size() ) { // check for other half of the gate!
	  while ( k < itePairs.size() 
	    && ( // if both literals match // TODO: have a more symmetry-breaking one here? there are 4 kinds of ITEs that can be represented with 4 variables, two work on the same output literal, combine them!
		( toInt(itePairs[i].l2) == toInt(~ itePairs[k].l2 ) && toInt(itePairs[i].l3) == toInt(~ itePairs[k].l3 ) )
	    )
	  ) k++;
	}
	
      if( k - i  >= replacePairs ) {
	multipleMatches = maxR > 0; // set to true, if multiple matchings could be found
	int thisR = k-i ;
	// cerr << "c found replaceable (" << thisR << ") ITE s=" << itePairs[j-1].l1 << " f=" << itePairs[j-1].l2 << " t=" << itePairs[j-1].l3 << endl;
	if( thisR > maxR ) {
	  maxI = i; maxJ = j; maxK = k; maxR = thisR; 
	}
	j = k;
      }
      i = j - 1; // jump to next matching
    }

    if( maxR >= replacePairs ) {
    // apply rewriting for the biggest matching!
	  iteMaxPairs = iteMaxPairs > maxJ-maxI ? iteMaxPairs : maxJ - maxI ;

	  // TODO: check for implicit full gate

	  // apply replacing/rewriting here (right,l1) -> (x); add clauses (-x,right,l1),(-x,-right,-l1)
	  const Var newX = nextVariable('i'); // done by procedure! bvaHeap.addNewElement();
	  if( opt_bvaAnalysisDebug ) cerr << "c introduce new variable " << newX + 1 << endl;
	  
	  for( int k = maxI; k < maxJ; ++ k ) {
	    ca[ itePairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c ITE-BVA deleted " << ca[itePairs[k].c2] << endl;
	    data.removedClause( itePairs[k].c2 );
	    Clause& c = ca[ itePairs[k].c1 ];
	    if( !ca[ itePairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == itePairs[k].l1 ) c[ci] = mkLit(newX,false);
	      else if (c[ci] == itePairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA into " << c << endl;
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l1);
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l2);
	    data.removedLiteral(itePairs[k].l1); data.removedLiteral(itePairs[k].l2); data.addedLiteral(mkLit(newX,false));
	    data.list(mkLit(newX,false)).push_back( itePairs[k].c1 );
	  }
	  for( int k = maxJ; k < maxK; ++ k ) {
	    ca[ itePairs[k].c2 ].set_delete(true); // all second clauses will be deleted, all first clauses will be rewritten
	    if( opt_bvaAnalysisDebug ) cerr  << "c ITE-BVA deleted " << ca[itePairs[k].c2] << endl;
	    data.removedClause( itePairs[k].c2 );
	    Clause& c = ca[ itePairs[k].c1 ];
	    if( !ca[ itePairs[k].c2 ].learnt() && c.learnt() ) c.set_learnt(false); // during inprocessing, do not remove other important clauses!
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA rewrite " << c << endl;
	    for( int ci = 0 ; ci < c.size(); ++ ci ) { // rewrite clause
	      if( c[ci] == itePairs[k].l1 ) c[ci] = mkLit(newX,true);
	      else if (c[ci] == itePairs[k].l2) {
		c.removePositionSorted(ci); 
		ci --;
	      }
	    }
	    c.sort();
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA into " << c << endl;
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l1);
	    data.removeClauseFrom(itePairs[k].c1,itePairs[k].l2);
	    data.removedLiteral(itePairs[k].l1); data.removedLiteral(itePairs[k].l2); data.addedLiteral(mkLit(newX,true));
	    data.list(mkLit(newX,true)).push_back( itePairs[k].c1 );
	  }	  
	  
	  // add new clauses
	  if( maxI < maxJ ) { // should always be the case
	    data.lits.clear();
	    data.lits.push_back( mkLit(newX,true) );
	    data.lits.push_back( itePairs[maxI].l1 );
	    data.lits.push_back( itePairs[maxI].l2 );
	    CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	    data.lits[1] = ~data.lits[1];data.lits[2] = itePairs[maxI].l3;
	    tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	  }
	  if( maxJ < maxK ) { // should always be the case
	    data.lits.clear();
	    data.lits.push_back( mkLit(newX,false) );
	    data.lits.push_back( itePairs[maxJ].l1 );
	    data.lits.push_back( itePairs[maxJ].l2 );
	    CRef tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	    data.lits[1] = ~data.lits[1];data.lits[2] = itePairs[maxJ].l3;
	    tmpRef = ca.alloc(data.lits, false); // no learnt clause!
	    ca[tmpRef].sort();
	    data.addClause( tmpRef );
	    data.getClauses().push( tmpRef );
	    if( opt_bvaAnalysisDebug ) cerr << "c ITE-BVA added " << ca[tmpRef] << endl;
	  }
	  
	  if( opt_bva_push > 1 && data[mkLit(newX,false)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,false))) ) bvaHeap.insert( toInt(mkLit(newX,false)) );
	  }
	  if( opt_bva_push > 1 && data[mkLit(newX,true)] > replacePairs ) {
	    if( !bvaHeap.inHeap( toInt(mkLit(newX,true))) ) bvaHeap.insert( toInt(mkLit(newX,true)) );
	  }
	  
	  // stats
	  didSomething = true;
	  iteFoundMatchings ++;
	  iteMatchSize += (maxK-maxI);
	  iteTotalReduction += (maxR ) - (maxJ > maxI ? 2 : 0 )  - (maxK > maxJ ? 2 : 0 ) ;
	  // TODO: look for the other half of the gate definition!
    }
    
    if( multipleMatches ) iteMultiMatchings ++;
    if( ( opt_bva_push > 0 && multipleMatches ) && data[right] > replacePairs ) { // readd only, if something might have been missed!
      assert( !bvaHeap.inHeap( toInt(right)) && "literal representing right hand side has to be removed from heap already" ) ;
      if( !bvaHeap.inHeap( toInt(right)) ) bvaHeap.insert( toInt(right) );
    }
    
    if(opt_bvaAnalysisDebug && checkLists("ITE: check data structure integrity") ) {
      assert( false && "integrity of data structures has to be ensured" ); 
    }
    
    itePairs.clear();
  }
  
  iteTime = cpuTime() - iteTime;
  return didSomething;
}


bool BoundedVariableAddition::bvaHandleComplement( const Lit right ) {
  data.clss.clear();
  const Lit left = ~right;
  for( uint32_t i = 0 ; i < data.list(right).size(); ++i )
  {
    const CRef C = data.list(right)[i];
    Clause& clauseC = ca[ C ];
    if( clauseC.can_be_deleted() ) continue;
    if( bva_debug > 2 && clauseC.size() < 2 ) cerr << "c [BVA] unit clause in complements" << endl;
    if( clauseC.size() < 2 ) continue;
    data.ma.nextStep();
    for( uint32_t j = 0; j < clauseC.size(); j ++ ) {
      if( clauseC[j] == right ) continue;
      data.ma.setCurrentStep( toInt(clauseC[j]));
    }
    // so far, left and right cannot be in the same clause for matching
    if ( data.ma.isCurrentStep( toInt(left) ) ) continue;
    data.ma.reset( toInt(right) );
    data.ma.setCurrentStep( toInt(left) );
    
    for( uint32_t j = 0 ; j < data.list(left).size(); ++j ) {
      CRef D = data.list(left)[j];
      Clause& clauseD = ca[ D ];
      if( clauseD.can_be_deleted() ) continue;
      if( clauseD.size() != clauseC.size() ) continue; // TODO: relax later!
      
     if( bva_debug > 1 ) { cerr << "c [CP2-BVAC] check clause " << j << " of literal " << left << ": " << ca[D] << endl; }
      for( uint32_t k = 0 ; k < clauseD.size() ; ++ k ){
	if( !data.ma.isCurrentStep( toInt(clauseD[k])) ){
	  goto nextD2;
	}
      }
      // remember to remove literal "right" from theses clauses
      data.clss.push_back(C);

      // remove D completely
      clauseD.set_delete(true);
      data.removedClause( D );
      
      // remove C from list
      data.removeClauseFrom( C, right, i);
      --i;
      
      break;
      
      nextD2:;
    }
    data.ma.reset( toInt(left) );
  }

  // remove literal right from clauses
  for( uint32_t i = 0; i < data.clss.size(); ++i ) {
    Clause& clause = ca[data.clss[i]];
    if( bva_debug > 1 ) { cerr << "c [CP2-BVAC] remove " << right << " from (" << i << "/"<< data.clss.size() << ")" << ca[data.clss[i]] << endl; }
    clause.remove_lit( right );
    data.removedLiteral( right );
    data.updateClauseAfterDelLit( clause );
    if( clause.size() == 1 ) {
      if( l_False == data.enqueue(clause[0] ) ) {
	return false;
      }
      clause.set_delete(true);
    }
  }
  
  if( bvaPush > 0 ) bvaHeap.insert( toInt(right) );
  return true;
}

Var BoundedVariableAddition::nextVariable(char type)
{
  Var nextVar = data.nextFreshVariable(type);
  
  // enlarge own structures
  bvaHeap.addNewElement(data.nVars());

  bvaCountMark .resize( data.nVars() * 2, lit_Undef);
  bvaCountCount.resize( data.nVars() * 2);
  bvaCountSize .resize( data.nVars() * 2);
  
  if( bva_debug > 3 ) cerr << "c enlarge count mark,size and count to " << bvaCountMark.size() << endl;
  
  return nextVar;
}


void BoundedVariableAddition::printStatistics(ostream& stream)
{
stream << "c [STAT] BVA " << processTime << "s, "
<< andTotalReduction + xorTotalReduction + iteTotalReduction << " reducedClauses, "
<< andReplacements + xorfoundMatchings + iteFoundMatchings << " freshVariables, "
<< endl;

stream << "c [STAT] AND-BVA "
<< andTime << " seconds, "
<< andReplacements << " freshVars, "
<< andTotalReduction << " reducedClauses, "
<< andDuplicates << " duplicateClauses, "
<< andComplementCount << " complLits, "
<< andReplacedOrs << " orGSubs, "
<< andReplacedMultipleOrs << " multiOGS, "
<< andMatchChecks << " checks, "
<< endl;

// print only if enabled!
if( opt_Xbva )
stream << "c [STAT] XOR-BVA " << xorTime << " seconds, " << xorfoundMatchings << " matchings, " << xorMultiMatchings << " multis, " << xorTotalReduction << " reducedClauses, " 
       << xorMatchSize << " matchSize, " << xorMaxPairs << " maxPair, " <<  xorMatchChecks << " checks, "  << endl;

if( opt_Ibva )
stream << "c [STAT] ITE-BVA " << iteTime << " seconds, " << iteFoundMatchings << " matchings, " << iteMultiMatchings << " multis, " << iteTotalReduction << " reducedClauses, " 
       << iteMatchSize << " matchSize, " << iteMaxPairs << " maxPair, " << iteMatchChecks << " checks, " << endl;
}


bool BoundedVariableAddition::variableAddtionMulti(bool sort)
{
  assert( false && "This method is not implemented yet" );
  return true;
}


bool BoundedVariableAddition::checkLists(const string& headline)
{
  bool ret = false;
  cerr << "c check data structures: " << headline << " ... " << endl;
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    for( int p = 0 ; p < 2; ++ p ) {
      const Lit l = p == 0 ? mkLit(v,false) : mkLit(v,true);
      for( int i = 0 ; i < data.list(l).size(); ++ i ) {
	for( int j = i+1 ; j < data.list(l).size(); ++ j ) {
	  if( data.list(l)[i] == data.list(l)[j] ) {
	    ret = true;
	    cerr << "c duplicate " << data.list(l)[j] << " for lit " << l << " at " << i << " and " << j << " out of " << data.list(l).size() << endl;
	  }
	}
      }
    }
  }
  return ret;
}

void BoundedVariableAddition::destroy()
{
  bvaHeap.clear(true);
  vector< vector< CRef > >().swap( bvaMatchingClauses); 
  vector< Lit >().swap( bvaMatchingLiterals); 
  // use general mark array!
  vector< Lit >().swap( bvaCountMark);	
  vector< uint32_t >().swap( bvaCountCount);
  vector< uint64_t >().swap( bvaCountSize );
  clauseLits.clear(true) ;
}


void BoundedVariableAddition::removeDuplicateClauses( const Lit literal )
{
  for( uint32_t i = 0 ; i < data.list(literal).size() ; ++ i ) 
  {
    Clause& clause = ca[ data.list(literal)[i]];
    if( clause.can_be_deleted() ) { 
      data.list(literal)[i] = data.list(literal)[ data.list(literal).size() -1 ];
      data.list(literal).pop_back();
      --i; continue;
    }
  }
  
  for( uint32_t i = 0 ; i < data.list(literal).size() ; ++ i ) 
  {
    Clause& clauseI = ca[( data.list(literal)[i] )];
    for( uint32_t j = i+1 ; j < data.list(literal).size() ; ++ j ) 
    {
      CRef removeCandidate = data.list(literal)[j];
      Clause& clauseJ = ca[( removeCandidate )];
      if( clauseI.size() != clauseJ.size() ) continue;
      for( uint32_t k = 0 ; k < clauseI.size(); ++k ) {
	if( clauseI[k] != clauseJ[k] ) goto duplicateNextJ;
      }
      data.removedClause( removeCandidate );
      data.list(literal)[j] = data.list(literal)[ data.list(literal).size() -1 ];
      data.list(literal).pop_back();
      j--;
      duplicateNextJ:;
    }
  }
}