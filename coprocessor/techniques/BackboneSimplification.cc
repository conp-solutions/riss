/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "coprocessor/techniques/BackboneSimplification.h"
#include "coprocessor/ScopedDecisionLevel.h"
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
        ran = false;
    }

    void BackboneSimplification::printStatistics(std::ostream& stream) {
        stream << "no stats yet";
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

    std::vector<Lit>& BackboneSimplification::getBackbone() {
        if (!ran) {
            computeBackbone();
        }

        return backbone;
    }

    void BackboneSimplification::computeBackbone() {
        printf("c ########## Starting backbone computation ##########\n");

        if (ran) {
            printf("c # Already ran backbone computation\n");
            return;
        }

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
        remainingLiterals.reserve(solver.model.size());

        printf("c Solver has %i variables\n", solver.nVars());
        printf("c Solver has %i clauses\n", solver.nClauses());
        printf("c Model has %i variables\n", solver.model.size());

        for (int32_t i = 0; i < solver.model.size(); ++i) {
            assert(solver.model[i] != l_Undef);
            if (solver.model[i] == l_True) {
                remainingLiterals.emplace_back(mkLit(i, true));
            } else {
                remainingLiterals.emplace_back(mkLit(i, false));
            }
        }

        Riss::vec<Lit> unitLit(1);

        // Main loop over the literals
        for (auto& currentLiteral : remainingLiterals) {
            if (currentLiteral.x == -1) {
                continue;
            }

            printf("Checking literal %i\n", currentLiteral.x);

            ScopedDecisionLevel loopDecisionLevel(solver);

            // add negated literal
            auto negLit = ~currentLiteral;

            printf("Enqueueing: %i\n", negLit.x);

            unitLit[0] = negLit;

            // get new model
            if (solver.solveLimited(unitLit) == l_False) {
                printf("Found backbone variable\n");
                // if there's no model then the literal is in the backbone, because
                // apparently no model exists with the literal negated
                // use the remaining Literal for the backbone
                backbone.push_back(currentLiteral);
                continue;
            }

            auto& model = solver.model;

            printf("Model size now: %i\n", model.size());

            for (int32_t j = 0; j < model.size(); ++j) {
                assert(model[j] != l_Undef);
                if (remainingLiterals[j] == currentLiteral) {
                    // printf("Current Literal: %i, in Model: %i\n", currentLiteral.x, mkLit(j + 1, model[j] == l_True).x);
                    if (currentLiteral.x == mkLit(j, model[j] == l_True).x) {
                        printf("Should never happen\n");
                    }
                }
                if (remainingLiterals[j].x == -1) {
                    continue;
                }
                // check if signs are different
                /*if (model[j] != (sign(remainingLiterals[j]) ? l_False : l_True)) {
                    remainingLiterals[j].x = -1;
                }*/
            }
        }

        ran = true;
        printf("c ########## Finished backbone computation, found %lu variables ##########\n", backbone.size());
    }

} // namespace Coprocessor
