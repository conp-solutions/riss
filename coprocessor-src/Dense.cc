/****************************************************************************************[Dense.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Dense.h"

#include <fstream>

using namespace std;
using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - DENSE";

/// enable this parameter only during debug!
#if defined CP3VERSION  
static const bool debug_out = false;
#else
static BoolOption debug_out         (_cat, "cp3_dense_debug", "print debug output to screen",false);
#endif

Dense::Dense(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation)
: Technique(_ca,_controller)
, data(_data)
, propagation( _propagation )
, globalDiff(0)
{}

void Dense::compress(const char* newWhiteFile)
{
  if( propagation.process(data) == l_False ) data.setFailed();
  if( !data.ok() ) return;
  
  Compression compression;
  compression.variables = data.nVars();
  compression.mapping = (Var*)malloc( sizeof(Var) * (data.nVars()) );
  for( Var v = 0; v < data.nVars(); ++v ) compression.mapping[v] = -1;
  
  uint32_t* count = (uint32_t*)malloc( (data.nVars()) * sizeof(uint32_t) );
  memset( count, 0, sizeof( uint32_t) * (data.nVars()) );
  // count literals occuring in clauses
  for( uint32_t i = 0 ; i < data.getClauses().size(); ++i ){
    Clause& clause = ca[ data.getClauses()[i] ];
    if( clause.can_be_deleted() ) continue;
    uint32_t j = 0 ;
    for( ; j < clause.size(); ++j ){
      const Lit l = clause[j];
      
      assert( l_Undef == data.value(l) && "there cannot be assigned literals");
      assert(var(l) < data.nVars() );
      
      count[ var(l) ] ++;
    }
    // found empty clause?
    if( clause.size() == 0 ) { data.setFailed(); }
  }
 
  uint32_t diff = 0;
  for( Var v = 0 ; v < data.nVars(); v++ ){
    assert( diff <= v && "there cannot be more variables that variables that have been analyzed so far");
    if( count[v] != 0 || data.doNotTouch(v) || data.value( mkLit(v,false) ) != l_Undef ){ // be able to rewrite trail!
      compression.mapping[v] = v - diff;
    }
    else { diff++; assert( compression.mapping[v] == -1 ); }
  }
  free( count ); // do not need the count array any more
  
  // formula is already compact
  if( diff == 0 ) return;
  globalDiff += diff;
  
  // replace everything in the clauses
  if( debug_out > 0 ) cerr << "c [COM] compress clauses" << endl;
  for( int s = 0 ; s < 2; ++ s ) {
    vec<CRef>& list = s == 0 ? data.getClauses() : data.getLEarnts();
    for( uint32_t i = 0 ; i < list.size(); ++i ){
      Clause& clause = ca[ list[i] ];
      if( clause.can_be_deleted() ) continue;
      
      for( uint32_t j = 0 ; j < clause.size(); ++j ){
	const Lit l = clause[j];
	// if( debug > 1 ) cerr << "c compress literal " << l.nr() << endl;
	assert( compression.mapping[ var(l) ] != -1 && "only move variable, if its not moved to -1" );
	const bool p = sign(l);
	const Var v= compression.mapping[ var(l)];
	clause[j] = mkLit( v, p );
	// polarity of literal has to be kept
	assert( sign(clause[j]) == p && "sign of literal should not change" );
      }
    }
  }
  
  if( newWhiteFile != 0 ) {
    cerr << "c work with newWhiteFile " << newWhiteFile << endl;
    ofstream file;
    file.open( newWhiteFile, ios::out );
    // dump info
    // file << "original variables" << endl;
    for( Var v = 0; v < data.nVars(); ++ v )
      if( data.doNotTouch(v) ) file << compression.mapping[ v ] << endl;
    file.close();
  }
  
  // compress trail, so that it can still be used!
  for( uint32_t i = 0 ; i < data.getTrail().size(); ++ i ) {
    const Lit l = data.getTrail()[i];
    const bool p = sign(l);
    const Var v = compression.mapping[ var(l) ];
    if( debug_out ) cerr << "c trail " << var(l) + 1 << " to " << v+1 << endl;
    data.getTrail()[i] = mkLit( v, p );
  }
  
  if( debug_out ) {
    cerr << "c final trail: "; 
    for( uint32_t i = 0 ; i < data.getTrail().size(); ++ i ) cerr << " " << data.getTrail()[i];
    cerr << endl;
  }

  // rewriting everything finnished
  // invert mapping - and re-arrange all variable data
  for( Var v = 0; v < data.nVars() ; v++ )
  {
    if ( compression.mapping[v] != -1 ){
      // cerr << "c move intern var " << v << " to " << compression.mapping[v] << endl;
      data.moveVar( v, compression.mapping[v], v+1 == data.nVars() ); // map variable, and if done, re-organise everything
      // invert!
      compression.mapping[ compression.mapping[v] ] = v;
      // only set to 0, if needed afterwards
      if( compression.mapping[ v ] != v ) compression.mapping[ v ] = -1;
    }
  }

  map_stack.push_back( compression );
  
  return; 
}

void Dense::decompress(vec< lbool >& model)
{
  if( debug_out ) {
    cerr << "c model to work on: ";
    for( int i = 0 ; i < model.size(); ++ i ) cerr << (model[i] == l_True ? i+1 : -i-1) << " ";
    cerr << endl;
  }
  // walk backwards on compression stack!
  for( int i = map_stack.size() - 1; i >= 0; --i ) {
    Compression& compression = map_stack[i];
    if( debug_out ) cerr << "c [COM] change number of variables from " << model.size() << " to " << compression.variables << endl;
    // extend the assignment, so that it is large enough
    if( model.size() < compression.variables ){
      model.growTo( compression.variables, l_False  );
      while( data.nVars() < compression.variables ) data.nextFreshVariable('o');
    }
    // backwards, because variables will increase - do not overwrite old values!
    for( int v = compression.variables-1; v >= 0 ; v-- ){
      // use inverse mapping to restore the already given polarities (including UNDEF)
	if( compression.mapping[v] != v && compression.mapping[v] != -1 ) {
	  if( debug_out ) cerr << "c move variable " << v+1 << " to " << compression.mapping[v] + 1  << " -- falsify " << v+1 << endl;
	  
	  model[ compression.mapping[v] ] =  model[ v ];
	  // set old variable to some value
	  model[v] = l_False;
	} else {
	  if( model[v] == l_Undef ) {
	    model[v] = l_False; 
	    if( debug_out ) cerr << "c falsify " << v+1 << endl;
	  }
	}
    }
  }
}


void Dense::printStatistics( ostream& stream )
{
  cerr << "c [STAT] DENSE " << globalDiff << " gaps" << endl;
}
  
void Dense::destroy()
{
  
}