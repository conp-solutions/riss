#ifndef METHODS_H
#define METHODS_H

#include <cmath> // pow()
#include "useful.h"
#include <vector>
#include <utility> // std::pair
#include <string>
#include <cstring>
#include <sstream>
#include <iostream> 

void printVector(std::vector<double>& printable);
void printNValues(std::vector<double>& printable, int& n);

double mean (std::vector<double>& values);
double arithmeticMean (std::vector<double>& values, double& meanX);
std::pair <double, double>  normalizeEmpiricalAll (std::vector<double>& values);
std::pair <double, double> normalizeLogAll ( std::vector<double>& values );
double normalizeDivAll ( std::vector<double>& values );
void normalizeLog ( std::vector<double>& values, std::vector<std::pair <double,double> >& divisors );
void normalizeDiv ( std::vector<double>& values, const std::vector<double>& divisors );
void normalizeEmpirical (std::vector<double>& values, std::vector <std::pair<double,double> >& meanAll);

struct sort_pred{
  bool operator()(const std::pair<int, double> &left, const std::pair<int, double> &right)
   {
     return ( (left.second < right.second) );    
   }
};

int getNearestPoint( std::vector<std::pair<int, double> >& distances, const int& amountClasses );
double euclideanDistance ( std::vector<double>& a, std::vector<double>& b, int dimension );
/*
void readInputVector ( std::vector<double>& values, std::vector<int>& featureRows, int dimension ){
  std::string input, dummy;
  std::vector<double> featuresInit;
  getline( cin, input);
  std::stringstream inputSS;
  inputSS << input;
  inputSS >> dummy;
  double tmp;
  
  while (inputSS >> tmp){
    featuresInit.push_back(tmp);
  }
  for ( int i = 0; i < dimension; ++i ){
    values.push_back(featuresInit[featureRows[i]-1]); // -1 is crucial, because the instance row is missig (is simply no double)
  }
}*/


#endif








