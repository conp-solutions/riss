/*
 * File:   splitterSolver.h
 * Author: ahmed
 *
 * Created on December 26, 2012, 1:45 AM
 */

#ifndef PCASSO_SPLITTERSOLVER_H
#define PCASSO_SPLITTERSOLVER_H

#include "riss/core/Solver.h"
#include "pcasso/SolverInterface.h"

namespace Pcasso
{

class SplitterSolver : public Riss::Solver, public SolverInterface
{

    Riss::CoreConfig* coreConfig;

  public:
    SplitterSolver(Riss::CoreConfig* config) : Riss::Solver(config), coreConfig(config) {}

    virtual ~SplitterSolver() {}
    virtual void dummy() = 0;

    // Davide>
    void inline kill()
    {
        printf("\n"); printf("*** INTERRUPTED ***\n");
        _exit(1);
    }
};

} // namespace Pcasso

#endif  /* SPLITTERSOLVER_H */

