/****************************************************************************************[Dimacs.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2013       Norbert Manthey

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_Minisat_QDimacs_h
#define RISS_Minisat_QDimacs_h

#include <cstdio>
#include <vector>

#include "riss/utils/ParseUtils.h"
#include "riss/core/SolverTypes.h"
#include "riss/core/Dimacs.h"

namespace Riss
{

struct quantification {
    char kind; // 'e' for existential, 'a' for all
    std::vector<Riss::Lit> lits; // literals inside the quantifier
};

template<class B, class Solver>
static void readQuantifier(B& in, Solver& S, quantification& quantifier)
{
    int     parsed_lit, var;
    quantifier.lits.clear();
    for (;;) {
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) { break; }
        var = abs(parsed_lit) - 1;
        while (var >= S.nVars()) { S.newVar(); }
        quantifier.lits.push_back((parsed_lit > 0) ? Riss::mkLit(var) : ~Riss::mkLit(var));
    }
    /*
    std::cerr << "c parsed quantifier: " << quantifier.kind << std::endl;

    for( int i=0; i < quantifier.lits.size(); ++ i ) {
      const int l = toInt(quantifier.lits[i]);
    if (l % 2 == 0) std::cerr << " " << (l / 2) + 1 ;
    else std::cerr << " " << - (l/2) - 1;
    }
    std::cerr << std::endl;
    */
}

template<class B, class Solver>
static void parse_QDIMACS_main(B& in, Solver& S, std::vector<quantification>& quantifiers)
{
    Riss::vec<Riss::Lit> lits;
    int vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    for (;;) {
        skipWhitespace(in);
        if (*in == EOF) { break; }
        else if (*in == 'p') {
            if (eagerMatch(in, "p cnf")) {
                vars    = parseInt(in);
                clauses = parseInt(in);
                // SATRACE'06 hack
                // if (clauses > 4000000)
                //     S.eliminate(true);
                std::cerr << "c parsed " << vars << " variables and " << clauses << " clauses " << std::endl;
            } else {
                printf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p') {
            skipLine(in);
        } else if (*in == 'e') {

            quantifiers.push_back(quantification());
            quantifiers[quantifiers.size() - 1].kind = 'e';
            ++ in;
            readQuantifier(in, S, quantifiers[quantifiers.size() - 1]);
        } else if (*in == 'a') {
            quantifiers.push_back(quantification());
            quantifiers[quantifiers.size() - 1].kind = 'a';
            ++ in;
            readQuantifier(in, S, quantifiers[quantifiers.size() - 1]);
        } else {
            // std::cerr << "next symbol: (" << *in << ")" << std::endl;
            cnt++;
            readClause(in, S, lits);
            S.addClause_(lits);
        }
    }
    if (vars != S.nVars()) {
        fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of variables.\n");
    }
    if (cnt  != clauses) {
        fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of clauses.\n");
    }
}

// Inserts problem into solver.
//
template<class Solver>
static void parse_QDIMACS(gzFile input_stream, Solver& S, std::vector<quantification>& quantifiers)
{
    StreamBuffer in(input_stream);
    parse_QDIMACS_main(in, S, quantifiers);
}

} // namespace Riss

#endif
