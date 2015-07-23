/*
 * Classifier.h
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#ifndef RISS_CLASSIFIER_H_
#define RISS_CLASSIFIER_H_
#include <vector>
#include <string>
#include "classifier/Configurations.h"

// using namespace std;

typedef int ClassifierMode;

class Classifier
{
  private:
    Configurations& configurations;
    std::vector< std::string > classifiersNames;
    std::vector<int> classification;
    std::vector<double> probabilities;
    std::vector<bool> correct;
    int correctCount;
    ClassifierMode mode;
    double gainThreshold;
    int verbose;
    std::string wekaLocation;   // location of the weka tool
    std::string predictorLocation;
    bool nonZero;       // consider classes as best class, even when they have a prediction of 0?
    bool useTempFiles;
    const char* prefix;
    double classifyTime;
    double timeout;
  public:
    Classifier(Configurations& config, const char* prefix);
    virtual ~Classifier();
    void writeTestDetails(const char* wekaFile);
    std::vector<int>& classify(const char* wekaFile);
    std::vector<int>& classifyJava(const char* wekaFile);
    double trainModel(const char* wekaFile, int config, const char* tempclassifier,
                      int localmode, int ltrees,
                      int lfeatures);
    int trainBest(const char* wekaFile, int config);
    int train(const char* wekaFile);
    int test(const char* wekaFile);

    void setWekaLocation(const std::string& location)
    {
        wekaLocation = location;
    }

    void setNonZero(const bool& useNonZero)
    {
        nonZero = useNonZero;
    }

    const std::vector<int>& getClassification() const
    {
        return classification;
    }

    void setClassification(const std::vector<int>& classification)
    {
        this->classification = classification;
    }

    const std::vector<bool>& getCorrect() const
    {
        return correct;
    }

    void setCorrect(const std::vector<bool>& correct)
    {
        this->correct = correct;
    }

    int getCorrectCount() const
    {
        return correctCount;
    }

    void setCorrectCount(int correctCount)
    {
        this->correctCount = correctCount;
    }

    const std::vector<double>& getProbabilities() const
    {
        return probabilities;
    }

    void setProbabilities(const std::vector<double>& probabilities)
    {
        this->probabilities = probabilities;
    }

    ClassifierMode getMode() const
    {
        return mode;
    }

    void setMode(ClassifierMode mode)
    {
        this->mode = mode;
    }

    double getGainThreshold() const
    {
        return gainThreshold;
    }

    void setGainThreshold(double gainThreshold)
    {
        this->gainThreshold = gainThreshold;
    }

    bool isUseTempFiles() const
    {
        return useTempFiles;
    }

    void setUseTempFiles(bool useTempFiles)
    {
        this->useTempFiles = useTempFiles;
    }


    int getVerbose() const
    {
        return verbose;
    }

    void setVerbose(int verbose)
    {
        this->verbose = verbose;
    }

    const std::string& getPredictorLocation() const
    {
        return predictorLocation;
    }

    void setPredictorLocation(const std::string& predictorLocation)
    {
        this->predictorLocation = predictorLocation;
    }

    double getClassifyTime() const
    {
        return classifyTime;
    }

    void setClassifyTime(double classifyTime)
    {
        this->classifyTime = classifyTime;
    }

    double getTimeout() const
    {
        return timeout;
    }

    void setTimeout(double timeout)
    {
        this->timeout = timeout;
    }

    static const ClassifierMode RNDFOREST = 1;
    static const ClassifierMode AdaBoostM1 = 2;
    static const ClassifierMode MIXED = RNDFOREST | AdaBoostM1;
};


#endif /* CLASSIFIER_H_ */
