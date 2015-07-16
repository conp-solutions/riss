/*************************************************************************[ConfigurationSelector.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CONFIGURATIONSELECTOR_H_
#define RISS_CONFIGURATIONSELECTOR_H_

#warning remove namespace after prototyping
using namespace std;
#include <vector>
#include "mtl/sort.h"

class ConfigurationSelector {
  

public:
  
  /** select configuration based on some predefined selection procedure
   * @param table table of run times per configuration (first: instance, second: configuration)
   * @param selectedConfiguration vector with ID of selected configuration
   */
  void select( vector< vector<double> > &table, vec<int>& selectedConfiguration );
  
};


#endif