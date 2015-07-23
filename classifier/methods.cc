#include "methods.h"
#include <assert.h>


using namespace std;

void printVector(vector<double>& printable){
  for (int i = 0; i < printable.size(); ++i){
    cout << printable[i] << " ";
  }
  cout << endl;
}

void printNValues(vector<double>& printable, int& n){
  for (int i = 0; i < n; ++i){
    cout << printable[i] << " ";
  }
  cout << endl;
}

double mean (vector<double>& values) {
  
  int n = values.size();
  double sum = 0;
  
  for ( int i = 0; i < n; ++i ){
       sum += values[i];
  }
  
 // if (isinf(sum)) 
 //   exit (5);

  //cout << "Summe: " << sum << endl;
  return (sum / float(n));
}

double arithmeticMean (vector<double>& values, double& meanX){
  
 // double meanX = mean(values);
  int n = values.size();
  long double sum = 0;
  cout << meanX << endl;
  for ( int i = 0; i < n; ++i ){
    sum += pow((values[i]-meanX), 2)/float(n-1);
    cout << sum << " " ;
  }
  cout << sum << endl; 
//   sum = sum / float(n-1);
  
  return sqrt(sum);
  
}

pair <double, double>  normalizeEmpiricalAll (vector<double>& values){
  
  int n = values.size();
  double meanX = mean(values);
  double arithMeanX = arithmeticMean(values, meanX);
  
  for ( int i = 0; i < n; i++ ){
    values[i] = (values[i] - meanX) / arithMeanX;
  }
  
  return {meanX,arithMeanX};
  
}

pair <double, double> normalizeLogAll ( vector<double>& values ){
  for ( int i = 0; i < values.size(); ++i ){
    if ( values[i] == 0) continue; //skip zero, because it is not defined
    values[i] = log(values[i]);
  }
  
  double max = 0, min = 0;
  for ( int i = 0; i < values.size(); ++i ){
    if ( max < values[i]) max = values[i];
    if ( min > values[i]) min = values[i];
  }

  for ( int i = 0; i < values.size(); ++i ){
    if (values[i] > 0) values[i] = values[i] / max;
    if (values[i] < 0) values[i] = - values[i] / min; // keep it negative
    // if zero keep it zero!
  }
  return {min, max};
}

double normalizeDivAll ( vector<double>& values ){
  double max = 0;
  for ( int i = 0; i < values.size(); ++i ){
    if ( max < values[i]) max = values[i];
  }
  
  if (max == 0 ) return 1; 

  for ( int i = 0; i < values.size(); ++i ){
    values[i] = values[i] / max;
  }
  return max;
}

void normalizeLog ( vector<double>& values, vector<pair <double,double> >& divisors ){

  for ( int i = 0; i < values.size(); ++i ){
//     cout << values[i] << values.size() << endl;
    if (values[i] != 0) values[i] = log(values[i]);
    else continue;
    if (values[i] > 0) values[i] = values[i] / divisors[i].second;
    if (values[i] < 0) values[i] = - values[i] / divisors[i].first;
  }
}

void normalizeDiv ( vector<double>& values, const vector<double>& divisors ){

  for ( int i = 0; i < values.size(); ++i ){
    if (divisors[i] != 0) values[i] = (values[i])/divisors[i];
  }
}

void normalizeEmpirical (vector<double>& values, vector <pair<double,double> >& meanAll){
    
  int n = values.size();
   
  for ( int i = 0; i < n; i++ ){
    values[i] = (values[i] - meanAll[i].first) / meanAll[i].second;
  }
}


int getNearestPoint( vector<pair<int, double> >& distances, const int& amountClasses ){
  
  if ( distances[0].second < distances[1].second ) return distances[0].first;
  double ref = distances[0].second;
  int nearestNeighbours[amountClasses]{0};
  for ( int i = 0; i < distances.size(); ++i ){
    if ( distances[i].second == ref ){
      nearestNeighbours[distances[i].first]++;
    }
    else break;
  }
  int tmp = nearestNeighbours[0];
  int result = distances[0].first;
  for ( int i = 1; i < amountClasses; ++ i ) 
    if ( tmp < nearestNeighbours[i] ) {
      tmp = nearestNeighbours[i];
      result = i;
    }
    
  return result;
}

double euclideanDistance ( vector<double>& a, vector<double>& b, int dimension){
  
  double distance = 0;
 // printVector(a);
 // printVector(b);
  if (a.size()<dimension || b.size()< dimension) assert(false);
  for ( int i = 0; i< dimension; ++i ){
    distance += pow((a[i] - b[i]), 2);
  }
  return sqrt(distance);
}
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
