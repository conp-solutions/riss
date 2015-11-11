/*******************************************************************************************[xor.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_XOR_HH
#define RISS_XOR_HH

#include "riss/core/Solver.h"
#include "coprocessor/Technique.h"
#include "coprocessor/CoprocessorTypes.h"

#include "Propagation.h"
#include "EquivalenceElimination.h"

#include <vector>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** this class is used forthe gauss elimination algorithm
 */
class XorReasoning : public Technique<XorReasoning>
{

    CoprocessorData& data;
    Propagation& propagation;     // object that takes care of unit propagation
    EquivalenceElimination& ee;   // object that takes care of equivalent literal elimination

    double processTime, parseTime, reasonTime;
    int findChecks;
    int xors;         // number of found xors
    int xorClauses;       // count how many clauses have been used to encode the xors
    int resolvedInCls, subsFound, resFound;
    int resTauts, resStrength;

    int xorLimit, xorSteps;
    int foundEmptyLists, xorUnits, allUsed, xorDeducedUnits, eqs;
    int addedTernaryXors, addedQuadraryXors;
    int participatingXorClauses, participatingXorVariables;  // count number of participating clauses/variables
    float clauseRatio, variableRatio;       // count ratio of participating clauses/variables
    int xorProps, clsProps, simDecisions;   // count how many propagations are based on XORs or clauses during simulation

    Riss::vec<Riss::Var> xorBackdoor;
    Riss::MarkArray backdoorVariables;

    Riss::vec<Riss::Var> xorOrder; // store the order how of variables in the elimination

    /** compare two literals */
    struct VarLt {
        std::vector< std::vector <int> >& data;
        bool operator()(int& x, int& y) const
        {
            return data[ x ].size() < data[ y].size();
        }
        VarLt(std::vector<std::vector< int> >& _data) : data(_data) {}
    };

    /** structure to use during simple gauss algorithm */
    class GaussXor
    {
      public:
        std::vector<Riss::Var> vars;
        bool k; // polarity of XOR has to be true or false
        bool used; // indicate whether this constraint has been used for simplification already
        bool eq() const { return vars.size() == 2 ; }
        bool unit() const { return vars.size() == 1; }
        Riss::Lit getUnitLit() const { return unit() ? Riss::mkLit(vars[0], !k) : Riss::lit_Undef; }
        bool failed() const { return vars.size() == 0 && k; }
        GaussXor(const Riss::Clause& c) : k(true), used(false)    // build xor from clause
        {
            vars.resize(c.size());
            for (int i = 0 ; i < c.size(); ++ i) {
                vars[i] = var(c[i]);
                k = sign(c[i]) ? !k : k ; // change polarity of k, if literal is negative!
            }
        }
        GaussXor() : k(false), used(false) {} // create an empty xor

        int size() const { return vars.size(); }

        /** add the other xor to this xor
         * @param removed list of variables that have been removed from the xor
         * @param v1 temporary std::vector
         */
        void add(const GaussXor& gx, std::vector<Riss::Var>& removed, std::vector<Riss::Var>& v1, std::vector<Riss::Var>& newlyAdded)
        {
            k = (k != gx.k); // set new k!
            v1 = vars; // be careful here, its a copy operation!
            vars.clear();
            const std::vector<Riss::Var>& v2 = gx.vars;
            // generate new vars!
            int n1 = 0, n2 = 0;
            while (n1 < v1.size() && n2 < v2.size()) {
                if (v1[n1] == v2[n2]) {
                    removed.push_back(v2[n2]); // variables that appear in both XORs will be removed!
                    n1++; n2++;
                } else if (v1[n1] < v2[n2]) {
                    vars.push_back(v1[n1++]);
                } else {
                    newlyAdded.push_back(v2[n2]);
                    vars.push_back(v2[n2++]);
                }
            }
            for (; n1 < v1.size(); ++n1) { vars.push_back(v1[n1]); }
            for (; n2 < v2.size(); ++n2) {
                vars.push_back(v2[n2]);
                newlyAdded.push_back(v2[n2]);
            }
        }

    };

    Riss::vec<Riss::CRef> reAddedClauses; // clauses that have been added by reencoding intermediate XORs

  public:

    XorReasoning(CP3Config& _config, Riss::ClauseAllocator& _ca, Riss::ThreadController& _controller, CoprocessorData& _data,  Propagation& _propagation, EquivalenceElimination& _ee);

    void reset();

    /** apply the gauss elimination algorithm
    * @return true, if something has been altered
    */
    bool process();

    void printStatistics(std::ostream& stream);

    void giveMoreSteps();

    void destroy();

    // remove index from all variable lists that are listed in x
    void dropXor(int index, std::vector< Riss::Var >& x, std::vector< std::vector< int > >& occs);

    /** propagate found units in all related xors
     * @return true if no conflict was found, false if a conflict was found
     */
    bool propagate(std::vector< Riss::Lit >& unitQueue, Riss::MarkArray& ma, std::vector< std::vector< int > >& occs, std::vector< GaussXor >& xorList);

  protected:

    /** find xors in the formula, with given max size */
    bool findXor(std::vector< Coprocessor::XorReasoning::GaussXor >& xorList);

    /** encodes the given xor, and stores the newly added clauses to the clause list, if the according optoin is specified */
    void addXorAsClauses(GaussXor& simpX);

    /** add the given literals as a clause to Coprocessor */
    Riss::CRef addClause(const Riss::Lit* lits, int size, bool learnt = false);

    /** checks whether the newly added clauses subsume other clauses from the formula */
    void checkReaddedSubsumption();

    /** perform unit propagation without changing xors, can be used to implement a search that is aware of the XOR constraints
     * @param ignoreConflicts continue propagation even if a conflict was encountered
     * @return true if no conflict was found, false if a conflict was found
     */
    bool simulatePropagate(std::vector< Riss::Lit >& unitQueue, Riss::MarkArray& ma, std::vector< std::vector< int > >& occs, std::vector< GaussXor >& xorList, bool ignoreConflicts = true);
};

}

#endif
struct Gauss;
