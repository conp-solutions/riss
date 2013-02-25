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

  vector<int> seen; // remembers how many clauses per variable have been processed already
  
public:
  Resolving(ClauseAllocator& _ca, ThreadController& _controller);

  void process(CoprocessorData& data, bool post = false);

  /** inherited from @see Technique */
  void printStatistics( ostream& stream );
  
protected:
  
  /** resolve ternary clauses */
  void ternaryResolve(CoprocessorData& data);
  
  /** add redundant binary clauses */
  void addRedundantBinaries(CoprocessorData& data);

  /** check whether this clause already exists in the occurence list */
  bool hasDuplicate(vector< Minisat::CRef >& list, const vec< Lit >& c);
  
  /**
  * expects c to contain v positive and d to contain v negative
  * @return true, if resolvent is satisfied
  *         else, otherwise
  */
  bool resolve(const Clause & c, const Clause & d, const int v, vec<Lit> & resolvent);
  
  bool checkPush(vec<Lit> & ps, const Lit l);
  
  double processTime;
  unsigned addedTern2;
  unsigned addedTern3;
  unsigned addedBinaries;
};


}

#endif // RESOLVING_H
