/*
 * Configurations.h
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#ifndef CONFIGURATIONS_H_
#define CONFIGURATIONS_H_
#include <vector>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include "WekaDataset.h"

using namespace std;

class Configurations {
private:
	double timeout;
	const char* attrInfoFile;
	const char* definitionsFilename;
	vector<string> names;
	vector<string> classifierInfo;
	int startIndex;
	void readNames(const char* definitionsFilename);

	stringstream attrToRemove;
	int removeCount;
	vector<int> classIndexes;
	bool loaded;
public:
	Configurations(const char* definitionsFilename, const char* attrInfoFile);

	void removeIndex(int index);
	void addClassIndex(int index);
	void removeIndexes(const vector<int>& index);
	void removeIndexes(int offset, const vector<int>& index);
	void printRunningInfo();

	void loadConfigurationInfo();

	virtual ~Configurations();
	int getSize() const {
		return names.size();
	}

	const vector<string>& getNames() const {
		return names;
	}

	double getTimeInfo(istream& input);
	double getTimeInfo(istream& input, double penalization);
	bool isSolved(const char* cnfFile);
	bool isGood(double time);
	string nullInfo();
	string configInfo(istream& input, double penalization);
	string attrInfo(int startIndex);
	int getAttrIndex(int config) const{
		return startIndex+config;
	}

	const char* getAttrInfoFile() const {
		return attrInfoFile;
	}

	void setAttrInfoFile(const char* attrInfoFile) {
		this->attrInfoFile = attrInfoFile;
	}

	const vector<int>& getClassIndexes() const {
		return classIndexes;
	}

	void setClassIndexes(const vector<int>& classIndexes) {
		this->classIndexes = classIndexes;
	}


	const vector<string>& getClassifierInfo() const {
		return classifierInfo;
	}

	void setClassifierInfo(const vector<string>& classifierInfo) {
		this->classifierInfo = classifierInfo;
	}

	const char* getDefinitionsFilename() const {
		return definitionsFilename;
	}

	void setDefinitionsFilename(const char* definitionsFilename) {
		this->definitionsFilename = definitionsFilename;
	}
};


#endif /* CONFIGURATIONS_H_ */
