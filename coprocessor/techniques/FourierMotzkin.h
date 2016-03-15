/********************************************************************************[FourierMotzkin.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_FOURIERMOTZKIN_HH
#define RISS_FOURIERMOTZKIN_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

#include "Propagation.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for the fourier motzkin procedure on extracted cardinality constraints
 */
class FourierMotzkin : public Technique<FourierMotzkin>
{

    CoprocessorData& data;
    Propagation& propagation;
    Riss::Solver& solver;

    double processTime, amoTime, amtTime, fmTime, twoPrTime, deduceAloTime, semTime;
    int64_t steps, searchSteps;
    int fmLimit;
    int foundAmos, foundAmts, newAmos, newAlos, newAlks;
    int sameUnits, deducedUnits, propUnits;
    int addDuplicates;
    int irregular, pureAmoLits;
    int usedClauses;
    int cardDiff, discardedCards, discardedNewAmos;
    int removedCards, newCards;
    int addedBinaryClauses, addedClauses;
    int detectedDuplicates;
    int garbageCollects;

    int twoPrAmos, twoPrAmoLits; // stats for two pr amo lits
    int dedAlos;

    int semExtendedCards, semFailedExtendTries, semExtendLits, semReducedDegrees,
        semTotalProbes, semTotalFailedProbes, semNrDisabledClauses, semNrPreDisabledClauses, semUnits;
    uint64_t semSteps;


    Riss::vec<Riss::Lit> unitQueue; // store literals that should be propagated on the card constraints

    /** compare two literals */
    struct LitOrderHeapLt {
        CoprocessorData& data;
        bool operator()(int& x, int& y) const
        {
            return data[ Riss::toLit(x)] < data[Riss::toLit(y)];
        }
        LitOrderHeapLt(CoprocessorData& _data) : data(_data) {}
    };

    /** struct to handle ternary clauses efficiently */
    struct Ternary {
        Riss::Lit lit [3];
        Ternary(const Riss::Lit& a, const Riss::Lit& b, const Riss::Lit& c)
        {
            lit[0] = (a > b ? (b > c ? c : b) : (a > c ? c : a));       // min
            lit[2] = (a > b ? (a > c ? a : c) : (b > c ? b : c));       // max
            lit[1] = Riss::toLit(toInt(a) ^ toInt(b) ^ toInt(c) ^ toInt(lit[0]) ^ toInt(lit[2]));       // xor all three lits and min and max (the middle remains)
        }
        Riss::Lit operator[](const int position) const
        {
            return lit[position];
        }
    };

    /** represent a (mixed) cardinality constraint*/
  public:
    class CardC
    {
      private:

        // implement ID system for FM proofs
        int getNextID()
        {
            static int currentID = 0;
            return currentID ++;
        }

      public:
        std::vector<Riss::Lit> ll;
        std::vector<Riss::Lit> lr;
        int k;
      private:
        int id;
        int parentL, parentR; // ID of parent constraints
      public:

        CardC() : k(0), id(getNextID()), parentL(-1), parentR(-1) {}   // default constructor
        CardC(std::vector<Riss::Lit>& amo) : ll(amo), k(1), id(getNextID()), parentL(-1), parentR(-1) {};     // constructor to add amo constraint
        CardC(std::vector<Riss::Lit>& amk, int _k) : ll(amk), k(_k), id(getNextID()), parentL(-1), parentR(-1) {};     // constructor to add amk constraint
        CardC(const Riss::Clause& c) : k(-1), id(getNextID()), parentL(-1), parentR(-1) { lr.resize(c.size(), Riss::lit_Undef); for (int i = 0 ; i < c.size(); ++i) { lr[i] = c[i]; }  }     // constructor for usual clauses

        bool amo() const { return k == 1 && lr.size() == 0 ; }
        bool amt() const { return k == 2 && lr.size() == 0 ; }
        bool amk() const { return k >= 0 && lr.size() == 0 ; }
        bool alo() const { return k == -1 && ll.size() == 0; }
        bool alk() const { return k < 0 && ll.size() == 0; }
        bool isUnit() const { return (k + (int)lr.size()) == 0; } // all literals in ll have to be false, and all literals in lr have to be true
        bool failed() const { return (((int)lr.size() + k) < 0) ; }
        bool taut() const { return k >= (int)ll.size(); } // assume no literal appears both in ll and lr
        bool invalid() const { return k == 0 && ll.size() == 0 && lr.size() == 0; } // nothing stored in the constraint any more
        void invalidate() { if (!invalid()) { k = 0; std::vector<Riss::Lit>().swap(ll); std::vector<Riss::Lit>().swap(lr);} }

        void swap(CardC& other)     /** swap with other constraint */
        {
            const int t = other.k; other.k = k; k = t;
            int p = parentL; parentL = other.parentL; other.parentL = p;
            p = parentR; parentR = other.parentR; other.parentR = p;
            p = id; id = other.id; other.id = p;
            ll.swap(other.ll);
            lr.swap(other.lr);
        }

        void setParents(int idL, int idR) { assert(idL != idR && "cannot have the same parent twice"); assert(parentL == -1 && parentR == -1); parentL = idL; parentR = idR; }
        int getID() const { return id; }
        int getParentR() const { return parentR; }
        int getParentL() const { return parentL; }

    };

  public:
    FourierMotzkin(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation, Riss::Solver& _solver);

    void reset();

    /** extractes cardinality constraints and applies the fourier motzkin algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();


  protected:
    /** propagate the literals in unitQueue over all constraints*/
    bool propagateCards(Riss::vec<Riss::Lit>& unitQueue, std::vector< std::vector<int> >& leftHands, std::vector< std::vector<int> >& rightHands, std::vector<CardC>& cards, Riss::MarkArray& inAmo);

    /** check whether the given clause is already present in the given list */
    bool hasDuplicate(const std::vector<Riss::Lit>& c);

    /** given a set of cardinality constraints, and a BIG, try to deduce more AMOs following the two product encoding */
    void findTwoProduct(std::vector< CardC >& cards, BIG& big, std::vector< std::vector<int> >& leftHands);

    /** return whether a current set of literals already exists as AMO, or is subsumed by an existing one
     * Note: assumes the literal lits to be sorted, and all AMOs inside cards as well
     */
    bool amoExistsAlready(const std::vector< Riss::Lit >& lits, std::vector< std::vector< int > >& leftHands, std::vector<CardC>& cards);

    /** try to deduce ALO constraints
     *  if something like a board is encoded, then try to add additional ALO constraints (from dangerous reductions paper)
     */
    void deduceALOfromAmoAloMatrix(std::vector< CardC >& cards, std::vector< std::vector< int > >& leftHands);

    /** remove all the AMOs, whose effect is already covered by some other AMO */
    void removeSubsumedAMOs(std::vector< CardC >& cards, std::vector< std::vector< int > >& leftHands);

    /** given a formula, try to extract cardinality constraints semantically */
    void findCardsSemantic(std::vector<CardC>& cards,  std::vector< std::vector<int> >& leftHands);

    /** given a number x with n bits set, then the procedure returns the next number */
    LONG_INT nextNbitNumber(LONG_INT x) const;

    /** add all clauses to solver object -- code taken from @see Preprocessor::reSetupSolver, but without deleting clauses */
    void reSetupSolver();

    /** remove all clauses from the watch lists inside the solver */
    void cleanSolver();
};

//   inline std::ostream& operator<<( std::ostream& stream, const FourierMotzkin::CardC& card ) {
//       stream << "(" << card.ll << " <= " << card.k << " + " << card.lr << ")";
//       return stream;
//   }

}

#endif
