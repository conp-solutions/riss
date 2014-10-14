/* 
 * File:   splitterSolver.h
 * Author: ahmed
 *
 * Created on December 26, 2012, 1:45 AM
 */

#ifndef SPLITTERSOLVER_H
#define	SPLITTERSOLVER_H

#include "core/Solver.h"

using namespace Minisat;

namespace Pcasso {
    class SplitterSolver : public Solver {
      
      CoreConfig& coreConfig;
      
    public:
	SplitterSolver (CoreConfig& config)
	 : Solver(config) 
	 , coreConfig( config )
	 {}
      
      
        virtual ~SplitterSolver() {
        }
        virtual void dummy() = 0;
        void inline kill(){ // Davide>
        	printf("\n"); printf("*** INTERRUPTED ***\n");
        	_exit(1);
        }
    };

}


#endif	/* SPLITTERSOLVER_H */

