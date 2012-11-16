/*****************************************************************************[ClauseElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef CLAUSEELIMINATION_HH
#define CLAUSEELIMINATION_HH

#include "core/Solver.h"

#include "coprocessor-src/Technique.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement subsumption and strengthening, and related techniques
 */
class ClauseElimination : public Technique {
  
  vector<CRef> clause_processing_queue;
  
public:
  
  class WorkData {
  public:
    vector<Lit> cla;       // literals that have been added by cla
    MarkArray array;       // literals that have been marked by ala or cla
    MarkArray helpArray;   // literals that will be inside CLA(F,C) per step
    vector<Lit> toProcess; // literals that still need to be processed
    vector<Lit> toUndo;    // literals that have to be out to the undo information, if a cla clause is removed by ATE or ABCE
    int nextAla;           // position from which ala needs to be continued
    
    WorkData(int vars) : nextAla(0) { array.create(2*vars); helpArray.create(2*vars);}
    ~WorkData() {array.destroy();helpArray.destroy();}
    void reset () { cla.clear(); array.nextStep(); toProcess.clear(); toUndo.clear(); nextAla=0; }
  };
  
  ClauseElimination( ClauseAllocator& _ca, ThreadController& _controller );

  void eliminate(CoprocessorData& data);
  
  void initClause(const CRef cr); // inherited from Technique
  
protected:
  
  /** try to run CCE on clause cr, return true if clause has been removed */
  bool eliminate(Coprocessor::CoprocessorData& data, Coprocessor::ClauseElimination::WorkData& wData, Minisat::CRef cr);
  
  
  /*
   *  Parallel Stuff later!!
   */
  
  /** data for parallel execution */
  struct EliminationWorkData {
    ClauseElimination*     subsumption; // class with code
    CoprocessorData* data;        // formula and maintain lists
    unsigned int     start;       // partition of the queue
    unsigned int     end;
  };

  /** run parallel subsumption with all available threads */
  void parallelElimination(CoprocessorData& data);
  
public:

  /** converts arg into SubsumeWorkData*, runs subsumption of its part of the queue */
  static void* runParallelElimination(void* arg);

};

}

#endif