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

    public:
        void reset() const;

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
                               Coprocessor::Propagation& _propagation);

        /**
         * @brief Computes a backbone
         *
         * @return std::vector<Lit> The literals of the backbone
         */
        std::vector<Lit> getBackbone() const;

    protected:
    };

} // namespace Coprocessor
