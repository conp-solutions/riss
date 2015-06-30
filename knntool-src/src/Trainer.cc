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
  
  
  cout << "long num_vars_ = " << pca.get_num_variables() << ";" << endl;
  cout << "long num_records_ = " << pca.get_num_records() << ";" << endl;
  //cout << "long record_buffer_ = " << pca.get_
  cout << "std::string solver_ = " << pca.get_solver() << ";" << endl;
  cout << "bool do_normalize_ = " << pca.get_do_normalize() << ";" << endl;
  cout << "bool do_bootstrap_ = " << pca.get_do_bootstrap() << ";" << endl;
  cout << "long num_bootstraps_ = " << pca.get_num_bootstraps() << ";" << endl;
  cout << "long bootstrap_seed_ = " << pca.get_bootstrap_seed() << ";" << endl;
  cout << "long num_retained_ = " << pca.get_num_retained() << ";" << endl;
//   //arma::Mat<double> data_  = pca.get
//   arma::Col<double> energy_ = pca.get_energy_CC();
//   arma::Col<double> energy_boot_ = pca.get_energy_boot_CC();
//   arma::Col<double> eigval_ = pca.get_eigval_CC();
//   arma::Mat<double> eigval_boot_ = pca.get_eigval_boot_CC();
//   arma::Mat<double> eigvec_ = pca.get_eigvec_CC();
//   //arma::Mat<double> proj_eigvec_ = pca.get
//   arma::Mat<double> princomp_ = pca.get_princomp_CC();
//   arma::Col<double> mean_ = pca.get_mean_CC();
//   arma::Col<double> sigma_ = pca.get_sigma_CC();
    
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
    
    ofs << "#include <vector>;" << endl;
    ofs << "using namespace std;" << endl << endl;
    
    //int's
    ofs << "int amountClasses = " << amountClasses << ";" << endl;
    ofs << "int dimension = " << dimension << ";" << endl;
   
    //vector's
    ofs << "vector<string> classNames = { " << endl; 
    if (amountClasses > 0){
      for (int i = 0; i < amountClasses-1; ++i) ofs << "\t\"" << classNames[i] << "\"," << endl;
      ofs << "\t\"" << classNames[amountClasses-1] << "\"" << endl << " };" << endl;
    }
    
    ofs << "vector<identity> featureIdents = {" << endl;
    if (dimension > 0){
      for (int i = 0; i < dimension-1; ++i) ofs << "\t(" << featureIdents[i].first << ", \"" << featureIdents[i].second << "\")," << endl;
      ofs << "\t(" << featureIdents[dimension-1].first << ", \"" << featureIdents[dimension-1].second << "\")" << endl << "};" << endl;
    }
    
    ofs << "vector<int> allClass = { " << endl;
    if (solved.first > 0) {
      for ( int i = 0; i < solved.first-1; ++i ) ofs << "\t" << allClass[i] << "," << endl;
      ofs << "\t" << allClass[solved.first-1] <<  endl << " };" << endl;
    }
    else {
      cerr << "There is no solved instance! Exit." << endl;
      exit(1);
    }
    ofs.close();
    
    cout << "Data successfully written." << endl; 
  }
}

































