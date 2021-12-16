/***************************************************************************[ScopedDecisionLevel.h]
Copyright (c) 2021, Anton Reinhard, LGPL v2, see LICENSE
**************************************************************************************************/

#pragma once

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
} // namespace Coprocessor
