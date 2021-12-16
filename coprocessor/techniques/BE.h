/********************************************************************************************[BE.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Technique.h"
#include "coprocessor/techniques/BackboneSimplification.h"
#include "coprocessor/techniques/Propagation.h"
#include "riss/core/Solver.h"

#include <memory>
#include <set>

namespace Coprocessor {

    /**
     * @brief Class implementing Bipartition and Elimination Simplification as a procedure
     */
    class BE : public Technique<BE> {

        CoprocessorData& data;
        Propagation& propagation;
        BackboneSimplification& backboneSimpl;
        Solver& solver;

        /**
         * @brief The input and output variables of the bipartition algorithm
         */
        std::vector<Var> outputVariables;
        std::vector<Var> inputVariables;

    public:
        void reset();

        /**
         * @brief applies B+E algorithm
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
        BE(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation,
           BackboneSimplification& _backboneSimpl, Solver& _solver);

        /**
         * @brief Computes a the bipartition
         * @note Resulting input variables are saved in the input member
         */
        void computeBipartition();

        /**
         * @brief The elimination part of the algorithm
         */
        void eliminate();

        /**
         * @brief Tests whether variable x is defined in terms of input variables of the formula and variables that follow it in the sorted list of
         * variables
         *
         * @param index The index of the variable in the sorted list of variables
         * @param vars The list of all variables sorted by occurrence
         * @return bool True if x is defined, False if not or on timeout/error
         */
        bool isDefined(int32_t index, std::vector<Var>& vars);

    protected:
    };

} // namespace Coprocessor
