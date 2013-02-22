/*************************************************************************************[Resolving.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RESOLVING_H
#define RESOLVING_H

#include "coprocessor-src/CoprocessorTypes.h"

#include "coprocessor-src/Technique.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {

class Resolving  : public Technique
{

public:
  Resolving(ClauseAllocator& _ca, ThreadController& _controller);

  void process(CoprocessorData& data, bool post = false);

  /** inherited from @see Technique */
  void printStatistics( ostream& stream );
  
protected:
  
  void ternaryResolve(CoprocessorData& data);
  
  void addRedundantBinaries(CoprocessorData& data);

  
  double processTime;
  unsigned addedTernaries;
  unsigned addedBinaries;
};


}

#endif // RESOLVING_H
