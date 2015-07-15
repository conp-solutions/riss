/*
 * This class enhances the SAT-Solver Riss to work within Pcasso.
 *
 *
 * Author: Franziska Krueger
 *
 */

#ifndef SOLVEPRRISS_H
#define SOLVEPRRISS_H

#include "pfolio/PSolver.h"
#include "pcasso/InstanceSolver.h"

namespace Pcasso
{
class SolverPriss : public InstanceSolver
{
  public:
    SolverPriss(Riss::PfolioConfig *config, int threadsToUse);

    ~SolverPriss();

    Riss::PSolver solver;

    unsigned int        curPTLevel;         // Davide> Contains the pt_level of current node
    unsigned int        unsatPTLevel;
    TreeNode*           tnode;
    Statistics          localStat;        // Norbert> Local Statistics

    // forwarding to Riss
    Riss::Var           newVar(bool polarity = true, bool dvar = true, char type = 'o');
    bool                addClause_(Riss::vec<Riss::Lit>& ps, unsigned int pt_level = 0);
    inline int          nVars() const;
    inline bool         okay() const;
    inline void         interrupt();
    inline Riss::lbool  solveLimited(const Riss::vec<Riss::Lit>& assumps);
    inline void         setConfBudget(int64_t x);
    inline void         setPolarity(Riss::Var var, bool polarity);
    inline bool         getPolarity(Riss::Var var);
    inline void         setActivity(Riss::Var var, double activity);
    inline double       getActivity(Riss::Var var);
    inline void         reserveVars(Riss::Var var);

    // accessing members of Riss
    inline void         setVerbosity(int verbosity);
    inline uint64_t     getStarts();
    inline uint64_t     getDecisions();
    inline uint64_t     getConflicts();
    inline void         getModel(Riss::vec<Riss::lbool>& model);
    inline Riss::Lit    trailGet(const unsigned int index);         // return a specifit literal from the trail

    // for partitioning tree
    void                setupForPortfolio(const int nodeLevel);
    void                setTimeOut(double timeout);                               // FIXME: not implemented
    inline unsigned int getNumberOfTopLevelUnits() const;                         // returns the trail size of Riss
    inline unsigned int getLiteralPTLevel(const Riss::Lit& l) const { return 0; } // FIXME: return correct PT level

  private:
    Riss::PfolioConfig*   coreConfig;
    // TODO: varPT not working yet
    Riss::vec<unsigned> varPT;             // storing the PT level of each variable // FIXME Norbert: this has to be implemented for Priss, please send a kind reminder "in time" :)
};
}

#endif //SOLVERPRISS_H
