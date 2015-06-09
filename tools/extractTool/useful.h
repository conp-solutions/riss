/**
 *  Useful functions for parsing
 */

#ifndef RISS_USEFUL_H
#define RISS_USEFUL_H

#include <vector>
#include <string>
#include <cstring>

#include <cstdlib>

using namespace std;

bool is_number(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

// return n'th field or empty string
string Get( const std::string & s, unsigned int n ) {
    istringstream is( s );
    string field;
    do {
        if ( ! ( is >> field ) ) {
            return "";
        }
    } while( n-- != 0 );
    return field;
}


// string split(const std::string &s, unsigned int n) {
// 		char delim = ' ';
//     std::stringstream ss(s);
//     std::string item;
//     while(std::getline(ss, item, delim) || n-- > 0) {
// 
//     }
//     return item;
// }

string split(const std::string &s, unsigned int n,char delim = ' ') {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim) || n-- > 0) {

    }
    return item;
}

#endif
