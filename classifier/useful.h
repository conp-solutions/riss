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

// return n'th field or empty string
std::string Get( const std::string & s, unsigned int n );


std::string split(const std::string &s, unsigned int n,char delim);

#endif
