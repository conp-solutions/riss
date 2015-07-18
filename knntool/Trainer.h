#ifndef TRAINER_H_INCLUDED
#define TRAINER_H_INCLUDED

#include <vector>
#include <string>
#include <utility> // std::pair
#include <armadillo>

class Trainer {

  public:
    
    Trainer();
    
    typedef std::pair<int, std::string> identity;

    std::vector<std::vector<double> > features;
    std::vector<identity> featureIdents;

    std::vector<std::vector<double> > allTimes;
  
    std::vector<std::string> classNames;  
    std::vector<int> allClass;
    std::vector<std::vector <int> > classCols;

    std::vector<std::string> instances;

    void setBasic(int dimension, int amountClasses, int amountFiles, int standardClass,
                  int classAppearance[], std::pair<int,int> solved );
    
    void solvePCA(std::string out);
    
    void solveIFGR(std::string out);

    void writeData(std::string out);
    
    void writeCC();
    
  private:
    
    int dimension;
    int amountClasses;
    int amountFiles;
    int standardClass;
    int classAppearance[];
    std::pair<int,int> solved; //<solved, notsolved>
    
    std::vector<double> divisors;
    
    double calculateEntropy( std::vector<double>& classValues,std::vector<double>& values);
    double calculateIntristicInformation ( std::vector<double>& values );

    
};

#endif /* Trainer_H */
