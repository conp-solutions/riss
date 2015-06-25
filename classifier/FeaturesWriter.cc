/*
 * FeaturesWriter.cpp
 *
 *  Created on: Jan 4, 2014
 *      Author: gardero
 */

#include "classifier/FeaturesWriter.h"
#include <assert.h>

using namespace std;

FeaturesWriter::FeaturesWriter(int afeaturesNumber, int atimeout,  ostream& aoutput)
    : featuresNumber(afeaturesNumber),
      output(aoutput),
      timeout(atimeout),
      featuresCount(0)
{
}

FeaturesWriter::~FeaturesWriter()
{
    // TODO Auto-generated destructor stub
}

void FeaturesWriter::writeFeature(double value)
{
    if (featuresCount > 0) {
        output << ",";
    }
    output << value;
    featuresCount++;
    assert(featuresCount <= featuresNumber && "Increase the number of features");
}

void FeaturesWriter::fillWithUnknown()
{
    for (int i = featuresCount; i < featuresNumber - 1; ++i) {
        if (featuresCount > 0) {
            output << ",";
        }
        output << "?";
        featuresCount++;
    }
    output << ",yes";
}

string FeaturesWriter::getTimeoutDefinition()
{
    return "@attribute timeout {yes,no}";
}
