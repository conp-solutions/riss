/*
 * Debug.h
 *
 *  Created on: Jan 18, 2013
 *      Author: tirrolo
 */

#ifndef PCASSODEBUG_H_
#define PCASSODEBUG_H_

#include <iostream>
#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"

// using namespace std;
// using namespace Riss;

/// support debug output only if it is compiled in
#ifndef NDEBUG
  #define DOUT(x) ( {x} )
#else
  #define DOUT(x)
#endif

namespace Debug {

static const int pcasso_debug_verbosity = 0;

inline void
PRINTLN(Riss::vec<Riss::Lit>& v){
	if(pcasso_debug_verbosity > 0){
			for( int i = 0; i < v.size(); i++ )
				std::cerr << (sign(v[i]) ? "-" : "") << var(v[i])+1 << " ";
			std::cerr << std::endl;
	}
}
inline void
PRINTLN(Riss::vec<Riss::Lit>& v, unsigned int limit){
	if(pcasso_debug_verbosity > 0){
		for( int i = 0; i < limit; i++ )
			std::cerr << (sign(v[i]) ? "-" : "") << var(v[i])+1 << " ";
		std::cerr << std::endl;
	}
}
inline void
PRINTLN(Riss::Lit& l){
	if(pcasso_debug_verbosity > 0){
		std::cerr << (sign(l) ? "-" : "") << var(l)+1 << " ";
		std::cerr << std::endl;
	}
}
inline void PRINTLN(std::string s){
	if(pcasso_debug_verbosity > 0) std::cerr << s << std::endl;
}
inline void PRINTLN(const char* s){
	if(pcasso_debug_verbosity > 0) std::cerr << s << std::endl;
}
inline void PRINTLN(int i){
	if(pcasso_debug_verbosity > 0) std::cerr << i << std::endl;
}
inline void PRINT(std::string s){
	if(pcasso_debug_verbosity > 0) std::cerr << s;
}
inline void PRINT(const char* s){
	if(pcasso_debug_verbosity > 0) std::cerr << s;
}
inline void PRINT(int i){
	if(pcasso_debug_verbosity > 0) std::cerr << i;
}
inline void STOP(void){
	assert(false);
}
inline void PRINTLN_DEBUG(Riss::vec<Riss::Lit>& v){
	if( pcasso_debug_verbosity > 2 )
		PRINTLN(v);
}
inline void
PRINTLN_DEBUG(Riss::Lit& l){
	if(pcasso_debug_verbosity > 2){
		std::cerr << (sign(l) ? "-" : "") << var(l)+1 << " ";
		std::cerr << std::endl;
	}
}
inline void PRINTLN_DEBUG(std::string s){
	if(pcasso_debug_verbosity > 2) std::cerr << s << std::endl;
}
inline void PRINTLN_DEBUG(const char* s){
	if(pcasso_debug_verbosity > 2) std::cout << s << std::endl;
}
inline void PRINT_DEBUG(std::string s){
	if(pcasso_debug_verbosity > 2) std::cerr << s;
}
inline void PRINT_DEBUG(const char* s){
	if(pcasso_debug_verbosity > 2) std::cout << s;
}
inline void PRINTLN_DEBUG(int i){
	if(pcasso_debug_verbosity > 2) std::cout << i << std::endl;
}
inline void PRINTLN_NOTE(Riss::vec<Riss::Lit>& v){
	if( pcasso_debug_verbosity > 1 )
		PRINTLN(v);
}
inline void
PRINTLN_NOTE(Riss::Lit& l){
	if(pcasso_debug_verbosity > 1){
		std::cerr << (sign(l) ? "-" : "") << var(l)+1 << " ";
		std::cerr << std::endl;
	}
}
inline void PRINTLN_NOTE(std::string s){
	if(pcasso_debug_verbosity > 1) std::cerr << s << std::endl;
}
inline void PRINTLN_NOTE(const char* s){
	if(pcasso_debug_verbosity > 1) std::cout << s << std::endl;
}
inline void PRINT_NOTE(std::string s){
	if(pcasso_debug_verbosity > 1) std::cerr << s;
}
inline void PRINT_NOTE(const char* s){
	if(pcasso_debug_verbosity > 1) std::cout << s;
}
inline void PRINTLN_NOTE(int i){
	if(pcasso_debug_verbosity > 1) std::cout << i << std::endl;
}
inline void PRINT_NOTE(int i){
	if(pcasso_debug_verbosity > 1) std::cout << i;
}

} /* namespace davide */
#endif /* DEBUG_H_ */
