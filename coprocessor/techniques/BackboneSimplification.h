/************************************************************************[BackboneSimplification.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

#include "Propagation.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"
#include "riss/core/Solver.h"

#include <memory>
#include <set>

namespace Coprocessor {

    // small helper class taking care of creating and reverting decision levels on a solver
    // using RAII
    class ScopedDecisionLevel {
        Solver& solver;
        int decisionLevel;

    public:
        ScopedDecisionLevel(Solver& solver)
            : solver(solver)
            , decisionLevel(solver.decisionLevel()) {
            solver.newDecisionLevel();
        }

        ~ScopedDecisionLevel() {
            solver.cancelUntil(decisionLevel);
        }
    };

    /**
     * @brief Class implementing Backbone Simplification as a procedure
     *
     * @details For the original algorithm see: https://www.cril.univ-artois.fr/KC/documents/lagniez-marquis-aaai14.pdf
     * Application of this procedure will result in an equivalent output formula
     * The backbone of a formula is the set of all literals that are true in every model of the formula.
     */
    class BackboneSimplification : public Technique<BackboneSimplification> {

        CoprocessorData& data;
        Coprocessor::Propagation& propagation;
        Solver& solver;

        std::vector<Lit> backbone;

    public:
        void reset();

        /** applies blocked clause elimination algorithm
         * @return true, if something has been altered
         */
        bool process();

        void printStatistics(std::ostream& stream);

        void giveMoreSteps();

        void destroy();

        /**
         * @brief Construct a new Backbone Simplification procedure
         *
         * @param solver The solver to use for this
         */
        BackboneSimplification(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data,
                               Coprocessor::Propagation& _propagation, Solver& _solver);

        /**
         * @brief Computes a backbone
         * @note Resulting backbone is saved in the backbone member
         */
        void computeBackbone();

    protected:
    };

} // namespace Coprocessor
