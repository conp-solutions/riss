/**************************************************************************************[Technique.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef TECHNIQUE_HH
#define TECHNIQUE_HH

#include "core/Solver.h"

#include "coprocessor-src/CoprocessorThreads.h"

using namespace Minisat;

namespace Coprocessor {

/** template class for all techniques that should be implemented into Coprocessor
 *  should be inherited by all implementations of classes
 */
class Technique {

  bool modifiedFormula;         // true, if subsumption did something on formula
  bool isInitialized;           // true, if the structures have been initialized and the technique can be used
  uint32_t myDeleteTimer;       // timer to control which deleted variables have been seen already
  
protected:

  ClauseAllocator& ca;          // clause allocator for direct access to clauses
  ThreadController& controller; // controller for parallel execution
    
public:
  
  Technique( ClauseAllocator& _ca, ThreadController& _controller );
  
  /** return whether some changes have been applied since last time
   *  resets the counter after call
   */
  bool appliedSomething();

  /** call this method for each clause when the technique is initialized with the formula 
   *  This method should be overwritten by all techniques that inherit this class
   */
  void initClause( const CRef cr );
  
  /** free resources of the technique, which are not needed until the technique is used next time
   *  This method should be overwritten by all techniques that inherit this class
   */
  void destroy();
  
protected:
  /** call this method to indicate that the technique has applied changes to the formula */
  void didChange();
  
  /** reset counter, so that complete propagation is executed next time 
   *  This method should be overwritten by all techniques that inherit this class
   */
  void reset();

  /// indicate that this technique has been initialized (reset if destroy is called)
  void initializedTechnique();
  
  /// return true, if technique can be used without further initialization
  bool isInitializedTechnique();
  
  /** give delete timer */
  uint32_t lastDeleteTime();
  
  /** update current delete timer */
  void updateDeleteTime( const uint32_t deleteTime );
};

inline Technique::Technique( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller )
: modifiedFormula(false)
, isInitialized( false )
, myDeleteTimer( 0 )
, ca( _ca )
, controller( _controller )
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

inline void Technique::initClause(const CRef cr)
{
  assert( false && "This method has not been implemented." );   
}

inline void Technique::reset()
{
  assert( false && "This method has not been implemented." ); 
}

inline void Technique::initializedTechnique()
{
  isInitialized = true;
}

inline bool Technique::isInitializedTechnique()
{
  return isInitialized;
}


inline void Technique::destroy()
{
  assert( false && "This method has not been implemented." ); 
}

inline uint32_t Technique::lastDeleteTime()
{
  return myDeleteTimer;
}

inline void Technique::updateDeleteTime(const uint32_t deleteTime)
{
  myDeleteTimer = deleteTime;
}


}

#endif