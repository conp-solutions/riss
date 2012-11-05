/**************************************************************************************[Technique.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef TECHNIQUE_HH
#define TECHNIQUE_HH

#include "core/Solver.h"

using namespace Minisat;

/** template class for all techniques that should be implemented into Coprocessor
 *  should be inherited by all implementations of classes
 */
class Technique {

  bool modifiedFormula;  // true, if subsumption did something on formula
    
protected:
  
  ClauseAllocator& ca;  // clause allocator for direct access to clauses
    
public:
  
  Technique( ClauseAllocator& _ca );
  
  /** return whether some changes have been applied since last time
   *  resets the counter after call
   */
  bool appliedSomething();
  
protected:
  void didChange();
  
  /** reset counter, so that complete propagation is executed next time 
   *  This method should be overwritten by all clauses that inherit this class
   */
  void reset();

};

inline Technique::Technique( ClauseAllocator& _ca )
: modifiedFormula(false)
, ca( _ca )
{}

inline bool Technique::appliedSomething()
{
  bool modified = modifiedFormula;
  modifiedFormula = false;
  return modified; 
}

inline void Technique::didChange()
{
  modifiedFormula = true;
}

inline void Technique::reset()
{
  assert( false && "This method has not been implemented." ); 
}

#endif