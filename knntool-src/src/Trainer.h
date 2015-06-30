#ifndef TRAINER_H_INCLUDED
#define TRAINER_H_INCLUDED

#include <vector>
#include <string>
#include <armadillo>

using namespace std;

class Trainer {

  public:
    
    Trainer();
    
    typedef pair<int, string> identity;

    vector<vector<double> > features;
    vector<identity> featureIdents;

    vector<vector<double> > allTimes;
  
    vector<string> classNames;  
    vector<int> allClass;
    vector<vector <int> > classCols;

    vector<string> instances;

    void setBasic(int dimension, int amountClasses, int amountFiles, int standardClass, int classAppearance[], pair<int,int> solved );
    
    void solvePCA(string out);
    
    void writeData(string out);
    
    void writeCC();
    
  private:
    
    int dimension;
    int amountClasses;
    int amountFiles;
    int standardClass;
    int classAppearance[];
    pair<int,int> solved;
    
    vector<double> divisors;      

    
};

#endif /* Trainer_H */