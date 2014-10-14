/*************************************************************************************[libprissc.h]
Copyright (c) 2014, Norbert Manthey, All rights reserved.

 Headerffile to work with Riss as a library
 
 Methods provide basic access to the preprocessor
 
 At the moment only a single instance of the preprocessor can be initialized
 due to the option system, which currently relies on the Minisat option class
 
 
 NOTE: Building a tool with the dynamic library:
 1) make sure, the file libpriss.so is located in the right directory (here, in '.')
 2) to the usual link command of your tool add the following parameters:
  -L . -lpriss -lpthread -lz -lrt
  
 NOTE: Running your tool with the dynamic library:
 1) make sure the file libpriss.so is located at a place where it can be found
**************************************************************************************************/

#ifndef LIBPRISSC_H
#define LIBPRISSC_H

// to represent formulas and the data type of truth values
#include "stdint.h"


// use these values to cpecify the model in extend model
#ifndef l_True
#define l_True  0 // gcc does not do constant propagation if these are real constants.
#endif

#ifndef l_False
#define l_False 1
#endif 

#ifndef l_Undef
#define l_Undef 2
#endif

// #pragma GCC visibility push(hidden)
// #pragma GCC visibility push(default)
// #pragma GCC visibility pop // now we should have default!

// only if compiling with g++! -> has to be a way with defines!
extern "C" {

  /** initialize a solver instance, and return a pointer to the maintain structure 
   * @param threads number of threads that should be used (1 <= threads <= 64), will be adjusted if not in these bounds!
   */
  extern void* priss_init(int& threads, const char* configName);
  
  /** free the resources of the solver, set the pointer to 0 afterwards */
  extern void priss_destroy(void*& priss);
  
  /** add a literal to the solver, if lit == 0, end the clause and actually add it 
   *  @return 0, if addition is ok. 1, if adding this literal (0) leads to a bad state of the solver
   */
  extern int priss_add (void* priss, const int& lit);

  /** add the given literal to the assumptions for the next solver call */
  extern void priss_assume (void* priss, const int& lit);
  
  /** solve the formula that is currently present (priss_add) under the specified assumptions since the last call
   * Note: clears the assumptions after the solver run finished
   * @param nOfConflicts number of conflicts that are allowed for this SAT solver run (-1 = infinite)
   * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
   */
  extern int priss_sat (void* priss, const int& nOfConflicts);

  /** return the polarity of a variable in the model of the last solver run (if the result was sat) 
   * @return 1 = literal is true, -1 = literal is false, 0 = value is unknown
   */
  extern int priss_deref (const void* priss, const int& lit) ;
}

// #pragma GCC visibility pop // back to what we had before

#endif