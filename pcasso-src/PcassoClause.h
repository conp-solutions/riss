#ifndef PCASSOCLAUSE_H
#define PCASSOCLAUSE_H

#include "mtl/Vec.h"
#include <iostream> // Davide> Debug

using namespace Minisat;

namespace davide{
  
  struct PcassoClause{
    unsigned int size;
    vec<Lit> lits;
    
    PcassoClause(){};
    // copy constructor required by push_back
    PcassoClause(const PcassoClause& c){
      size = c.size;
      c.lits.copyTo(lits);
    }
    PcassoClause& operator = (const PcassoClause& c){
      
      if( this != &c ){ // Davide> Self-copy protection
	lits.clear(true);
	c.lits.copyTo(lits);
	size = c.size;
      }
      return *this;
    }
  };

}

#endif
