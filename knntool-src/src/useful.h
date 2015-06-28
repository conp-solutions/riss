/**
 *  Useful functions for parsing
 */

#ifndef USEFUL_H
#define USEFUL_H

#include <vector>
#include <string>
#include <cstring>
#include <sstream>

#include <cstdlib>

using namespace std;

// return n'th field or empty string
string Get( const std::string & s, unsigned int n );


string split(const std::string &s, unsigned int n,char delim);

#endif