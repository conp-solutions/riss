/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTYPES_HH
#define COPROCESSORTYPES_HH

#include "core/Solver.h"

#include "coprocessor-src/LockCollection.h"

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
  Solver* solver; 
  /* TODO to add here
   * occ list
   * counters
   * methods to update these structures
   * no statistical counters for each method, should be provided by each method!
   */
  uint32_t numberOfVars;                // number of variables
  uint32_t numberOfCls;                 // number of clauses
  ComplOcc occs;                        // list of clauses, per literal
  vector<int32_t> lit_occurrence_count; // number of literal occurrences in the formula
  
  Lock dataLock;                        // lock for parallel algorithms to synchronize access to data structures
  
  // TODO decide whether a vector of active variables would be good!
  
public:
  CoprocessorData(ClauseAllocator& _ca, Solver* _solver);
  
  // init all data structures for being used for nVars variables
  void init( uint32_t nVars );
  
  // free all the resources that are used by this data object, 
  void destroy();

  int32_t& operator[] (const Lit l ); // return the number of occurrences of literal l
  int32_t operator[] (const Var v ); // return the number of occurrences of variable v
  vector<CRef>& list( const Lit l ); // return the list of clauses, which have literal l
  uint32_t nCls()  const { return numberOfCls; }
  uint32_t nVars() const { return numberOfVars; }
  
// adding, removing clauses and literals =======
  void addClause (      const CRef cr );                 // add clause to data structures, update counters
  bool removeClauseFrom (const Minisat::CRef cr, const Lit l); // remove clause reference from list of clauses for literal l, returns true, if successful
  
// formula statistics ==========================
  void addedLiteral(   const Lit l, const int32_t diff = 1);  // update counter for literal
  void removedLiteral( const Lit l, const int32_t diff = 1);  // update counter for literal
  void addedClause (   const CRef cr );                   // update counters for literals in the clause
  void removedClause ( const CRef cr );                 // update counters for literals in the clause
  
  void correctCounters();
  
};


inline CoprocessorData::CoprocessorData(ClauseAllocator& _ca, Solver* _solver)
: ca ( _ca )
, solver( _solver )
{
}

inline void CoprocessorData::init(uint32_t nVars)
{
  occs.resize( nVars * 2 );
  lit_occurrence_count.resize( nVars * 2, 0 );
  numberOfVars = nVars;
}

inline void CoprocessorData::destroy()
{
  ComplOcc().swap(occs); // free physical space of the vector
  vector<int32_t>().swap(lit_occurrence_count);
}

inline void CoprocessorData::addClause(const Minisat::CRef cr)
{
  const Clause & c = ca[cr];
  if( c.can_be_deleted() ) return;
  for (int l = 0; l < c.size(); ++l)
  {
    occs[toInt(c[l])].push_back(cr);
    addedLiteral( c[l] );
  }
  numberOfCls ++;
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
  const Clause & c = ca[cr];
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
  const Clause & c = ca[cr];
  for (int l = 0; l < c.size(); ++l)
  {
    removedLiteral( c[l] );
  }
  numberOfCls --;
}

inline void CoprocessorData::removedLiteral(Lit l, int32_t diff)
{
  lit_occurrence_count[toInt(l)] -= diff;
}

inline int32_t& CoprocessorData::operator[](const Lit l)
{
  return lit_occurrence_count[toInt(l)];
}

inline int32_t CoprocessorData::operator[](const Var v)
{
  return lit_occurrence_count[toInt(mkLit(v,0))] + lit_occurrence_count[toInt(mkLit(v,1))];
}

inline vector< Minisat::CRef >& CoprocessorData::list(const Lit l)
{
  return occs[ toInt(l) ];
}

inline void CoprocessorData::correctCounters()
{
  numberOfVars = solver->nVars();
  numberOfCls = 0;
  // reset to 0
  for (int v = 0; v < solver->nVars(); v++)
    for (int s = 0; s < 2; s++)
      lit_occurrence_count[ toInt(mkLit(v,s)) ] = 0;
  // re-calculate counters!
  for( int i = 0 ; i < solver->clauses.size(); ++ i ) {
    const Clause & c = ca[ solver->clauses[i] ];
    if( c.can_be_deleted() ) continue;
    numberOfCls ++;
    for( int j = 0 ; j < c.size(); j++) lit_occurrence_count[ toInt(c[j]) ] ++; // increment all literal counters accordingly
  }    
  for( int i = 0 ; i < solver->learnts.size(); ++ i ) {
    const Clause & c = ca[ solver->learnts[i] ];
    if( c.can_be_deleted() ) continue;
    numberOfCls ++;
    for( int j = 0 ; j < c.size(); j++) lit_occurrence_count[ toInt(c[j]) ] ++; // increment all literal counters accordingly
  }    
}


}

#endif