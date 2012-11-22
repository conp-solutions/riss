/************************************************************************[EquivalenceElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef EQUIVALENCEELIMINATION_HH
#define EQUIVALENCEELIMINATION_HH

#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include "coprocessor-src/Technique.h"
#include "coprocessor-src/Propagation.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement hidden tautology elimination
 */
class EquivalenceElimination : public Technique {
  
  int steps;                   // how many steps is the worker allowed to do

  char* eqLitInStack;			/// mark whether an element is in the stack
  char* eqInSCC;			/// indicate whether a literal has already been found in another SCC (than it cannot be in the current one)
  uint32_t eqIndex;			/// help variable for finding SCCs
  vector< Lit > eqStack;		/// stack for the tarjan algorithm
  vector< int32_t > eqNodeLowLinks;	/// stores the lowest link for a node
  vector< int32_t > eqNodeIndex;	/// index per node
  vector< Lit > eqCurrentComponent;	/// literals in the currently searched SCC

  vector<Lit> replacedBy;              /// stores which variable has been replaced by which literal
  
  char* isToAnalyze;                   /// stores that a literal has to be analyzed further
  vector<Lit> eqDoAnalyze;             /// stores the literals to be analyzed
  
  Propagation& propagation;            /// object that takes care of unit propagation
  
public:
  
  EquivalenceElimination( ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::Propagation& _propagation );
  
  /** run subsumption and strengthening until completion */
  void eliminate(Coprocessor::CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique


protected:

  /** apply equivalences stored in data object to formula
   * @return true, if new binary clauses have been created (for completion)
   */
  bool applyEquivalencesToFormula( CoprocessorData& data );
  
  /** find all strongly connected components on binary implication graph 
   * @param externBig use extern big as basis for tarjan algorithm
   */
  void findEquivalencesOnBig(Coprocessor::CoprocessorData& data, vector< vector< Lit > >* externBig = 0);
  
  /** perform tarjan algorithm to find SCC on binary implication graph */
  void eqTarjan(Lit l, Lit list, Coprocessor::CoprocessorData& data, Coprocessor::BIG& big, vector< vector< Lit > >* externBig = 0);
  
  /** returns the literal, that represents the Equivalence-class of l */
  Lit getReplacement(Lit l ) const;
  
  /** sets literal replacement, fails if not possible 
   * @return false, if this equivalence results in a conflict
   */
  bool setEquivalent(Lit representative, Lit toReplace);
};

}

#endif