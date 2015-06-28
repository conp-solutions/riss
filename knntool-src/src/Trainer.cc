#include <vector>
#include <string>
#include <iostream> 

#include "pca.h"

#include "methods.h"
#include "Trainer.h"

using namespace std;

Trainer::Trainer(){
}
 
void Trainer::setBasic(int dimension, int amountClasses, int amountFiles, int standardClass, int classAppearance[]) {

  this->dimension = dimension;
  this->amountClasses = amountClasses;
  this->amountClasses = amountFiles;
  this->standardClass = standardClass;
  this->classAppearance[amountClasses] = *classAppearance;
  
}

void Trainer::solvePCA() {
  
  // centering the data

  //centeringData(features);
  
      //printVector(features[1]);
  double divisors[dimension];      
  for ( int i = 0; i < dimension; ++i ){
    divisors[i] = normalizeDivAll(features[i]);
    //printVector(features[i]);
  }
  
  // create a pca object
  stats::pca pca(dimension);
  
  
  for (int k = 0; k < features[1].size(); ++k ){
    vector<double> record(dimension);
    for ( int i = 0; i < dimension; ++i ){ 
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
    //eigenvectors[i].resize(var);
    //printVector(eigenvectors[i]);
  }
  
  cout << "eigenvectors: " << eigenvalues.size() << endl;
  vector<vector<double> > database;

//   for (int i = 0; i< database.size(); ++i){
//     database.push_back(vector<double>(n));
//   }
  cout << pca.get_num_retained() << endl;
  //var = pca.get_num_retained();
  //pca.set_num_retained(var);
  
   //mva_MatrixMult(features, eigenvectors, database, features.size(), n, features.size());
  //transform (database, features, eigenvectors);
  //for (int i = 0; i < database.size(); ++i){
   // printVector(database[i]);
  //}

  cout << "There are " << eigenvectors.size() << " eigenvectors/eigenvalues." << endl;
  cout << "The new Dimension is " << var << "." << endl;

  cout << "Eigenvalues:" << endl;
  printNValues(eigenvalues, var);
  cout << pca.check_projection_accurate() << endl;
  
  pca.save("knn");
    
  
}

