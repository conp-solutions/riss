/**************************************************************************************[Circuit.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Circuit.h"

using namespace Coprocessor;

Circuit::Circuit(ClauseAllocator& _ca)
: ca (_ca)
{}

int Circuit::extractGates(CoprocessorData& data, vector< Circuit::Gate >& gates)
{
  // create BIG
  big = new BIG( );
  big->create(ca,data,data.getClauses(),data.getLEarnts());
  
  // create the list of all ternary / quadrary clauses
  ternaries.resize( data.nVars() * 2 );
  quads.resize( data.nVars() * 2 );
  for ( int v = 0 ; v < 2*data.nVars(); ++v )
  {
    ternaries[v].clear();
    quads[v].clear();
  }
  for ( int p = 0 ; p < 2; ++ p ) {
    vec<CRef>& list = p == 0 ? data.getClauses() : data.getLEarnts() ;
    for( int i = 0 ; i < list.size(); ++ i ) {
      const Clause& c = ca[ list[i] ];
      if( c.can_be_deleted() ) continue;
      if( c.size() ==3 ) {
	for( int j = 0 ; j < 3; ++ j )
	  ternaries[ toInt(c[j]) ].push_back( ternary(j==0 ? c[2] : c[0] ,
                                                      j==1 ? c[2] : c[1]) );
      } else if (c.size() == 4 ) {
	for( int j = 0 ; j < 4; ++ j ) 
	  quads[ toInt(c[j]) ].push_back( quad( j == 0 ? c[3] : c[0],
						 j == 1 ? c[3] : c[1],
					         j == 2 ? c[3] : c[2]) );
      }
    }
  }
  
  // try to find all gates with output variable v for all the variables  
  for ( Var v = 0 ; v < data.nVars(); ++v )
    getGatesWithOutput(v,gates);
}

void Circuit::getGatesWithOutput(const Var v, vector< Circuit::Gate >& gates)
{

}

void Circuit::getANDGates(const Var v, vector< Circuit::Gate >& gates)
{

}

void Circuit::getAMO_ANDGates(const Var v, vector< Circuit::Gate >& gates)
{

}


void Circuit::getHASUMGates(const Var v, vector< Circuit::Gate >& gates)
{

}

void Circuit::getITEGates(const Var v, vector< Circuit::Gate >& gates)
{

}

void Circuit::getXORGates(const Var v, vector< Circuit::Gate >& gates)
{

}

