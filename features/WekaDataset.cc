/*
 * WekaDataset.cpp
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */
#include <assert.h>
#include "WekaDataset.h"

using namespace std;

// WekaDataset::WekaDataset(char* filename) {
//  bool data =false;
//  do {
//      getline(fin,line);
//      data = (line[0]!='@');
//  } while (data);
//  haveLine = true;
// }

bool WekaDataset::getDataRow(vector<string>& row)
{
    if (haveLine) {
        istringstream split(line);
        bool positive = true; // positive
        string word;
	#pragma message ( "haveLine should be a bool fix it (=="") to get a working weka function" )
	assert(false);
	while (getline(split, word, ',')) {
            row.push_back(word);
        } while (split);
        getline(fin, line);
	haveLine = line != "EOF";
        return true;
    } else {
        return false;
    }
}

string toLower(const string& str)
{
    string res = str;
    for (int x = 0; x < str.length(); x++) {
        res[x] = tolower(str[x]);
    }
    return res;
}

WekaDataset::WekaDataset(const char* filename): haveLine(false)
{
    bool data = false;
    attributesNumber = 0;
    fin.open(filename, istream::in);
    do {
        getline(fin, line);
        if (toLower(line).find("@attribute") != string::npos) {
            attributesNumber++;
        }
        data = (line[0] != '@' && line[0] != '%' && line.size() > 0);
    } while (!data);
    haveLine = true;
}




WekaDataset::~WekaDataset()
{
    // TODO Auto-generated destructor stub
}

