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
  int foundAmos,foundAmts,newAmos,newAlos,newAlks;
  int sameUnits,deducedUnits,propUnits;
  int addDuplicates;
  int irregular, pureAmoLits;
  int usedClauses;
  int cardDiff, discardedCards, discardedNewAmos;
  int removedCards, newCards;
  int addedBinaryClauses,addedClauses;
  int detectedDuplicates;
  
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
    bool amk() const { return k >= 0 && lr.size() == 0 ; }
    bool alo() const { return k == -1 && ll.size() == 0; }
    bool alk() const { return k < 0 && ll.size() == 0; }
    bool isUnit() const { return (k + (int)lr.size()) == 0; } // all literals in ll have to be false, and all literals in lr have to be true
    bool failed() const { return (((int)lr.size() + k) < 0) ; }
    bool taut() const { return k >= (int)ll.size(); } // assume no literal appears both in ll and lr
    bool invalid() const { return k==0 && ll.size() == 0 && lr.size() == 0; } // nothing stored in the constraint any more
    void invalidate() { k=0;ll.clear();lr.clear();}
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
  
protected:
  /** propagate the literals in unitQueue over all constraints*/
  bool propagateCards( vec<Lit>& unitQueue, vector< vector<int> >& leftHands, vector< vector<int> >& rightHands, vector<CardC>& cards,MarkArray& inAmo);
  
  /** check whether the given clause is already present in the given list */
  bool hasDuplicate(const vector<Lit>& c);
  
};

}

#endif