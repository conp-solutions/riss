/*******************************************************************************************[BVA.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef BVA_HH
#define BVA_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"
#include "Propagation.h"

// using namespace Riss;

namespace Coprocessor
{

/** this class is used for bounded variable addition (replace patterns by introducion a fresh variable)
 */
class BoundedVariableAddition : public Technique<BoundedVariableAddition>
{

    CoprocessorData& data;
    Propagation propagation;

    // attributes for current run
    bool doSort;          // ensure that all clauses are sorted afterwards (assume, they are sorted before)

    // statistics
    double processTime;       // seconds of process time
    double andTime, iteTime, xorTime; // seconds per technique

    uint32_t andDuplicates;       // how many duplicate clauses have been found
    uint32_t andComplementCount;  // how many complementary literals have been found (strengthening)
    uint32_t andReplacements; // how many new variables could be introduced
    uint32_t andTotalReduction;   // how many clauses have been reduced
    uint32_t andReplacedOrs;      // how many disjunctions could be replaced by the fresh variable
    uint32_t andReplacedMultipleOrs;  // how many times could multiple or gates be replaced
    int64_t andMatchChecks;

    int xorfoundMatchings;
    int xorMultiMatchings;
    int xorMatchSize;
    int xorMaxPairs;
    int xorFullMatches;
    int xorTotalReduction;
    int64_t xorMatchChecks;

    int iteFoundMatchings;
    int iteMultiMatchings;
    int iteMatchSize;
    int iteMaxPairs ;
    int iteTotalReduction;
    int64_t iteMatchChecks;


    // work data
    /// compare two literals
    struct LitOrderBVAHeapLt {
        CoprocessorData& data;
        bool operator()(int& x, int& y) const
        {
            return data[ Riss::toLit(x)] > data[Riss::toLit(y)]; // more frequent literal should be least element!
        }
        LitOrderBVAHeapLt(CoprocessorData& _data) : data(_data) {}
    };

    // structures that would be created on during functions again and again
    std::vector< std::vector< Riss::CRef > > bvaMatchingClauses; // found std::pairs of clauses
    std::vector< Riss::Lit > bvaMatchingLiterals; // literals that stay in the match
    // use general mark array!
    std::vector< Riss::Lit > bvaCountMark;    // mark literal candidates (a) for the current literal(b)
    std::vector< uint32_t > bvaCountCount; // count occurence of a together with b
    std::vector< uint64_t > bvaCountSize; // count occurence of a together with b
    Riss::vec<Riss::Lit> clauseLits;          // std::vector that is added for clause definitions

  public:
    BoundedVariableAddition(Coprocessor::CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, Coprocessor::CoprocessorData& _data, Coprocessor::Propagation& _propagation);

    void reset();

    /** applies bounded variable addition algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

  protected:

    /** perform AND-bva */
    bool andBVA();

    /** perform ITE-bva */
    bool iteBVAhalf();
    bool iteBVAfull();

    /** perform XOR-bva */
    bool xorBVAhalf();
    bool xorBVAfull();

    /** prototype implementation of a BVA version that can replace multiple literals
    */
    bool variableAddtionMulti(bool sort = true);

    /** sub-routine of BVA to handle complementary literals
    * @param right literal that represents the right side
    * @return false, if shrinking a clause to unit led to a failed enqueue (UNSAT)
    */
    bool bvaHandleComplement(const Riss::Lit& right, Riss::Heap< Coprocessor::LitOrderHeapLt >& bvaHeap);

    /** introduce a fresh variable, update the size of all required structures*/
    Riss::Var nextVariable(char type, Riss::Heap<LitOrderHeapLt>& bvaHeap);

    /** check data structures */
    bool checkLists(const std::string& headline);

    /** std::pair of literals and clauses, including sort operator */
    struct xorHalfPair {
        Riss::Lit l1, l2;
        Riss::CRef c1, c2;
        xorHalfPair(Riss::Lit _l1, Riss::Lit _l2, Riss::CRef _c1, Riss::CRef _c2) : l1(_l1), l2(_l2), c1(_c1), c2(_c2) {}
        xorHalfPair() : l1(Riss::lit_Undef), l2(Riss::lit_Undef), c1(Riss::CRef_Undef), c2(Riss::CRef_Undef) {}

        /** generate an order, so that std::pairs that belong to the same XOR gate are placed behind each other */
        bool operator>(const xorHalfPair& other) const
        {
            return (toInt(l2) > toInt(other.l2));
        }
        bool operator<(const xorHalfPair& other) const
        {
            return (toInt(l2) < toInt(other.l2));
        }

    };

    struct iteHalfPair {
        Riss::Lit l1, l2, l3;
        Riss::CRef c1, c2;
        iteHalfPair(Riss::Lit _l1, Riss::Lit _l2, Riss::Lit _l3, Riss::CRef _c1, Riss::CRef _c2)
            : l1(_l1), l2(_l2), l3(_l3), c1(_c1), c2(_c2) {}

        iteHalfPair() : l1(Riss::lit_Undef), l2(Riss::lit_Undef), l3(Riss::lit_Undef), c1(Riss::CRef_Undef), c2(Riss::CRef_Undef) {}

        /** generate an order, so that std::pairs that belong to the same ITE gate are placed behind each other */
        bool operator>(const iteHalfPair& other) const
        {
            const Riss::Var iv2 = var(l2); const Riss::Var jv2 = var(other.l2);
            const Riss::Var iv3 = var(l3); const Riss::Var jv3 = var(other.l3);
            const bool signDiff = (sign(l2));
            return (iv2 > jv2
                    || (iv2 == jv2 &&  iv3 > jv3)
                    || (iv2 == jv2 &&  iv3 == jv3 && signDiff)
                   );
        }
        bool operator<(const iteHalfPair& other) const
        {
            return other > *this;
        }
    };

    /** remove duplicate clauses from the clause list of the given literal*/
    void removeDuplicateClauses(const Riss::Lit&   literal);

  public:
    // parameters
    bool bvaComplement;       /// treat found complements special?
    uint32_t bvaPush;     /// which literals to push to queue again (0=none,1=original,2=all)
    bool bvaRewEE;        /// run rewEE after BVA found new gates?
    int64_t bvaALimit;        /// number of checks until and-bva is aborted
    int64_t bvaXLimit;        /// number of checks until xor-bva is aborted
    int64_t bvaILimit;        /// number of checks until ite-bva is aborted
    bool bvaRemoveDubplicates;    /// remove duplicate clauses from occurrence lists
    bool bvaSubstituteOr; /// when c = (a AND b) is found, also replace (-a OR -b) by -c
};

}; // end namespace coprocessor

#endif
