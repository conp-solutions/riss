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
#include "libpca/pca.h"
#include "knnvar.cc"

#include <cmath> // pow()
#include "useful.h"


extern const int amountClassesCC;
extern const int dimensionCC;
extern const std::vector<std::string> classNamesCC;
extern const std::vector<identity> featureIdentsCC;
extern const std::vector<int> allClassCC;
extern const std::vector<double> divisorsCC;
extern const int solvedCC;

typedef pair<int, double> classEstimation; 

std::string computeKNN ( int k, std::vector<double>& features );
std::string computeIgrKNN ( int k, int zero, std::string dataset );

std::vector<double> readInputVector ();
  
#endif
