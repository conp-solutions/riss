#include <vector>
#include <string>
#include <iostream> 
#include <fstream>

#include "pca.h"

#include "methods.h"
#include "Trainer.h"

using namespace std;

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

void Trainer::solvePCA(string out) {
  
  //printVector(features[1]);
  double divisors[dimension];      
  for ( int i = 0; i < dimension; ++i ){
    divisors[i] = normalizeDivAll(features[i]);
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
  
  // get the first 'var' entries of the eigenvectors
//   vector <vector<double> > eigenvectors;
//   for (int i = 0; i < eigenvalues.size(); ++i){
//     eigenvectors.push_back(pca.get_eigenvector(i));
    //eigenvectors[i].resize(var);
    //printVector(eigenvectors[i]);
//   }
  
//   vector<vector<double> > database;

//   for (int i = 0; i< database.size(); ++i){
//     database.push_back(vector<double>(n));
//   }

  pca.set_num_retained(var);
//   var = pca.get_num_retained();
 
//  pca.solve();


//   cout << "There are " << eigenvectors.size() << " eigenvectors/eigenvalues." << endl;
  cout << "\tNew dimension:\t\t\t\t" << pca.get_num_retained()  << endl;

  // verbose?!
//   cout << "Eigenvalues:" << endl;
//   printNValues(eigenvalues, var);
  cout << "\tProjection accurate:\t\t\t" << pca.check_projection_accurate() << endl;
  
  pca.save(out);
     
}

void Trainer::writeData(string out) {
  
  ofstream ofs (out.c_str(), ofstream::out);
  
  if (ofs.is_open()){
    
    ofs << "Amount classes: " << amountClasses << endl;
    
    for (int i = 0; i < amountClasses; ++i) ofs << classNames[i] << endl;
    
    ofs << "Dimension: " << dimension << endl;
    
    for (int i = 0; i < dimension; ++i)
      ofs << featureIdents[i].second << " " << featureIdents[i].first << endl;
    
    ofs << "data: " << endl;
    for ( int i = 0; i < solved.first; ++i ){
      ofs << allClass[i] << " " << endl;
    }
    ofs.close();
    
    cout << "Data successfully written." << endl; 
  }
}





