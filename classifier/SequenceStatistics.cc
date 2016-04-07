/*
 * SecuenceStatistics.cpp
 *
 *  Created on: Oct 22, 2013
 *      Author: gardero
 */

#include "classifier/SequenceStatistics.h"
#include <cfloat>
#include <cstdio>
#include <string>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <assert.h>

using namespace std;

void SequenceStatistics::initAll()
{
    min = DBL_MAX;
    modeCount = 0;
    max = 0;
    sum = 0;
    sumXsq = 0;
    count = 0;
    stDeviation = 0;
    median = 0;
    mean = 0;
    quantilesCount = 0;
    mode = 0;
    entropy = 0;
    computedFlag = false;
    valuesCount = 0;
    valuesRate = 0;
    zerocount = 0;
    computingDerivative = false;
    derivative = nullptr;
    countingZeros = true;
}

//SequenceStatistics::SequenceStatistics() {
//  initAll();
//}

SequenceStatistics::SequenceStatistics(bool computingDerivative)
{
    initAll();
    this->computingDerivative = computingDerivative;
    if (computingDerivative) {
        derivative = new SequenceStatistics(false);
    }
}

SequenceStatistics::SequenceStatistics(bool computingDerivative, bool countingZeros)
{
    initAll();
    this->computingDerivative = computingDerivative;
    this->countingZeros = countingZeros;
    if (computingDerivative) {
        derivative = new SequenceStatistics(false);
    }
}

SequenceStatistics::~SequenceStatistics()
{
    vector<double>().swap(values);
    if (computingDerivative) {
        delete derivative;
    }
}

void SequenceStatistics::addValue(double value)
{
    if (value > 0) {
        count++;
        sum += value;
        sumXsq += (value * value);
        if (std::isnan(sum) || std::isnan(sumXsq)) {
            printf("wrong!!!\n");
        }
        if (value > max) {
            max = value;
        }
        if (value < min) {
            min = value;
        }

        double delta = value - mean;
        mean = mean + delta / count;
        stDeviation = stDeviation + delta * (value - mean);

        values.push_back(value); // you get space here anyways, if there is not already sufficient space
    } else {
//      assert(countingZeros && "zero values are added, need to count them");
        zerocount++;
    }
}

uint64_t SequenceStatistics::compute(int quantilesCount)
{
    uint64_t operations = 0;
    if (count > 0) {
        assert(!computedFlag && "Computing can only be performed once");
        this->quantilesCount = quantilesCount;
        double qsize = count / quantilesCount;
        int i;
        double entropysum = 0;
        sort(values.begin(), values.end());
        operations += quantilesCount * log(quantilesCount);
        // TODO compute the mode and quantiles
        for (int i = 1; i < quantilesCount; ++i) {
            quantiles.push_back(values[i * qsize]);
        }
        operations += quantilesCount;
//          mean = sum/count;
        stDeviation = (count > 1) ? stDeviation / (count - 1) : 0.0;
        int mcount = 1;
        double m = values[0];
        for (i = 1; i < count; ++i) {
            if (m == values[i]) {
                mcount++;
            } else {
                valuesCount++;
                // entropy computation
                entropysum += (mcount * log(mcount));
                // mode computation
                if (mcount > modeCount) {
                    modeCount = mcount;
                    mode = m;
                }
                m = values[i];
                mcount = 1;
            }
        }
        operations += count;
        valuesCount++;
        valuesRate = double(valuesCount) / count;
        // entropy computation
        entropysum += (mcount * log(mcount));
        // mode computation
        if (mcount > modeCount) {
            modeCount = mcount;
            mode = m;
        }
        entropy = log(count) - entropysum / count;
        if (computingDerivative) {
            for (i = 1; i < count; ++i) {
                derivative->addValue(values[i] - values[i - 1]);
            }
            operations += count;
            operations += derivative->compute(quantilesCount);
        }
    } else {
        min = 0;
        for (int i = 1; i < quantilesCount; ++i) {
            quantiles.push_back(0);
        }
        if (computingDerivative) {
            operations += quantilesCount;
            operations += derivative->compute(quantilesCount);
        }
    }
    computedFlag = true;
    return operations;
}

void SequenceStatistics::tostdio() const
{
    printf(
        "zcount: %d min: %.1lf, max: %.1lf, mode: %.1lf, mean: %.2lf, stdev: %.2lf, entropy: %.4lf\n",
        zerocount, min, max, mode, mean, stDeviation, entropy);
    int i;
    for (i = 0; i < quantiles.size(); ++i) {
        printf("Q%d: %.1f, ", (i + 1), quantiles[i]);
    }
    printf("\n");
    if (computingDerivative) {
        derivative->tostdio();
    }
}

//} /* namespace features */

void SequenceStatistics::valuesToVector(vector<double>& v) const
{
    if (countingZeros) {
        v.push_back(zerocount);
    }
    v.push_back(min);
    v.push_back(max);
    v.push_back(mode);
    v.push_back(mean);
    v.push_back(stDeviation);
    v.push_back(entropy);
    v.push_back(valuesRate);
    int i;
    for (i = 0; i < quantiles.size(); ++i) {
        v.push_back(quantiles[i]);
    }
    if (computingDerivative) {
        derivative->valuesToVector(v);
    }
}

void SequenceStatistics::namesToVector(string prefix, vector<string>& v) const
{
    if (countingZeros) {
        v.push_back(prefix + " zcount");
    }
    v.push_back(prefix + " min");
    v.push_back(prefix + " max");
    v.push_back(prefix + " mode");
    v.push_back(prefix + " mean");
    v.push_back(prefix + " stdev");
    v.push_back(prefix + " entropy");
    v.push_back(prefix + " valuesRate");
    int i;
    for (i = 0; i < quantiles.size(); ++i) {
        stringstream sstm;
        sstm << prefix << " Q" << (i + 1);
        v.push_back(sstm.str());
    }
    if (computingDerivative) {
        derivative->namesToVector(prefix + "_derivative", v);
    }
}

void SequenceStatistics::infoToVector(string prefix, vector<string>& names,
                                      vector<double>& values) const
{
    assert(
        names.size() == values.size()
        && "[begin] features names and values do not match");
    namesToVector(prefix, names);
    valuesToVector(values);
    assert(
        names.size() == values.size()
        && "[end] features names and values do not match");
}

static inline ostream& operator<<(ostream& other,
                                  const SequenceStatistics& seq)
{
    const vector<double> v = seq.getValues();
    other << "# N V" << endl;
    for (int i = 0; i < v.size(); ++i) {
        other << "  " << i + 1 << " " << v[i] << endl;
    }
    return other;
}

void SequenceStatistics::toStream(ostream& other) const
{
    other << *this;
}

void SequenceStatistics::valuesToStream(ostream& other) const
{
    vector<double> v;
    valuesToVector(v);
    for (int i = 0; i < v.size(); ++i) {
        other << v[i] << ", ";
    }
}

void SequenceStatistics::toStream(const char* filename, const char* ext) const
{
    std::ofstream fout;
    std::string result = std::string(filename) + std::string(ext);
    fout.open(result.c_str(), ios::out);
    toStream(fout);
    fout.close();
}

inline const vector<double>& SequenceStatistics::getQuantiles() const
{
    return quantiles;
}

int SequenceStatistics::countValue(int value)
{
    assert(computedFlag && "compute statistics before counting values");
    int mcount = 0;
    for (int i = 0; i < count; ++i) {
        if (value == values[i]) {
            mcount++;
        } else {
            if (value > 0) {
                break;
            }
        }
    }
    return mcount;
}
