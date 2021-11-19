/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "BackboneSimplification.h"
#include "riss/core/SolverTypes.h"

#include <algorithm>
#include <cmath>

using namespace Riss;

namespace Coprocessor {

    BackboneSimplification::BackboneSimplification(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data,
                                                   Propagation& _propagation, Solver& _solver)
        : Technique(_config, _ca, _controller)
        , data(_data)
        , propagation(_propagation)
        , solver(_solver) {
    }

    void BackboneSimplification::giveMoreSteps() {
    }

    void BackboneSimplification::destroy() {
    }

    void BackboneSimplification::reset() {
        backbone.clear();
    }

    bool BackboneSimplification::process() {
        computeBackbone();

        // add new unit clauses
        for (const Lit& l : backbone) {
            data.enqueue(l);
        }

        // propagate
        propagation.process(data, true);

        return true;
    }

    void BackboneSimplification::computeBackbone() {
        assert(solver.decisionLevel() == 0 && "Only use backbone computation on decision level 0");

        // make new decision level
        ScopedDecisionLevel outerLevel(solver);

        // Compute a model
        if (!solver.solve()) {
            // if the formula is unsatisfiable the backbone is empty
            return;
        }

        // add literals from the model to potentially be part of the backbone
        std::vector<Lit> remainingLiterals;
        for (int32_t i = 0; i < solver.model.size(); ++i) {
            assert(solver.model[i] != l_Undef);
            if (solver.model[i] == l_True) {
                remainingLiterals.emplace_back(mkLit(i));
            } else {
                remainingLiterals.emplace_back(mkLit(i, true));
            }
        }

        Riss::vec<Lit> unitLit(1);

        // Main loop
        for (auto& currentLiteral : remainingLiterals) {
            if (currentLiteral.x == -1) {
                continue;
            }

            ScopedDecisionLevel loopDecisionLevel(solver);

            // add negated literal
            unitLit[0] = ~currentLiteral;
            solver.addUnitClauses(unitLit);

            // get new model
            if (!solver.solve()) {
                // if there's no model then the literal is in the backbone, because
                // apparently no model exists with the literal negated
                // use the remaining Literal for the backbone
                backbone.push_back(currentLiteral);
                continue;
            }

            auto& model = solver.model;

            for (auto j = 0; j < model.size(); ++j) {
                if (remainingLiterals[j].x == -1) {
                    continue;
                }
                // check if signs are different
                if (model[j] != (sign(remainingLiterals[j]) ? l_True : l_False)) {
                    remainingLiterals[j].x = -1;
                }
            }
        }
    }
} // namespace Coprocessor
