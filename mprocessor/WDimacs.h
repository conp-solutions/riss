/***************************************************************************************[WDimacs.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2013, Peter Steinke
Copyright (c) 2014, Norbert Manthey

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*************************************************************************************************/
#ifndef RISS_WDimacs_h
#define RISS_WDimacs_h

#include <stdio.h>
#include <fstream>

#include "riss/utils/ParseUtils.h"
#include "riss/core/SolverTypes.h"
#include "riss/core/Solver.h"


//=================================================================================================
// DIMACS Parser:
namespace Riss
{

typedef int64_t Weight;

template<class B, class Solver>
static void readWCNFClause(B& in, Solver& S, Riss::vec<Riss::Lit>& lits, int32_t original_vars)
{
    int     parsed_lit, var;
    lits.clear();
    for (;;) {
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) { break; }
        var = abs(parsed_lit) - 1;
        if (original_vars < var) {
            fprintf(stderr, "PARSE ERROR! DIMACS header mismatch: wrong number of variables. Variable: %d \n", var);
            exit(3);
        }
        lits.push((parsed_lit > 0) ? Riss::mkLit(var) : ~Riss::mkLit(var));
    }
}


/**
 * parse the input and add the (relaxed) clauses to the solver
 *
 * @param variableWeights for each (relaxation or unit clause) variable, store the weight in the given std::vector!
 * @return number of variables in the WCNF file (higher variables are relax variables)
 */
template<class B, class Solver>
static unsigned parse_WCNF_main(B& in, Solver& S, Riss::vec<Weight>& literalWeights)
{
    Riss::vec<Riss::Lit> lits;
    literalWeights.clear();
    int32_t vars    = 0;
    int32_t original_vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    int weighted_cnt = 0, weighted_units = 0, hard_cnt = 0, empty_weighted = 0;
    Weight top  = 0;
    Weight weight = 0;
    bool isWcnf = false;
    bool foundPline = false; // do indicate that clauses can be parsed
    for (;;) {
        skipWhitespace(in);
        if (*in == EOF) { break; }
        else if (*in == 'p') {
            foundPline = true;
            ++in;
            if (!eagerMatch(in, " ")) { printf("PARSE ERROR! Expected space. Unexpected char: %c\n", *in), exit(3); }

            if (*in != 'w') {
                // ++in;
                if (!eagerMatch(in, "cnf")) { printf("PARSE ERROR! Expected cnf. Unexpected char: %c\n", *in), exit(3); }
                vars    = parseInt(in);
                clauses = parseInt(in);
                original_vars = vars;
                while (S.nVars() < original_vars) { S.newVar(false, true, 'o'); }   // add "enough" variables for the original problem
                literalWeights.growTo(2 * S.nVars(), 0);   // all variables in the orignial-variable range do not have a weight (yet)
                std::cerr << "c p cnf " << vars << " " << clauses << std::endl;
            } else if (eagerMatch(in, "wcnf")) {
                isWcnf = true;
                original_vars = vars = parseInt(in);
                clauses = parseInt(in);
                while (*in == 9 || *in == 32) { ++in; } // skip whitespace except new line / return
                if (*in < '0' || *in > '9') { top = 0; }
                else { top = parseInt(in); }
                std::cerr << "c wcnf " << vars << " " << clauses << " " << top << std::endl;
                while (S.nVars() < original_vars) { S.newVar(false, true, 'o'); }  // add "enough" variables for the original problem
                literalWeights.growTo(2 * S.nVars(), 0);   // all variables in the orignial-variable range do not have a weight (yet)
            } else {
                printf("PARSE ERROR! Expected wcnf. Unexpected char: %c\n", *in), exit(3);
            }
            S.reserveVars(original_vars);
        } else if (*in == 'c') {
            skipLine(in);
        } else {
            if (!foundPline) { printf("PARSE ERROR! Expected p-line before the first clause. Unexpected char: %c\n", *in); exit(3); }
            cnt++;
            if (isWcnf) { weight = parseInt(in); } // a weight should be here, hence parse an extra number
            else { weight = 1; }

            readWCNFClause(in, S, lits, original_vars);

            if (weight > 0) {   // if the weight is 0, then noghting has to be done!
                // std::cerr << "c found clause " << lits << " with weight " << weight << "(max=" << top << ") vars=" << vars <<std::endl;
                if (weight != top || !isWcnf) {  // this clause is weighted, or the instance is not a WCNF
                    if (lits.size() != 1) {   // what happens to empty clauses?
                        weighted_cnt++;
                        vars++;
                        Riss::Var relaxVariables = S.newVar(false, false, 'r'); // add a relax variable
                        literalWeights.push(0); literalWeights.push(0); // weights for the two new literals!
                        literalWeights[ toInt(Riss::mkLit(relaxVariables)) ] += weight;   // assign the weight of the current clause to the relaxation variable!

                        if (lits.size() == 0) {   // found empty weighted clause
                            // to ensure that this clause is falsified, add the compementary unit clause!
                            S.addClause(Riss::mkLit(relaxVariables));
                            empty_weighted ++;
                        }
                        lits.push(~ Riss::mkLit(relaxVariables));     // add a negative relax variable to the clause
                        S.freezeVariable(relaxVariables, true);   // set this variable as frozen!
                        S.addClause(lits);
                    } else {
                        literalWeights[ toInt(lits[0]) ] += weight;   // if variable is true, then the clause is false, henec pay the cost!
                        S.freezeVariable(var(lits[0]), true);   // set this variable as frozen!
                    }
                } else { // this is a hard clause!
                    hard_cnt++;
                    S.addClause(lits); // handles empty clauses!
                }
            }
        }
    }
    std::cerr << "c clss: " << cnt << "(" << clauses << "), vars=" << S.nVars() << "(" << original_vars << "+" << weighted_cnt << ") weighted units: " << weighted_units << " hard cls: " << hard_cnt << " emptyWeighted: " << empty_weighted << std::endl;
    std::cerr.flush();
    if (S.okay()) {
        if (original_vars + weighted_cnt != S.nVars()) { fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of variables.\n"); }
        if (cnt  != clauses) { fprintf(stderr, "WARNING! DIMACS header mismatch: wrong number of clauses.\n"); }
    }

    return original_vars;
}

/**
 * Inserts problem into solver.
 *
 * @return number of variables in the WCNF file (higher variables are relax variables)
 */
template<class Solver>
static unsigned parse_WCNF(gzFile input_stream, Solver& S, Riss::vec<Weight>& literalWeights)
{
    Riss::StreamBuffer in(input_stream);
    return parse_WCNF_main(in, S, literalWeights);
}

//=================================================================================================
} // namespace Riss
#endif
