#ifndef METHODS_H
#define METHODS_H

#include <cmath> //pow()
#include "useful.h"
#include <vector>
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

#endif
