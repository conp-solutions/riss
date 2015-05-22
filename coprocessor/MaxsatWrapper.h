/*********************************************************************************[MaxsatWrapper.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef MAXSATWRAPPER_HH
#define MAXSATWRAPPER_HH

#include "coprocessor/Coprocessor.h"

namespace Coprocessor {

class Mprocessor {

public:  // TODO get this right by having getters and setters
  // from coprocessor
  Coprocessor::CP3Config cpconfig;
  Preprocessor* preprocessor;
  Solver* S;
  vec<int> literalWeights; /// weights seem to be ints
  
  // from open-wbo
  int problemType;
  int hardWeight; 
  int currentWeight;
  int sumWeights;
  
  int specVars, specCls; // clauses specified in the header
  int fullVariables;     // variables after parsing the formula
  
public:
  
  Mprocessor( const char* configname );
  
  ~Mprocessor();
  
  // from open-wbo maxsat solver
  void setProblemType(int type);       // Set problem type.
  int getProblemType();                // Get problem type.
  int getCurrentWeight();              // Get 'currentWeight'.
  void updateSumWeights(int weight);   // Update initial 'ubCost'.
  void setCurrentWeight(int weight);   // Set initial 'currentWeight'.
  
  void setHardWeight(int weight);      // Set initial 'hardWeight'.

  int     nVars      ()      const;       /// The current number of variables.
  Var     newVar    (bool polarity = true, bool dvar = true, char type = 'o'); // Add a new variable with parameters specifying variable mode.
  
  void addHardClause(vec<Lit> &lits);             // Add a new hard clause.
  void addSoftClause(int weight, vec<Lit> &lits); // Add a new soft clause.
  
  void setSpecs( int specifiedVars, int specifiedCls ) ;
  
  /// return that the solver state is still ok (check for unsat)
  bool okay();
  
  /// apply simplification
  void simplify();
  
  /// 
  
};

};

#endif