/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef SUBSUMPTION_HH
#define SUBSUMPTION_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;

/** This class implement subsumption and strengthening, and related techniques
 */
class Subsumption : public Technique {
  
public:
  
  Subsumption( ClauseAllocator& _ca );
  
  
  /** run subsumption and strengthening until completion */
  void subsumeStrength();
  
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */

protected:
  
  bool hasToSubsume();       // return whether there is something in the subsume queue
  lbool fullSubsumption();   // performs subsumtion until completion
  
  bool hasToStrengthen();    // return whether there is something in the strengthening queue
  lbool fullStrengthening(); // performs strengthening until completion, puts clauses into subsumption queue
};

#endif