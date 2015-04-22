/*
 * FeaturesWriter.h
 *
 *  Created on: Jan 4, 2014
 *      Author: gardero
 */

#include <ostream>
#include <string>
#ifndef FEATURESWRITER_H_
#define FEATURESWRITER_H_

using namespace std;

class FeaturesWriter {
private:
	ostream& output;
	int featuresNumber;
	int featuresCount;
	int timeout;
public:
	FeaturesWriter(int afeaturesNumber, int atimeout, ostream& aoutput);
	virtual ~FeaturesWriter();

	const ostream& getOutput() const {
		return output;
	}

	void writeFeature(double value);
	void fillWithUnknown();
	void close();
	int getFeaturesNumber() const {
		return featuresNumber;
	}

	void setFeaturesNumber(int featuresNumber) {
		this->featuresNumber = featuresNumber;
	}
	string getTimeoutDefinition();
};


#endif /* FEATURESWRITER_H_ */
