/*************************************************************************************[Resolving.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_RESOLVING_H
#define RISS_RESOLVING_H

#include "coprocessor/CoprocessorTypes.h"

#include "coprocessor/Technique.h"
#include "Propagation.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

class Resolving : public Technique<Resolving>
{
    CoprocessorData& data;
    Propagation& propagation; // object that takes care of unit propagation

    std::vector<int> seen;    // remembers how many clauses per variable have been processed already

  public:
    Resolving(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation);

    bool process(bool post = false);

    /** inherited from @see Technique */
    void printStatistics(std::ostream& stream);

    void destroy();

    void giveMoreSteps();

  protected:

    /** resolve ternary clauses */
    void ternaryResolve();

    /** add redundant binary clauses */
    void addRedundantBinaries();

    /** check whether this clause already exists in the occurence list */
    bool hasDuplicate(std::vector< Riss::CRef >& list, const Riss::vec< Riss::Lit >& c);

    /**
    * expects c to contain v positive and d to contain v negative
    * @return true, if resolvent is satisfied
    *         else, otherwise
    */
    bool resolve(const Riss::Clause& c, const Riss::Clause& d, const int v, Riss::vec<Riss::Lit>& resolvent);

    // check whether a std::vector of lits subsumes a given clause
    bool ordered_subsumes(const Riss::vec<Riss::Lit>& c, const Riss::Clause& other) const;
    bool ordered_subsumes(const Riss::Clause& c, const Riss::vec<Riss::Lit>& other) const;

    bool checkPush(Riss::vec<Riss::Lit>& ps, const Riss::Lit& l);

    double processTime;
    unsigned addedTern2;
    unsigned addedTern3;
    unsigned addedBinaries;
    unsigned res3steps;
    unsigned add2steps;
    unsigned removedViaSubsubption;
    unsigned detectedDuplicates;

    /** compare two literals */
    struct VarOrderHeapLt {
        CoprocessorData& data;
        bool operator()(const Riss::Var& x, const Riss::Var& y) const
        {
            return data[ x] < data[y];
        }
        VarOrderHeapLt(CoprocessorData& _data) : data(_data) {}
    };
    Riss::Heap<VarOrderHeapLt> resHeap; // heap that stores the variables according to their frequency (dedicated for BVA)

};


}

#endif // RESOLVING_H
