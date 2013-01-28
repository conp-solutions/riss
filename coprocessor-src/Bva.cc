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


const int debug = 0;
	
BoundedVariableAddition::BoundedVariableAddition(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data)
: Technique( _ca, _controller )
, data( _data )
, doSort( true )
, bvaHeap( data )
, bvaComplement(true)
, bvaPush(2)
, bvaLimit(20000000)
, bvaRemoveDubplicates(true)
, bvaSubstituteOr(false)
{

}

bool BoundedVariableAddition::variableAddtion(bool firstCall) {
  bool didSomething=false;
  
	vector<Lit> stack;
	
	uint32_t duplicates = 0;
	uint32_t complementCount = 0;
	int32_t currentReduction = 0;
	
	bool addedNewAndGate = false;
	
	// for l in F
	while (bvaHeap.size() > 0 && (data.unlimited() || bvaLimit > 0) ) {
	  // interupted ?
	  if( data.isInterupted() ) break;
	  
	  const Lit right = toLit(bvaHeap[0]);
	  assert( bvaHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

	  bvaHeap.removeMin();
	  if( debug > 2 && bvaHeap.size() > 0 ) cerr << "c [BVA] new first item: " << bvaHeap[0] << endl;
	  
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
	  
	  if( debug > 2 ) cerr << "c BVA test right: " << right << endl;
	  // while literals can be found that increase the reduction:
	  while( true )
	  {
	  
	  stack.clear();
	  index ++; // count column of literal that will be added
	  
	  // create the stack with all matching literals
	  for( uint32_t i = 0 ; i <  bvaMatchingClauses[0].size(); ++i )
	  {
	    // reserve space
	    while ( bvaMatchingClauses.size() < index+1 ) bvaMatchingClauses.push_back( vector<CRef>() );
	    
	    // continue if clause is empty
	    CRef C =  bvaMatchingClauses[0][i];
	    if( C == 0 ) continue;
	    Clause& clauseC = ca[ C];
	    if( clauseC.can_be_deleted() ) continue;
	    // no unit clauses!
	    if( clauseC.size() == 1 ) continue;
	    data.ma.nextStep();	// mark current step
	    
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
	      
	      if( match == lit_Undef ) { duplicates++; continue; } // found a matching clause
	      assert ( match != right && "matching literal is the right literal");
	
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
	  if( debug > 2 ) cerr << "c BVA selected left: " << left << endl;
	  // tackle complement specially, if found
	  if( bvaComplement && foundRightComplement ) left = ~right;
	  
	  // found clause(s) [right, l,...], [-right,l,...]
	  if( left == ~right && bvaComplement ) {
	    didSomething = true;
	    if( debug > 1 ) cerr << "c [BVA] found complement " << left << endl;
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
	    if( debug > 1 && bvaHeap.size() > 0 ) cerr << "c [BVA] heap(" << bvaHeap.size() << ") with at 0: " << bvaHeap[0] << endl;
	    break;
	  }
	  
	  // do not continue, if there won't be a reduction
	  // dows not hold in the multi case!!
	  if( index == 1 && max < 3 ) {
	    if( debug > 2 ) cerr << "c [BVA] interrupt because of too few matchings" << right << " @" << index << " with " << max << endl;
	    break;
	  }
	  if( debug > 2 ) cerr << "c [BVA] index=" << index << " max=" << max << " rightList= " << data.list(right).size() << " leftList= " << data.list(left).size() << endl;

	  // heuristically remove duplicates!
	  if( max > data.list(left).size() || max > data.list(right).size() ) {
	    if( debug > 1 ) cerr << "c remove duplicate clauses from left list " << left << " with index= " << index << endl;
	    uint32_t os = data.list(left).size();
	    if( debug > 1 ) {
	      cerr << "c reduced list by " << os - data.list(left).size() << " to " << data.list(left).size() << endl;
	      for( uint32_t j = 0 ; j<data.list(left).size(); ++j ) { cerr <<  data.list(left)[j] << endl;}
	    }
	    index --; continue; // redo the current literal!
	  }
	  
	  // calculate new reduction and compare!
	  int32_t newReduction = ( (int)(bvaMatchingLiterals.size() + 1) * (int)max ) - ( (int)max + (int)(bvaMatchingLiterals.size() + 1) ); 
	  if( debug > 1 ) cerr << "c BVA new reduction: " << newReduction << " old: " << currentReduction << endl;
	  
	  // check whether new reduction is better, if so, update structures and proceed with adding matching clauses to structure
	  if( newReduction > currentReduction )
	  {
	    if( debug > 2 ) cerr << "c [BVA] keep literal " << left << endl;
	    currentReduction = newReduction;
	    bvaMatchingLiterals.push_back( left );
	  } else break;
	  
	  const uint32_t matches = bvaCountCount[ toInt(left) ];

	  // get vector for the literal that will be added now
	  while ( bvaMatchingClauses.size() < bvaMatchingLiterals.size() ) bvaMatchingClauses.push_back( vector<CRef>() );
	  
	  // if there has been already the space, simply re-use the old space
	  bvaMatchingClauses[index].clear();
	  bvaMatchingClauses[index].assign( bvaMatchingClauses[0].size(),0);
	  
	  assert (max <= data.list( left).size() );
	  const vector<CRef>& rightList = bvaMatchingClauses[ 0 ];
	  vector<CRef>& leftList = data.list( left); // will be sorted below
	  uint32_t foundCurrentMatches = 0;
	  // for each clause in Mcls, find matching clause in list of clauses with literal "left"
	  for( uint32_t i = 0 ; i < rightList.size(); ++i ) {
	    if( rightList[i] == 0 ) {
	      continue; // to skip clauses that have been de-selected
	    }
	    const CRef C = rightList[i];
	    const Clause& clauseC = ca[ C ];
	    if( clauseC.can_be_deleted() ) continue;
	    if( debug > 2 && clauseC.size() < 2 ) cerr << "c [BVA] unit clause in right clauses (" << clauseC.size() << " " << endl;
	    if( clauseC.size() < 2 ) continue;
	    data.ma.nextStep();
	    for( uint32_t j = 0; j < clauseC.size(); j ++ ) {
	      if( clauseC[j] == right ) continue;
	      data.ma.setCurrentStep( toInt(clauseC[j]) );
	    }
	    // so far, left and right cannot be in the same clause for matching
	    if ( data.ma.isCurrentStep( toInt(left) ) ) { 
	      bvaMatchingClauses[ 0 ][i] = 0;	// FIXME: check whether excluding this clause is necessary (assumption: yes)
	      continue;
	    }
	    data.ma.reset( toInt(right) ); data.ma.setCurrentStep( toInt(left) );
	    unsigned oldFoundCurrentMatches = foundCurrentMatches;
	    for( uint32_t j = foundCurrentMatches ; j < leftList.size(); ++j ) {
	      CRef D = leftList[j];
	      const Clause& clauseD = ca[ D];
	      if( clauseD.can_be_deleted() ) continue;

	      if( debug > 2 && clauseD.size() < 1 ) cerr << "c [BVA] found unit clause in list" << left << endl;
	      
	      CRef tmp2 = 0;
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
	    if( foundCurrentMatches == oldFoundCurrentMatches ) bvaMatchingClauses[0][i] = 0;
	  }
	  // return number of matching clauses
	  
	  } // end while improving a literal
	  
	  // continue with next literal, if there has not been any reduction
	  if( bvaMatchingLiterals.size() < 2 ) {
	    if( bvaHeap.size() > 0 )
	    if( debug > 2 ) cerr << "c [BVA] continue because not two matching literals (" << right << "), bvaHeap[" << bvaHeap.size() << "],0=" << bvaHeap[0] << endl;
	    continue;
	  }
	  
	  // create the new clauses!
	  const Var newX = nextVariable();
	  didSomething = true;
	  assert( !bvaHeap.inHeap( toInt(right) ) && "by adding new variable, heap should not contain the currently removed variable" );
	  
	  if( debug > 1 ) cerr << "c [BVA] heap size " << bvaHeap.size() << endl;
	  Lit replaceLit = mkLit( newX, false );
	  // for now, use the known 2!
	  assert( bvaMatchingLiterals.size() <= bvaMatchingClauses.size() ); // could be one smaller, because stack has only one element
	  
	  if( debug > 1 ) {
	    cerr << "c [BVA] matching literals:";
	    for( uint32_t i = 0 ; i < bvaMatchingLiterals.size(); ++ i ) cerr << " " << bvaMatchingLiterals[i];
	    cerr << endl;
	  }
	  
	  for( uint32_t i = 0 ; i < bvaMatchingLiterals.size(); ++ i ) {
	    vector<CRef>& iClauses = bvaMatchingClauses[i];
	    if( i == 0 ) {	// clauses that are in the occurrenceList list of right
	      // clauses of right literal, alter, so that they will be kept
	      for( uint32_t j = 0 ; j < iClauses.size(); ++ j )
	      {
		if( iClauses[j] == 0 ) continue; // don't bvaHeap with 0-pointers
		bool replaceFlag = false;
		Clause& clauseI = ca[ iClauses[j] ];
		if( clauseI.can_be_deleted() ) continue;
		if( debug > 2 && clauseI.size() < 2) cerr << "c [BVA] unit clause for replacement (" << clauseI.size() << ")" << endl;
		assert( ! clauseI.can_be_deleted() && "matching clause is ignored" );
		
		// take care of learned clauses (if the replace clause is learned and any matching clause is original, keep this clause!)
		for( uint32_t k = 1; k< bvaMatchingLiterals.size(); ++ k ) {
		  if( !ca[ bvaMatchingClauses[k][j] ].learnt() ) { clauseI.set_learnt(false); break; }
		}
		for( uint32_t k = 0 ; k< clauseI.size(); ++k ) {
		  if( clauseI[k] == right ) { 
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
		for( uint32_t k = 0 ; k < data.list( right).size(); ++k ) {
		  if( iClauses[j] == data.list( right)[k] ) {
		    data.removeClauseFrom(iClauses[j], right, k );
		    break;
		  }
		}
		
		// add clause into occurrenceList list of new variable
		data.list( replaceLit ). push_back(iClauses[j]);
	      }
	    } else {
	      assert( iClauses.size() == bvaMatchingClauses[0].size() );
	      // remove remaining clauses completely from formula
	      for( uint32_t j = 0 ; j < iClauses.size(); ++ j ) {
		if( bvaMatchingClauses[0][j] != 0 && iClauses[j] != 0 ) {
		  Clause& clauseI = ca[ iClauses[j] ];
		  clauseI.set_delete(true);
		  data.removedClause( iClauses[j] );
		}
	      }
	    }
	  }
	  assert( bvaMatchingLiterals.size() > 1 && "too few matching literals");
	  
	  // and-gate needs to be written explicitely -> add the one missing clause, if something has been found
	  if( debug > 2 ) cerr << "c [BVA] current reduction: " << currentReduction << endl;
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
		uint32_t j = 0;
		for(  ; j < clause.size(); ++ j ) {
		  const Lit literal = clause[j];
		  if( data.ma.isCurrentStep( toInt(literal) ) ) {
		    data.removeClauseFrom(clRef, literal);
		    clause[j] =  mkLit(newX, false) ;
		    data.removedLiteral(literal);
		    data.addedLiteral( mkLit(newX, false) );
		    data.list( mkLit(newX, false) ).push_back( clRef );
		    break;
		  }
		}
		for( ; j < clause.size(); ++ j ) {
		  const Lit literal = clause[j];
		  if( data.ma.isCurrentStep( toInt(literal) ) ) {
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
		  }
		}
	      }
	      
	      // add and-clause only once
	      if( !addedAnd ) {
		addedAnd = true;
		addedNewAndGate = true;
		clauseLits.clear();
		for( uint32_t j = 0 ; j < bvaMatchingLiterals.size(); ++j )
		  clauseLits.push( ~bvaMatchingLiterals[j] );
		clauseLits.push( mkLit( newX, true ) );
		CRef tmpRef = ca.alloc(clauseLits, false); // no learnt clause!
		data.addClause( tmpRef );
		data.getClauses().push( tmpRef );
	      }
	    }
	    
	  }

	  clauseLits.clear();
	  clauseLits.push( mkLit(newX,false) );
	  clauseLits.push( lit_Undef );
	  
	  for( uint32_t i = 0; i < bvaMatchingLiterals.size(); ++i ) {
	    clauseLits[1] = bvaMatchingLiterals[i];
	    CRef newClause = ca.alloc(clauseLits, false); // no learnt clause!
	    data.addClause( newClause );
	    data.getClauses().push( newClause );
	  }
	  
	  if( bvaPush > 0 ) {
	    if( debug > 1 &&  bvaHeap.inHeap(toInt(right)) ) cerr << "c [BVA] add item that is not removed: " << right << endl;
	    if( bvaHeap.inHeap(toInt(right)) ) bvaHeap.update(toInt(right));
	    else bvaHeap.insert( toInt(right) );
	  }
	  if( bvaPush > 1 ) {
	    if( bvaMatchingLiterals.size() > 2 ) {
	      if( bvaHeap.inHeap( toInt(mkLit(newX, false)) )) bvaHeap.update( toInt(mkLit(newX, false)));
	      else bvaHeap.insert( toInt(mkLit( newX, true ) ));
	    }
	  }
	  if( debug > 1 && bvaHeap.size() > 0 ) cerr << "c [BVA] heap(" << bvaHeap.size() << ") with at 0: " << bvaHeap[0] << endl;
	  
	} // end for l in F
  endBVA:;
  
// 	  if( debug > 2 )
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
    if( debug > 2 && clauseC.size() < 2 ) cerr << "c [BVA] unit clause in complements" << endl;
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
      
     if( debug > 1 ) { cerr << "c [CP2-BVAC] check clause " << j << " of literal " << left << ": " << ca[D] << endl; }
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
    if( debug > 1 ) { cerr << "c [CP2-BVAC] remove " << right << " from (" << i << "/"<< data.clss.size() << ")" << ca[data.clss[i]] << endl; }
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
  bvaHeap.addNewElement(nextVar);

  bvaCountMark.resize( nextVar * 2);
  bvaCountCount.resize( nextVar * 2);
  bvaCountSize.resize( nextVar * 2);
}


void BoundedVariableAddition::printStatistics(ostream& stream)
{
stream << "c [STAT] BVA " << endl;
}


bool BoundedVariableAddition::variableAddtionMulti(bool sort)
{
  assert( false && "This method is not implemented yet" );
  return true;
}
