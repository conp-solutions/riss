/********************************************************************++++++++++++++++++[Shuffler.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

/** this class re-implements the rand-method so that it can be used more independently
 */
#include <stdint.h>
#include <vector>

#include "core/Solver.h"
using namespace Minisat;

namespace Coprocessor {
  
  class Randomizer {
    uint64_t hold;
  public:
    Randomizer() { hold = 0; };
    
    /** sets the current random value
    */
    void set( uint64_t newValue ){ hold = newValue; }

    /** return the next random value
    */
    uint64_t rand() { return (((hold = hold * 214013L + 2531011L) >> 16) & 0x7fff); }
    
    /** return the next random value modulo some other value
    */
    uint64_t rand(uint64_t upperBound) { uint64_t ret = rand(); return upperBound > 0 ? ret % upperBound : 0; }
  };


  class VarShuffler 
  {
    uint32_t variables;
    vector< Lit > replacedBy;
    uint32_t seed;
    bool shuffledAlready;
    
    Randomizer randomizer;
    
  public:
    VarShuffler() : variables(0), seed(0), shuffledAlready(false) {}
    
    /** set seed fo shuffling (it is a pivate seed, independent from rand() */
      void setSeed( uint32_t s ) { seed = s ; randomizer.set(seed); }
    
    /** create a shuffle - mapping */
      void setupShuffling(uint32_t vars) {
	// shuffle only once!
	if( variables != 0 ) return;
	variables = vars ;
	
	  // creae shuffle array
	  replacedBy.resize( vars, NO_LIT );
	  for( Var v = 0 ; v < vars; ++v ) replacedBy[v] = mkLit(v,false);
		
	  for( Var v = 0 ; v < vars; ++v ) { 
	    uint32_t r = randomizer.rand() % vars + 1;
	    Lit lr = replacedBy[v];
	    replacedBy[v] = replacedBy[r];
	    replacedBy[r] = lr;
	  }

	  for( Var v = 0 ; v < vars; ++v ) { 
	    const uint32_t r = randomizer.rand() & 1;
	    if( r == 1 ) replacedBy[v] = replacedBy[v].complement();
	  }
      }
    
    /** apply the mapping to the formula */
      void shuffle( vector<CRef>& clauses, ClauseAllocator& ca, bool shuffleOrder = false ){
	// shuffle formula
	for( uint32_t i = 0 ; i < clauses.size(); ++ i ) 
	{
	  Clause& c = ca[ clauses[i] ];
	  for( uint32_t j = 0 ; j < c.size(); ++ j )
	  {
	    const Lit l = c[j];
	    c[j] =  sign(l) ? ~ replacedBy[ var(l) ] : replacedBy[var(l)] ;
	  }
	}
	// shuffle order of clauses
	if( shuffleOrder ) {
	  for( uint32_t i = 0 ; i < clauses.size(); ++ i ) {
	    const CRef tmp = clauses[i];
	    int tmpPos = randomizer.rand( clauses.size() );
	    clauses[i] = clauses[tmpPos];
	    clauses[tmpPos] = tmp;
	  }
	}

      }
      
      /** remap model to original variables */
      void unshuffle( vec<Lit>& model )
      {
	vec<Lit> copy;
	model.copyTo(copy);
	
	in max = variables < model.size() ? variables : model.size();
	for( Var v = 0; v < max; ++v ) {
	  model [v] = sign( replacedBy[v]) ? ~copy[ var(replacedBy[v]) ]  :  copy[ var(replacedBy[v]) ];
	}
      }

  };

};