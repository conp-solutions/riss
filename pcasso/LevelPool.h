/**
   Davide> 
   A LevelPool is a pool of clauses. It is identified by its position in the
   PartitionTree. 
 **/

#ifndef LEVEL_POOL_H
#define LEVEL_POOL_H

#include <string>
#include <vector>
#include <map>
#include "riss/mtl/Vec.h"
#include "riss/mtl/Sort.h"
#include "riss/core/SolverTypes.h"
#include "pcasso/PcassoClause.h"
#include "riss/utils/LockCollection.h"
#include "pcasso/RWLock.h"
#include "riss/utils/Debug.h"
// #include "riss/utils/Statistics-mt.h"

#include <iostream>
#include <fstream>

using namespace Riss;
using namespace std;

namespace PcassoDavide{

class LevelPool{
//	bool remove;
	string code;
	bool full;
	int endP;

public:

	LevelPool(int _max_size);

	RWLock levelPoolLock;

	int writeP;     // index for whatever clause
	int max_size;

	void dumpClauses( const char* filename){

		//    	fstream s;
		//    	s.open(filename, fstream::out);
		//    	s << "c [POOL] all collected shared clauses begin: " << endl;
		//    	for( unsigned int i = 0 ; i < shared_clauses.size(); ++i )
		//    	{
		//    		clause& c = shared_clauses[i];
		//    		for (unsigned int j = 0; j < c.size; j++)
		//    			s << (sign(c.lits[j]) ? "-" : "") << var(c.lits[j])+1 << " ";
		//    		s << "0\n";
		//    	}
		//    	s << "c [POOL] all collected shared clauses end: " << endl;
		//    	s.close();
	}

	bool duplicate( const vec<Lit>& c );
	bool add_shared(vec<Lit>& lits, unsigned int nodeID, bool disable_dupl_removal=false, bool disable_dupl_check=false);


	/** Reads positions until the writeP position **/
	void getChunk(int readP, vec<Lit>& chunk);

public:
	inline string getCode(void) const{
		return code;
	}

	inline void setCode(const string& _code){
		code = _code;
	}
	inline bool isFull() {
		return full;
	}
	vec<Lit> shared_clauses;
};
}

#endif
