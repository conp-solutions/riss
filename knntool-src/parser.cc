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

double calculateIntristicInformation ( vector<double>& values )
{  
  // make sure that values is sorted! It normally takes at calculateEntropy place!

  double reference = values[0];
  int j = 0, i = 0, length = values.size();
  vector<int> amountAll;
  amountAll.push_back(0);
  while ( j < length ){
    if ( values[j] < reference ){
      j++;
      exit(3);
     //TODO write an assert
      continue;
    }	
    if ( values[j] > reference ){
      amountAll.push_back(1);
      reference = values[j];
      i++;
      j++;
    }
    if ( values[j] == reference ){
      amountAll[i]++; //check only for values which classValues contains;
      j++;
    }
  }
  
  double intristicInformation = 0; //start to sum up at 0
  
  for (int i = 0; i < amountAll.size(); ++i){
//     cout << amountAll[i] << endl;
    double probIntrist = amountAll[i]/float(length);
    if (probIntrist == 1 || probIntrist == 0) continue;
    intristicInformation -= (probIntrist * (log(probIntrist) / log(2)));
  }
  
  return intristicInformation;  
}

double calculateEntropy( vector<double>& classValues,vector<double>& values)
{
  sort(values.begin(), values.end());
  sort(classValues.begin(), classValues.end());
  
  int i = 0, j = 0, length = values.size();
  double reference = classValues[0];
  vector<int> amountGood;
  amountGood.push_back(0);
  vector<int> amountSum;
  amountSum.push_back(0);
  
  for (int k = 0; k < classValues.size(); ++k){
    
    if (classValues[k] == reference){
      amountGood[i]++;
      while ( j < length ){	
	if ( values[j] < reference ){
	  j++;
	  continue;
	}	
	if ( values[j] > reference ){
	  break;
	}
	if ( values[j] == reference ){
	  j++;
	  amountSum[i]++; //check only for values which classValues contains;
	}
      }
      //count someting
      continue;
    }
    if (classValues[k] > reference){
      reference = classValues[k];
      amountGood.push_back(1);
      amountSum.push_back(0);
      i++;      
      while ( j < length ){	
	if ( values[j] < reference ){
	  j++;
	  continue;
	}	
	if ( values[j] > reference ){
	  break;
	}
	if ( values[j] == reference ){
	  j++;
	  amountSum[i]++; //check only for values which classValues contains;
	}
      }
    }    
     else assert(false && "the classValues vector is not sorted correctly!");
  }
  
  int size = amountGood.size();
  
  //compute entropy
  
  double entropy =  0; //initial 'worst' case
  double temp;

  for (int i = 0; i < size; ++i){
    double probGood = amountGood[i] / float(amountSum[i]);
    if (probGood == 1) continue;
    temp = (-( probGood * (log(probGood)/log(2))) - ((1 - probGood) * (log(1-probGood)/log(2)))) *( amountSum[i]/float(length));
    entropy += temp;
  }  
  
  return entropy;

  
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
		if (isinf(temp) || temp > 1e+100) temp = 1e+100;    //TODO really naive way, might implement a better one
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
  
  //calculate entropy
  
  for ( int j = 0; j < amountClasses; ++j ){
    if ( classAppearance[j] == 0 || classAppearance[j] == solved){
      classEntropy.push_back(0);
      continue;
    }
    double good = classAppearance[j] / float(solved);
   classEntropy.push_back( - (good * (log(good)/log(2))) - ((1-good) * (log((1-good))/log(2))) );
  }
    
  //calculate informtion gain ratio
  
  vector<vector< double> > informationGainRatio;
   
  for ( int j = 0; j < amountClasses; ++j ){
    informationGainRatio.push_back(vector<double>());
    if (classes[j].size() == 0){
      for ( int i = 0; i < size; ++i ) informationGainRatio[j].push_back(0);
      continue; // entropy should be 0 ( in this case the log is not defined)
     }
    for ( int i = 0; i < size; ++i ) {
      vector<double> values;
      vector<double> classValues;

      for (int k = 0; k < features[i].size(); ++k ) values.push_back(features[i][k]);
      for ( int k = 0; k < classes[j].size(); ++k ){
	classValues.push_back(values[classes[j][k]]);
	
      }

      double entropy = calculateEntropy(classValues, values);
      double intristicInformation = calculateIntristicInformation(values);
      
      if (intristicInformation != 0) informationGainRatio[j].push_back((classEntropy[j]-entropy)/intristicInformation); // put the information together
      else informationGainRatio[j].push_back(0); // 0 is worst case --> has no ratio
    }
  }

  
 // sum up each of the information gain ratio values
 
  pair<double, int> sumRatio[features.size()] = {{0,0}};
  
  for ( int i = 0; i < size; ++i ){
    double temp = 0;
    for ( int j = 0; j < amountClasses; ++j ){
      temp += informationGainRatio[j][i];
    }
      sumRatio[i] = {temp, i};
  }
  
  sort(sumRatio, sumRatio + size, sort_pred() );

  for ( int i = 0; i < dimension; ++i ){
    cout << " Sum: " << sumRatio[i].first << " Row: " << featureRows[sumRatio[i].second] << " FeatureName: " << featureNames[sumRatio[i].second] << endl;
  }
    
//   for ( int i = 0; i < amountClasses; ++i ){
//       for ( int k = 0; k < classes[i].size(); ++k ){
// 	cout << i << " " ;
// 	for ( int j = 0; j < dimension; ++j) {
// 	  cout << features[sumRatio[j].second][classes[i][k]] << " ";
// 	}
// 	cout << endl;
//       }
//     }

  stats::pca pca(size);	
  //pca.set_do_bootstrap(true, 100);

  for (int k = 0; k < features[1].size(); ++k ){
    vector<double> record(size);
    for ( int i = 0; i < size; ++i ){ 
      record[i]=features[i][k];     
      //record.push_back(i);
     //cout << record[i] << " "; 
    }
    pca.add_record(record);
    cout << endl;
 }
   pca.solve(); 
    	cout<<"Energy = "<<pca.get_energy()<<" ("<<
       		stats::utils::get_sigma(pca.get_energy_boot())<<")"<<endl;
 
 	vector<double> eigenvalues = pca.get_eigenvalues();
 	cout<<"First three eigenvalues = "<<eigenvalues[0]<<", "
 									  <<eigenvalues[1]<<", "
 									  <<eigenvalues[2]<<endl;
	cout << endl;
	vector<double> eigenvector = pca.get_eigenvector(eigenvalues[0]);
	for (int i=0; i < eigenvalues.size(); ++i) 
	  cout << eigenvalues[i] << " ";
	cout << endl;
	for (int i=0; i < eigenvector.size(); ++i) 
	  cout << eigenvector[i] << " ";
	cout << endl;
 	cout<<"Orthogonal Check = "<<pca.check_eigenvectors_orthogonal()<<endl;
 	cout<<"Projection Check = "<<pca.check_projection_accurate()<<endl;
	pca.save("pca_results");
    exit (1);
  
  // write header
  
  if (ofs.is_open()){
    
    ofs << "Amount classes: " << amountClasses << " NormalizeMod: " << normalizeMod << endl;
    
    for (int i = 0; i < amountClasses; ++i) ofs << classNames[i] << endl;
    
    ofs << "Dimension: " << dimension << endl;
    
    if ( normalizeMod == "log" ){
      pair <double, double> divisors[dimension];
      
      for ( int i = 0; i < dimension; ++i ){
	divisors[i] = normalizeLogAll(features[sumRatio[i].second]);
      }
        
      for (int i = 0; i < dimension; ++i) ofs << featureNames[sumRatio[i].second] << " " << featureRows[sumRatio[i].second] << " " << divisors[i].first << " " << divisors[i].second << endl;
      // be careful with the featureRows vector --> rows in the csv-file and the classifier vector starting by 0 (not the indices) 
    }
    
    else if ( normalizeMod == "div" ){
      double divisors[dimension];      
      for ( int i = 0; i < dimension; ++i ){
	divisors[i] = normalizeDivAll(features[sumRatio[i].second]);
      }
      
      for (int i = 0; i < dimension; ++i) ofs << featureNames[sumRatio[i].second] << " " << featureRows[sumRatio[i].second] << " " << divisors[i] << endl;
      // be careful with the featureRows vector --> rows in the csv-file and the classifier vector starting by 0 (not the indices) 
    }
    
    else if ( normalizeMod == "emp" ){
      pair<double,double> means[dimension];
      for ( int i = 0; i < dimension; ++i ){
	means[i] = normalizeEmpiricalAll(features[sumRatio[i].second]);
      }
      
      for (int i = 0; i < dimension; ++i) ofs << featureNames[sumRatio[i].second] << " " << featureRows[sumRatio[i].second] << " " << means[i].first << " " << means[i].second << endl;
    }    
    
    else {
      cout << "Wrong normalizeMod used. (choose between 'emp', 'div' or 'log')" << endl;
      exit(1);
    }
    	
    //pca.set_do_bootstrap(true, 100);

  // write body
    ofs << "data: " << endl;
    for ( int i = 0; i < amountClasses; ++i ){
      for ( int k = 0; k < classes[i].size(); ++k ){
	ofs << i << " " ;
	//	vector<double> record;
	for ( int j = 0; j < dimension; ++j) {
	  ofs << features[sumRatio[j].second][classes[i][k]] << " ";
	 // cout << features[sumRatio[j].second][classes[i][k]] << " ";
	//  record.push_back(features[sumRatio[j].second][classes[i][k]]);
	}
	//cout << endl;
	ofs << endl;
	//pca.add_record(record);
      }
    }
  	cout<<"Solving ..."<<endl;
	//pca.solve();
		//cout<<"Energy = "<<pca.get_energy()<<" ("<<
      		//stats::utils::get_sigma(pca.get_energy_boot())<<")"<<endl;

	//const vector<double> eigenvalues = pca.get_eigenvalues();
	//cout<<"First three eigenvalues = "<<eigenvalues[0]<<", "
	//				  <<eigenvalues[1]<<", "
	//				  <<eigenvalues[2]<<endl;

	//cout<<"Orthogonal Check = "<<pca.check_eigenvectors_orthogonal()<<endl;
	//cout<<"Projection Check = "<<pca.check_projection_accurate()<<endl;

	//pca.save("pca_results");
    ofs.close();
  }
  else cout << "Unable to open output file!" << endl;
  // additional output
  


  
  int sum = 0;
  cout.precision(10);
  for (int j = 0; j < amountClasses; ++j) {
    cout << " Class: " << j << " Appearance: " << classAppearance[j] << " Probability: " << classAppearance[j] / float(found) << endl;
    sum += classAppearance[j];
  }
  cout << "Succesfully classified instances: " << sum << endl;
  cout << "Not solved instances: " << notSolved << endl;
  return 0;
}

