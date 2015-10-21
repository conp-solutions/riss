/********************************************************************++++++++++++++++++[Shuffler.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_SHUFFLER_HH
#define RISS_SHUFFLER_HH

#include <stdint.h>
#include <vector>

#include "riss/core/Solver.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/CP3Config.h"
// using namespace Riss;

namespace Coprocessor
{

/** this class re-implements the rand-method so that it can be used more independently */
class Randomizer
{
    uint64_t hold;
  public:
    Randomizer() { hold = 0; };

    /** sets the current random value
    */
    void set(uint64_t newValue) { hold = newValue; }

    /** return the next random value
    */
    uint64_t rand() { return (((hold = hold * 214013L + 2531011L) >> 16) & 0x7fff); }

    /** return the next random value modulo some other value
    */
    uint64_t rand(uint64_t upperBound) { uint64_t ret = rand(); return upperBound > 0 ? ret % upperBound : 0; }
};


class VarShuffler
{
    CP3Config& config;
    uint32_t variables;
    std::vector< Riss::Lit > replacedBy;
    uint32_t seed;

    bool shuffledAlready;

    Randomizer randomizer;

  public:
    VarShuffler(CP3Config& _config);

    /** apply full shuffling process to clauses */
    void process(Riss::vec< Riss::CRef >& clauses, Riss::vec< Riss::CRef >& learnts, Riss::vec< Riss::Lit >& trail, uint32_t vars, Riss::ClauseAllocator& ca);

    /** remap model to original variables */
    void unshuffle(Riss::vec<Riss::lbool>& model, uint32_t vars);

  protected:
    /** set seed fo shuffling (it is a pivate seed, independent from rand() */
    void setSeed(uint32_t s);

    /** create a shuffle - mapping */
    void setupShuffling(uint32_t vars);

    /** apply the mapping to the formula */
    void shuffle(Riss::vec<Riss::CRef>& clauses, Riss::ClauseAllocator& ca, bool shuffleOrder = false);

    /** apply mapping */
    void shuffle(Riss::vec<Riss::Lit>& lits, bool shuffleOrder = false);



};

} // namespace Coprocessor

#endif
