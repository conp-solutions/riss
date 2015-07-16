/*************************************************************************[ConfigurationSelector.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CONFIGURATIONSELECTOR_H_
#define RISS_CONFIGURATIONSELECTOR_H_

#warning remove namespace after prototyping
using namespace std;
#include <vector>

/** class that implements selection strategies to assign a configuration to instances */
class ConfigurationSelector {
  
  
  double solveTimeout;
  int maxConfigs; // number of configurations to be selected most (only for robust)

  
  
  void selectFastest( vector< vector<double> > &table, vector<int>& selectedConfiguration) {
    int instances = table.size();
    int configs = table[0].size();
    
    // check for each instance and select the fastest
    for( int i = 0; i < instances; ++ i ) 
    {
      int minP = -1; // minimum position
      for( int j = 0 ; j < configs; ++ j ) {
	if( table[i][j] < 0 ) continue; // do not consider unsolved fields
	if( minP == -1 && table[i][j] <= solveTimeout) minP = j;     // none so far, take this, if solved fast enough
	else if(  minP != -1 && table[i][minP] > table[i][j] ) minP = j; // otherwise, compare
      }
      selectedConfiguration[i] = minP; // -1, if none is selected
    }
  }
  
  
  
  void selectRobustThenFast( vector< vector<double> > &table, vector<int>& selectedConfiguration) {
    int instances = table.size();
    int configs = table[0].size();
    
    int countSolved [ configs ];
    for( int j = 0 ; j < configs; ++j ) countSolved[j] = 0;
    
    // find most robust
    for( int i = 0; i < instances; ++ i ) 
    {
      for( int j = 0 ; j < configs; ++ j ) {
	if( table[i][j] >= 0 ) countSolved[j] ++;
      }
    }
    
    // select most robust configuration
    int mostRobust = 0;
    for( int j = 1 ; j < configs; ++ j ) {
      if( countSolved[mostRobust] < countSolved[j] ) mostRobust = j;
    }
    
    // check for each instance and select the fastest
    for( int i = 0; i < instances; ++ i ) 
    {
      if( table[i][mostRobust] >= 0 && table[i][mostRobust] <= solveTimeout ) {         // this configuration can be solved by the most robust one, select it
	selectedConfiguration[i] = mostRobust; // -1, if none is selected
	continue;
      }
      
      int minP = -1;  // minimum position
      for( int j = 0 ; j < configs; ++ j ) {
	if( table[i][j] < 0 ) continue; // do not consider unsolved fields
	if( minP == -1 && table[i][j] <= solveTimeout ) minP = j;     // none so far, take this, if solved fast enough
	else if( minP != -1 && table[i][minP] > table[i][j] ) minP = j; // otherwise, compare
      }
      selectedConfiguration[i] = minP; // -1, if none is selected
    }
  }
  
  void selectRobust( vector< vector<double> > &table, vector<int>& selectedConfiguration) {
    int instances = table.size();
    int configs = table[0].size();
    
    int consideredAlready [ configs ];
    int countSolved [ configs ];
    for( int j = 0 ; j < configs; ++j ) {
      countSolved[j] = 0;
      consideredAlready[j] = 0; // not selected yet
    }
    
    int configsToSelect = maxConfigs > configs ? configs : maxConfigs; // upper limit for iterations in the round
    
    
    for( int i = 0; i < instances; ++ i ) { selectedConfiguration[i] = -1; } // make sure everything is not selected at the beginning
    
    // number of configs to be selected
    for( int s = 0 ; s < configsToSelect; ++s  ) {
    
      // find most robust remaining config of remaining formulas
      for( int i = 0; i < instances; ++ i ) 
      {
	if( selectedConfiguration[i] != -1 ) continue; // only consider instances that are still unsolved
	for( int j = 0 ; j < configs; ++ j ) {
	  if( consideredAlready[j] != 0 ) continue; // do not consider this configuration any more
	  if( table[i][j] >= 0 ) countSolved[j] ++;
	}
      }
      
      // select most robust configuration
      int mostRobust = 0;
      for( int j = 1 ; j < configs; ++ j ) {
	if( countSolved[mostRobust] < countSolved[j] ) mostRobust = j;
      }
      
      // we did not solve more formulas by the remaining configurations
      if( countSolved[mostRobust] == 0 ) {
	break;
      }
      
      for( int i = 0; i < instances; ++ i ) 
      {
	if( selectedConfiguration[i] != -1 ) continue; // do not overwrite previously assigned configs
	if( table[i][ mostRobust ] >= 0 ) selectedConfiguration[i] = mostRobust;
      }
    }
    
  }
  
  
public:
  
  ConfigurationSelector() : solveTimeout( 4000000 ) {}
  
  /** only run times below the given timeout count as being solved */
  void setSolvedTimeout ( double timeout ) { solveTimeout = timeout; }
  
  /** set number of configurations that should be considered for the robust construction */
  void setMaxConfigs( int mc ) { maxConfigs = mc; }
    
  Enum SelectionType {
    fastest,
    robust,
    robustThenFast,
  };
  
  /** select configuration based on some predefined selection procedure
   * @param table table of run times per configuration (first: instance, second: configuration)
   * @param selectedConfiguration vector with ID of selected configuration
   */
  void select( vector< vector<double> > &table, vector<int>& selectedConfiguration, SelectionType type) {
    if( table.size() == 0 ) return; 
    
    if( type == SelectionType.fastest ) return selectFastest(table, selectedConfiguration);
    if( type == SelectionType.robustThenFast ) return selectRobustThenFast(table, selectedConfiguration);
    if( type == SelectionType.robust ) return selectRobust(table, selectedConfiguration);
    // TODO add more selection routines here, e.g. if twice as fast as most robust, distribute equally, ...
  }
  
};


#endif