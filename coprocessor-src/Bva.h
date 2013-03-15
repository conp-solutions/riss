/*******************************************************************************************[BVA.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef BVA_HH
#define BVA_HH

#include "core/Solver.h"
#include "coprocessor-src/Technique.h"
#include "coprocessor-src/CoprocessorTypes.h"

using namespace Minisat;

namespace Coprocessor {

/** this class is used for bounded variable addition (replace patterns by introducion a fresh variable)
 */
class BoundedVariableAddition : public Technique  {
    
  CoprocessorData& data;
  
  // attributes for current run
  bool doSort;			// ensure that all clauses are sorted afterwards (assume, they are sorted before)
  
  // statistics
  uint32_t duplicates;		// how many duplicate clauses have been found
  uint32_t complementCount;	// how many complementary literals have been found (strengthening)
  uint32_t replacements;	// how many new variables could be introduced
  uint32_t totalReduction;	// how many clauses have been reduced
  uint32_t replacedOrs;		// how many disjunctions could be replaced by the fresh variable
  uint32_t replacedMultipleOrs;	// how many times could multiple or gates be replaced
  double processTime;		// seconds of process time
  
  
  // work data
  /// compare two literals
  struct LitOrderBVAHeapLt {
        CoprocessorData & data;
        bool operator () (int& x, int& y) const {
	    return data[ toLit(x)] < data[toLit(y)]; 
        }
        LitOrderBVAHeapLt(CoprocessorData & _data) : data(_data) {}
  };
  Heap<LitOrderBVAHeapLt> bvaHeap; // heap that stores the variables according to their frequency (dedicated for BVA)
  // structures that would be created on during functions again and again
  vector< vector< CRef > > bvaMatchingClauses; // found pairs of clauses
  vector< Lit > bvaMatchingLiterals; // literals that stay in the match
  // use general mark array!
  vector< Lit > bvaCountMark;	// mark literal candidates (a) for the current literal(b)
  vector< uint32_t > bvaCountCount; // count occurence of a together with b
  vector< uint64_t > bvaCountSize; // count occurence of a together with b
  vec<Lit> clauseLits;			// vector that is added for clause definitions
 
public:
  BoundedVariableAddition( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data );
  
  void reset();
  
  /** applies bounded variable addition algorithm
  * @return true, if something has been altered
  */
  bool variableAddtion(bool sort = true);

  /** prototype implementation of a BVA version that can replace multiple literals
  */
  bool variableAddtionMulti(bool sort = true);
  
  void printStatistics(ostream& stream);

  void destroy();
  
protected:
  
  /** sub-routine of BVA to handle complementary literals
  * @param right literal that represents the right side
  * @return false, if shrinking a clause to unit led to a failed enqueue (UNSAT)
  */
  bool bvaHandleComplement( const Lit right );

  /** introduce a fresh variable, update the size of all required structures*/
  Var nextVariable();

  /** check data structures */
  bool checkLists(const string& headline);
  
  /** other bva like formula analysis */
  void analysis();
  
public:
  // parameters
  bool bvaComplement;		/// treat found complements special?
  uint32_t bvaPush;		/// which literals to push to queue again (0=none,1=original,2=all)
  bool bvaRewEE;		/// run rewEE after BVA found new gates?
  uint32_t bvaLimit;		/// number of checks until bva is aborted
  bool bvaRemoveDubplicates;	/// remove duplicate clauses from occurrence lists
  bool bvaSubstituteOr;	/// when c = (a AND b) is found, also replace (-a OR -b) by -c
};

};

#endif