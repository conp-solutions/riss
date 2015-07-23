/*********************************************************************************[MaxsatWrapper.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_MAXSATWRAPPER_HH
#define RISS_MAXSATWRAPPER_HH

#include "coprocessor/Coprocessor.h"

namespace Coprocessor
{

class Mprocessor
{

  public:  // TODO get this right by having getters and setters
    // from coprocessor
    Coprocessor::CP3Config cpconfig;
    Preprocessor* preprocessor;
    Riss::Solver* S;
    Riss::vec<int> literalWeights; /// weights seem to be ints

    // from open-wbo
    int problemType;
    int hardWeight;
    int currentWeight;
    int sumWeights;
    bool hasNonUnitSoftClauses; // indicate whether there are soft clauses that are not a unit clause

    int specVars, specCls; // clauses specified in the header
    int fullVariables;     // variables after parsing the formula

    int debugLevel; // how much

  public:

    Mprocessor(const char* configname);

    ~Mprocessor();

    void setDebugLevel(int level) { debugLevel = level; }

    // from open-wbo maxsat solver
    void setProblemType(int type);       // Set problem type.
    int getProblemType();                // Get problem type.
    int getCurrentWeight();              // Get 'currentWeight'.
    void updateSumWeights(int weight);   // Update initial 'ubCost'.
    void setCurrentWeight(int weight);   // Set initial 'currentWeight'.

    void setHardWeight(int weight);      // Set initial 'hardWeight'.

    int     nVars()      const;             /// The current number of variables.
    Riss::Var     newVar(bool polarity = true, bool dvar = true, char type = 'o');     // Add a new variable with parameters specifying variable mode.

    void addHardClause(Riss::vec<Riss::Lit>& lits);             // Add a new hard clause.

    /** Add a new soft clause.
     *  @return true, if soft clause has been a unit clause
     */
    bool addSoftClause(int weight, Riss::vec< Riss::Lit >& lits);

    void setSpecs(int specifiedVars, int specifiedCls) ;

    /// return that the solver state is still ok (check for unsat)
    bool okay();

    /// apply simplification
    void simplify();

    ///

};

};

#endif
