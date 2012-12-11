/************************************************************************[EquivalenceElimination.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef EQUIVALENCEELIMINATION_HH
#define EQUIVALENCEELIMINATION_HH

#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Circuit.h"

#include "coprocessor-src/Technique.h"
#include "coprocessor-src/Propagation.h"
#include "coprocessor-src/Subsumption.h"

#include <vector>
#include <deque>

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
  Subsumption& subsumption;		/// object that takes care of subsumption and strengthening
  
public:
  
  EquivalenceElimination( ClauseAllocator& _ca, ThreadController& _controller, Propagation& _propagation, Subsumption& _subsumption  );
  
  /** run equivalent literal elimination */
  void eliminate(CoprocessorData& data);

  void initClause(const CRef cr); // inherited from Technique

protected:

  /** apply equivalences stored in data object to formula
   * @return true, if new binary clauses have been created (for completion)
   */
  bool applyEquivalencesToFormula( CoprocessorData& data );
  
  /** check based on gates that have been extracted, whether more equivalent literals can be found!
   * @return true, if new equivalent literals have been found
   */
  bool findGateEquivalences( CoprocessorData& data, vector< Circuit::Gate > gates );
  bool findGateEquivalencesNew(CoprocessorData& data, vector< Circuit::Gate >& gates);
  
  /** find all strongly connected components on binary implication graph 
   * @param externBig use extern big as basis for tarjan algorithm
   */
  void findEquivalencesOnBig(CoprocessorData& data, vector< vector< Lit > >* externBig = 0);
  
  /** return literals that have to be equivalent because of the two gates 
   * @param replacedBy stores for each variable the literal that represents its equivalence class
   */
  bool checkEquivalence( const Circuit::Gate& g1, const Circuit::Gate& g2, Lit& e1, Lit& e2);
  
  /** perform tarjan algorithm to find SCC on binary implication graph */
  void eqTarjan(Lit l, Lit list, CoprocessorData& data, BIG& big, vector< vector< Lit > >* externBig = 0);

  /** check whether the clause c has duplicates in the list of literal l
   *  Note: assumes that all clauses are sorted!
   *  @return true, if there are duplicates, so that c can be deleted
   */
  bool hasDuplicate( vector< Minisat::CRef >& list, const Clause& c );
  
  /** check whether this gate can be processed for equivalence checks */
  bool allInputsStamped(Circuit::Gate& g, vector< unsigned int >& bitType);
  
  /** check the current gate for equivalent literals, enqueue them to the "replacedBy" structure, invalidate the gate */
  void processGate       (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable);
  
  
  void processANDgate    (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque< Var >* activeVariables=0);
  void processGenANDgate (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque< Var >*  activeVariables=0);
  void processExOgate    (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque<Var>* activeVariables=0);
  void processITEgate    (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque< Var >*  activeVariables=0);
  void processXORgate    (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque< Var >*  activeVariables=0);
  void processFASUMgate  (CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, deque< int >& queue, vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active = 0, deque< Var >*  activeVariables=0);
  
  /** enqueue all successor gates of the given gate g into the queue, stamp output variables, have a limit when to stop?! */
  void enqueueSucessorGates(Circuit::Gate& g, deque< int > queue, vector<Circuit::Gate>& gates, vector< unsigned int >& bitType, vector< vector<int32_t> >& varTable);
  
  /** write the AIGER circuit that can be found based on the clauses in the formula to a file in aag format */
  void writeAAGfile( CoprocessorData& data );
  
  /** returns the literal, that represents the Equivalence-class of l */
  Lit getReplacement(Lit l ) const;
  
  /** sets literal replacement, fails if not possible 
   * @return false, if this equivalence results in a conflict
   */
  bool setEquivalent(Lit representative, Lit toReplace);
};

}

#endif