/*
 * WekaDataset.h
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#ifndef WEKADATASET_H_
#define WEKADATASET_H_
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
class WekaDataset {
private:
	ifstream fin;
	string line;
	bool haveLine;
	int attributesNumber;
public:
	WekaDataset(const char* filename);
	bool getDataRow(vector<string>& row);
	virtual ~WekaDataset();

	int getAttributesNumber() const {
		return attributesNumber;
	}

	void setAttributesNumber(int attributesNumber) {
		this->attributesNumber = attributesNumber;
	}
};

#endif /* WEKADATASET_H_ */
