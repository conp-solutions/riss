/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef SUBSUMPTION_HH
#define SUBSUMPTION_HH

#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;

class Subsumption {
  ClauseAllocator& ca;
  
  /*  TODO: add queues and other attributes here!
   */
  
public:
  
  Subsumption( ClauseAllocator& _ca );
  
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */
  
  
  bool hasToSubsume();       // return whether there is something in the subsume queue
  lbool fullSubsumption();   // performs subsumtion until completion
  
  bool hasToStrengthen();    // return whether there is something in the strengthening queue
  lbool fullStrengthening(); // performs strengthening until completion, puts clauses into subsumption queue
};

#endif