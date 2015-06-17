/*
 * FeaturesWriter.h
 *
 *  Created on: Jan 4, 2014
 *      Author: gardero
 */

#ifndef RISS_FEATURESWRITER_H_
#define RISS_FEATURESWRITER_H_

#include <ostream>
#include <string>

// using namespace std;

class FeaturesWriter
{
  private:
    std::ostream& output;
    int featuresNumber;
    int featuresCount;
    int timeout;
  public:
    FeaturesWriter(int afeaturesNumber, int atimeout, std::ostream& aoutput);
    virtual ~FeaturesWriter();

    const std::ostream& getOutput() const
    {
        return output;
    }

    void writeFeature(double value);
    void fillWithUnknown();
    void close();
    int getFeaturesNumber() const
    {
        return featuresNumber;
    }

    void setFeaturesNumber(int featuresNumber)
    {
        this->featuresNumber = featuresNumber;
    }

    std::string getTimeoutDefinition();
};


#endif /* FEATURESWRITER_H_ */
