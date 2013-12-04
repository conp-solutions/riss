/******************************************************************************************[BCE.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/bce.h"

using namespace Coprocessor;

BlockedClauseElimination::BlockedClauseElimination( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data )
: Technique(_config, _ca,_controller),
data(_data)
{
  
}

void BlockedClauseElimination::reset()
{
  
}
  
bool BlockedClauseElimination::process()
{
  // parameters:
  bool orderComplements = true; // sort the heap based on the occurrence of complementary literals
  bool bceBinary = true; // remove binary clauses during BCE
  int bceLimit = 10000000;
  bool opt_bce_verbose = false; // output operation steps
  
  // attributes
  int bceSteps;
  
  LitOrderBCEHeapLt comp(data); // use this sort criteria!
  Heap<LitOrderBCEHeapLt> bceHeap(comp);  // heap that stores the variables according to their frequency (dedicated for BVA)
  
  // setup own structures
  bceHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v )
  {
    if( data.doNotTouch(v) ) continue; // do not consider variables that have to stay fixed!
    if( data[  mkLit(v,false) ] > 2 ) if( !bceHeap.inHeap(toInt(mkLit(v,false))) )  bceHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] > 2 ) if( !bceHeap.inHeap(toInt(mkLit(v,true))) )   bceHeap.insert( toInt(mkLit(v,true))  );
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
    data.ma.reset( toInt(right) ); // indicate that this literal has been processed now
    
    // check whether a clause is a tautology wrt. the other clauses
    const Lit left = ~right; // complement
    int keptClauses = 0;
    for( int i = 0 ; i < data.list(right).size(); ++ i ) {
      Clause& c = ca[ data.list(right)[i] ];
      if( c.can_be_deleted() || (!bceBinary && c.size() == 2) ) continue; // do not work on uninteresting clauses!
      
      bool isBlocked = true;
      for( int j = 0 ; j < data.list(left).size(); ++ j ) {
	Clause& d = ca[ data.list(left)[j] ];
	if( d.can_be_deleted() ) continue; // do not work on uninteresting clauses!
	bceSteps ++;
	if( ! tautologicResolvent( c,d,right ) ) {
	  isBlocked = false; break;
	}
      }
      if( isBlocked ) {
	// add the clause to the stack 
	    c.set_delete(true);
	    data.addCommentToProof("blocked clause during BCE");
	    data.addToProof(c,true);
            data.removedClause( data.list(right)[i] );
	    for( int k = 0 ; k < c.size(); ++ k ) {
	      if( bceHeap.inHeap( toInt(~c[k]) ) ) bceHeap.update( toInt(~c[k]) ); // update entries in heap
	      else bceHeap.insert( toInt(~c[k]) ); // eager insert // TODO: check whether this is good!
	      data.ma.setCurrentStep( toInt(~c[k]) ); // mark complementary literals, since they can be blocking after removing this clause
	    }
            didChange();
            if ( !c.learnt() ) {
		if( opt_bce_verbose > 2) cerr << "c remove blocked clause " << ca[data.list(right)[i]] << endl;
                data.addToExtension(data.list(right)[i], right);
	    }
	// remove the clause
      } else {
	data.list(right)[keptClauses++] = data.list(right)[i]; // memorize kept clauses
      }
    }
    data.list(right).resize( keptClauses ); // update occurrence list for this literal
  }
  
  
  return false;
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
    } else if( c[i] == d[i] ) { // same literal
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