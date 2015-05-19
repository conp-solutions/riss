/* 
 * File:   splitterSolver.h
 * Author: ahmed
 *
 * Created on December 26, 2012, 1:45 AM
 */

#ifndef SPLITTERSOLVER_H
#define	SPLITTERSOLVER_H

#include "riss/core/Solver.h"

namespace Pcasso {

class SplitterSolver : public Riss::Solver {
      
    Riss::CoreConfig& coreConfig;
      
public:
	SplitterSolver (CoreConfig& config) : Solver(config) , coreConfig( config ) {}
      
    virtual ~SplitterSolver() {}
    virtual void dummy() = 0;

    void inline kill(){ // Davide>
    	printf("\n"); printf("*** INTERRUPTED ***\n");
    	_exit(1);
    }
};

} // namespace Pcasso

#endif	/* SPLITTERSOLVER_H */

