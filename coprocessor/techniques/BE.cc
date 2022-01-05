/***********************************************************************[BackboneSimplification.cc]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#include "coprocessor/techniques/BE.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/ScopedDecisionLevel.h"
#include "coprocessor/techniques/BackboneSimplification.h"
#include "riss/core/SolverTypes.h"

#include <algorithm>
#include <cmath>

using namespace Riss;

namespace Coprocessor {

    BE::BE(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation,
           BackboneSimplification& _backboneSimpl, Solver& _solver)
        : Technique(_config, _ca, _controller)
        , data(_data)
        , propagation(_propagation)
        , backboneSimpl(_backboneSimpl)
        , solver(_solver) {
    }

    void BE::giveMoreSteps() {
    }

    void BE::destroy() {
    }

    void BE::printStatistics(std::ostream& stream) {
        stream << "no stats yet";
    }

    void BE::reset() {
        outputVariables.clear();
    }

    bool BE::process() {
        computeBipartition();

        eliminate();

        return true;
    }

    void BE::computeBipartition() {
        printf("c ########## Starting bipartition computation ##########\n");

        assert(solver.decisionLevel() == 0 && "Only use bipartition computation on decision level 0");

        printf("c Solver has %i variables\n", solver.nVars());
        printf("c Solver has %i clauses\n", solver.nClauses());
        printf("c Model has %i variables\n", solver.model.size());

        // line 1
        auto& backbone = backboneSimpl.getBackbone();
        outputVariables.reserve(backbone.size());
        for (const auto& lit : backbone) {
            outputVariables.emplace_back(var(lit));
        }

        // make new decision level
        ScopedDecisionLevel outerLevel(solver);

        // line 2
        std::vector<Var> vars;
        vars.reserve(solver.nVars());
        for (auto i = 0; i < solver.nVars(); ++i) {
            vars.emplace_back(i);
        }
        std::sort(vars.begin(), vars.end(), [&](const Var& a, const Var& b) {
            return data[a] < data[b];
        });

        // line 3

        // line 4
        for (int i = 0; i < vars.size(); ++i) {
            if (isDefined(i, vars)) {
                outputVariables.emplace_back(vars[i]);
            } else {
                inputVariables.emplace_back(vars[i]);
            }
        }

        printf("c ########## Finished bipartition computation, found %lu output variables ##########\n", outputVariables.size());
    }

    void BE::eliminate() {

        
    }

    bool BE::isDefined(const int32_t index, std::vector<Var>& vars) {
        
        const Var x = vars[index];
        // every new variable made from existing ones will just get nVars added to themselves
        // this way no collisions can happen
        const Var xPrime = x + solver.nVars();

        printf("Checking definedness of %i", x);

        std::vector<Clause> addedClauses;

        // add sigma prime
        auto& clauses = data.getClauses();
        for (auto i = 0; i < clauses.size(); ++i) {
            auto& clause = clauses[i];

            // TODO: make new clause from clause with all literals + nVars
        }

        for (auto& var : vars) {
            // add two clauses:
            // TODO add clause (~sZ, ~var, var + nVars)
            // TODO add clause (~sZ, var, ~(var + nVars))
        }

        // add assumptions
        for (auto i = index; i < vars.size(); ++i) {
            // TODO add unit/assumption sZ for vars[i]
        }

        for (auto& v : inputVariables) {
            // TODO add unit/assumption sZ for v
        }

        // TODO: add x and ~xprime unit clauses

        return true; // TODO: correct solve call
    }

} // namespace Coprocessor
