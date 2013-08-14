/*******************************************************************************************[BCE.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef BCE_HH
#define BCE_HH

#include "core/Solver.h"
#include "coprocessor-src/Technique.h"
#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;

namespace Coprocessor {

/** this class is used for blocked clause elimination procedure
 */
class BlockedClauseElimination : public Technique  {
    
  CoprocessorData& data;
  
public:
  BlockedClauseElimination( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data );

  void reset();
  
  /** applies blocked clause elimination algorithm
  * @return true, if something has been altered
  */
  bool process();
    
  void printStatistics(ostream& stream);

  void giveMoreSteps();
  
  void destroy();
};

}

#endif