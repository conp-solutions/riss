#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <fstream>
#include <utility> // std::pair
#include "Trainer.h"

#include "useful.h"
// #include "methods.h"


typedef std::pair<int, std::string> identity;

void parseFiles(Trainer& T, std::ifstream& featuresFile, std::ifstream& timesFile, int timeout); 
bool validTimes (double& timeout);

int getFastestClass ();
int getStandardClass (Trainer& T, int amountClasses);

int getFeatures(Trainer& T); //returns dimension
int getClassNames(Trainer& T, std::ifstream& timesFile);
int getTimes(Trainer& T, int amountClasses, std::ifstream& timesFile); //returns the amount of files
std::pair<int, int> select(std::ifstream& featuresFile, int classAppearance[], int standardClass, int timeout, int amountClasses, int dimension);
  
  
#endif

