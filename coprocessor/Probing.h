/***************************************************************************************[Probing.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef PROBING_HH
#define PROBING_HH

#include "riss/core/Solver.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"

#include "coprocessor/Propagation.h"
#include "coprocessor/EquivalenceElimination.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

// forward declaration

/** class that implements probing techniques */
class Probing : public Technique
{

    CoprocessorData& data;
    Riss::Solver& solver;
    Propagation& propagation;         // object that takes care of unit propagation
    EquivalenceElimination& ee;       // object that takes care of equivalent literal elimination

    // necessary local variables
    std::vector<Riss::Var> variableHeap;
    Riss::vec<Riss::Solver::VarFlags> prPositive;
    Riss::vec<Riss::Solver::VarFlags> prL2Positive;

    Riss::vec<Riss::Lit> learntUnits;
    std::vector<Riss::Lit> doubleLiterals;
    std::vector<Riss::CRef> l2conflicts;
    std::vector<Riss::CRef> l2implieds;


  public:
    Probing(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation, EquivalenceElimination& _ee, Riss::Solver& _solver);

    /** perform probing and clause vivification
     * @return false, if formula is UNSAT
     */
    bool process();

    /** This method should be used to print the statistics of the technique that inherits from this class
     */
    void printStatistics(std::ostream& stream);

    void destroy();

    void giveMoreSteps();

  protected:

    /** perform special propagation for probing (track ternary clauses, LHBR if enabled) */
    Riss::CRef prPropagate(bool doDouble = true);

    /** perform conflict analysis and enqueue each unit clause that could be learned
     * note: prLits contains the learned clause (not necessarily 1st UIP!)
     * @return false, nothing has been learned
     */
    bool prAnalyze(Riss::CRef confl);

    /** perform double look ahead on literals that have been traced during level1 probing
     * @return true, if procedure jumped back at level 0
     */
    bool prDoubleLook(Riss::Lit l1decision);

    /** perform probing */
    void probing();

    /** perform clause vivification */
    void clauseVivification();

    /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
    void reSetupSolver();

    /** remove all clauses from the watch lists inside the solver */
    void cleanSolver();

    /** sort literals in clauses */
    void sortClauses();

    // staistics
    unsigned probeLimit;      // step limit for probing
    unsigned probeChecks;     // number of performed steps
    unsigned probeLHBRs;      // number of clauses that have been added by LHBR
    double processTime;       // seconds for probing
    unsigned l1implied;       // number of found l1 implied literals
    unsigned l1failed;        // number of found l1 failed literals
    unsigned l1learntUnit;    // learnt unit clauses
    unsigned l1ee;        // number of found l1 equivalences
    unsigned l2implied;       // number of literals implied on level2
    unsigned l2failed;        // number of found l2 failed literals
    unsigned l2ee;        // number of found l2 equivalences
    unsigned totalL2cand;     // number of l2 probe candidates
    unsigned probes;      // number of probes
    unsigned probeCandidates; // number of probing candidates
    unsigned l2probes;        // number of l2 probes
    double viviTime;      // seconds spend for vivification
    unsigned viviLits;        // number of removed literals through vivification
    unsigned viviCls;     // number of clauses modified by vivification
    unsigned viviCands;       // number of clauses candidates for vivification
    unsigned viviLimit;       // step limit for vivification
    unsigned viviChecks;      // number of steps performed by vivification
    unsigned viviSize;        // size of clauses that are vivified
    unsigned lhbr_news;       // number of clauses that have been added by lhbr
};

};

#endif
