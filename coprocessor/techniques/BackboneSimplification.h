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

        std::vector<Lit> backbone;
        bool ran = false;

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

    protected:
    };

} // namespace Coprocessor
