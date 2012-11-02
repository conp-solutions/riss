/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTYPES_HH
#define COPROCESSORTYPES_HH

/** Data, that needs to be accessed by coprocessor and all the other classes
 */
class CoprocessorData
{
  /* TODO to add here
   * occ list
   * counters
   * methods to update these structures
   * no statistical counters for each method, should be provided by each method!
   */
public:
  CoprocessorData() {}
  
  // init all data structures for being used for nVars variables
  void init( uint32_t nVars ) {}
  
  // free all the resources that are used by this data object
  void destroy() {};
};

#endif