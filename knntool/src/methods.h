#ifndef METHODS_H
#define METHODS_H

#include <cmath> //pow()
#include "useful.h"
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream> 

using namespace std;

void printVector(vector<double>& printable);
void printNValues(vector<double>& printable, int& n);

double mean (vector<double>& values);
double arithmeticMean (vector<double>& values, double& meanX);
pair <double, double>  normalizeEmpiricalAll (vector<double>& values);
pair <double, double> normalizeLogAll ( vector<double>& values );
double normalizeDivAll ( vector<double>& values );
void normalizeLog ( vector<double>& values, vector<pair <double,double> >& divisors );
void normalizeDiv ( vector<double>& values, const vector<double>& divisors );
void normalizeEmpirical (vector<double>& values, vector <pair<double,double> >& meanAll);

struct sort_pred{
  bool operator()(const pair<int, double> &left, const pair<int, double> &right)
   {
     return ( (left.second < right.second) );    
   }
};

int getNearestPoint( vector<pair<int, double> >& distances, const int& amountClasses );
double euclideanDistance ( vector<double>& a, vector<double>& b, int dimension );
/*
void readInputVector ( vector<double>& values, vector<int>& featureRows, int dimension ){
  string input, dummy;
  vector<double> featuresInit;
  getline( cin, input);
  stringstream inputSS;
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








