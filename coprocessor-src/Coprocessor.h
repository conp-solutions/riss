/***********************************************************************************[Coprocessor.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSOR_HH
#define COPRECESSOR_HH


#include "core/Solver.h"

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Subsumption.h"


using namespace Minisat;

/** Main class that connects all the functionality of the preprocessor Coprocessor
 */
class Coprocessor {

  // friends
  
  // attributes
  int32_t threads;    // number of threads that can be used by the preprocessor
  Solver* solver;     // handle to the solver object that stores the formula
  ClauseAllocator& ca; // reference to clause allocator
  
  CoprocessorData data;  // all the data that needs to be accessed by other classes (preprocessing methods)
  
public:
  
  Coprocessor( ClauseAllocator& _ca, Solver* solver, int32_t _threads=-1 );
  ~Coprocessor();
  
  // major methods to start preprocessing
  lbool preprocess();
  lbool inprocess();
  lbool preprocessScheduled();
  
  /* TODO:
   - extra classes for each extra techniques, which are friend of coprocessor class
   - extend model
   - ...
  */
  
private:
  Subsumption subsumption;
  
};

#endif