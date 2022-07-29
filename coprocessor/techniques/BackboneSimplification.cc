/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "coprocessor/techniques/BackboneSimplification.h"
#include "coprocessor/ScopedDecisionLevel.h"
#include "riss/core/SolverTypes.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace Riss;

namespace {
    inline std::string groupingToString(Coprocessor::GROUPED group) {
        switch (group) {
        case Coprocessor::GROUPED::NOT:
            return "none";
        case Coprocessor::GROUPED::CONJUNCTIVE:
            return "conjunction";
        case Coprocessor::GROUPED::DISJUNCTIVE:
            return "disjunction";
        default:
            return "unknown";
        }
    }

    inline std::string sortingToString(Coprocessor::LIKELIHOOD_HEURISTIC heuristic) {
        switch (heuristic) {
        case Coprocessor::LIKELIHOOD_HEURISTIC::DIV:
            return "division";
        case Coprocessor::LIKELIHOOD_HEURISTIC::MULT:
            return "multiplication";
        default:
            return "unknown";
        }
    }
} // namespace

namespace Coprocessor {

    BackboneSimplification::BackboneSimplification(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data,
                                                   Propagation& _propagation, Solver& _solver)
        : Technique(_config, _ca, _controller)
        , data(_data)
        , propagation(_propagation)
        , solver(_solver)
        , varUsed(solver.nVars(), false)
        , noUsedVars(0)
        , ran(false)
        , dirtyCache(true)
        , conflictBudget(_config.opt_backbone_nconf)
        , likelihood_heuristic(static_cast<LIKELIHOOD_HEURISTIC>((int)_config.opt_backbone_sorting))
        , grouping(static_cast<GROUPED>((int)_config.opt_backbone_grouping))
        , nGrouping(_config.opt_backbone_ngrouping)
        , lookedAtPercent(_config.opt_backbone_decreasePercent)
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
        noUsedVars = 0;
        dirtyCache = true;
    }

    void BackboneSimplification::printStatistics(std::ostream& stream) {
        stream << "c [STAT] BACKBONE max nconf: " << conflictBudget << std::endl;
        stream << "c [STAT] BACKBONE nGrouping: " << nGrouping << std::endl;
        stream << "c [STAT] BACKBONE grouping strat: " << groupingToString(grouping) << std::endl;
        stream << "c [STAT] BACKBONE sorting strat: " << sortingToString(likelihood_heuristic) << std::endl;
        stream << "c [STAT] BACKBONE lookedAtPercent: " << lookedAtPercent << std::endl;
        stream << "c [STAT] BACKBONE found variables: " << backbone.size() - unitsBefore << std::endl;
        stream << "c [STAT] BACKBONE units before: " << unitsBefore << std::endl;
        stream << "c [STAT] BACKBONE nConf: " << totalConflits << std::endl;
        stream << "c [STAT] BACKBONE timed out calls: " << timedOutCalls << std::endl;
        stream << "c [STAT] BACKBONE removed literals through found model: " << crossCheckRemovedLiterals << std::endl;
        stream << "c [STAT] BACKBONE time copying: " << copyTime << std::endl;
        stream << "c [STAT] BACKBONE time searching: " << searchTime << std::endl;
        stream << "c [STAT] BACKBONE solverTime: " << solverTime << std::endl;
        stream << "c [STAT] BACKBONE solver calls: " << nSolve << std::endl;
        stream << "c [STAT] BACKBONE used variables: " << noUsedVars << std::endl;
    }

    bool BackboneSimplification::process() {
        computeBackbone();

        // add new unit clauses
        for (const Lit& l : backbone) {
            data.enqueue(l);
        }

        // propagate
        propagation.process(data, true);

        static bool first = true;
        if (first) {
            printStatistics(std::cerr);
            first = false;
        }

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

        // hide class' grouping and ngrouping and use local ones instead, because they may be changed
        GROUPED grouping = this->grouping;
        int nGrouping = this->nGrouping;

        // sanity check for grouping
        if (nGrouping == 1 && grouping != GROUPED::NOT) {
            std::cerr << "nGrouping 1 only makes sense without grouping, reverting to no groups" << std::endl;
            grouping = GROUPED::NOT;
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
        std::vector<Lit> possibleBackboneLiterals;
        possibleBackboneLiterals.reserve(ownSolver->model.size());

        for (int32_t i = 0; i < ownSolver->model.size(); ++i) {
            assert(ownSolver->model[i] != l_Undef);
            if (ownSolver->model[i] == l_True) {
                possibleBackboneLiterals.emplace_back(mkLit(i, false));
            } else {
                possibleBackboneLiterals.emplace_back(mkLit(i, true));
            }
        }

        std::queue<Lit> litsToCheck;
        {
            // can't sort the original so make a copy and sort that
            std::vector<Lit> backboneLitsTemp;
            backboneLitsTemp.reserve(noUsedVars);

            for (const auto& lit : possibleBackboneLiterals) {
                if (varUsed[var(lit)]) {
                    backboneLitsTemp.emplace_back(lit);
                }
            }
            switch (grouping) {
            case GROUPED::CONJUNCTIVE:
                // sort unlikely first
                std::sort(backboneLitsTemp.begin(), backboneLitsTemp.end(), [this](const Lit& x, const Lit& y) {
                    return getBackboneLikelihood(var(x)) < getBackboneLikelihood(var(y));
                });
                break;
            case GROUPED::DISJUNCTIVE:
                // sort likely first
                std::sort(backboneLitsTemp.begin(), backboneLitsTemp.end(), [this](const Lit& x, const Lit& y) {
                    return getBackboneLikelihood(var(x)) > getBackboneLikelihood(var(y));
                });
                break;
            case GROUPED::NOT:
                // no need to sort
                break;
            default:
                assert(false);
            }

            // now fill the queue in the correct order
            for (const auto& lit : backboneLitsTemp) {
                litsToCheck.push(lit);
            }
        }

        Riss::vec<Lit> unitLit;

        int lookedAtVars = 0;
        int nGroupingDecreased = 1;
        // Main loop over the literals
        while (!litsToCheck.empty()) {
            unitLit.clear();

            const auto currentLiteral = litsToCheck.front();
            litsToCheck.pop();
            ++lookedAtVars;

            if (possibleBackboneLiterals[var(currentLiteral)].x == -1) {
                continue;
            }

            if (!varUsed[var(currentLiteral)]) {
                std::cout << "c what the fuck" << std::endl;
                // skip variables that are units
                continue;
            }

            // std::cout << "c checking " << (sign(currentLiteral) ? '-' : ' ') << var(currentLiteral) << std::endl;

            if (nGrouping > 1 && lookedAtVars > lookedAtPercent * static_cast<double>(nGroupingDecreased) * static_cast<double>(noUsedVars)) {
                nGrouping = nGrouping / 2;
                std::cout << "c Decreasing nGrouping to " << nGrouping << " after " << lookedAtVars << " lookedAtVars" << std::endl;
                if (nGrouping == 1) {
                    grouping = GROUPED::NOT;
                }
                nGroupingDecreased++;
            }

            ownSolver->model.clear();

            // now set conflict budget
            ownSolver->setConfBudget(conflictBudget);

            // get new model
            switch (grouping) {
            case GROUPED::NOT:
                unitLit.push(~currentLiteral);
                break;
            case GROUPED::CONJUNCTIVE:
            case GROUPED::DISJUNCTIVE:
                unitLit.push(~currentLiteral);
                for (int j = 1; j < nGrouping; ++j) {
                    if (litsToCheck.empty()) {
                        break;
                    }
                    if (possibleBackboneLiterals[var(litsToCheck.front())].x == -1) {
                        litsToCheck.pop();
                        ++lookedAtVars;
                        --j;
                        continue;
                    }
                    // std::cout << "c adding " << (sign(litsToCheck.front()) ? '-' : ' ') << var(litsToCheck.front()) << " to the group" <<
                    // std::endl;
                    unitLit.push(~(litsToCheck.front()));
                    litsToCheck.pop();
                    ++lookedAtVars;
                }
                break;
            }
            Riss::lbool solveResult;

            {
                MethodTimer t(&solverTime);
                if (grouping == GROUPED::DISJUNCTIVE && unitLit.size() > 1) {
                    // for disjunctive, add a new clause with all the "units" and remove it afterwards
                    ScopedDecisionLevel dec(*ownSolver);
                    ownSolver->integrateNewClause(unitLit);
                    solveResult = ownSolver->solveLimited({});
                    // newly integrated clauses are at the end

                } else {
                    solveResult = ownSolver->solveLimited(unitLit);
                }
                ++nSolve;
            }

            ownSolver->assumptions.clear();

            if (solveResult == l_False) {
                // std::cout << "No Solution, ";
                switch (grouping) {
                case GROUPED::NOT:
                    // std::cout << "found variable" << std::endl;
                    // if there's no model then the literal is in the backbone, because
                    // apparently no model exists with the literal negated
                    // use the remaining Literal for the backbone
                    backbone.push_back(currentLiteral);
                    continue;
                case GROUPED::CONJUNCTIVE:
                    // std::cout << "readding " << unitLit.size() << " units to the to check list" << std::endl;
                    // if conjunctive grouping was used, no info was gained, retry the vars again later
                    for (int k = 0; k < unitLit.size(); ++k) {
                        litsToCheck.push(~(unitLit[k]));
                    }
                    continue;
                case GROUPED::DISJUNCTIVE:
                    // std::cout << "found variables" << std::endl;
                    //  if disjunctive grouping was used, no solution means that all lits were in the backbone
                    for (int k = 0; k < unitLit.size(); ++k) {
                        backbone.push_back(~(unitLit[k]));
                    }
                    continue;
                }
            }

            if (solveResult == l_Undef) {
                // if we ran out of conflict budget just continue
                // conjunctive grouping should be faster to solve than single, so don't try again there
                if (grouping == GROUPED::DISJUNCTIVE) {
                    // for disjunctive grouping trying them alone might yield better luck
                    for (int k = 0; k < unitLit.size(); ++k) {
                        litsToCheck.push(~(unitLit[k]));
                    }
                }

                ++timedOutCalls;
                continue;
            }

            // std::cout << "Cross checking models" << std::endl;
            //  check the model against the remaining candidates
            auto& model = ownSolver->model;
            for (int32_t j = 0; j < model.size(); ++j) {
                assert(model[j] != l_Undef);
                if (possibleBackboneLiterals[j].x == -1) {
                    continue;
                }
                // check if signs are different
                if (sign(possibleBackboneLiterals[j]) != (model[j] != l_True)) {
                    // std::cout << "eliminated " << (sign(possibleBackboneLiterals[j]) ? '-' : ' ') << var(possibleBackboneLiterals[j]) << std::endl;
                    possibleBackboneLiterals[j].x = -1;
                    ++crossCheckRemovedLiterals;
                }
            }
        }

        ran = true;
        totalConflits = ownSolver->conflicts;

        /*
        std::cout << "backbone vars found: ";
        for (const auto& b : backbone) {
            std::cout << (sign(b) ? '-' : ' ') << var(b) << ", ";
        }
        std::cout << std::endl;
        */
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

        std::cout << "c learnt clauses: " << solver.nLearnts() << std::endl;
        unitsBefore = backbone.size();

        for (const auto& it : varUsed) {
            if (bool(it))
                ++noUsedVars;
        }
    }

    double BackboneSimplification::getBackboneLikelihood(const Var x) {
        static std::unordered_map<Var, std::uint32_t> cache;

        if (dirtyCache) {
            cache.clear();
            cache.reserve(noUsedVars);
            dirtyCache = false;
        }

        if (cache.find(x) != cache.end()) {
            return cache.at(x);
        }

        // positive and negative x literal
        Lit pX = mkLit(x, false);
        Lit nX = mkLit(x, true);

        // find number of clauses where x appears positive/negative
        std::uint32_t xpos = 0;
        std::uint32_t xneg = 0;

        for (std::size_t i = 0; i < data.getClauses().size(); ++i) {
            const auto& clause = ca[data.getClauses()[i]];
            const Lit* lit = (const Lit*)(clause);
            for (int j = 0; j < clause.size(); ++j) {
                if (lit[j] == pX) {
                    ++xpos;
                    // no need to check rest of clause
                    continue;
                }
                if (lit[j] == nX) {
                    ++xneg;
                    // no need to check rest of clause
                    continue;
                }
            }
        }

        double likelihood;

        switch (likelihood_heuristic) {
        case LIKELIHOOD_HEURISTIC::MULT:
            if (xneg == 0 || xpos == 0) likelihood = 1000000;
            else likelihood = 1. / (xpos * xneg);
            break;
        case LIKELIHOOD_HEURISTIC::DIV:
            if (xneg == 0 || xpos == 0) likelihood = 1000000;
            else likelihood = (xpos > xneg) ? static_cast<double>(xpos) / xneg : static_cast<double>(xneg) / xpos;
            break;
        default:
            assert(false);
            break;
        }

        cache.emplace(x, likelihood);
        return likelihood;
    }

} // namespace Coprocessor
