/*
 * This class enhances the SAT-Solver Riss to work within Pcasso.
 *
 *
 * Author: Franziska Krueger
 *
 */

#ifndef SOLVERRISS_H
#define SOLVERRISS_H

#include "riss/core/Solver.h"
#include "pcasso/InstanceSolver.h"

namespace Pcasso {
    class SolverRiss : public InstanceSolver {
    public:
        SolverRiss(Riss::CoreConfig* config);

        ~SolverRiss();

        Riss::Solver solver;

        unsigned         curPTLevel;         // Davide> Contains the pt_level of curNode
        unsigned int     lastLevel;        // option related to clause sharing
        TreeNode*        tnode;
        vector<unsigned> shared_indeces;
        Statistics       localStat;        // Norbert> Local Statistics

        // enhanced solver functions for parallel needs
        Riss::Var        newVar             (bool polarity = true, bool dvar = true, char type = 'o');
        bool             addClause_         ( Riss::vec<Riss::Lit>& ps, unsigned int pt_level = 0);
        void             printClause(Riss::CRef cr);

        // for partitioning tree
        void             setTimeOut         (double timeout);        // specify a number of seconds that is allowed to be executed
        void             setupForPortfolio  (const int nodeLevel);

        inline int       nVars              () const;
        inline bool      okay               () const;
        inline void      interrupt          ();
        inline Riss::lbool     solveLimited       (const Riss::vec<Riss::Lit>& assumps);
        inline void      setVerbosity       (int verbosity);
        inline void      setConfBudget      (int64_t x);
        // return a specifit literal from the trail
        inline Riss::Lit trailGet           (const unsigned int index);
        // return the number of top level units from that are on the trail
        inline unsigned int getTopLevelUnits() const    ;
        inline unsigned int getNodePTLevel  ()          ;
        inline uint64_t     getStarts       ()          ;
        inline uint64_t     getDecisions    ()          ;
        inline uint64_t     getConflicts    ()          ;
        inline void         getModel        (Riss::vec<Riss::lbool>& model);
        inline unsigned int getLiteralPTLevel(const Riss::Lit& l) const{ return 0; } // TODO: return correct PT level

    private:
        Riss::CoreConfig*   coreConfig;
        // TODO: varPT not working yet
        Riss::vec<unsigned> varPT;             // storing the PT level of each variable
    };
}

#endif //SOLVERRISS_H
