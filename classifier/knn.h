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

#include <cmath> // pow()
#include "useful.h"

typedef pair<int, double> classEstimation; 

std::string computeKNN ( int k, std::vector<double>& features );
std::string computeIgrKNN ( int k, int zero, std::string dataset );

std::vector<double> readInputVector ();
  
#endif
