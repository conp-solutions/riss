/*
 * Abstract class for instance solver in Pcasso
 *
 * Author: Franziska
 *
 */

#ifndef INSTANCESOLVER_H
#define INSTANCESOLVER_H

#include "pcasso/SolverInterface.h"
#include "riss/utils/Statistics-mt.h"    // Statistics
#include "riss/core/Solver.h"
#include "riss/core/SolverTypes.h"
#include <stdint.h>

class TreeNode; // to avoid circular dependencies

namespace Pcasso
{
class InstanceSolver : public SolverInterface
{

  public:
    virtual ~InstanceSolver()
    {
    }

    // from solver
    //

    virtual int         nVars() const = 0;
    virtual Riss::Var   newVar(bool polarity = true, bool dvar = true, char type = 'o') = 0;
    virtual void        setConfBudget(int64_t x) = 0;
    virtual bool        okay() const = 0;
    virtual uint64_t    getStarts() = 0;
    virtual uint64_t    getDecisions() = 0;
    virtual uint64_t    getConflicts() = 0;
    virtual Riss::lbool solveLimited(const Riss::vec<Riss::Lit>& assumps) = 0;
    virtual void        setVerbosity(int verbosity) = 0;
    virtual void        getModel(Riss::vec<Riss::lbool>& model) = 0;
    virtual void        setPolarity(Riss::Var var, bool polarity) = 0;
    virtual bool        getPolarity(Riss::Var var) = 0;
    virtual void        setActivity(Riss::Var var, double activity) = 0;
    virtual double      getActivity(Riss::Var var) = 0;
    virtual void        reserveVars(Riss::Var var) = 0;
    virtual void        interrupt() = 0;
    virtual void        setCommunication(Riss::Communicator& comunicator) = 0;

    // for partitioning tree
    //

    unsigned int        curPTLevel;                    // level of partitioning tree
    unsigned int        unsatPTLevel;                  // level where the unsat formular was found
    TreeNode*           tnode;
    Statistics          localStat;                    // Norbert> Local Statistics

    virtual unsigned int    getNumberOfTopLevelUnits() const = 0;               // return the number of top level units from that are on the trail
    virtual void            setTimeOut(double timeout) = 0;                     // specify a number of seconds that is allowed to be executed
    virtual Riss::Lit       trailGet(const unsigned int index) = 0;             // return a specifit literal from the trail
    virtual unsigned        getLiteralPTLevel(const Riss::Lit& l) const = 0;    // get PT level of literal
    virtual bool            addClause_(Riss::vec<Riss::Lit>& ps, unsigned int level = 0) = 0;
    virtual void            setupForPortfolio(const int nodeLevel) = 0;         // set properties of the solver deoending on node level (for portfolio solver)
};
}

#endif //INSTANCESOLVER_H
