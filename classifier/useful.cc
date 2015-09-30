
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

#include <cstdlib>
#include "useful.h"

using namespace std;

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


string split(const std::string &s, unsigned int n,char delim = ' ') {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim) || n-- > 0) {

    }
    return item;
}