/********************************************************************************[VarFileParser.h]
Copyright (c) 2012 Norbert Manthey
*************************************************************************************************/

#ifndef RISS_VARFILEPARSER_H
#define RISS_VARFILEPARSER_H

#include <iostream>

#include <fstream>
#include <sstream>
#include <string>
// for int types
#include <inttypes.h>
#include <cstring>
#include <string>

#include <vector>
#include <assert.h>

// using namespace std;

namespace Riss
{

/** parse files, that contain variables or variable ranges
 *
 *  Format of file: per line a variable, of a variable range per "a..b"
 */
class VarFileParser
{
    std::fstream file;
  public:
    /** open the specified file
    */
    VarFileParser(const std::string& filename);

    /** extract the variables from the file and store them in the std::vector vars
     * @return maximum variable in file
    */
    int extract(std::vector<int>& vars);
};

/*****
    developer section
****/

;

inline int VarFileParser::extract(std::vector<int>& vars)
{
    file.seekg(0);
    std::string line;
    int max = 0;
    while (getline(file, line)) {
        if (line.size() == 0) { continue; }
        // ignore comment lines
        if (line.at(0) == 'c') { continue; }
        if (line.find('.') != std::string::npos) {
            // range of numbers
            uint32_t dotPos = line.find('.');
            std::string first = line.substr(0, line.find('.'));
            int firstVar = atoi(first.c_str());

            line = line.substr(dotPos + 2);
            int secondVar = atoi(line.c_str());
            for (int v = firstVar; v <= secondVar; v++) { vars.push_back(v); }
            max = max >= secondVar ? max : secondVar;
        } else {
            int nr = atoi(line.c_str());   // can handle negative values
            // single number
            if (nr == 0) { std::cerr << "c WARNING: found 0 in variable file" << std::endl; }
            else { vars.push_back(nr); }
            max = max >= nr ? max : nr;
        }
    }
    return max; // cannot handle negative values!
}


inline VarFileParser::VarFileParser(const std::string& filename)
{
    file.open(filename.c_str(), std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "c variable parser was not able to open file " << filename << std::endl;
    }
}

}

#endif
