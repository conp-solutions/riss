#ifndef TOSTRING_H
#define TOSTRING_H

#include <sstream>
#include <string>

// Davide> This file simply contains some useful things

namespace davide{
  
  template <class T>
    inline std::string to_string (const T& t)
    {
      std::stringstream ss;
      ss << t;
      return ss.str();
    }
}

#endif