/********************************************************************************[FourierMotzkin.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef FOURIERMOTZKIN_HH
#define FOURIERMOTZKIN_HH

#include "core/Solver.h"
#include "coprocessor-src/Technique.h"
#include "coprocessor-src/CoprocessorTypes.h"

#include "coprocessor-src/Propagation.h"

using namespace Minisat;

namespace Coprocessor {

/** this class is used for the fourier motzkin procedure on extracted cardinality constraints
 */
class FourierMotzkin : public Technique  {
    
  CoprocessorData& data;
  Propagation& propagation;
  
  double processTime,amoTime,fmTime;
  int steps;
  int fmLimit;
  int foundAmos;
  int sameUnits,deducedUnits;
  int addDuplicates;
  int irregular;
  int usedClauses;
  int discardedCards;
  int removedCards, newCards;
  
  /** represent a (mixed) cardinality constraint*/
  class CardC {
  public:
    vector<Lit> ll;
    vector<Lit> lr;
    int k;
    CardC( vector<Lit>& amo ) : ll(amo), k(1) {}; // constructor to add amo constraint
    CardC( const Clause& c ) :k(-1) { lr.resize(c.size(),lit_Undef); for(int i = 0 ; i < c.size(); ++i ) lr[i] = c[i]; }// constructor for usual clauses
    bool amo() const { return k == 1 && lr.size() == 0 ; }
    bool amt() const { return k == 2 && lr.size() == 0 ; }
    bool isUnit() const { return (k + (int)lr.size()) == 0; }
    bool failed() const { return ((int)lr.size() + k) < 0; }
    bool taut() const { return k > (int)ll.size(); } // assume no literal appears both in ll and lr
    CardC() : k(0) {} // default constructor
  };
  
  /// compare two literals
  struct LitOrderHeapLt {
        CoprocessorData & data;
        bool operator () (int& x, int& y) const {
	    return data[ toLit(x)] > data[toLit(y)]; 
        }
        LitOrderHeapLt(CoprocessorData & _data) : data(_data) {}
  };
  
public:
  FourierMotzkin( ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation );

  void reset();
  
  /** extractes cardinality constraints and applies the fourier motzkin algorithm
  * @return true, if something has been altered
  */
  bool process();
    
  void printStatistics(ostream& stream);

  void giveMoreSteps();
  
  void destroy();
};

}

#endif