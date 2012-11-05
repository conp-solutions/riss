/***********************************************************************************[Propagation.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef PROPAGATION_HH
#define PROPAGATION_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;

/** this class is used for usual unit propagation, probing and distillation/asyymetric branching
 */
class Propagation : public Technique  {
  /*  TODO: add queues and other attributes here!
   */
  uint32_t lastPropagatedLiteral;  // store, which literal position in the trail has been propagated already to avoid duplicate work
  
  
public:
  
  Propagation( ClauseAllocator& _ca );
  

  void reset();
  
  /** perform usual unit propagation, but shrinks clause sizes also physically
   *  will run over all clauses with satisfied/unsatisfied literals (that have not been done already)
   *  @return l_Undef, if no conflict has been found, l_False if there has been a conflict
   */
  lbool propagate(CoprocessorData& data, Solver* solver);
};

#endif