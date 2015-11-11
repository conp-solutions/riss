/*************************************************************************[ConfigurationSelector.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CONFIGURATIONSELECTOR_H_
#define RISS_CONFIGURATIONSELECTOR_H_

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
	  if( table[i][j] >= 0  && table[i][j] <= solveTimeout ) countSolved[j] ++;
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
	if( table[i][ mostRobust ] >= 0 && table[i][mostRobust] <= solveTimeout ) selectedConfiguration[i] = mostRobust;
      }
    }
    
  }
  
  void selectEqualNumber( vector< vector<double> > &table, vector<int>& selectedConfiguration) {
    int instances = table.size();
    int configs = table[0].size();
    
    int consideredAlready [ configs ];
    int solvedOnRemaining [ configs ];
    for( int j = 0 ; j < configs; ++j ) {
      solvedOnRemaining[j] = 0;
      consideredAlready[j] = 0; // not selected yet
    }
    
    vector<int> remainingIndexes( table.size());
    remainingIndexes.clear();
    for( int i = 0 ; i < instances; ++i ) { 
      bool solvable = false;
	for( int j = 0 ; j < configs; ++ j ) {
	  if( table[i][j] >= 0 && table[i][j] <= solveTimeout  ) {
	    solvedOnRemaining[j] ++; // count number of solved on whole set
	    solvable = true; // this formula can be solved, and hence can be selected
	  }
	}
	if( solvable ) remainingIndexes.push_back( i ); // add this formula to the set of formulas that can be solve/have to be considered
    }
    
    // as long as there are formulas that can be solved
    while( remainingIndexes .size() > 0 ) {

      int minC = 0;
      // select least frequently used configuration, stop if no more formulas are left
      for ( int j = 0 ; j < configs; ++j ) {
	if( consideredAlready[j] < consideredAlready[minC] ) minC = j;
      }
      
  #if 0 
      // alternative, effect not clear ...
      // select worst configuration, stop if no more formulas are left
      for ( int j = 0 ; j < configs; ++j ) {
	if( solvedOnRemaining[j] < solvedOnRemaining[minC] ) minC = j;
      }
  #endif
  
      if( solvedOnRemaining[minC] == 0 ) break; // stop if no more formulas can be solved
      
      // select fastest solution of that configuration on all formulas
      int fastestIndex = -1;
      int fastestR = -1;
      for( int r = 0; r < remainingIndexes.size(); ++ r ) {
	int i = remainingIndexes[r];
	
	if( table[i][minC] < 0 ) continue; // this formula cannot be solved by this configuration
	if( table[i][minC] > solveTimeout ) continue; // this formula cannot be solved in the timeout
	// all entries here can be solved by configuration minC
	if( fastestIndex == -1 ) { 
	  fastestIndex = i; // this is the first instance config minC can solve
	  fastestR = r; // memorize position in the index array
	}
	else {
	  if( table[fastestIndex][minC] > table[i][minC] ) {
	    fastestIndex = i; // this is the fastest instance that minC can solve (until here)
	    fastestR = r; // memorize position in the index array
	  }
	}
      }
      assert( fastestIndex != -1 && fastestR != -1 && "there had to be an instance that could be solved" );
      
      // assign this configuration to the selected index
      selectedConfiguration[fastestIndex] = minC;
      
      consideredAlready[minC] ++; // count that this configuration has been used one more time
      
      // update solvedOnRemaining counts
      for( int j = 0 ; j < configs; ++j ) {
	if( table[fastestIndex][j] >= 0 && table[fastestIndex][j] <= solveTimeout  ) solvedOnRemaining[j] --; // number of solved formulas decreases for the configuration j
      }
      
      // remove this index from the remaining formulas
      remainingIndexes[ fastestIndex ] = remainingIndexes[remainingIndexes.size() -1 ]; // move last position forward
      remainingIndexes.pop_back(); // remove last element, that has been move forward before
    
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
    equalNumbers,
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
    if( type == SelectionType.equalNumbers ) return selectEqualNumber(table, selectedConfiguration);
    // TODO add more selection routines here, e.g. if twice as fast as most robust, distribute equally, ...
  }
  
};


#endif
