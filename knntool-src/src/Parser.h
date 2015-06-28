#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <fstream>
#include "Trainer.h"

#include "useful.h"
// #include "methods.h"


using namespace std;

  typedef pair<int, string> identity;
  
  void parseFiles(Trainer& T, ifstream& featuresFile, ifstream& timesFile, int timeout); 
  bool validTimes (double& timeout);
  
  int getFastestClass ();
  int getStandardClass (Trainer& T, int amountClasses);
  
  int getFeatures(Trainer& T); //returns dimension
  int getClassNames(Trainer& T, ifstream& timesFile);
  int getTimes(Trainer& T, int amountClasses, ifstream& timesFile); //returns the amount of files
  pair<int, int> select(ifstream& featuresFile, int classAppearance[], int standardClass, int timeout, int amountClasses, int dimension);
  
  
#endif

