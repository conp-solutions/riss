#ifndef RISS_TOSTRING_H
#define RISS_TOSTRING_H

#include <sstream>
#include <string>

// Davide> This file simply contains some useful things

namespace PcassoDavide
{

template <class T>
inline std::string to_string(const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}
}

#endif
