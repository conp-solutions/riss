/***************************************************************************************[Circuit.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef CIRCUIT_HH
#define CIRCUIT_HH

#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** This class implement hidden tautology elimination
 */
class Circuit {
  ClauseAllocator& ca;
  
  struct ternary{ Lit l1,l2;
    ternary(Lit _l1, Lit _l2 ) : l1(_l1), l2(_l2) {}
  };
  struct quad{ Lit l1,l2,l3;
    quad(Lit _l1, Lit _l2, Lit _l3 ) : l1(_l1), l2(_l2), l3(_l3) {}
  };
  
  BIG* big;                            // binary implication graph
  vector< vector<ternary> > ternaries; // ternary clauses per literal
  vector< vector<quad> > quads;        // quad-clauses per literal, cref for ca
  
public:
  
  Circuit (ClauseAllocator& _ca );
  
  class Gate {
    Lit lits[4];

public:
    enum Type { // kind of the gate
      AND,      // AND gate
      AMO,      // at-most-one gate
      GenAND,   // generic AND
      ITE,      // if-then-else gate
      XOR,      // XOR gate
      HA_SUM,   // half adder sum bit
      INVALID,  // not a gate
    };
    
    enum Encoded { // which clauses have been found in the formula
      FULL,
      POS_BLOCKED,
      NEG_BLOCKED
    };
    
    Gate(const Clause& c, const Type _type, const Encoded e, const Lit output = lit_Undef); // generic AND
    Gate(Lit x, Lit s, Lit t, Lit f, const  Type _type); // ITE, HA-SUM
    Gate(Lit x, Lit a, Lit b, const Type _type); // AND, XOR
     
    const Type getType() const { return type; }
    
    bool isInvalid() const { return type == INVALID ; }
    
  private:
    Type type;
    Encoded encoded;

    Lit& x () {assert (type != XOR && "gate cannot be XOR"); return lits[0]; } // output 
    Lit& a () {assert (type != ITE && "gate cannot be ITE"); return lits[1]; } // AND, HA-SUM
    Lit& b () {assert (type != ITE && "gate cannot be ITE"); return lits[2]; } // AND, HA-SUM
    Lit& c () {assert (type == HA_SUM && "gate has to be HA_SUM"); return lits[3]; } // HA-SUM
    Lit& s () {assert (type == ITE && "gate has to be ITE"); return lits[1]; } // ITE selector
    Lit& t () {assert (type == ITE && "gate has to be ITE"); return lits[2]; } // ITE true branch
    Lit& f () {assert (type == ITE && "gate has to be ITE"); return lits[3]; } // ITE false branch

  };
  
  
  int extractGates( CoprocessorData& data, vector< Gate >& gates );
  
private:
  
  /** check whether this variable is the output an AND gate
   *  XOR gates will be given only for the smallest variable in the gate, because their output cannot be identified
   */
  void getGatesWithOutput(const Var v, vector<Gate>& gates);

  /** method that looks for AND gates (multiple AND might form an AMO gate!)*/
  void getANDGates(const Var v, vector<Gate>& gates);
  /** one AMO can be understood as many AND gates! */
  void getAMO_ANDGates(const Var v, vector<Gate>& gates);
  /** method that looks for XOR gate( stores it only in list, if v is the smallest variable of the gate)*/
  void getXORGates(const Var v, vector<Gate>& gates);
  /** method that looks for ITE gates */
  void getITEGates(const Var v, vector<Gate>& gates);
  /** method that looks for half adder sum gates (have gate only if v is smallest variale) */
  void getHASUMGates(const Var v, vector<Gate>& gates);
};


};

#endif