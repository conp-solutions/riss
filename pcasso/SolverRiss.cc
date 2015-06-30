#include "pcasso/SolverRiss.h"

using namespace Riss;
using namespace std;

namespace Pcasso
{
SolverRiss::SolverRiss(CoreConfig *config) :
    solver(config),
    coreConfig(config),
    lastLevel(0),
    curPTLevel(0)
{
    cerr << "c using Riss as solver within Pcasso (WIP)" << endl << flush;
}

SolverRiss::~SolverRiss()
{

}

/**
* Lets Riss add a new variable and store the pt level for it
*/
Var SolverRiss::newVar(bool polarity, bool dvar, char type)
{
    return solver.newVar(polarity, dvar, type);
}

/**
* Lets Riss add the clause and saves the pt level for the new clause.
*/
bool SolverRiss::addClause_(vec<Lit>& ps, unsigned int pt_level)
{
    return solver.addClause_(ps);
}

void SolverRiss::setTimeOut(double timeout)
{
    // TODO: not used jet
}

/**
* Inits some solver variables depending on the node level.
* This is used for portfolio solver.
*/
void SolverRiss::setupForPortfolio(const int nodeLevel)
{
    if (nodeLevel == 1) {
        solver.K = 0.8;
        solver.R = 1.5;
    } else if (nodeLevel == 2) {
        solver.firstReduceDB = 10000;
    } else if (nodeLevel == 3) {
        solver.rnd_init_act = true;
    } else if (nodeLevel == 4) {
        solver.sizeLBDQueue = 150;
    } else { // default diversification
        srand(rand());
        solver.rnd_init_act = true;
        solver.rnd_pol = true;
    }
}

int       SolverRiss::nVars() const    { return solver.nVars(); }
bool      SolverRiss::okay() const    { return solver.okay(); }
void      SolverRiss::interrupt()          { solver.interrupt(); }
inline lbool     SolverRiss::solveLimited(const Riss::vec<Riss::Lit>& assumps) { return solver.solveLimited(assumps); }
inline void      SolverRiss::setVerbosity(int verbosity)    { solver.verbosity = verbosity; }
inline void      SolverRiss::setConfBudget(int64_t x) { solver.setConfBudget(x); }
// return a specifit literal from the trail
inline Riss::Lit SolverRiss::trailGet(const unsigned int index) { return solver.trail[index]; }
// return the number of top level units from that are on the trail
inline unsigned int SolverRiss::getTopLevelUnits() const    { return solver.trail.size(); } //( solver.trail_lim.size() > 1 ) ? solver.trail_lim[1] : solver.trail.size(); }
inline unsigned int SolverRiss::getNodePTLevel()          { return curPTLevel; }
inline uint64_t     SolverRiss::getStarts()          { return solver.starts; }
inline uint64_t     SolverRiss::getDecisions()          { return solver.decisions; }
inline uint64_t     SolverRiss::getConflicts()          { return solver.conflicts; }
inline void         SolverRiss::getModel(Riss::vec<Riss::lbool>& model) { solver.model.copyTo(model); }
} // namespace Pcasso
