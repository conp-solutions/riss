/******************************************************************************************[BVA.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Bva.h"

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
static IntOption  opt_bva_limit            (_cat, "cp3_bva_limit",   "number of steps allowed for BVA", 20000000, IntRange(0, INT32_MAX));

static BoolOption opt_bvaComplement         (_cat, "cp3_bva_compl",   "treat complementary literals special", true);
static BoolOption opt_bvaRemoveDubplicates (_cat, "cp3_bva_dupli",   "remove duplicate clauses", true);
static BoolOption opt_bvaSubstituteOr      (_cat, "cp3_bva_subOr",   "try to also substitus disjunctions", false);

static IntOption  bva_debug                (_cat, "bva-debug",       "Debug Output of BVA", 0, IntRange(0, 4));
	
BoundedVariableAddition::BoundedVariableAddition(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data)
: Technique( _ca, _controller )
, data( _data )
, doSort( true )
, duplicates(0)
, complementCount(0)
, replacements(0)
, totalReduction(0)
, replacedOrs(0)
, replacedMultipleOrs(0)
, processTime(0)
, bvaHeap( data )
, bvaComplement(opt_bvaComplement)
, bvaPush(opt_bva_push)
, bvaLimit(opt_bva_limit)
, bvaRemoveDubplicates(opt_bvaRemoveDubplicates)
, bvaSubstituteOr(opt_bvaSubstituteOr)
{

}

bool BoundedVariableAddition::variableAddtion(bool _sort) {
  bool didSomething=false;

  MethodTimer processTimer( &processTime );
  doSort = _sort;

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
	while (bvaHeap.size() > 0 && (data.unlimited() || bvaLimit > 0) ) {
	  
	  if( bva_debug > 1 ) cerr << "c next major loop iteration with heapSize " << bvaHeap.size() << endl;
	  
	  if( bva_debug > 2 )
	    if( checkLists("check data structure integrity") )
	      assert( false && "there cannot be duplicate clause entries in clause lists!" );
	  
	  // interupted ?
	  if( data.isInterupted() ) break;
	  
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
	  for( uint32_t i = 0 ; i <  bvaMatchingClauses[0].size(); ++i )
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
	      bvaLimit = (bvaLimit > 0 ) ? bvaLimit - 1 : 0;
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
	      
	      if( match == lit_Undef ) { duplicates++; continue; } // found a matching clause
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
	  
	  if( left == lit_Undef ) break;
	  
	  if( bva_debug > 2 ) cerr << "c BVA selected left: " << left << endl;
	  // tackle complement specially, if found
	  if( bvaComplement && foundRightComplement ) left = ~right;
	  
	  // found clause(s) [right, l,...], [-right,l,...]
	  if( left == ~right && bvaComplement ) {
	    didSomething = true;
	    if( bva_debug > 1 ) cerr << "c [BVA] found complement " << left << endl;
	    if( !bvaHandleComplement( right ) ) {
	      data.setFailed();
	      return didSomething;
	    }
	    complementCount ++;
	    if( !bvaHeap.inHeap( toInt(right) ) ) bvaHeap.insert( toInt(right) ) ;
	    else bvaHeap.update( toInt(right) );
	    if( !bvaHeap.inHeap( toInt(left) ) ) bvaHeap.insert( toInt(left) ) ;
	    else bvaHeap.update( toInt(left) );
	    // if complement has been found, this complement is treated and the next literal is picked
	    if( bva_debug > 1 && bvaHeap.size() > 0 ) cerr << "c [BVA] heap(" << bvaHeap.size() << ") with at 0: " << toLit(bvaHeap[0]) << endl;
	    break;
	  }
	  
	  // do not continue, if there won't be a reduction
	  // dows not hold in the multi case!!
	  if( index == 1 && max < 3 ) {
	    if( bva_debug > 2 ) cerr << "c [BVA] interrupt because of too few matchings " << right << " @" << index << " with " << max << endl;
	    break;
	  }
	  if( bva_debug > 0 ) cerr << "c [BVA] index=" << index << " max=" << max << " rightList= " << data.list(right).size() << " leftList= " << data.list(left).size() << endl;

	  // heuristically remove duplicates!
	  if( max > data.list(left).size() || max > data.list(right).size() ) {
	    if( bva_debug > 1 ) cerr << "c remove duplicate clauses from left list " << left << " with index= " << index << endl;
	    uint32_t os = data.list(left).size();
	    if( bva_debug > 1 ) {
	      cerr << "c reduced list by " << os - data.list(left).size() << " to " << data.list(left).size() << endl;
	      for( uint32_t j = 0 ; j<data.list(left).size(); ++j ) { cerr <<  data.list(left)[j] << endl;}
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
	  const Var newX = nextVariable();
	  replacements++; totalReduction += currentReduction;
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
		  clauseI.set_delete(true);
		}
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
		replacedOrs ++;
		replacedMultipleOrs = (isNotFirst ? replacedMultipleOrs + 1 : replacedMultipleOrs );
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

Var BoundedVariableAddition::nextVariable()
{
  Var nextVar = data.nextFreshVariable();
  
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
stream << "c [STAT] BVA " 
<< processTime << "s, "
<< replacements << " freshVars, "
<< totalReduction << " reducedClauses, "
<< duplicates << " duplicateClauses, "
<< complementCount << " complLits, "
<< replacedOrs << " orGSubs, "
<< replacedMultipleOrs << " multiOGS, "
<< endl;
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