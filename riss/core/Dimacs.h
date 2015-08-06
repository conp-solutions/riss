/****************************************************************************************[Dimacs.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

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

#ifndef RISS_Minisat_Dimacs_h
#define RISS_Minisat_Dimacs_h

#include <cstdio>

#include "riss/utils/ParseUtils.h"
#include "riss/core/SolverTypes.h"

namespace Riss
{

//=================================================================================================
// DIMACS Parser:

template<class B, class Solver>
static void readClause(B& in, Solver& S, vec<Lit>& lits)
{
    int     parsed_lit, var;
    lits.clear();
    for (;;) {
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) { break; }
        var = abs(parsed_lit) - 1;
        while (var >= S.nVars()) { S.newVar(); }
        lits.push((parsed_lit > 0) ? mkLit(var) : ~mkLit(var));
    }
}

template<class B, class Solver>
static void parse_DIMACS_main(B& in, Solver& S, bool isProof = false)
{
    vec<Lit> lits;
    int vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    for (;;) {
        skipWhitespace(in);
        if (*in == EOF) { break; }
        else if (*in == 'p') { // check header information for CNF formula (variables and clauses)
            if (eagerMatch(in, "p cnf")) {
                vars    = parseInt(in);
                clauses = parseInt(in);
                S.reserveVars(vars); // reserve space for the variables, so that there is less fragmentation
            } else {
                printf("c PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p') {
            skipLine(in);
        } else {
            cnt++;
            readClause(in, S, lits);
            S.addInputClause_(lits); // tell the solver that this clause is an input clause (used only for proof verification)
            S.addClause_(lits);
        }
    }

    if (vars != S.nVars()) {
        fprintf(stderr, "c WARNING! DIMACS header mismatch: wrong number of variables.\n");
    }
    if (cnt  != clauses) {
        fprintf(stderr, "c WARNING! DIMACS header mismatch: wrong number of clauses.\n");
    }
}

// Inserts problem into solver.
//
template<class Solver>
static void parse_DIMACS(gzFile input_stream, Solver& S)
{
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, S);
}

//=================================================================================================

template<class B, class Solver>
static ProofStyle parse_proof_main(B& in, Solver& S, bool isProof = false)
{
    vec<Lit> lits;
    int vars      = 0;
    int clauses   = 0;
    int cnt       = 0;
    bool isDelete = false;
    ProofStyle returnedStyle = unknownProof;
    for (;;) {
        skipWhitespace(in);
        if (*in == EOF) { break; }
        if (*in == 'o') { // check proof format of given proof
            if (eagerMatch(in, "o proof DR")) {
                if (*in == 'U') {
                    ++ in;
                    if (*in == 'P') {
                        returnedStyle = drupProof;
                        skipLine(in);
                    }
                } else  {
                    if (eagerMatch(in, "AT")) {
                        returnedStyle = dratProof;
                        skipLine(in);
                    }
                }
            } else {
                printf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 's') {
            if (!eagerMatch(in, "s UNSAT")) {
                printf("WARNING: solution not claimed to be unsatisfiable\n");
            }
            skipLine(in);
        } else if (*in == 'c') {
            skipLine(in);
        } else if (*in == 'd') { // found delete information
            if (isDelete) { printf("PARSE ERROR! Unexpected char in delete section: %c\n", *in), exit(3); }
            isDelete = true;
            ++ in;
            // forward until next symbol to be able to read the clause
        } else {
            cnt++;
            readClause(in, S, lits);
            S.addClause_(lits, isDelete);
            isDelete = false; // set delete information back to normal
        }
    }

    // warn only for the formula, not for the proof
    if (!isProof) {
        if (vars != S.nVars()) {
            fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of variables.\n");
        }
        if (cnt  != clauses) {
            fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of clauses.\n");
        }
    }
    return returnedStyle;
}

// Inserts proof into proof checker.
//
template<class Solver>
static ProofStyle parse_proof(gzFile input_stream, Solver& S)
{
    StreamBuffer in(input_stream);
    return parse_proof_main(in, S, true);
}

//=================================================================================================

/** check whether the given model satisfies the given file
 *  @return true, if the model satisfies all clauses in the formula
 */
bool check_DIMACS(gzFile input_stream, vec<lbool>& model)
{
    StreamBuffer in(input_stream);
    vec<Lit> lits;
    int cnt = 0;
    bool failed = false;
//     cerr << "c start parsing and verifying file" << endl;
    for (;;) {
        skipWhitespace(in);
        if (*in == EOF) { break; }
        else if (*in == 'c' || *in == 'p') {
            skipLine(in);
        } else {
            // read clause
            int parsed_lit, variable;
            cnt ++;
            lits.clear();
            bool satisfied = false;
            for (;;) {
                parsed_lit = parseInt(in);
//      cerr << "c parsed number: " << parsed_lit << endl;
                if (parsed_lit == 0) { break; }
                variable = abs(parsed_lit) - 1;
                const Lit l = (parsed_lit > 0) ? mkLit(variable) : ~mkLit(variable);
                lits.push(l);
                if (variable < model.size()) {
                    satisfied = satisfied || ((model[ variable ] == l_True && !sign(l)) || (model[ variable ] == l_False && sign(l))) ;
                }
            }
            if (!satisfied) {
                failed = true;
                printf("c no model -- does not satisfy clause [%d] ", cnt);
                for (int i =  0 ; i < lits.size(); ++ i) { printf(" %d", sign(lits[i]) ? - var(lits[i]) - 1 : var(lits[i]) + 1); }
                printf("\n");
                break;
            }

        }

    }

    return !failed;
}

//=================================================================================================
}

#endif
