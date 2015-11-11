/*******************************************************************************************[hbr.h]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_HBR_HH
#define RISS_HBR_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/techniques/Propagation.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used to compute hyper binary resolution
 * basic idea of the implemented algorithm is taken from the solver description of
 * minisat_bcd (SAT Race 2015) by Jingchao Chen
 */
class HyperBinaryResolution : public Technique<HyperBinaryResolution>
{

    CoprocessorData& data;
    Coprocessor::Propagation& propagation;

    // attributes
    int hbrSteps;
    int foundMultiHBR, foundHBR;
    Clock hbrTime; // clocks for the two methods

    /** pair of literals, which always is stored sorted */
    class LitPair
    {
        Lit x, y;
      public:
        LitPair(const Lit& a, const Lit& b) : x(a <= b ? a : b), y(a > b ? a : b) {}
        LitPair() : x(lit_Undef), y(lit_Undef) {}
        Lit getX() const { return x; }
        Lit getY() const { return y; }

        bool operator < (LitPair p)  const { return x < p.x || (x == p.x && y < p.y);  }
        bool operator != (LitPair p) const { return x != p.x || y != p.y; }
    };

    // to store the literals that should be used in the next iteration of the algorithm
    MarkArray nextRound;
    BIG big;
    vector< LitPair > toBeAdded;

  public:
    HyperBinaryResolution(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Coprocessor::Propagation& _propagation);

    void reset();

    /** applies blocked clause elimination algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

  protected:

    /** run hyper binary resolution */
    bool hyperBinaryResolution();

    /** add a new binary clause to the formula, if it does not exist already
     @return true, if the new clause did not exist already
     */
    bool addHBR(const Lit& otherLit, const Lit& clauseLit);

};

}

#endif
