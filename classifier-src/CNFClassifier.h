/*********************************************************************************[CNFClassifier.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef CNFCLASSIFIER_H
#define CNFCLASSIFIER_H


#include "core/Solver.h"

using namespace std;
using namespace Minisat;

/** Main class that performs the feature extraction and classification 
 * TODO: might be split into FeatureExtraction and CNFClassifier class
 */
class CNFClassifier
{

   ClauseAllocator& ca;    // clause storage
   const vec<CRef>& clauses; // vector to indexes of clauses into storage
   int nVars;         // number of variables
  
public:
CNFClassifier(ClauseAllocator& _ca, const vec<CRef>& _clauses, int _nVars);

~CNFClassifier();

/** return the names of the features */
std::vector<std::string> featureNames();
  

/** extract the features of the given formula */
std::vector<double> extractFeatures();

};

#endif // CNFCLASSIFIER_H
