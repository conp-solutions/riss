/***************************************************************************************[Probing.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef PROBING_HH
#define PROBING_HH

#include "core/Solver.h"
#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Technique.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {
  
/** class that implements probing techniques */
class Probing : public Technique {
  
  CoprocessorData& data;
  
public:
  Probing( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data);
  
  bool probe();
  
  /** This method should be used to print the statistics of the technique that inherits from this class
   */
  void printStatistics( ostream& stream );
};
  
};

#endif