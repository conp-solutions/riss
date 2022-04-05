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
        , varUsed(solver.nVars(), false)
        , conflictBudget(_config.opt_backbone_nconf)
        , nSolve(0)
        , unitsBefore(0)
        , totalConflits(0)
        , timedOutCalls(0)
        , crossCheckRemovedLiterals(0)
        , copyTime(0)
        , searchTime(0)
        , solverTime(0) {
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
        crossCheckRemovedLiterals = 0;
        copyTime = 0;
        searchTime = 0;
        solverTime = 0;
        nSolve = 0;
        unitsBefore = 0;
    }

    void BackboneSimplification::printStatistics(std::ostream& stream) {
        stream << "c [STAT] BACKBONE found variables: " << backbone.size() - unitsBefore << std::endl;
        stream << "c [STAT] BACKBONE nConf: " << totalConflits << ", timed out calls: " << timedOutCalls
               << ", removed literals through found model: " << crossCheckRemovedLiterals << std::endl;
        stream << "c [STAT] BACKBONE time copying: " << copyTime << "s, time searching: " << searchTime << "s, solverTime: " << solverTime << "s" << std::endl;
        stream << "c [STAT] BACKBONE solver calls: " << nSolve << std::endl;
        int units = 0;
        for (const auto& it : varUsed) {
            if (bool(it)) ++units;
        }
        stream << "c [STAT] BACKBONE used " << units << " variables" << std::endl;
    }

    bool BackboneSimplification::process() {
        printf("c Before: Solver has %i variables, %i clauses\n", solver.nVars(), solver.nClauses());

        computeBackbone();

        // add new unit clauses
        for (const Lit& l : backbone) {
            data.enqueue(l);
        }

        // propagate
        propagation.process(data, true);
        
        printf("c After Prop: Solver has %i variables, %i clauses\n", solver.nVars(), solver.nClauses());

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

        Riss::lbool ret;
        {
            MethodTimer t(&solverTime);
            // Compute a model
            ret = ownSolver->solveLimited({});
        }

        if (ret == l_False) {
            // if the formula is unsatisfiable the backbone is empty
            std::cerr << "c Unsatisfiable" << std::endl;
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

            if (!varUsed[var(currentLiteral)]) {
                // skip variables that are units
                continue;
            }

            ownSolver->model.clear();

            // now set conflict budget
            ownSolver->setConfBudget(conflictBudget);

            // get new model
            unitLit.push(~currentLiteral);
            Riss::lbool solveResult;

            {
                MethodTimer t(&solverTime);
                solveResult = ownSolver->solveLimited(unitLit);
                ++nSolve;
            }

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

            // this literal is not in the backbone, since a model was found
            currentLiteral.x = -1;

            // check the model against the remaining candidates
            auto& model = ownSolver->model;
            for (int32_t j = 0; j < model.size(); ++j) {
                assert(model[j] != l_Undef);
                if (remainingLiterals[j].x == -1) {
                    continue;
                }
                // check if signs are different
                if (sign(remainingLiterals[j]) != (model[j] != l_True)) {
                    remainingLiterals[j].x = -1;
                    ++crossCheckRemovedLiterals;
                }
            }
        }

        ran = true;
        totalConflits = ownSolver->conflicts;

        printf("c After: Solver has %i variables, %i clauses\n", solver.nVars(), solver.nClauses());
    }

    void BackboneSimplification::copySolver() {
        MethodTimer timer(&copyTime);

        // init the solver
        solverconfig = new Riss::CoreConfig("");
        cp3config = new Coprocessor::CP3Config("-no-backbone -no-be");
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
            for (int j = 0; j < clause.size(); ++j) {
                assembledClause.push(clause[j]);
                varUsed[var(clause[j])] = true;
            }
            ownSolver->integrateNewClause(assembledClause);
            assembledClause.clear();
        }

        for (std::size_t i = 0; i < solver.trail.size(); ++i) {
            backbone.emplace_back(solver.trail[i]);
        }

        unitsBefore = backbone.size();
        std::cout << "c found " << unitsBefore << " units to put into backbone immediately" << std::endl;
    }

} // namespace Coprocessor
