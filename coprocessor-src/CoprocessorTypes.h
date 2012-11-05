/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTYPES_HH
#define COPROCESSORTYPES_HH

#include "core/Solver.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

typedef std::vector<std::vector <CRef> > ComplOcc;

/** Data, that needs to be accessed by coprocessor and all the other classes
 */
class CoprocessorData
{
  ClauseAllocator& ca;
  /* TODO to add here
   * occ list
   * counters
   * methods to update these structures
   * no statistical counters for each method, should be provided by each method!
   */
  ComplOcc occs;                // list of clauses, per literal
  vector<int32_t> lit_occurrence_count; // number of literal occurrences in the formula
  
public:
  CoprocessorData(ClauseAllocator& _ca);
  
  // init all data structures for being used for nVars variables
  void init( uint32_t nVars );
  
  // free all the resources that are used by this data object, 
  void destroy();

  int32_t operator[] (const Lit l ); // return the number of occurrences of literal l
  int32_t operator[] (const Var v ); // return the number of occurrences of variable v
  
  
// adding, removing clauses and literals =======
  void addClause (      const CRef cr );                 // add clause to data structures, update counters
  bool removeClauseFrom (const Minisat::CRef cr, const Lit l); // remove clause reference from list of clauses for literal l, returns true, if successful
  
// formula statistics ==========================
  void addedLiteral(   const Lit l, const int32_t diff = 1);  // update counter for literal
  void removedLiteral( const Lit l, const int32_t diff = 1);  // update counter for literal
  void addedClause (   const CRef cr );                   // update counters for literals in the clause
  void removedClause ( const CRef cr );                 // update counters for literals in the clause
  
};


inline CoprocessorData::CoprocessorData(ClauseAllocator& _ca)
: ca ( _ca )
{
}

inline void CoprocessorData::init(uint32_t nVars)
{
  occs.resize( nVars * 2 );
  lit_occurrence_count.resize( nVars * 2, 0 );
}

inline void CoprocessorData::destroy()
{
  ComplOcc().swap(occs); // free physical space of the vector
  vector<int32_t>().swap(lit_occurrence_count);
}

inline void CoprocessorData::addClause(const Minisat::CRef cr)
{
  Clause & c = ca[cr];
  for (int l = 0; l < c.size(); ++l)
  {
    occs[toInt(c[l])].push_back(cr);
    addedLiteral( c[l] );
  }
}

inline bool CoprocessorData::removeClauseFrom(const Minisat::CRef cr, const Lit l)
{
  vector<CRef>& list = occs[toInt(l)];
  for( int i = 0 ; i < list.size(); ++ i )
  {
    if( list[i] == cr ) {
      list[i] = list[ list.size() - 1 ];
      list.pop_back();
      return true;
    }
  }
  return false;
}

inline void CoprocessorData::addedClause(const Minisat::CRef cr)
{
  Clause & c = ca[cr];
  for (int l = 0; l < c.size(); ++l)
  {
    addedLiteral( c[l] );
  }
}

inline void CoprocessorData::addedLiteral(const Lit l, const  int32_t diff)
{
  lit_occurrence_count[toInt(l)] += diff;
}

inline void CoprocessorData::removedClause(const Minisat::CRef cr)
{

}

inline void CoprocessorData::removedLiteral(Lit l, int32_t diff)
{

}

}

#endif