#ifndef KNN_H
#define KNN_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <limits> 
#include <cmath>
#include <algorithm>
#include <assert.h>
#include "../libpca/pca.h"
#include "knnvar.cc"

#include <cmath> //pow()
#include "useful.h"

using namespace std;

  extern const int amountClassesCC;
  extern const int dimensionCC;
  extern const vector<string> classNamesCC;
  extern const vector<identity> featureIdentsCC;
  extern const vector<int> allClassCC;
  extern const vector<double> divisorsCC;
  extern const int solvedCC;
    
  typedef pair<int, double> classEstimation; 
  
  string computeKNN ( int k, vector<double> features );
  string computeIgrKNN ( int k, int zero, string dataset );
  
  vector<double> readInputVector ();
  
#endif