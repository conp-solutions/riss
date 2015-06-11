/***************************************************************************************[libcp3c.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.

 File to work with Coprocessor as a library

 Methods provide basic access to the preprocessor

 At the moment only a single instance of the preprocessor can be initialized
 due to the option system, which currently relies on the Minisat option class
**************************************************************************************************/

// to represent formulas and the data type of truth values
#include <vector>
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

extern "C" {

    /** initialize a preprocessor instance, and return a pointer to the maintain structure */
    extern void* CPinit();

    /** call the preprocess method of the preprocessor */
    extern void CPpreprocess(void* preprocessor);

    /** destroy the preprocessor and set its value to 0 */
    extern void CPdestroy(void*& preprocessor);

    /** parse the options of the command line and pass them to the preprocessor */
    extern void CPparseOptions(void* preprocessor, int& argc, char** argv, bool strict = false);

    /** add a literal to the solver, if lit == 0, end the clause and actually add it */
    extern void CPaddLit(void* preprocessor, int lit);

    /** output the current internal formula into the specified variable
     * Note: the separation symbols between single clauses is the integer 0
     */
    extern void CPwriteFormula(void* preprocessor, std::vector<int>& formula);

    /** freeze the given variable, so that it is not altered semantically
     * Note: the variable might still be pushed, so that it is necessary to call giveNewLit()
     */
    extern void CPfreezeVariable(void* preprocessor, int variable);

    /** returns the new literal of a literal
     * @param oldLit literal before calling preprocess()
     * @return representation of the literal after preprocessing
     */
    extern int CPgetReplaceLiteral(void* preprocessor, int oldLit);

    /** recreate the variables of the given model from the state of the preprocessor
     *  Note: will copy the model twice to be able to change the data type of the model into minisat vector Riss::Vec
     */
    extern void CPpostprocessModel(void* preprocessor, std::vector<uint8_t>& model);

    /** returns the number of variables of the formula that is inside the preprocessor
     *  Note: the number of variables can be higher inside the preprocessor, if techniques like
     *  rewriting or BVA have been used, since these techniques introduce new variables
     */
    extern int CPnVars(void* preprocessor);

    /** return state of preprocessor */
    extern bool CPok(void* preprocessor);

    /** return whether a given literal is already mapped to false */
    extern bool CPlitFalsified(void* preprocessor, int lit);
}
