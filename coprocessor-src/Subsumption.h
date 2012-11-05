/************************************************************************************[Subsumption.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef SUBSUMPTION_HH
#define SUBSUMPTION_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement subsumption and strengthening, and related techniques
 */
class Subsumption : public Technique {
  
  vector<CRef> clause_processing_queue;
  
public:
  
  Subsumption( ClauseAllocator& _ca );
  
  
  /** run subsumption and strengthening until completion */
  void subsumeStrength(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique
  
  /* TODO:
   *  - init
   *  - add to queue
   * stuff for parallel subsumption
   *  - struct for all the parameters that have to be passed to a thread that should do subsumption
   *  - static method that performs subsumption on the given part of the queue without stat updates
   */

protected:
  
  bool hasToSubsume();       // return whether there is something in the subsume queue
  lbool fullSubsumption(CoprocessorData& data);   // performs subsumtion until completion
  void subsumption_worker (CoprocessorData& data, unsigned start, unsigned end); // subsume certain set of elements of the processing queue
  
  bool hasToStrengthen();    // return whether there is something in the strengthening queue
  lbool fullStrengthening(CoprocessorData& data); // performs strengthening until completion, puts clauses into subsumption queue
  
  
  

};

}

#endif