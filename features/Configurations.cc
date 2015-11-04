/*
 * Configurations.cpp
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#include "classifier/Configurations.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

//Configurations::Configurations(): startIndex(0), timeout(800000), attrInfoFile("attrInfo.txt"){
//  readNames("configurations.txt");
//}

Configurations::Configurations(const char* definitionsFilename, const char* attrInfoFile):
    startIndex(0),
    removeCount(0), timeout(800000),
    loaded(false)
{
    readNames(definitionsFilename);
    this->attrInfoFile = attrInfoFile;
}

Configurations::~Configurations()
{
    // TODO Auto-generated destructor stub
}

double Configurations::getTimeInfo(istream& input)
{
    return getTimeInfo(input, 0);
}

bool Configurations::isSolved(const char* cnfFile)
{
//  for (int i = 0; i < getSize(); ++i) {
//      if (getTimeInfo(cnfFile,i)>0)
//          return true;
//  }
    return false;
}

double Configurations::getTimeInfo(istream& input, double penalization)
{
    string word;
    input >> word;
    if (word.compare("-") == 0) {
        return -1;
    } else {
        double time;
        std::istringstream i(word); // have another stream, to not parse twice from the stream input
        if (!(i >> time)) {   // parse time into double
            return -1 ;    // if fails, then return -1
        }
        return time + penalization; // otherwise return the time
    }
}

bool Configurations::isGood(double time)
{
    return time < timeout && time >= 0;
}

string Configurations::nullInfo()
{
    string res = "";
    for (int i = 0; i < getSize(); ++i) {
        res = res + ",?,?";
    }
    return res;
}

string Configurations::attrInfo(int startIndex)
{
    this->startIndex = startIndex;
    string res = "";
    for (int i = 0; i < getSize(); ++i) {
        res = res + "@attribute " + names[i] + "_runtime real" + "\n";
        removeIndex(startIndex + 2 * i);
        res = res + "@attribute " + names[i] + "_class {good,bad}" + "\n";
        addClassIndex(startIndex + 2 * i + 1);
    }
    return res;
}

void Configurations::readNames(const char* definitionsFilename)
{
    this->definitionsFilename = definitionsFilename;
    ifstream fin;
    fin.open(definitionsFilename, ofstream::in);
    if (!fin) { cerr << "failed to open configurations file " << definitionsFilename << endl; }
    string word;
    while (fin >> word) {
        names.push_back(word);
    }
    fin.close();
}

string Configurations::configInfo(istream& input, double penalization)
{
    string classInfo = "";
    stringstream timeInfo;
    timeInfo.setf(ios::fixed);
    timeInfo.precision(4);
    for (int i = 0; i < getSize(); ++i) {
        double time = getTimeInfo(input, penalization);
        timeInfo << "," << time;
        classInfo = classInfo + "," + (isGood(time) ? "good" : "bad");

    }
    return classInfo + timeInfo.str();
}

void Configurations::removeIndex(int index)
{
    removeCount++;
    if (removeCount == 1) {
        attrToRemove << index;
    } else {
        attrToRemove << "," << index;
    }
}

void Configurations::removeIndexes(const vector<int>& indexes)
{
    removeIndexes(0, indexes);
}
void Configurations::removeIndexes(int offset, const vector<int>& indexes)
{
    for (int i = 0; i < indexes.size(); i++) {
        removeIndex(offset + indexes[i]);
    }
}

void Configurations::printRunningInfo()
{
    ofstream output;
    output.open(attrInfoFile, fstream::out);
    for (int i = 0; i < classIndexes.size(); ++i) {
        output << "-c " << classIndexes[i] << " ";
        stringstream pre;
        for (int k = 0; k < classIndexes.size(); ++k) {
            if (i != k) {
                pre << classIndexes[k] << ",";
            }
        }
        output << "-F \"weka.filters.unsupervised.attribute.Remove -R "
               << pre.str().c_str() << attrToRemove.str().c_str() << "\"" << endl;
    }
    output.close();
}

void Configurations::addClassIndex(int index)
{
    classIndexes.push_back(index);
}


int getIndex(string line)
{
    stringstream split(line);
    string w;
    split >> w;
    int index;
    split >> index;
    return index;
}

void Configurations::loadConfigurationInfo()
{
    if (!loaded) {
        classifierInfo.clear();
        classIndexes.clear();
        ifstream input;
        input.open(attrInfoFile, istream::in);
        for (int config = 0; config < getSize(); config++) {
            string line1;
            getline(input, line1);
            int index = getIndex(line1);
            classIndexes.push_back(index);
            classifierInfo.push_back(line1);
        }
        input.close();
        loaded = true;
    }
}
