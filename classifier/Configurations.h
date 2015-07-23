/*
 * Configurations.h
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#ifndef RISS_CONFIGURATIONS_H_
#define RISS_CONFIGURATIONS_H_
#include <vector>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include "classifier/WekaDataset.h"

// using namespace std;

class Configurations
{
  private:
    double timeout;
    const char* attrInfoFile;
    const char* definitionsFilename;
    std::vector<std::string> names;
    std::vector<std::string> classifierInfo;
    int startIndex;
    void readNames(const char* definitionsFilename);

    std::stringstream attrToRemove;
    int removeCount;
    std::vector<int> classIndexes;
    bool loaded;
  public:
    Configurations(const char* definitionsFilename, const char* attrInfoFile);

    void removeIndex(int index);
    void addClassIndex(int index);
    void removeIndexes(const std::vector<int>& index);
    void removeIndexes(int offset, const std::vector<int>& index);
    void printRunningInfo();

    void loadConfigurationInfo();

    virtual ~Configurations();
    int getSize() const
    {
        return names.size();
    }

    const std::vector<std::string>& getNames() const
    {
        return names;
    }

    double getTimeInfo(std::istream& input);
    double getTimeInfo(std::istream& input, double penalization);
    bool isSolved(const char* cnfFile);
    bool isGood(double time);
    std::string nullInfo();
    std::string configInfo(std::istream& input, double penalization);
    std::string attrInfo(int startIndex);
    int getAttrIndex(int config) const
    {
        return startIndex + config;
    }

    const char* getAttrInfoFile() const
    {
        return attrInfoFile;
    }

    void setAttrInfoFile(const char* attrInfoFile)
    {
        this->attrInfoFile = attrInfoFile;
    }

    const std::vector<int>& getClassIndexes() const
    {
        return classIndexes;
    }

    void setClassIndexes(const std::vector<int>& classIndexes)
    {
        this->classIndexes = classIndexes;
    }


    const std::vector<std::string>& getClassifierInfo() const
    {
        return classifierInfo;
    }

    void setClassifierInfo(const std::vector<std::string>& classifierInfo)
    {
        this->classifierInfo = classifierInfo;
    }

    const char* getDefinitionsFilename() const
    {
        return definitionsFilename;
    }

    void setDefinitionsFilename(const char* definitionsFilename)
    {
        this->definitionsFilename = definitionsFilename;
    }
};


#endif /* CONFIGURATIONS_H_ */
