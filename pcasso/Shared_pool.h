/*
 * File:   Shared_pool.h
 * Author: tirrolo
 *
 * Created on June 17, 2012, 12:08 PM
 */

#ifndef PCASSO_SHARED_POOL_H
#define PCASSO_SHARED_POOL_H

#include <vector>

#include "riss/core/SolverTypes.h"
#include "riss/utils/LockCollection.h"
#include "riss/mtl/Vec.h"
#include "riss/mtl/Sort.h"
#include "pcasso/PcassoClause.h"

// Debug
#include <stdio.h>

#include <iostream>
#include <fstream>

// using namespace Riss;
// using namespace std;

namespace PcassoDavide
{

static ComplexLock shared_pool_lock = ComplexLock(); // library can be build with this?

class Shared_pool
{
  public:

    static bool duplicate(const PcassoClause& c);
    static void add_shared(Riss::vec<Riss::Lit>& lits, unsigned int size);
    // static void delete_shared_clauses(void);

    const static unsigned int max_size = 10000;

    static std::vector<PcassoClause> shared_clauses;

    static void dumpClauses(const char* filename)
    {

        std::fstream s;
        s.open(filename, std::fstream::out);
        s << "c [POOL] all collected shared clauses begin: " << std::endl;
        for (unsigned int i = 0 ; i < shared_clauses.size(); ++i) {
            PcassoClause& c = shared_clauses[i];
            for (unsigned int j = 0; j < c.size; j++)
            { s << (sign(c.lits[j]) ? "-" : "") << var(c.lits[j]) + 1 << " "; }
            s << "0\n";
        }
        s << "c [POOL] all collected shared clauses end: " << std::endl;
        s.close();
    }

  private:
    Shared_pool();
    Shared_pool(const Shared_pool& orig);
    virtual ~Shared_pool();

};

}

#endif  /* SHARED_POOL_H */

