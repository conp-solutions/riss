/***************************************************************************************[Circuit.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CIRCUIT_HH
#define RISS_CIRCUIT_HH

#include <cstring>

#include "riss/core/Solver.h"

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/CP3Config.h"

#include <vector>

// using namespace Riss;
// using namespace std;

namespace Coprocessor
{

/** This class implement hidden tautology elimination
 */
class Circuit
{
    CP3Config& config;
    Riss::ClauseAllocator& ca;

    struct ternary {
        Riss::Lit l1, l2;
        ternary(Riss::Lit _l1, Riss::Lit _l2) : l1(_l1), l2(_l2) {}
    };
    struct quad {
        Riss::Lit l1, l2, l3;
        quad(Riss::Lit _l1, Riss::Lit _l2, Riss::Lit _l3) :
            l1(_l1)
            , l2(_l2)
            , l3(_l3)
        {
            if (l1.x > l2.x) { const Riss::Lit tmp = l1; l1 = l2; l2 = tmp; }   // first two are okay!
            if (l2.x > l3.x) { const Riss::Lit tmp = l2; l2 = l3; l3 = tmp; }   // also check last candidate!
            if (l1.x > l2.x) { const Riss::Lit tmp = l1; l1 = l2; l2 = tmp; }   // now, first two have to be compared again!
        }
    };

    BIG* big;                            // binary implication graph
    std::vector< std::vector<ternary> > ternaries; // ternary clauses per literal
    std::vector< std::vector<quad> > quads;        // quad-clauses per literal, cref for ca

  public:

    Circuit(CP3Config& _config, Riss::ClauseAllocator& _ca);
    ~Circuit();

    class Gate
    {
        union {
            Riss::Lit lits[4];
            struct {
                Riss::Lit x;           // output literal
                Riss::Lit* externLits; // external literals
                int size;        // number of input literals
            } e;
        } data;

        bool inQueue; // is this gate currently in some queue (e.g. for structural hashing)?
        int touched;  // how often has this gate been touched?

      public:
        enum Type { // kind of the gate
            AND,      // AND gate
            ExO,      // exactly-one gate
            GenAND,   // generic AND
            ITE,      // if-then-else gate
            XOR,      // XOR gate
            FA_SUM,   // half adder sum bit
            INVALID,  // not a gate
        };

        enum Encoded { // which clauses have been found in the formula
            FULL,
            POS_BLOCKED, // clauses with the positive output literal occurrence are blocked
            NEG_BLOCKED  // clauses with the negative output literal occurences are blocked TODO: can these gates have even more inputs than encoded?
        };

        Gate(const Riss::Clause& c, const Type _type, const Encoded e, const Riss::Lit& output = Riss::lit_Undef);       // generic gate
        Gate(const std::vector<Riss::Lit>& c, const Type _type, const Encoded e, const Riss::Lit& output = Riss::lit_Undef);  // generic gate
        Gate(const Riss::Lit& x, const Riss::Lit& s, const Riss::Lit& t, const Riss::Lit& f, const Coprocessor::Circuit::Gate::Type& _type, const Coprocessor::Circuit::Gate::Encoded e);      // ITE, FA_SUM
        Gate(const Riss::Lit& x, const Riss::Lit& a, const Riss::Lit& b, const Coprocessor::Circuit::Gate::Type& _type, const Coprocessor::Circuit::Gate::Encoded& e);      // AND, XOR
        ~Gate();
        Gate(const Gate& other);
        /** Note: this operator does not copy the memory for the external literals, but simply copies the pointer! */
        Gate& operator=(const Gate& other);

        Type getType() const { return type; }

        bool isInvalid() const { return (type == INVALID) ; }

        bool isFull() const { return encoded == FULL; }

        Riss::Lit getOutput() const { return (type != GenAND && type != ExO) ? (const Riss::Lit) x() : data.e.x; }

        void print(std::ostream& stream) const ;   // write gate to a stream

        /** free resources, if necessary */
        void destroy();

        void invalidate();
      private:
        Type type;
        Encoded encoded;

      public:
        Riss::Lit& x() {assert(type != XOR && type != GenAND && type != ExO && "gate cannot be XOR"); return data.lits[0]; }   // output
        Riss::Lit& a() {assert(type != ITE && "gate cannot be ITE"); return data.lits[1]; }   // AND, FA_SUM
        Riss::Lit& b() {assert(type != ITE && "gate cannot be ITE"); return data.lits[2]; }   // AND, FA_SUM
        Riss::Lit& c() {assert((type == FA_SUM || type == XOR) && "gate has to be FA_SUM"); return data.lits[3]; }   // FA_SUM
        Riss::Lit& s() {assert(type == ITE && "gate has to be ITE"); return data.lits[1]; }   // ITE selector
        Riss::Lit& t() {assert(type == ITE && "gate has to be ITE"); return data.lits[2]; }   // ITE true branch
        Riss::Lit& f() {assert(type == ITE && "gate has to be ITE"); return data.lits[3]; }   // ITE false branch
        Riss::Lit& get(const int index) { assert(type == ExO || type == GenAND); return data.e.externLits[index]; }

        const Riss::Lit& x() const {return data.lits[0]; }  // output
        const Riss::Lit& a() const {return data.lits[1]; }  // AND, FA_SUM
        const Riss::Lit& b() const {return data.lits[2]; }  // AND, FA_SUM
        const Riss::Lit& c() const {return data.lits[3]; }  // FA_SUM
        const Riss::Lit& s() const {return data.lits[1]; }  // ITE selector
        const Riss::Lit& t() const {return data.lits[2]; }  // ITE true branch
        const Riss::Lit& f() const {return data.lits[3]; }  // ITE false branch
        const Riss::Lit& get(const int index) const { return data.e.externLits[index]; }
        int size() const { assert(type == ExO || type == GenAND); return data.e.size; }

        bool isInQueue() const { return inQueue; }
        void putInQueue() { assert(inQueue == false && "cannot put twice in a queue"); inQueue = true; }
        void takeFromQueue() { inQueue = false; }
        int touch() { return ++touched; }
    };


    void extractGates(Coprocessor::CoprocessorData& data, std::vector< Coprocessor::Circuit::Gate >& gates);

  private:

    /** check whether this variable is the output an AND gate
     *  XOR gates will be given only for the smallest variable in the gate, because their output cannot be identified
     */
    void getGatesWithOutput(const Riss::Var v, std::vector<Gate>& gates, CoprocessorData& data);

    /** method that looks for AND gates (multiple AND might form an ExO gate!)*/
    void getANDGates(const Riss::Var& v, std::vector<Gate>& gates, CoprocessorData& data);
    /** one ExO can be understood as many AND gates! */
    void getExOGates(const Riss::Var& v, std::vector<Gate>& gates, CoprocessorData& data);
    /** method that looks for XOR gate( stores it only in list, if v is the smallest variable of the gate)*/
    void getXORGates(const Riss::Var& v, std::vector<Gate>& gates, CoprocessorData& data);
    /** method that looks for ITE gates */
    void getITEGates(const Riss::Var& v, std::vector<Gate>& gates, CoprocessorData& data);
    /** method that looks for half adder sum gates (have gate only if v is smallest variale) */
    void getFASUMGates(const Riss::Var& v, std::vector<Gate>& gates, CoprocessorData& data);
};


};

#endif
