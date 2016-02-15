/*
 * SecuenceStatistics.h
 *
 *  Created on: Oct 22, 2013
 *      Author: gardero
 */
#ifndef RISS_SECUENCESTATISTICS_H_
#define RISS_SECUENCESTATISTICS_H_

#include <vector>
#include <iterator>
#include <cstdio>
#include <inttypes.h>

// using namespace std;

class SequenceStatistics
{
  private:
    double min, max, mode, median, stDeviation, entropy;
    int modeCount, valuesCount;
    double mean, valuesRate;
    std::vector<double> values;
    std::vector<double> quantiles;
    int quantilesCount;
    double sum, sumXsq;
    int count;
    int zerocount;
    bool computedFlag;
    bool computingDerivative;
    bool countingZeros;
    SequenceStatistics *derivative;
    void initAll();

  public:
//  SequenceStatistics();
    SequenceStatistics(bool computingDerivative);
    SequenceStatistics(bool computingDerivative, bool countingZeros);
    virtual ~SequenceStatistics();
    void addValue(double value);
    uint64_t compute(int quantiles);
    void tostdio() const;
    void valuesToVector(std::vector<double>& v) const;
    void namesToVector(std::string prefix, std::vector<std::string>& v) const;
    void infoToVector(std::string prefix, std::vector<std::string>& names,
                      std::vector<double>& values) const;
    void toStream(std::ostream& other) const;
    void valuesToStream(std::ostream& other) const;
    void toStream(const char* filename, const char* ext) const;

    int getCount() const
    {
        return count;
    }

    double getMax() const
    {
        return max;
    }

    double getMean() const
    {
        return mean;
    }

    double getMin() const
    {
        return min;
    }

    double getMode() const
    {
        return mode;
    }

    int getModeCount() const
    {
        return modeCount;
    }

    double getMedian() const
    {
        return median;
    }

    const std::vector<double>& getQuantiles() const;

    double getStDeviation() const
    {
        return stDeviation;
    }

    const std::vector<double>& getValues() const
    {
        return values;
    }

    double getEntropy() const
    {
        return entropy;
    }

    int countValue(int value);

    int getZerocount() const
    {
        return zerocount;
    }

    bool isComputingDerivative() const
    {
        return computingDerivative;
    }

    void setComputingDerivative(bool computingDerivative)
    {
        this->computingDerivative = computingDerivative;
    }

    bool isCountingZeros() const
    {
        return countingZeros;
    }

    void setCountingZeros(bool countingZeros)
    {
        this->countingZeros = countingZeros;
    }
};


#endif /* SECUENCESTATISTICS_H_ */
