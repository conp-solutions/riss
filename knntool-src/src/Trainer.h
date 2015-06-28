#ifndef TRAINER_H_INCLUDED
#define TRAINER_H_INCLUDED

#include <vector>
#include <string>

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

    void setBasic(int dimension, int amountClasses, int amountFiles, int standardClass, int classAppearance[] );
    
    void solvePCA();
    
  private:
    
    int dimension;
    int amountClasses;
    int amountFiles;
    int standardClass;
    int classAppearance[];
    
};

#endif /* Trainer_H */