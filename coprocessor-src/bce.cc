/******************************************************************************************[BCE.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/bce.h"

using namespace Coprocessor;

BlockedClauseElimination::BlockedClauseElimination( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Coprocessor::Propagation& _propagation )
: Technique(_config, _ca,_controller)
, data(_data)
, propagation( _propagation )
{
  
}


void BlockedClauseElimination::reset()
{
  
}
  
bool BlockedClauseElimination::process()
{
  // parameters:
  bool orderComplements = true; // sort the heap based on the occurrence of complementary literals
  bool bceBinary = false; // remove binary clauses during BCE
  int bceLimit = 10000000;
  bool opt_bce_verbose = false; // output operation steps
  bool opt_bce_bce = true; // actually remove blocked clauses
  bool opt_bce_cle = true; // perform covered literal elimination
  bool opt_bce_debug = false; // debug output
  bool opt_cle_debug = false; // debug output
  
  assert( (opt_bce_bce || opt_bce_cle) && "something should be done in this method!" );
  
  // attributes
  int bceSteps = 0;
  int cleCandidates = 0; // number of clauses that have been checked for cle
  int remBCE = 0, remCLE = 0, cleUnits=0; // how many clauses / literals have been removed
  
  if( data.hasToPropagate() ) {
    propagation.process(data, true); // propagate, if there's something to be propagated
    modifiedFormula = modifiedFormula  || propagation.appliedSomething();
    if( !data.ok() ) return modifiedFormula;
  }
  
  
  LitOrderBCEHeapLt comp(data); // use this sort criteria!
  Heap<LitOrderBCEHeapLt> bceHeap(comp);  // heap that stores the variables according to their frequency (dedicated for BVA)
  
  // setup own structures
  bceHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    if( data.doNotTouch(v) ) continue; // do not consider variables that have to stay fixed!
    if( data[  mkLit(v,false) ] > 0 ) if( !bceHeap.inHeap(toInt(mkLit(v,false))) )  bceHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] > 0 ) if( !bceHeap.inHeap(toInt(mkLit(v,true))) )   bceHeap.insert( toInt(mkLit(v,true))  );
  }
  data.ma.resize(2*data.nVars());
  data.ma.nextStep();
  
  // do BCE on all the literals of the heap
  while (bceHeap.size() > 0 && (data.unlimited() || bceLimit > bceSteps) && !data.isInterupted() )
  {
    // interupted ?
    if( data.isInterupted() ) break;
    
    const Lit right = toLit(bceHeap[0]);
    assert( bceHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");
    bceHeap.removeMin();
    
    if( data.doNotTouch(var(right)) ) continue; // do not consider variables that have to stay fixed!
    
    // check whether a clause is a tautology wrt. the other clauses
    const Lit left = ~right; // complement
    data.lits.clear(); // used for covered literal elimination
    int keptClauses = 0;
    for( int i = 0 ; i < data.list(right).size(); ++ i ) 
    {
      Clause& c = ca[ data.list(right)[i] ];
      if( c.can_be_deleted() || (!bceBinary && c.size() == 2) ) continue; // do not work on uninteresting clauses!
      
      if( opt_bce_cle ) {
	data.lits.clear(); // collect the literals that could be removed by CLE
	for( int k = 0 ; k < c.size(); ++k ) if( right != c[k] ) data.lits.push_back( c[k] );
      }
      bool canCle = false; // yet, no resolvent has been produced -> cannot perform CLE
      bool emptyCle = false; // yet, nothing about the set of covering literals is known
      bool isBlocked = true; // yet, no resolvent has been produced -> has to be blocked
      for( int j = 0 ; j < data.list(left).size(); ++ j ) {
	Clause& d = ca[ data.list(left)[j] ];
	if( d.can_be_deleted() ) continue; // do not work on uninteresting clauses!
	bceSteps ++;
	if( ! tautologicResolvent( c,d,right ) ) {
	  isBlocked = false; // from here on, the given clause is not a blocked clause since a non-tautologic resolvent has been produced
	  if( ! opt_bce_cle || data.lits.size() == 0 ) break; // however, for cle further checks have to be/can be done!
	} else if (opt_bce_cle) {
	  // build intersection of resolvents!
	  canCle = true; // there was some clause that could be used for resolution
	  data.ma.nextStep();
	  for( int k = 0 ; k < d.size(); ++ k ) data.ma.setCurrentStep( toInt(d[k]) ); // mark all literals of this clause
	  // keep literals, that occurred before, and in this clause
	  int keptCle = 0;
	  for( int k = 0 ; k < data.lits.size(); ++ k ) {
	    if( data.ma.isCurrentStep( toInt(data.lits[k]) ) ) {
	      data.lits[ keptCle++ ] = data.lits[k];
	    }
	  }
	  data.lits.resize( keptCle ); // remove all the other literals
	  // cerr << "c resolving " << c << " with " << d << " keeps the literals " << data.lits << endl;
	}
      }
      if( opt_bce_bce && isBlocked ) {
	// add the clause to the stack 
	    c.set_delete(true);
	    data.addCommentToProof("blocked clause during BCE");
	    data.addToProof(c,true);
            data.removedClause( data.list(right)[i] );
	    remBCE ++; // stats
	    for( int k = 0 ; k < c.size(); ++ k ) {
	      if( bceHeap.inHeap( toInt(~c[k]) ) ) bceHeap.update( toInt(~c[k]) ); // update entries in heap
	      else bceHeap.insert( toInt(~c[k]) ); // eager insert // TODO: check whether this is good!
	    }
            didChange();
            if ( !c.learnt() ) {
		if( opt_bce_verbose > 2) cerr << "c remove blocked clause " << ca[data.list(right)[i]] << endl;
                data.addToExtension(data.list(right)[i], right);
	    }
	// remove the clause
      } else {
	data.list(right)[keptClauses++] = data.list(right)[i]; // memorize kept clauses
	if( opt_bce_cle && canCle && data.lits.size() > 0 ) { 
	  // cle can actually be performed:
	  didChange();
	  remCLE += data.lits.size(); 
	  int k = 0, l = 0; // data.lits is a sub set of c, both c and data.lits are ordered!
	  int keptLiterals = 0;
	  if( opt_cle_debug ) cerr << "c cle turns clause " << c << endl;
	  while( k < c.size() && l < data.lits.size() ) {
	    if( c[k] == data.lits[l] ) {
	      // remove the literal from the clause and remove the clause from that literals structures, as well as decrease the occurrence counter
	      data.removedLiteral( c[k] );
	      data.removeClauseFrom( data.list(right)[i], c[k] ); // remove the clause from the list of c[k]
	      k++; l++;
	    } else if ( c[k] < data.lits[l] ) {
	      c[keptLiterals ++] = c[k]; // keep this literal, since its not removed!
	      k++;
	    } else l ++; // if ( data.lits[l] < c[k] ) // the only left possibility!
	  }
	  for( ; k < c.size(); ++ k ) c[keptLiterals ++] = c[k]; // keep the remaining literals as well!
	  assert( (keptLiterals + data.lits.size() == c.size()) && "the size of the clause should not shrink without a reason" );
	  c.shrink(  data.lits.size() ); // remvoe the other literals from this clause!
	  if( opt_cle_debug ) cerr << "c into clause " << c << " by removing " << data.lits.size() << " literals, namely: " << data.lits << endl;
	  if( c.size() == 1 ) {
	    cleUnits ++;
	    if( l_False == data.enqueue(c[0] ) ) {
	      return false;
	    }
	    c.set_delete(true);
	  }
	  for( l = 0 ; l < data.lits.size(); ++ l ) {
	    if( bceHeap.inHeap( toInt(~data.lits[l] )  ) ) bceHeap.update( toInt(~data.lits[l] ) ); // there is the chance for ~right to become blocked!
	    else bceHeap.insert( toInt(~data.lits[l] ) );
	  }
	}
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
    data.list(right).resize( keptClauses ); // update occurrence list for this literal
  }
  
  cerr << "c [STAT] bce: " << bceSteps << " steps, " << remBCE << " remBCE, " << remCLE << " remCLE, " << cleUnits << " cleUnits, " << endl;
  
  return modifiedFormula;
}
    
bool BlockedClauseElimination::tautologicResolvent(const Clause& c, const Clause& d, const Lit l)
{
  int i = 0, j = 0;
  while ( i < c.size() && j < d.size() ) 
  {
    if( c[i] == l ) { // skip this literal!
      i++;
    } else if( d[j] == ~l ) { // skip this literal!
      j++;
    } else if( c[i] == d[j] ) { // same literal
      i++; j++;
    } else if( c[i] == ~d[j] ) { // complementary literal -> tautology!
      return true; 
    } else if( c[i] < d[j] ) {
      i++;
    } else if( d[j] < c[i]  ) {
      j ++;
    }
  }
  return false; // a complementarly literal was not found in both clauses
}
    
    
void BlockedClauseElimination::printStatistics(ostream& stream)
{
  
}

void BlockedClauseElimination::giveMoreSteps()
{
  
}
  
void BlockedClauseElimination::destroy()
{
  
}