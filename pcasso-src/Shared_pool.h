/* 
 * File:   Shared_pool.h
 * Author: tirrolo
 *
 * Created on June 17, 2012, 12:08 PM
 */

#ifndef SHARED_POOL_H
#define	SHARED_POOL_H

#include <vector>

#include "core/SolverTypes.h"
#include "utils/LockCollection.h"
#include "mtl/Vec.h"
#include "mtl/Sort.h"
#include "pcasso-src/PcassoClause.h"

// Debug
#include <stdio.h>

#include <iostream>
#include <fstream>

using namespace Minisat;
using namespace std;

namespace davide{
   
  static ComplexLock shared_pool_lock = ComplexLock(); // library can be build with this?
   
   class Shared_pool {
   public:
      
     static bool duplicate( const PcassoClause& c );
     static void add_shared(vec<Lit>& lits, unsigned int size);
     // static void delete_shared_clauses(void);
      
     const static unsigned int max_size = 10000;
     
     static vector<PcassoClause> shared_clauses;

     static void dumpClauses( const char* filename){
	
       fstream s;
       s.open(filename, fstream::out);
       s << "c [POOL] all collected shared clauses begin: " << endl;
       for( unsigned int i = 0 ; i < shared_clauses.size(); ++i )
       {
	  PcassoClause& c = shared_clauses[i];
	  for (unsigned int j = 0; j < c.size; j++)
	    s << (sign(c.lits[j]) ? "-" : "") << var(c.lits[j])+1 << " ";
	  s << "0\n"; 
       }
       s << "c [POOL] all collected shared clauses end: " << endl;
       s.close();
     }
     
   private:
     Shared_pool();
     Shared_pool(const Shared_pool& orig);
     virtual ~Shared_pool();
     
   };
   
}

#endif	/* SHARED_POOL_H */

