/***************************************************************************************[libcp3c.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include <vector>


/** File to work with CP3 as a library
 *  Methods provide basic access to the preprocessor
 *  At the moment only a single instance of the preprocessor can be initialized
 *  due to the option system, which currently relies on the Minisat option class
 */


// use these values to cpecify the model in extend model
#ifndef l_True
#define l_True  (lbool((uint8_t)0)) // gcc does not do constant propagation if these are real constants.
#endif

#ifndef l_False
#define l_False (lbool((uint8_t)1))
#endif 

#ifndef l_Undef
#define l_Undef (lbool((uint8_t)2))
#endif

extern "C" {

  /** initialize a preprocessor instance, and return a pointer to the maintain structure */
  extern void* cp3initPreprocessor();
  
  /** call the preprocess method of the preprocessor */
  extern void cp3preprocess();
  
  /** destroy the preprocessor and set its value to 0 */
  extern void cp3destroyPreprocessor(void*& preprocessor);
  
  /** parse the options of the command line and pass them to the preprocessor */
  extern void cp3parseOptions (void* preprocessor, int& argc, char** argv, bool strict = false);
  
  /** add a literal to the solver, if lit == 0, end the clause and actually add it */
  extern void cp3add (void* preprocessor, int lit);
  
  /** output the current internal formula into the specified variable
   * Note: the separation symbols between single clauses is the integer 0
   */
  extern void cp3dumpFormula(void* preprocessor, std::vector<int>& formula );
  
  /** freeze the given variable, so that it is not altered semantically 
   * Note: the variable might still be pushed, so that it is necessary to call giveNewLit()
   */
  extern void cp3freezeExtern(void* preprocessor, int variable );
  
  /** returns the new literal of a literal 
   * @param oldLit literal before calling preprocess()
   * @return representation of the literal after preprocessing
   */
  extern int cp3giveNewLit(void* preprocessor, int oldLit );
  
  /** recreate the variables of the given model from the state of the preprocessor 
   *  Note: will copy the model twice to be able to change the data type of the model into minisat vector Minisat::Vec
   */
  extern void cp3extendModel(void* preprocessor, std::vector<uint8_t>& model );

}