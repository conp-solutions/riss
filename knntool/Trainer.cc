#include <vector>
#include <string>
#include <iostream> 
#include <fstream>
#include <assert.h>

#include "libpca/pca.h"
#include "methods.h"
#include "Trainer.h"

using namespace std;


double Trainer::calculateIntristicInformation ( vector<double>& values )
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

double Trainer::calculateEntropy( vector<double>& classValues,vector<double>& values)
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
     else assert(false && "the classValues vector is not correctly sorted!");
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

Trainer::Trainer(){
}
 
void Trainer::setBasic(int dimension, int amountClasses, int amountFiles, int standardClass, int classAppearance[], pair<int,int> solved) {

  this->dimension = dimension;
  this->amountClasses = amountClasses;
  this->amountFiles = amountFiles;
  this->standardClass = standardClass;
  this->classAppearance[amountClasses] = *classAppearance;
  this->solved = solved;
  
}

void Trainer::solveIFGR(string out) {
        
  string normalizeMod = "div";
  vector<double> classEntropy;

  //calculate entropy
  
  for ( int j = 0; j < amountClasses; ++j ){
    if ( classAppearance[j] == 0 || classAppearance[j] == solved.first){
      classEntropy.push_back(0);
      continue;
    }
    double good = classAppearance[j] / float(solved.first);
   classEntropy.push_back( - (good * (log(good)/log(2))) - ((1-good) * (log((1-good))/log(2))) );
  }
    
  //calculate informtion gain ratio
  
  vector<vector< double> > informationGainRatio;
   
  for ( int j = 0; j < amountClasses; ++j ){
    informationGainRatio.push_back(vector<double>());
    if (classCols[j].size() == 0){
      for ( int i = 0; i < dimension; ++i ) informationGainRatio[j].push_back(0);
      continue; // entropy should be 0 ( in this case the log is not defined)
     }
    for ( int i = 0; i < dimension; ++i ) {
      vector<double> values;
      vector<double> classValues;

      for (int k = 0; k < features[i].size(); ++k ) values.push_back(features[i][k]);
      for ( int k = 0; k < classCols[j].size(); ++k ){
	classValues.push_back(values[classCols[j][k]]);
	
      }

      double entropy = calculateEntropy(classValues, values);
      double intristicInformation = calculateIntristicInformation(values);
      
      if (intristicInformation != 0) informationGainRatio[j].push_back((classEntropy[j]-entropy)/intristicInformation); // put the information together
      else informationGainRatio[j].push_back(0); // 0 is worst case --> has no ratio
    }
  }

  
 // sum up each of the information gain ratio values
 
  pair<double, int> sumRatio[features.size()] = {{0,0}};
  
  for ( int i = 0; i < dimension; ++i ){
    double temp = 0;
    for ( int j = 0; j < amountClasses; ++j ){
      temp += informationGainRatio[j][i];
    }
      sumRatio[i] = {temp, i};
  }
  
  sort(sumRatio, sumRatio + dimension, sort_pred() );

  for ( int i = 0; i < dimension; ++i ){
    cout << " Sum: " << sumRatio[i].first << " Row: " << featureIdents[sumRatio[i].second].first << " FeatureName: " << featureIdents[sumRatio[i].second].second << endl;
  }
  
  // write header
    
  ofstream ofs (out.c_str(), ofstream::out);

  if (ofs.is_open()){
    
    ofs << "Amount classes: " << amountClasses << " NormalizeMod: " << normalizeMod << endl;
    
    for (int i = 0; i < amountClasses; ++i) ofs << classNames[i] << endl;
    
    ofs << "Dimension: " << dimension << endl;
    
    if ( normalizeMod == "log" ){
      pair <double, double> divisors[dimension];
      
      for ( int i = 0; i < dimension; ++i ){
	divisors[i] = normalizeLogAll(features[sumRatio[i].second]);
      }
        
      for (int i = 0; i < dimension; ++i) ofs << featureIdents[sumRatio[i].second].second << " " << featureIdents[sumRatio[i].second].first << " " << divisors[i].first << " " << divisors[i].second << endl;
      // be careful with the featureIdents vector --> rows in the csv-file and the classifier vector starting by 0 (not the indices) 
    }
    
    else if ( normalizeMod == "div" ){
      double divisors[dimension];      
      for ( int i = 0; i < dimension; ++i ){
	divisors[i] = normalizeDivAll(features[sumRatio[i].second]);
      }
      
      for (int i = 0; i < dimension; ++i) ofs << featureIdents[sumRatio[i].second].second << " " << featureIdents[sumRatio[i].second].first << " " << divisors[i] << endl;
      // be careful with the featureIdents vector --> rows in the csv-file and the classifier vector starting by 0 (not the indices) 
    }
    
    else if ( normalizeMod == "emp" ){
      pair<double,double> means[dimension];
      for ( int i = 0; i < dimension; ++i ){
	means[i] = normalizeEmpiricalAll(features[sumRatio[i].second]);
      }
      
      for (int i = 0; i < dimension; ++i) ofs << featureIdents[sumRatio[i].second].second << " " << featureIdents[sumRatio[i].second].first << " " << means[i].first << " " << means[i].second << endl;
    }
    
    else {
      cout << "Wrong normalizeMod used. (choose between 'emp', 'div' or 'log')" << endl;
      exit(1);
    }
    
  // write body
    ofs << "data: " << endl;
  
    for ( int i = 0; i < amountClasses; ++i ){
      for ( int k = 0; k < classCols[i].size(); ++k ){
	ofs << i << " " ;
	for ( int j = 0; j < dimension; ++j) {
	  ofs << features[sumRatio[j].second][classCols[i][k]] << " ";
	 // cout << features[sumRatio[j].second][classes[i][k]] << " ";
	}
	ofs << endl;
      }
    }
}
}

void Trainer::solvePCA(string out) {
  
/*  
  stats::pca pca("dummy");
  
  //pca.save("test");
  
 exit(0);*/
  
  //printVector(features[1]);
  for ( int i = 0; i < dimension; ++i ){
    divisors.push_back(normalizeDivAll(features[i]));
  }
  
  // create a pca object
  stats::pca pca(dimension);
  // set normalize mod
  //pca.set_do_normalize(true);
  
  
  for (int k = 0; k < features[1].size(); ++k ){
    vector<double> record(dimension);
    for ( int i = 0; i < dimension; ++i ){ 
      record[i]=features[i][k];     
    }
    pca.add_record(record);

  }
  
  // solve...
  pca.solve(); 
  
  // calculate the new dimension
  vector<double> eigenvalues = pca.get_eigenvalues();
  double threshold = 0.95;
  int var = 0;
  for (; threshold > 0 && var < eigenvalues.size(); ++var){
    threshold -= eigenvalues[var];
  }


  pca.set_num_retained(var);

  cout << "\tNew dimension:\t\t\t\t" << pca.get_num_retained()  << endl;

  cout << "\tProjection accurate:\t\t\t" << pca.check_projection_accurate() << endl;
  pca.save("pca");
  pca.saveCC();
  vector<double> test = pca.get_principalrow(0);
  printVector(test);
    
}

void Trainer::writeData(string out) {
  
  ofstream ofs (out.c_str(), ofstream::out);
  
  if (ofs.is_open()){
    
    ofs << "Amount classes: " << amountClasses << endl;
    
    for (int i = 0; i < amountClasses; ++i) ofs << classNames[i] << endl;
    
    ofs << "Dimension: " << dimension << endl;
    
    for (int i = 0; i < dimension; ++i)
      ofs << featureIdents[i].second << " " << featureIdents[i].first << " " << divisors[i] << endl;
    
    ofs << "data: " << endl;
    for ( int i = 0; i < solved.first; ++i ){
      ofs << allClass[i] << " " << endl;
    }
    ofs.close();
    
    cout << "Data successfully written." << endl; 
  }
}

void Trainer::writeCC() {

  ofstream ofs ("knnvar.cc", ofstream::out);
  
  if (ofs.is_open()){
    
    ofs << "#include <vector>" << endl;
    ofs << "#include <string>" << endl << endl;
    ofs << "using namespace std;" << endl << "typedef pair<int, string> identity;" << endl;
    
    //int's
    ofs << "const int amountClassesCC = " << amountClasses << ";" << endl;
    ofs << "const int dimensionCC = " << dimension << ";" << endl;
    ofs << "const int solvedCC = " << solved.first << ";" << endl;
   
    //vector's
    ofs << "const vector<string> classNamesCC = { " << endl; 
    if (amountClasses > 0){
      for (int i = 0; i < amountClasses-1; ++i) ofs << "\t\"" << classNames[i] << "\"," << endl;
      ofs << "\t\"" << classNames[amountClasses-1] << "\"" << endl << " };" << endl;
    }
    else ofs << "};" << endl;
    
    ofs << "const vector<identity> featureIdentsCC = {" << endl;
    if (dimension > 0){
      for (int i = 0; i < dimension-1; ++i) ofs << "\t{" << featureIdents[i].first << ", \"" << featureIdents[i].second << "\"}," << endl;
      ofs << "\t{" << featureIdents[dimension-1].first << ", \"" << featureIdents[dimension-1].second << "\"}" << endl << "};" << endl;
    }
    else ofs << "};" << endl;

    ofs << "const vector<int> allClassCC = { " << endl;
    if (solved.first > 0) {
      for ( int i = 0; i < solved.first-1; ++i ) ofs << "\t" << allClass[i] << "," << endl;
      ofs << "\t" << allClass[solved.first-1] <<  endl << " };" << endl;
    }
    else ofs << "};" << endl;

    ofs << "const vector<double> divisorsCC = { " << endl;
    if (dimension > 0) {
      for ( int i = 0; i < dimension-1; ++i ) ofs << "\t" << divisors[i] << "," << endl;
      ofs << "\t" << divisors[dimension-1] <<  endl << " };" << endl;
    }
    else ofs << "};" << endl;

    ofs.close();
    
    cout << "Data successfully written." << endl; 
  }
  else {
    cerr << "Output file error!" << endl;
    exit(1);
  }
}

































