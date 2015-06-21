#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <limits> 
#include <cmath>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <pca.h>

#include "useful.h"
#include "methods.h"

using namespace std;

// struct sort_pred{
//   bool operator()(const pair<double, int> &left, const pair<double ,int> &right)
//    {
//      return ( (left.first > right.first) );    
//    }
// };

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
void centeringData(vector<vector<double> >& features){
  
  for (int i = 0; i < features.size(); ++i){  
     
    double meanF = mean(features[i]);
    //cout << "Vorhher: " << meanF << endl;
     //printVector(features[i]);
    for (int j = 0; j < features[i].size(); ++j){
       features[i][j] = features[i][j] - meanF;
    }
  
    //cout << "Nachher: " << mean(features[i]) << endl;   
    //printVector(features[i]);
    
  } 
}

void transform(vector<vector<double> >& database, vector<vector<double> >& features, vector<vector<double> >& eigenvectors, int var){
  
  int n = features[1].size(); //amount of files
  int m = features.size(); //amount of features
 
  for (int i = 0; i < n; ++i){   //for each file
    
    vector<double> tmp0;
    
    for (int j = 0; j < var; ++j){ //for each feature
      
      //dot-product
      double tmp1;
      
      for (int k = 0; k < m; ++k){
	cout << features[k][i] << " * " << eigenvectors[k][j] << endl;
	tmp1 += features[k][i] * eigenvectors[k][j];
      }
    tmp0.push_back(tmp1);
    }
    database.push_back(tmp0);
  }
}

int getStandardClass (vector<vector<double> >& times, int& amountClasses){
  
  pair <int, double> appearance[amountClasses]{{0,0}};
  
  for ( int i = 0; i < times.size(); ++i ){
    for ( int j = 0; j < times[i].size(); ++j ){
      if ( times[i][j] != -1 ){
	appearance[j].first++;
	appearance[j].second += times[i][j];
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

int getFastestClass ( vector<double>& times )
{
  int size = times.size();
  int minTime = 0;
  for (int i = 0; i < size; ++i){
    if (times[i] == -1) continue;
    if (times[i] < times[minTime] || times[minTime] == -1) minTime = i;
  }

    if (times[minTime] == -1) return times.size(); // all configurations timed out..
    else return minTime;
}


// if the cnf is solved by all configurations it is not interesting for the classifier.
bool validTimes (vector<double>& times, double& timeout){
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


  
int main( int argc, const char* argv[] )
{
  
/* reading the command
 */

  if (argc < 8){
    cout << "c ERROR wrong amount of parameters" << endl;
    exit(1);
  }

  // opening the features.csv file
  ifstream featuresFile ( argv[1], ios_base::in);
  // opening the times.dat
  ifstream timesFile ( argv[2], ios_base::in);
  // opening the features-calculation-time file
  ifstream featureCalculation ( argv[3], ios_base::in);
  // output file (dataset) 
  ofstream ofs (argv[4], ofstream::out);
  string normalizeMod = argv[5];
  int dimension = atoi(argv[6]);
  double timeout = atof(argv[7]);
 
  
  if(!featuresFile) {
    cerr << "c ERROR cannot open file " << argv[1] << " for reading" << endl;
    exit(1);
  }  
  if(!timesFile) {
    cerr << "c ERROR cannot open file " << argv[2] << " for reading" << endl;
    exit(1);
  }
  if(!featureCalculation) {
    cerr << "c ERROR cannot open file " << argv[3] << " for reading" << endl;
    exit(1);
  } 

  // declaring some variables
  
  string line, timeline, calcfeatures;
  int row = 0, col = 0, select = 0, solved = 0;
  int amountClasses = 0, amountInstances = 0;
  int found = 0, notSolved = 0;
  
  vector<vector<double> > features;
  vector<int> featureRows;
  vector<string> featureNames;
  vector<string> classNames;
  vector<double> classEntropy;
  vector<vector <int> > classes;
  vector<vector<double> > times;
  
//read header of the features.csv
  getline(featuresFile, line);
  cout << Get(line, 0).c_str() << endl;
  
  if ( Get(line, 0) != "instance" ) {
    cout << "ERROR " << argv[1] << " is not the correct format!" << endl;
    exit(1);
  }
  
  // read the names of the features and remember the rows & sort out some features which aren't helpful
  while ( Get(line, row) != "" ) {
    
    const string field = Get(line, row).c_str();
    
    if (field.find("time") != string::npos || field == "instance"){
      row++;
      continue;
    }
    featureNames.push_back(field);
    featureRows.push_back(row);
    features.push_back(vector<double>()); //initialize the feature vector
    select++;
    row++;
  }
  
  cout << select << " features of " << row-1 << " selected." << endl;
  
  int size = featureNames.size();
  
  
// Read header of the timesFile
  getline (timesFile, timeline);
  row = 1; // row 0 is the instance [reuse the variable]
  while ( Get(timeline, row) != "") {
    classNames.push_back(Get(timeline, row));
    row++;
  }
  
  amountClasses = row - 1; // instance-row is no Class!
  int classAppearance[amountClasses] = {0};
  
  for (int i = 0; i < amountClasses; ++i) classes.push_back(vector<int>());
  
  //extract instances and classes of the solved training data (best time)
  vector<string> instances;
  getline (featureCalculation, calcfeatures); // remove the header
  if ( Get(calcfeatures, 0).c_str() == "instance" ) {
    cout << "c ERROR missing header in " << argv[3] << endl;
    exit(1);
  }
  while(getline (timesFile, timeline)){
    getline (featureCalculation, calcfeatures);
    stringstream sstime(timeline);
    string tmp1;
    sstime >> tmp1;
    instances.push_back(tmp1);
    times.push_back(vector<double>()); // index matches with col
    found++;
    for (int i = 0; i < amountClasses; ++i){
    sstime >> tmp1;
      if ( tmp1 == "-" ) {
	times[col].push_back(-1);
      }
      else {
	times[col].push_back( atof(tmp1.c_str()) + atof(Get(calcfeatures, 5).c_str()) );
      }
    }
    col++;
  }
  
  col =  0;
  int standardClass = getStandardClass ( times, amountClasses );
  cout << "Found " << amountClasses << " classes. Class number " << standardClass << "is the standard class." << endl;
  
  while( getline (featuresFile, line) )
  {
    string instance = Get(line, 0).c_str();   // extraxt the cnf file name
    
      if (instances[col] == instance){
	int cnfclass;
	
	if (!validTimes(times[col], timeout)){
	  col++;
	  continue;
	}
	
	if ( times[col][standardClass] != -1 ) cnfclass = standardClass;
	else cnfclass = getFastestClass(times[col]);
	//cnfclass = getFastestClass(times[col]);
	
	
	    if (cnfclass == times[col].size()) {
	      cnfclass = 100; //TODO standartClass
	      notSolved ++;
	    }
	    else {
	      classAppearance[cnfclass]++; //count the appearance
	      classes[cnfclass].push_back(solved); //remember the cols where the classes are
	      solved++;
	      for ( int j = 0; j < size; ++j ){
		double temp = atof(Get(line, featureRows[j]).c_str());
		if (isinf(temp) || temp > 10e+99) 
		  temp = double(10e+99);    //TODO really naive way, might implement a better one
		features[j].push_back(temp);
	      }
	    }
      }
      else assert (false && " Wrong order in the .csv oder times file!");

    col++;  
  } 
  timesFile.close();
  featuresFile.close();
  
  amountInstances = col; //exept header
 
  cout << found << " instances of " << amountInstances << " found." <<  endl;
  if (found == 0) exit(2); //prevent SEG FAULT
    
  
  ///////////////////////////////////////////////////////////
  ///////////////////// begin with PCA //////////////////////

  // centering the data

  centeringData(features);
  
  // create a pca object
  stats::pca pca(size);	

  // 
  for (int k = 0; k < features[1].size(); ++k ){
    vector<double> record(size);
    for ( int i = 0; i < size; ++i ){ 
      record[i]=features[i][k];     
    }
    pca.add_record(record);

  }
  
  // solve...
 // pca.set_do_normalize(true);
  pca.solve(); 
  
  // calculate the new dimension
  vector<double> eigenvalues = pca.get_eigenvalues();
  double threshold = 0.95;
  int var = 0;
  for (; threshold > 0 && var < eigenvalues.size(); ++var){
    threshold -= eigenvalues[var];
  }
  
  // get the first 'var' entries of the eigenvectors
  vector <vector<double> > eigenvectors;
  for (int i = 0; i < eigenvalues.size(); ++i){
    eigenvectors.push_back(pca.get_eigenvector(i));
    eigenvectors[i].resize(var);
    //printVector(eigenvectors[i]);
  }
  
  cout << "There are " << eigenvectors.size() << " eigenvectors/eigenvalues." << endl;
  cout << "The new Dimension is " << var << "." << endl;

  cout << "Eigenvalues:" << endl;
  printNValues(eigenvalues, var);
  
  // calculate new database
  vector<vector<double> > database;
  transform (database, features, eigenvectors, var);
  
  for (int i = 0; i < database.size(); ++i){
    printVector(database[i]);
  }
  
  
//   double sum = 0;
//   for ( int i = 0; i < eigenvalues.size(); ++i ){
//        sum += eigenvalues[i];
//   }
//   cout << "SUMME: " << sum << endl;
  
	pca.save("pca_results");

  
 
  exit (0);
}

