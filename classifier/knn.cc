/*
 * Classifier.cpp
 *
 *  Created on: Jun 4, 2015
 *      Author: Aaron Stephan
 */
#include "knn.h"
#include "methods.h"

using namespace std;

string computeKNN ( int k, vector<double>& features){
  
  int zero = 0; //TODO change type

  vector< vector<double> > points;
  vector<pair<int, double> > distances;
  vector<double> values;   
  
 // vector<double> features = readInputVector();
        
  cerr << "NormalizeMod = div" << endl;
  
   for ( int i = 0; i < dimensionCC; ++i ){
    values.push_back(features[featureIdentsCC[i].first-1]); // -1 is crucial, because the instance row is missig (is simply no double)
  }
  normalizeDiv(values, divisorsCC); //normalize the input vector as well


  int sizeDataset = allClassCC.size();
  for ( int i = 0; i < sizeDataset; ++i ){
    distances.push_back(pair<int,double>());
    distances[i].first = allClassCC[i];
  }
  
  
  /////////////// begin projection ////////////
    // create a pca object
  //stats::pca pca("dummy");
  stats::pca pca(416);
  pca.load("pca");
  values = pca.to_principal_space(values);
    //vector <double> test = pca.get_principalrow(2);
    //cout << test.size() << values.size()<< endl;
  for (int i = 0; i < sizeDataset; ++i){
    points.push_back(pca.get_principalrow(i));
  }
  //printVector(points[0]);
  //cout << endl << points.size() << points[1].size()<< endl << endl;
  ////////////// calculate distance ////////////
  for ( int i = 0; i < sizeDataset; ++i ){
     distances[i].second = euclideanDistance(values, points[i], pca.get_num_retained());
  }
  
  sort(distances.begin(), distances.end(), sort_pred());
  for ( int i = 0; i < 30; ++i ) cerr << distances[i].second << " class: " << distances[i].first << endl;
  
  classEstimation nearestClassNeigbours[amountClassesCC]{{0,0}}; // start counting by zero
  
  int result = 100; // TODO some configuration 
  
  if ( distances[0].second < zero ) result = getNearestPoint(distances, amountClassesCC);
  
  else {
    for (int i = 0; i < k; ++i) {
      nearestClassNeigbours[distances[i].first].first++;
      nearestClassNeigbours[distances[i].first].second += distances[i].second;
    }
    
    //if something happens //TODO
    result = 0;
    classEstimation max = nearestClassNeigbours[0];
    
    for (int i = 1; i < amountClassesCC; ++i) { // start by 1 because max is nearestClassNeigbours[0]
      cerr << nearestClassNeigbours[i].first << " " << nearestClassNeigbours[i].second << endl;
      if (max.first <= nearestClassNeigbours[i].first){
	if (max.first == nearestClassNeigbours[i].first){  // nearestNeighbours << sum of the distance
	  if (max.second > nearestClassNeigbours[i].second){
	    max = nearestClassNeigbours[i];
	    result = i;
	  }
	}
	else{
	  max = nearestClassNeigbours[i];
	  result = i;
	}
      }
    }
  }
  cout << result << " " << classNamesCC[result] << endl;
  cerr << "Dimension: " << dimensionCC << endl;
  cerr << "Size of the dataset: " << sizeDataset << " instances." << endl;
  return classNamesCC[result];
  
}

string computeIgrKNN ( int k, int zero, string dataset ){
  
  vector< vector<double> > points;
  vector<string> featureNames;
  vector<int> featureRows;
  vector<string> classNames;
  vector<pair<int, double> > distances;
  vector<double> values;   
  
// opening the dataset file
  ifstream trainedData ( dataset.c_str(), ios_base::in);
  if(!trainedData) {
    cerr << "c ERROR cannot open file " << dataset << " for reading" << endl;
    exit(1);
  }
  
  stringstream ss;
  int dimension, amountClasses;
  string dummy, normalizeMod;
  
  ss << trainedData.rdbuf();
  
  ss >> dummy >> dummy >> amountClasses >> dummy >> normalizeMod;
  cerr << amountClasses;
  
  for (int i = 0; i < amountClasses; ++i){
    string name;
    ss >> name ;
    classNames.push_back(name);
    cerr << classNames[i] << endl;
  }
  
  ss >> dummy >> dimension; // first line   
 
  
  if (normalizeMod == "log"){
    
    cerr << "NormalizeMod = log" << endl;
    vector<pair <double,double> > divisors;
    
    for ( int i = 0; i < dimension; ++i){
      string name;
      int row;
      double divisorMax, divisorMin;
      ss >> name >> row >> divisorMin >> divisorMax;
      featureNames.push_back(name);
      featureRows.push_back(row);
      divisors.push_back({divisorMin, divisorMax});
    }        
    
   // readInputVector(values, featureRows, dimension);
   // normalizeLog(values, divisors); //normalize the input vector as well
    
  }
  
  else if (normalizeMod == "div"){
        
    cerr << "NormalizeMod = div" << endl;
    vector<double> divisors;
    
    for ( int i = 0; i < dimension; ++i){
      string name;
      int row;
      double divisor;
      ss >> name >> row >> divisor;
      featureNames.push_back(name);
      featureRows.push_back(row);
      divisors.push_back(divisor);
    }    
    vector<double> features = readInputVector();

    cerr << "NormalizeMod = div" << endl;
  
    for ( int i = 0; i < dimension; ++i ){
      values.push_back(features[featureRows[i]-1]); // -1 is crucial, because the instance row is missig (is simply no double)
    }
    
    normalizeDiv(values, divisors); //normalize the input vector as well
  
  }  
  else {
    cerr << "NormalizeMod " << normalizeMod << " is not implemented in this Version. (use 'div' or 'log' instead)" << endl;
    exit(1);
  }
  
  int j = 0;
  int temp;
  ss >> dummy;
  if (dummy != "data:") assert (false && " The dataset-file is invalid! "); // "proofs" the correct input 

  while ( ss>>temp ){
    distances.push_back(pair<int, double>());
    distances[j].first = temp;
    points.push_back(vector<double>());
    
    for ( int i = 0; i < dimension; ++i) {
      double temp;
      ss >> temp;
      points[j].push_back(temp);
    }
    j++;
  }

  int sizeDataset = distances.size();

  
  for ( int i = 0; i < sizeDataset; ++i ){
     distances[i].second = euclideanDistance(values, points[i], dimension);
  }
  
  sort(distances.begin(), distances.end(), sort_pred());
  for ( int i = 0; i < 30; ++i ) cerr << distances[i].second << " class: " << distances[i].first << endl;
  
  assert(amountClasses > 0 && "array must have at least 1 fields");
  classEstimation nearestClassNeigbours[amountClasses];
  // start counting by zero
  nearestClassNeigbours[0] = pair<int,int>(0,0);
  
  int result = 100; // TODO some configuration 
  
  if ( distances[0].second < zero ) result = getNearestPoint(distances, amountClasses);
  
  else {
    for (int i = 0; i < k; ++i) {
      nearestClassNeigbours[distances[i].first].first++;
      nearestClassNeigbours[distances[i].first].second += distances[i].second;
    }
    
    //if something happens //TODO
    result = 0;
    classEstimation max = nearestClassNeigbours[0];
    
    for (int i = 1; i < amountClasses; ++i) { // start by 1 because max is nearestClassNeigbours[0]
      cerr << nearestClassNeigbours[i].first << " " << nearestClassNeigbours[i].second << endl;
      if (max.first <= nearestClassNeigbours[i].first){
	if (max.first == nearestClassNeigbours[i].first){  // nearestNeighbours << sum of the distance
	  if (max.second > nearestClassNeigbours[i].second){
	    max = nearestClassNeigbours[i];
	    result = i;
	  }
	}
	else{
	  max = nearestClassNeigbours[i];
	  result = i;
	}
      }
    }
  }
  cout << result << " " << classNames[result] << endl;
  
  cerr << "Dimension: " << dimension << endl;
  cerr << "Size of the dataset: " << sizeDataset << " instances." << endl;
  
  return classNames[result];
  
}


vector<double> readInputVector (){
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
  
  return featuresInit;
}
