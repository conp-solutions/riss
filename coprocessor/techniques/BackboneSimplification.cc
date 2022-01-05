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
        , solver(_solver)
        , conflictBudget(_config.opt_backbone_nconf)
        , totalConflits(0)
        , timedOutCalls(0)
        , copyTime(0)
        , searchTime(0) {
        printf("### Using conflict budget %d\n", conflictBudget);
    }

    void BackboneSimplification::giveMoreSteps() {
    }

    void BackboneSimplification::destroy() {
        delete solverconfig;
        delete cp3config;
        delete ownSolver;
    }

    void BackboneSimplification::reset() {
        destroy();
        backbone.clear();
        ran = false;
        totalConflits = 0;
        timedOutCalls = 0;
        copyTime = 0;
        searchTime = 0;
    }

    void BackboneSimplification::printStatistics(std::ostream& stream) {
        stream << "c [STAT] BACKBONE found variables: " << backbone.size() << std::endl;
        stream << "c [STAT] BACKBONE nConf: " << totalConflits << " timed out calls: " << timedOutCalls << std::endl;
        stream << "c [STAT] BACKBONE time copying: " << copyTime << "s time searching: " << searchTime << "s" << std::endl;
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
        if (ran) {
            // running again on the same thing is never beneficial
            return;
        }

        copySolver();

        MethodTimer timer(&searchTime);

        // set conflict budget to infinity
        ownSolver->budgetOff();

        auto ret = ownSolver->solveLimited({});

        // Compute a model
        if (ret == l_False) {
            // if the formula is unsatisfiable the backbone is empty
            return;
        }

        // add literals from the model to potentially be part of the backbone
        std::vector<Lit> remainingLiterals;
        remainingLiterals.reserve(solver.model.size());

        for (int32_t i = 0; i < ownSolver->model.size(); ++i) {
            assert(ownSolver->model[i] != l_Undef);
            if (ownSolver->model[i] == l_True) {
                remainingLiterals.emplace_back(mkLit(i, false));
            } else {
                remainingLiterals.emplace_back(mkLit(i, true));
            }
        }

        Riss::vec<Lit> unitLit;

        // Main loop over the literals
        for (auto& currentLiteral : remainingLiterals) {
            if (currentLiteral.x == -1) {
                continue;
            }

            ownSolver->model.clear();

            // now set conflict budget
            ownSolver->setConfBudget(conflictBudget);

            // get new model
            unitLit.push(~currentLiteral);
            ownSolver->integrateAssumptions(unitLit);
            auto solveResult = ownSolver->solveLimited(unitLit);
            ownSolver->assumptions.clear();
            unitLit.clear();

            if (solveResult == l_False) {
                // if there's no model then the literal is in the backbone, because
                // apparently no model exists with the literal negated
                // use the remaining Literal for the backbone
                backbone.push_back(currentLiteral);
                continue;
            }

            if (solveResult == l_Undef) {
                // if we ran out of conflict budget just continue
                ++timedOutCalls;
                continue;
            }

            auto& model = ownSolver->model;

            for (int32_t j = 0; j < model.size(); ++j) {
                assert(model[j] != l_Undef);
                if (remainingLiterals[j] == currentLiteral) {
                    // printf("Current Literal: %i, in Model: %i\n", currentLiteral.x, mkLit(j + 1, model[j] == l_True).x);
                    if (currentLiteral.x == mkLit(j, model[j] != l_True).x) {
                        assert(false);
                    }
                }
                if (remainingLiterals[j].x == -1) {
                    continue;
                }
                // check if signs are different
            }
        }

        ran = true;
        totalConflits = ownSolver->conflicts;
    }

    void BackboneSimplification::copySolver() {
        MethodTimer timer(&copyTime);

        // init the solver
        solverconfig = new Riss::CoreConfig("");
        cp3config = new Coprocessor::CP3Config("-no-backbone -no-dense -no-propagation");
        ownSolver = new Riss::Solver(solverconfig);
        ownSolver->setPreprocessor(cp3config);

        // -- copy the solver --
        // register variables
        for (auto i = 0; i < solver.nVars(); ++i) {
            ownSolver->newVar();
        }

        // copy clauses
        Riss::vec<Lit> assembledClause;
        for (std::size_t i = 0; i < solver.clauses.size(); ++i) {
            const auto& clause = ca[solver.clauses[i]];
            const Lit* lit = (const Lit*)(clause);
            for (int j = 0; j < clause.size(); ++j) {
                assembledClause.push(lit[j]);
            }
            ownSolver->integrateNewClause(assembledClause);
            assembledClause.clear();
        }
    }

} // namespace Coprocessor
