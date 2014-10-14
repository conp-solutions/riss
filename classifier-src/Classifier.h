/*
 * Classifier.h
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#ifndef CLASSIFIER_H_
#define CLASSIFIER_H_
#include <vector>
#include <string>
#include "Configurations.h"

using namespace std;

typedef int ClassifierMode;

class Classifier {
private:
	Configurations& configurations;
	vector< string > classifiersNames;
	vector<int> classification;
	vector<double> probabilities;
	vector<bool> correct;
	int correctCount;
	ClassifierMode mode;
	double gainThreshold;
	int verbose;
	string wekaLocation;	// location of the weka tool
	string predictorLocation;
	bool nonZero;		// consider classes as best class, even when they have a prediction of 0?
	bool useTempFiles;
	const char* prefix;
	double classifyTime;
	double timeout;
public:
	Classifier(Configurations& config,const char* prefix);
	virtual ~Classifier();
	void writeTestDetails(const char* wekaFile);
	vector<int>& classify(const char* wekaFile);
	vector<int>& classifyJava(const char* wekaFile);
	double trainModel(const char* wekaFile, int config, const char* tempclassifier,
			int localmode, int ltrees,
			int lfeatures);
	int trainBest(const char* wekaFile, int config);
	int train(const char* wekaFile);
	int test(const char* wekaFile);

	void setWekaLocation(const string& location) {
	  wekaLocation = location;
	}
	
	void setNonZero(const bool& useNonZero) {
	  nonZero = useNonZero;
	}
	
	const vector<int>& getClassification() const {
		return classification;
	}

	void setClassification(const vector<int>& classification) {
		this->classification = classification;
	}

	const vector<bool>& getCorrect() const {
		return correct;
	}

	void setCorrect(const vector<bool>& correct) {
		this->correct = correct;
	}

	int getCorrectCount() const {
		return correctCount;
	}

	void setCorrectCount(int correctCount) {
		this->correctCount = correctCount;
	}

	const vector<double>& getProbabilities() const {
		return probabilities;
	}

	void setProbabilities(const vector<double>& probabilities) {
		this->probabilities = probabilities;
	}

	ClassifierMode getMode() const {
		return mode;
	}

	void setMode(ClassifierMode mode) {
		this->mode = mode;
	}

	double getGainThreshold() const {
		return gainThreshold;
	}

	void setGainThreshold(double gainThreshold) {
		this->gainThreshold = gainThreshold;
	}

	bool isUseTempFiles() const {
		return useTempFiles;
	}

	void setUseTempFiles(bool useTempFiles) {
		this->useTempFiles = useTempFiles;
	}


	int getVerbose() const {
		return verbose;
	}

	void setVerbose(int verbose) {
		this->verbose = verbose;
	}

	const string& getPredictorLocation() const {
		return predictorLocation;
	}

	void setPredictorLocation(const string& predictorLocation) {
		this->predictorLocation = predictorLocation;
	}

	double getClassifyTime() const {
		return classifyTime;
	}

	void setClassifyTime(double classifyTime) {
		this->classifyTime = classifyTime;
	}

	double getTimeout() const {
		return timeout;
	}

	void setTimeout(double timeout) {
		this->timeout = timeout;
	}

	static const ClassifierMode RNDFOREST = 1;
	static const ClassifierMode AdaBoostM1 = 2;
	static const ClassifierMode MIXED = RNDFOREST | AdaBoostM1;
};


#endif /* CLASSIFIER_H_ */
