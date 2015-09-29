/**************************************************************************************[librissc.h]
Copyright (c) 2013-2015, Norbert Manthey, All rights reserved.

 Headerffile to work with Riss as a library

 Methods provide basic access to the preprocessor

 At the moment only a single instance of the preprocessor can be initialized
 due to the option system, which currently relies on the Minisat option class


 NOTE: Building a tool with the dynamic library:
 1) make sure, the file libriss.so is located in the right directory (here, in '.')
 2) to the usual link command of your tool add the following parameters:
  -L . -lriss -lpthread -lz -lrt

 NOTE: Running your tool with the dynamic library:
 1) make sure the file libriss.so is located at a place where it can be found
**************************************************************************************************/

#ifndef LIBRISSC_H
#define LIBRISSC_H

// to represent formulas and the data type of truth values
#include "stdint.h"

// use these values to specify the model in extend model
#ifndef l_True
    #define l_True  0 // gcc does not do constant propagation if these are real constants.
#endif

#ifndef l_False
    #define l_False 1
#endif

#ifndef l_Undef
    #define l_Undef 2
#endif

/**
 * The internal variable representation of Riss ranges from 0 to n-1. However, this interface assumes that all
 * passed variables range between 1 and n. All conversions are made within the Riss-library implementation.
 */

// only if compiling with g++! -> has to be a way with defines!
#ifdef __cplusplus
extern "C" {
#endif

/** return the name of the solver and its version
 *  @return std::string that contains the verison of the solver
 */
extern const char* riss_signature();

/** initialize a solver instance, and return a pointer to the maintain structure
 */
extern void* riss_init();

/** initialize a solver instance, and return a pointer to the maintain structure
 * @param presetConfig name of a configuration that should be used
 */
extern void* riss_init_configured(const char* presetConfig);

/** set the random seed of the solver
 * @param seed random seed for double random generator ( must be between 0 and 1 )
 */
extern void riss_set_randomseed(void* riss, double seed);

/** free the resources of the solver, set the pointer to 0 afterwards */
extern void riss_destroy(void** riss);

/** add a new variables in the solver
 * @return number of the newly generated variable
 */
extern int riss_new_variable(const void* riss) ;

/** add a literal to the solver, if lit == 0, end the clause and actually add it (lit is in external 1-N variable representation)
 *  @return 0, if addition is ok. 1, if adding this literal (0) leads to a bad state of the solver
 */
extern int riss_add(void* riss, const int lit);

/** add the given literal to the assumptions for the next solver call */
extern void riss_assume(void* riss, const int lit);

/** add a variable as prefered search decision (will be decided in this order before deciding other variables)
 * Note: converts variable from external to internal representation automatically
 */
extern void riss_add_prefered_decision(void* riss, const int variable);

/** clear all prefered decisions that have been added so far */
extern void riss_clear_prefered_decisions(void* riss);

/** set a callback to a function that should be frequently tested by the solver to be noticed that the current search should be interrupted
* Note: the state has to be used as argument when calling the callback
* @param terminationState pointer to an external state object that is used in the termination callback
* @param terminationCallbackMethod pointer to an external callback method that indicates termination (return value is != 0 to terminate)
 */
extern void riss_set_termination_callback(void* riss, void* terminationState, int (*terminationCallbackMethod)(void* state));

/** apply unit propagation (find units, not shrink clauses) and remove satisfied (learned) clauses from solver
 * @return 1, if simplification did not reveal an empty clause, 0 if an empty clause was found (or inconsistency by unit propagation)
 */
extern int riss_simplify(const void* riss) ;

/** solve the formula that is currently present (riss_add) under the specified assumptions since the last call
 * Note: clears the assumptions after the solver run finished
 * add the nOfConflicts limit -1 (infinite)
 * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
 */
extern int riss_sat(void* riss);

/** solve the formula that is currently present (riss_add) under the specified assumptions since the last call
 * Note: clears the assumptions after the solver run finished
 * @param nOfConflicts number of conflicts that are allowed for this SAT solver run (-1 = infinite)
 * @return status of the SAT call: 10 = satisfiable, 20 = unsatisfiable, 0 = not finished within number of conflicts
 */
extern int riss_sat_limited(void* riss, const int64_t nOfConflicts);

/** return the polarity of a variable in the model of the last solver run (if the result was sat)
 * @return 1 = literal is true, -1 = literal is false, 0 = value is unknown
 */
extern int riss_deref(const void* riss, const int lit) ;

/** give number of literals that are present in the conflict clause that has been produced by analyze_final
 *  @return number of literals in the conflict clause
 */
extern int riss_conflict_size(const void* riss) ;

/** return the literals of the conflict clause at the specified position
 *  @return a literal of the conflict clause
 */
extern int riss_conflict_lit(const void* riss, const int position) ;

/** check whether a given assumption variable (literal is turned into the corresponding variable) is present in the current conflict clause (result of analyzeFinal)
* @return 1 if the assumption variable is part of the conflict, 0 otherwise.
*/
extern int riss_assumption_failed(void* riss, int lit);

/** returns the number of variables that are currently used by the solver
 * @return number of currently maximal variables
 */
extern int riss_variables(const void* riss) ;

/** returns the current number of assumption literals for the next solver call
 * @return number of currently added assumptions for the next solver call
 */
extern int riss_assumptions(const void* riss) ;

/** returns the number of (added) clauses that are currently used by the solver (does not include learnt clauses)
 * @return number of clauses (not including learnt clauses)
 */
extern int riss_clauses(const void* riss) ;


#ifdef __cplusplus
}
#endif

#endif
