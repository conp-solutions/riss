/************************************************************************[BackboneSimplification.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"
#include "coprocessor/techniques/Propagation.h"
#include "riss/core/Solver.h"

#include <memory>
#include <set>

namespace Coprocessor {

    enum class LIKELIHOOD_HEURISTIC {
        MULT, // npos * nneg
        DIV,  // npos / nneg or npos / nneg whichever is > 1
    };
    enum class GROUPED { NOT, CONJUNCTIVE, DISJUNCTIVE };

    /**
     * @brief Class implementing Backbone Simplification as a procedure
     *
     * @details For the original algorithm see: https://www.cril.univ-artois.fr/KC/documents/lagniez-marquis-aaai14.pdf
     * Application of this procedure will result in an equivalent output formula
     * The backbone of a formula is the set of all literals that are true in every model of the formula.
     */
    class BackboneSimplification : public Technique<BackboneSimplification> {

        CoprocessorData& data;
        Propagation& propagation;
        Solver& solver;

        Riss::Solver* ownSolver;
        Coprocessor::CP3Config* cp3config;
        Riss::CoreConfig* solverconfig;
        Riss::vec<Riss::Lit> assumptions; // current set of assumptions that are used for the next SAT call

        std::vector<Lit> backbone;
        std::vector<bool> varUsed; // "map" from variable to whether it is used in the solver, i.e. whether it is not a unit
        int noUsedVars;
        bool ran;
        bool dirtyCache;

        int conflictBudget; // how many conflicts is the solver allowed to have before aborting the search for a model
        LIKELIHOOD_HEURISTIC likelihood_heuristic;
        GROUPED grouping;
        int nGrouping;
        double lookedAtPercent; // after how many of the used vars havebeen looked at will ngrouping be decreased

        int nSolve;      // number of solve calls done
        int unitsBefore; // number of units before backbone was called
        int totalConflits;
        int timedOutCalls;
        int crossCheckRemovedLiterals;
        double copyTime;
        double searchTime;
        double solverTime;

    public:
        void reset();

        /**
         * @brief applies backbone simplification
         * @return true, if something has been altered
         */
        bool process();

        void printStatistics(std::ostream& stream);

        void giveMoreSteps();

        void destroy();

        /**
         * @brief Returns the backbone
         * @note Computes it, if it wasn't computed already
         */
        std::vector<Lit>& getBackbone();

        /**
         * @brief Construct a new Backbone Simplification procedure
         *
         * @param solver The solver to use for this
         */
        BackboneSimplification(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data,
                               Propagation& _propagation, Solver& _solver);

        /**
         * @brief Computes a backbone
         * @note Resulting backbone is saved in the backbone member
         */
        void computeBackbone();

        /**
         * @brief Copies the state of the solver to the ownSolver
         */
        void copySolver();

    protected:
        /**
         * @brief Returns how likely it is that x is in the backbone according to some heuristic
         *
         * @param x
         * @return double
         */
        double getBackboneLikelihood(Var x);
    };

} // namespace Coprocessor
