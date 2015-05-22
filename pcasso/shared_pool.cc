#include "Shared_pool.h"

using namespace Riss;
using namespace std;

namespace PcassoDavide {

// A static vector always need a declaration
vector<PcassoClause> Shared_pool::shared_clauses;

void Shared_pool::add_shared(vec<Lit>& lits, unsigned int size){
  
#ifndef NDEBUG
  
  if( shared_clauses.size() % 128 == 0 ) std::cerr << "c [POOL] added a shared clause nr. " << shared_clauses.size() << " of size " << size << std::endl;
  
#endif
  
  PcassoClause c;
  
  c.size = size;
  lits.copyTo(c.lits);
  // for( unsigned int i = 0; i < size; i++ ){
  //   c.lits.push(mkLit(var(lits[i]), sign(lits[i])));
  // }

  sort(c.lits); 

  // Remove duplicates
  Lit p; int i, j;
  for (i = j = 0, p = lit_Undef; i < c.lits.size(); i++)
    if ( c.lits[i] == ~p ) // contains a tautology
      return;
    else if ( c.lits[i] != p ) // No duplicated
      c.lits[j++] = p = c.lits[i];
  c.lits.shrink_(i - j);

#ifndef NDEBUG

  if( c.lits.size() == 0 ) printf("CLAUSE OF SIZE ZERO ADDED TO SHARED POOL\n");
  
#endif
    
  // if( !duplicate(c) ){ 
  shared_clauses.push_back(c);
  // }
}

bool Shared_pool::duplicate(const PcassoClause& c){
  
     for( unsigned int i = 0; i < shared_clauses.size(); i++ )
       if( shared_clauses[i].size == c.size ){
     	 unsigned int j = 0;
     	 while( j < c.size && ( toInt(shared_clauses[i].lits[j]) == toInt(c.lits[j]) ) ) j++;
     	 if( j == c.size ) return true;
       }
      return false; 
}

// void Shared_pool::delete_shared_clauses(){ // Davide> Still to be tested
//      for( unsigned int i = 0; i < shared_clauses.size(); i++ ){
//         shared_clauses[i].lits.clear(true); // with memory deallocation
//      }
//      shared_clauses.clear();
//   }

} // namespace PcassoDavide
