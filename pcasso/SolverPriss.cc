#include "pcasso/SolverPriss.h"

using namespace Riss;
using namespace std;

namespace Pcasso
{
SolverPriss::SolverPriss(PfolioConfig *config, int threadsToUse) :
    solver(config, 0, threadsToUse), // setup solver with given configuration, no extra default name, and the given number of threads
    coreConfig(config),
    curPTLevel(0)
{
}

SolverPriss::~SolverPriss()
{

}

Var SolverPriss::newVar(bool polarity, bool dvar, char type)
{
    return solver.newVar(polarity, dvar, type);
}

/**
* Lets Riss add the clause and saves the pt level for the new clause.
*/
bool SolverPriss::addClause_(vec<Lit>& ps, unsigned int pt_level)
{
    return solver.addClause_(ps);
}

void SolverPriss::setTimeOut(double timeout)
{
    // TODO: not used jet
}

/**
* Inits some solver variables depending on the node level.
* This is used for portfolio solver.
*/
void SolverPriss::setupForPortfolio(const int nodeLevel)
{
    //FIXME: does nothing
//    if (nodeLevel == 1) {
//        solver.K = 0.8;
//        solver.R = 1.5;
//    } else if (nodeLevel == 2) {
//        solver.firstReduceDB = 10000;
//    } else if (nodeLevel == 3) {
//        solver.rnd_init_act = true;
//    } else if (nodeLevel == 4) {
//        solver.sizeLBDQueue = 150;
//    } else { // default diversification
//        solver.rnd_init_act = true;
//        solver.rnd_pol = true;
//    }
}

// forwarded methods to Riss
inline int      SolverPriss::nVars() const               { return solver.nVars(); }
inline bool     SolverPriss::okay() const                { return true; } // FIXME: solver.okay(); }
inline void     SolverPriss::interrupt()                 { solver.interrupt(); }
inline lbool    SolverPriss::solveLimited(const Riss::vec<Riss::Lit>& assumps) { return solver.solveLimited(assumps); }
inline void     SolverPriss::setConfBudget(int64_t x)    { solver.setConfBudget(x); }

inline void     SolverPriss::setVerbosity(int verbosity) { solver.verbosity = verbosity; }
inline uint64_t SolverPriss::getStarts()                 { return 0; } // FIXME: solver.starts; }
inline uint64_t SolverPriss::getDecisions()              { return 0; } // FIXME: solver.decisions; }
inline uint64_t SolverPriss::getConflicts()              { return 0; } // FIXME: solver.conflicts; }
inline void     SolverPriss::getModel(Riss::vec<Riss::lbool>& model) { solver.model.copyTo(model); }
inline Lit      SolverPriss::trailGet(const unsigned int index) { Lit l; return l; } // FIXME: solver.trail[index]; }
inline unsigned int SolverPriss::getNumberOfTopLevelUnits() const    { return 0; } // FIXME: solver.trail.size(); }
} // namespace Pcasso
