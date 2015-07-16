#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream> 
#include <assert.h>
#include <math.h> 
#include "Parser.h"

using namespace std;

int getStandardClass (Trainer& T, int amountClasses){
  
  pair <int, double> appearance[amountClasses]{{0,0}};
  
  for ( int i = 0; i < T.allTimes.size(); ++i ){
    for ( int j = 0; j < T.allTimes[i].size(); ++j ){
      if ( T.allTimes[i][j] != -1 ){
	appearance[j].first++;
	appearance[j].second += T.allTimes[i][j];
      }
    }
  }

  int ref = 0;
  for ( int i = 1; i < amountClasses; ++i ){ // initial class 0 //TODO should be the extra class?!
    if (appearance[i].first > appearance[ref].first) ref = i;
    if (appearance[i].first == appearance[ref].first){
      if (appearance[i].second < appearance[ref].second) ref = i;
    }
  }
  return ref;
}

int getFastestClass (vector<double>& times) {
  int size = times.size();
  int minTime = 0;
  for (int i = 0; i < size; ++i){
    if (times[i] == -1) continue;
    if (times[i] < times[minTime] || times[minTime] == -1) minTime = i;
  }

    if (times[minTime] == -1) return (-1); // all configurations timed out..
    else return minTime;
}

// if the cnf is solved by all configurations it is not interesting for the classifier.
bool validTimes (vector<double>& times, int timeout) {
  bool valid = false;
  
  for ( int i = 0; i< times.size(); ++i ){
    if (times[i] == -1) 
      valid = true;
    if (times[i] > timeout){
      valid = true;
      times[i] = -1; // it can be interpreted as not solved
    }
  }
  
  return valid;
}

int getFeatures(Trainer& T, ifstream& featuresFile) {
  
  string line;
  int row = 0;
  int select = 0;
  
  //read header of the features.csv
  getline(featuresFile, line);    

  if ( Get(line, 0) != "instance" ) {
    cout << "ERROR! Header in the feature-file is missing or wrong!" << endl;
    exit(1);
  }
  
  // read the names of the features and remember the rows & sort out some features which aren't helpful
  while ( Get(line, row) != "" ) {
    
    const string field = Get(line, row).c_str();
    
    if (field.find("time") != string::npos || field == "instance"){
      row++;
      continue;
    }
    T.featureIdents.push_back(identity(row,field));
    T.features.push_back(vector<double>()); //initialize the feature vector
    select++;
    row++;
  }
  
  return select;
}

int getClassNames(Trainer& T, ifstream& timesFile) {
  
  string line;
  int row = 1;  // row 0 is the instance 
  
  // Read header of the timesFile
  getline (timesFile, line);
  while ( Get(line, row) != "") {
    T.classNames.push_back(Get(line, row));
    row++;
  }
  
  return (row - 1); // instance-row is no Class!
}
  
int getTimes(Trainer& T,int amountClasses, ifstream& timesFile)
{
  string line;
  int found = 0, col = 0;
  
  while(getline (timesFile, line)){
    
    stringstream sstime(line);   //extract instance name
    string tmp1;
    sstime >> tmp1;
    T.instances.push_back(tmp1);
    
    T.allTimes.push_back(vector<double>()); // index matches with col
    for (int i = 0; i < amountClasses; ++i){
    sstime >> tmp1;
      if ( tmp1 == "-" ) {
	T.allTimes[col].push_back(-1); // not solved instances are marked with -1
      }
      else {
	T.allTimes[col].push_back(atof(tmp1.c_str()));
      }
    }
    col++;
  }
  
  if (col == 0) exit(1); //prevent SEG FAULT
  
  return col;
}

pair<int, int> select(Trainer& T, ifstream& featuresFile, int classAppearance[], int standardClass, int timeout, int amountClasses, int dimension, int amountFiles) {
  
  string line;
  int col = 0;
  int notSolved = 0, solved = 0;
  bool decideFastest = true; //TODO implement as flag
  
  for (int i = 0; i < amountFiles; ++i){
   validTimes(T.allTimes[i], timeout); // check if timeout is in the expected range, insert's -1 if time is < timeout or even not solved (it's still -1)
  }
  
  for (int i = 0; i < amountClasses; ++i) T.classCols.push_back(vector<int>());
  
  while( getline (featuresFile, line) )
  {
    string instance = Get(line, 0).c_str();   // extract the cnf file name

    if (T.instances[col] == instance){      // lines must match!
      int cnfclass;
      /*	
      if (!validTimes(T.allTimes[col], timeout)){ 
      col++;
      continue;
      } //TODO check if it is better kicking all the instances which are solved by all configurations
      */
      if (decideFastest){
	
	if ( T.allTimes[col][standardClass] != -1 ) cnfclass = standardClass;
	else {

	  cnfclass = getFastestClass(T.allTimes[col]);	

	}
      } 
      else {
	cnfclass = getFastestClass(T.allTimes[col]);	
      }
      if (cnfclass == -1) { //skip not solved formula's
	cnfclass = standardClass;
	notSolved ++;
      }
      else {
	T.allClass.push_back(cnfclass);
	classAppearance[cnfclass]++; //count the appearance
	T.classCols[cnfclass].push_back(solved); //remember the cols where the classes are
	solved++;
	for ( int j = 0; j < dimension; ++j ){
	  double temp = atof(Get(line, T.featureIdents[j].first).c_str());
	  if (std::isinf(temp) || temp > 10e+99) 
	  temp = double(10e+99);    //TODO really naive way, might implement a better one
	  T.features[j].push_back(temp);
	}
      }
    }
    else assert (false && " Wrong order in the .csv or times file!");

    col++;  
  } 

   return pair<int, int>(solved, notSolved);
}
  
void parseFiles(Trainer& T, ifstream& featuresFile, ifstream& timesFile, int timeout){
  
  int dimension = getFeatures(T, featuresFile);
  cout << "\tInput dimension (#Features):\t\t" << dimension << endl;
  
  int amountClasses = getClassNames(T, timesFile);
  cout << "\tAmount of classes:\t\t\t" << amountClasses << endl;
  
  int amountFiles = getTimes(T, amountClasses, timesFile);
  cout << "\tAmount of instances:\t\t\t" << amountFiles << endl << endl;
  
  int standardClass = getStandardClass(T, amountClasses);
  int classAppearance[amountClasses] = {0};
  pair<int, int> solved = select(T, featuresFile, classAppearance, standardClass, timeout, amountClasses, dimension, amountFiles);
    
  cout << "\tSuccesfully classified instances:\t" << solved.first << endl;
  cout << "\tNot solved instances:\t\t\t" << solved.second << endl;
  cout << "\tStandardclass:\t\t\t\t" << standardClass << " (" << T.classNames[standardClass] << ")" << endl;
  cout << endl;

  T.setBasic(dimension, amountClasses, amountFiles, standardClass, classAppearance ,solved);
  
}
