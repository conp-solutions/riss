/********************************************************************************[CNFClassifier.cc]
 Copyright (c) 2013, Norbert Manthey, All rights reserved.
 **************************************************************************************************/

#include "classifier/CNFClassifier.h"
#include "riss/utils/System.h" // for cpuTime
#include "coprocessor/CoprocessorTypes.h" // for binary implication graph
#include <sstream>
#include <fstream>
#include <iostream>

#include <limits.h>
#include <math.h>

#include "riss/mtl/Sort.h"
#include "knn.h"

using namespace Riss;
using namespace std;


// /** print literals into a stream */
// static inline ostream& operator<<(ostream& other, const Lit& l) {
//     if (l == lit_Undef)
//         other << "lUndef";
//     else if (l == lit_Error)
//         other << "lError";
//     else
//         other << (sign(l) ? "-" : "") << var(l) + 1;
//     return other;
// }

// /** print a clause into a stream */
// static inline ostream& operator<<(ostream& other, const Clause& c) {
//     other << "[";
//     for (int i = 0; i < c.size(); ++i)
//         other << " " << c[i];
//     other << "]";
//     return other;
// }*/
// /*
// /** print elements of a vector */
// template<typename T>
// static inline std::ostream& operator<<(std::ostream& other, const std::vector<T>& data) {
//     for (int i = 0; i < data.size(); ++i)
//         other << " " << data[i];
//     return other;
// }

// /** print elements of a vector */
// template<typename T>
// static inline std::ostream& operator<<(std::ostream& other, const vec<T>& data) {
//     for (int i = 0; i < data.size(); ++i)
//         other << " " << data[i];
//     return other;
// }

CNFClassifier::CNFClassifier(ClauseAllocator& _ca, vec<CRef>& _clauses, int _nVars) :
    ca(_ca), clauses(_clauses), nVars(_nVars)
{
    quantilesCount = 4;
    precision = 4;
    dumpingPlots = false;
    computingResolutionGraph = true;
    computingClausesGraph = true;
    computingClausesGraph = true;
    computingRWH = true;
    attrFileName = nullptr;
    computeBinaryImplicationGraph = true;
    computeConstraints = true;
    computeXor = true;
    outputFileName = nullptr;
    plotsFileName = nullptr;
    maxClauseSize = -1;
    computingDerivative = false;
    verb = 1;
}

CNFClassifier::~CNFClassifier()
{

}
string CNFClassifier::getConfig(Solver& S)
//string CNFClassifier::getConfig ( Riss::Solver& S, std::string dbName)
{
#warning MIGHT BE DONE OUTSIDE OF THIS METHOD
    S.verbosity = 0;

    // here convert the found unit clauses of the solver back as real clauses!
    if (S.trail.size() > 0) {
        S.buildReduct();
        vec<Lit> ps; ps.push(lit_Undef);
        for (int j = 0; j < S.trail.size(); ++j) {
            const Lit l = S.trail[j];
            ps[0] = l;
            CRef cr = S.ca.alloc(ps, false);
            S.clauses.push(cr);
        }
    }

    vector<double> features; // temporary storage
    extractFeatures(features); // also print the formula name!!

    return computeKNN(5, features);
}


std::vector<std::string> CNFClassifier::featureNames()
{
    // TODO: should return the names of the features in the order returned by the extractFeatures method
    return featuresNames;
}

bool contains(const Clause& c, Lit l)
{
    for (int i = 0; i < c.size(); i++) {
        if (l == c[i]) {
            return true;
        }
    }
    return false;
}

// old quadratic implementation
/*
int resolventSize(const Clause& c1, const Clause& c2) {
    int complementCount = 0, sharedCount = 0;
    for (int i = 0; i < c1.size(); ++i) {
        Lit l = c1[i];
        Lit op = ~l;
        if (contains(c2, op))
            complementCount++;
        if (contains(c2, l))
            sharedCount++;
    }
    if (complementCount == 1)
        return c1.size() + c2.size() - sharedCount - 2;
    else if (complementCount > 1)
        return 0;
    return -1;
}
*/

int resolventSize(const Clause& c1, const Clause& c2)
{
    int complementCount = 0, sharedCount = 0;
    int i = 0 , j = 0;
    while (i < c1.size() && j < c2.size()) {
        if (c1[i] == c2[j]) {   // share a literal
            sharedCount ++;
            i++; j++;
        } else if (c1[i] == ~c2[j]) {   // complement count
            if (++ complementCount > 1) { return -1; }   // early abort for tautologic resolvents
            i++; j++;
        } else if (c1[i] < c2[j]) {   // neither shared nor tautologic
            i++;
        } else {
            j++;
        }
    }
    assert(complementCount <= 1 && "complement > 1 has to be catched within the resolution loop above!");
    if (complementCount == 1) {
        assert(c1.size() + c2.size() - sharedCount - 2 >= 0 && "resolvent cannot be less then the empty clause!");
        return c1.size() + c2.size() - sharedCount - 2;
    } else { return -1; } // tautology!
}

uint64_t CNFClassifier::buildClausesAndVariablesGrapths(BipartiteGraph& clausesVariablesP, BipartiteGraph& clausesVariablesN,
        vector<double>& ret)
{

    Graph *variablesGraph = nullptr;
    if (computingVarGraph) { variablesGraph = new Graph(nVars, computingDerivative); }
    uint64_t operations = 0, operationsV = 0;
    double time1 = cpuTime();
    vector<int> clsSizes(10, 0);   // stores number of clauses of a given size (here, store for 1 to 8)
    // iterate over all clauses and parse all the literals
    for (int i = 0; i < clauses.size(); ++i) {
        //You should know the difference between Java References, C++ Pointers, C++ References and C++ copies.
        // Especially if you write your own methods and pass large objects that you want to modify, this makes a difference!
        const Clause& c = ca[clauses[i]];
        clsSizes[ c.size() + 1 < clsSizes.size() ? c.size() : clsSizes.size() - 1 ] ++; // cumulate number of occurrences of certain clause sizes

        double wvariable = pow(2, -c.size());
        for (int j = 0; j < c.size(); ++j) {
            const Lit& l = c[j]; // for read access only, you could use a read-only reference of the type literal.
            const Lit cpl = ~l;  // build the complement of the literal
            const Var v = var(l); // calculate a variable from the literal
            const bool isNegative = sign(l); // check the polarity of the literal
            if (isNegative) {
                clausesVariablesN.addEdge(v, i);
            } else {
                clausesVariablesP.addEdge(v, i);
            }


            // Adding edges for the variable graph
            if (computingVarGraph) {
                for (int k = j + 1; k < c.size(); ++k) {
                    operationsV += variablesGraph->addAndCountUndirectedEdge(v,
                                   var(c[k]), wvariable);
                }
            }

            assert(var(~l) == var(l) && "the two variables have to be the same");
        }

    }

    int i, nsize, horn = 0;
    SequenceStatistics cdeg(computingDerivative, false), // this cannot have zero values
                       cmax(computingDerivative, false), // this cannot have zero values
                       vdeg(computingDerivative),
                       vmax(computingDerivative);
    // Statistics for the degrees
    double sizeN, sizeP;
    for (i = 0; i < clauses.size(); ++i) {
        sizeN = clausesVariablesN.getBCount(i);
        sizeP = clausesVariablesP.getBCount(i);
        nsize = sizeN + sizeP;
        cdeg.addValue(nsize);
        const double max0 = max(sizeN, sizeP);
        if (nsize > 0) {
            cmax.addValue(max0 / nsize);
        }
        if (nsize - max0 <= 1) {
            horn++;
        }
    }
    operations += clauses.size();
    operations += cdeg.compute(quantilesCount);
    maxClauseSize = cdeg.getMax();
    operations += cmax.compute(quantilesCount);

    for (i = 0; i < nVars; ++i) {
        sizeN = clausesVariablesN.getAjacencyW(i).size();
        sizeP = clausesVariablesP.getAjacencyW(i).size();
        nsize = sizeN + sizeP;
        vdeg.addValue(nsize);
        const double max0 = max(sizeN, sizeP);
        if (nsize > 0) {
            vmax.addValue(max0 / nsize);
        }
    }


    featuresNames.push_back("clauses");
    ret.push_back(clauses.size());
    featuresNames.push_back("vars");
    ret.push_back(nVars);


    operations += nVars;
    operations += vdeg.compute(quantilesCount);
    operations += vmax.compute(quantilesCount);
    if (computingVarGraph) {
        operationsV += variablesGraph->computeStatistics(quantilesCount);
    }
    // Statistics for the weights

    for (int i = 1 ; i < clsSizes.size(); ++ i) {
        stringstream iterFname;
        iterFname << "clauses_size_";
        if (i + 1 == clsSizes.size()) { iterFname << ">=_"; }
        iterFname << i;
        featuresNames.push_back(iterFname.str());
        ret.push_back(clsSizes[i]);
    }

    featuresNames.push_back("horn_clauses");
    ret.push_back(horn);


    cdeg.infoToVector("clause-variable degree", featuresNames, ret);
    if (dumpingPlots) {
        cdeg.toStream(plotsFileName, ".cv-deg");
    }

    // variable-clause degree sequence statistics

    vdeg.infoToVector("variable-clause degree", featuresNames, ret);
    if (dumpingPlots) {
        vdeg.toStream(plotsFileName, ".vc-deg");
    }

    // clause-variable positive degree sequence statistics

    cmax.infoToVector("clause-variable polarity", featuresNames, ret);
    if (dumpingPlots) {
        cmax.toStream(plotsFileName, ".cv-pdeg");
    }

    // variable-clause positive degree sequence statistics

    vmax.infoToVector("variable-clause polarity", featuresNames, ret);
    if (dumpingPlots) {
        vmax.toStream(plotsFileName, ".cv-pdeg");
    }

    if (computingVarGraph) {
        variablesGraph->getDegreeStatistics().infoToVector("variables graph degree", featuresNames, ret);
        if (dumpingPlots) {
            variablesGraph->getDegreeStatistics().toStream(plotsFileName, ".var-deg");
        }

        variablesGraph->getWeightStatistics().infoToVector("variables graph weights", featuresNames, ret);
        if (dumpingPlots) {
            variablesGraph->getWeightStatistics().toStream(plotsFileName, ".var-wgt");
        }
    }
    time1 = (cpuTime() - time1);
    timeIndexes.push_back(ret.size());
    featuresNames.push_back("Clause-Var and Var graphs time");
    ret.push_back(time1);

    featuresNames.push_back("Clause-Var steps");
    ret.push_back(operations);

    if (computingVarGraph) {
        featuresNames.push_back("Var graph steps");
        ret.push_back(operationsV);
        delete variablesGraph;
    }
    return operations + operationsV;
}

uint64_t CNFClassifier::buildResolutionAndClausesGrapths(const BipartiteGraph& clausesVariablesP, const BipartiteGraph& clausesVariablesN, vector<double>& ret)
{
    // here i build the graph with edges between 2 clauses if they share a variable.
    uint64_t operationsC = 0, operationsRes = 0;
    if (computingClausesGraph || computingResolutionGraph) {
        double time1 = cpuTime();

        Graph clausesGraph(clauses.size(), computingDerivative);
        Graph resolutionGraph(clauses.size(), computingDerivative);
        for (int i = 0; i < nVars; ++i) {
            const vector<int> al = clausesVariablesP.getAjacencyW(i);
            for (int k = 0; k < al.size(); ++k) {
                int clausek = al[k];

                if (computingClausesGraph) {
                    for (int j = k + 1; j < al.size(); ++j) {
                        int clausej = al[j];
                        operationsC += clausesGraph.addAndCountUndirectedEdge(clausek, clausej, 1);
                    }
                }

                if (computingResolutionGraph) {

                    const vector<int>  aln = clausesVariablesN.getAjacencyW(i);
                    for (int j = 0; j < aln.size(); ++j) {
                        int clausej = aln[j];

                        // compute the actual weight which is the length of the resolvent.
                        int rs = resolventSize(ca[clauses[clausej]], ca[clauses[clausek]]);
                        operationsRes += (ca[clauses[clausej]].size() + ca[clauses[clausek]].size());
                        // return code -1 means tautology, any higher return code is the size of the clause
                        if (rs == 0) {
                            cerr << "empty clause doing resolution with " << ca[clauses[clausej]] << " and " << ca[clauses[clausek]] << endl;
                        }
                        assert(rs != 0 && "the formula is unsatisfiable");

                        if (rs > 0) {
                            resolutionGraph.addUndirectedEdge(clausej, clausek, pow(2, -rs));
                        }

                    }

                }
            }

            // adding clauses connected by negative literals....

            const vector<int>  aln = clausesVariablesN.getAjacencyW(i);
            for (int k = 0; k + 1 < aln.size(); ++k) {
                int clausek = aln[k];

                if (computingClausesGraph) {
                    for (int j = k + 1; j < aln.size(); ++j) {
                        int clausej = aln[j];
                        operationsC += clausesGraph.addAndCountUndirectedEdge(clausek, clausej, 1);
                        operationsC += clausesGraph.addAndCountUndirectedEdge(clausej, clausek, 1);
                    }
                }
            }

        }

        if (computingResolutionGraph) {
            operationsRes += resolutionGraph.computeOnlyStatistics(quantilesCount);
            resolutionGraph.getDegreeStatistics().infoToVector("resolution graph degree", featuresNames, ret);
            resolutionGraph.getWeightStatistics().infoToVector("resolution graph weights", featuresNames, ret);
            if (dumpingPlots) {
                resolutionGraph.getDegreeStatistics().toStream(plotsFileName, ".res-deg");
                resolutionGraph.getWeightStatistics().toStream(plotsFileName, ".res-wgt");
            }
            featuresNames.push_back("Resolution graph steps");
            ret.push_back(operationsRes);
        }
        if (computingClausesGraph) {
            operationsC += clausesGraph.computeStatistics(quantilesCount);
            clausesGraph.getDegreeStatistics().infoToVector("clauses graph degree", featuresNames, ret);
            clausesGraph.getWeightStatistics().infoToVector("clauses graph weights", featuresNames, ret);
            if (dumpingPlots) {
                clausesGraph.getDegreeStatistics().toStream(plotsFileName,
                        ".cl-deg");
                clausesGraph.getWeightStatistics().toStream(plotsFileName,
                        ".cl-wgt");
            }

            featuresNames.push_back("Clause graph steps");
            ret.push_back(operationsC);
        }
        time1 = (cpuTime() - time1);
        timeIndexes.push_back(ret.size());
        featuresNames.push_back("Resolution and Clause graphs time");
        ret.push_back(time1);
    }
    return operationsC + operationsRes;
}

inline char* replaceStr(string str, char old, char newch)
{
    char* res = new char[str.size() + 1];
    for (int i = 0; i < str.size(); ++i) {
        res[i] = (str[i] == old) ? newch : str[i];
    }
    res[str.size()] = '\0';
    return res;
}

void CNFClassifier::fband(vector<double>& ret)
{
    // norbert ... begin constraint calculation
    /**
     *  This code generated data for four graphs!
     *  1) binary implication graph (big), the literal-clause graph for binary clauses only
     *  2) an exactly-one graph, for sets of literals where for each set only a single literal can be set to true
     *  3) a FULL-AND-graph for constraints like x <-> AND(a,b,c), where edges could go from -a,-b,-c to -x; and edges from x to a,b,c (literals, do not know whether variables makes sense)
     *  4) a BLOCKED-AND-graph, for constraints like x <- AND( a,b,c ), where edges could also go from a,b,c to x (or variables, same as above ... )
     */
    // TODO: time measuring for binary implication graph (BIG) and scanning for constraints
    // fill litToClsMap map..
    if (computeBinaryImplicationGraph || computeConstraints || computeXor) {
        double time1 = cpuTime();
        vector< vector<CRef> > litToClsMap; // norbert: FIXME you might replace this structure with the graph you want to add

        // Enrique, graphs declaration:
        int nLiterals = nVars * 2;
        uint64_t bigSteps = 0;
        Graph exactlyOneLiterals(nLiterals, computingDerivative);
        Graph fullAndGraph(nLiterals, computingDerivative);
        Graph blockedAndGraph(nLiterals, computingDerivative);
        litToClsMap.resize(2 * nVars); // setup enough vectors
        for (int i = 0; i < clauses.size(); ++i) {
            for (int j = 0; j < ca[clauses[i]].size(); ++j)
                litToClsMap[Riss::toInt(ca[clauses[i]][j])].push_back(
                    clauses[i]);
        }
        Coprocessor::BIG big; // graph that works only on the binary clauses with a small memory representation
        big.create(ca, nVars, clauses);
        bigSteps += clauses.size(); // had to touch each clause once!
        // TODO: here, this graph could be used to create another sequence-statistics object!
        // Code for binary implication degrees:
        SequenceStatistics binStat(computingDerivative);
        for (int i = 0; i < nLiterals; ++i) {
            int degreeOfLiteral = big.getSize(mkLit(i / 2, i % 2 == 1)); //
            binStat.addValue(degreeOfLiteral);
        }
        bigSteps += binStat.compute(quantilesCount);
        binStat.infoToVector("bin implication graph degree", featuresNames,
                             ret);
        if (dumpingPlots) {
            binStat.toStream(plotsFileName, ".binG-deg");
        }
        time1 = (cpuTime() - time1);
        featuresNames.push_back("Bin Implication graph steps");
        ret.push_back(bigSteps);
        timeIndexes.push_back(ret.size());
        featuresNames.push_back("Bin Implication graph time");
        ret.push_back(time1);
        time1 = cpuTime();
        uint64_t constraintSteps = 0;
        if (computeConstraints) {
            MarkArray ma;
            ma.create(2 * nVars); // field for set-checks
            vector<Lit> foundAndOutputs; // store the output literals for a given AND-gate that has been found with a clause
            for (int i = 0; i < clauses.size(); ++i) {
                const Clause& c = ca[clauses[i]]; // check this clause
                if (c.size() <= 2) {
                    continue;    // for constraints, only larger clauses are interesting
                }

                constraintSteps ++;
                // if( c.size() > 3 ) continue; // we might have another parameter enabling this check - since scanning larger clauses consumes more time
                // first, check for all the full AND gates ...
                bool hasAllANDS = true; // if this stays true, then the clause actually represents an exactly-one constraint
                for (int j = 0; j < c.size(); ++j) {
                    foundAndOutputs.clear();
                    const Lit output = c[j];
                    const Lit* impliedList = big.getArray(output); // get all literals that are implied by the literal "output", i.e. there are clauses (output -> lit) = (-output \lor lit)
                    ma.nextStep();
                    for (int k = 0; k < big.getSize(output); ++k)
                        ma.setCurrentStep(
                            toInt(
                                impliedList[k]
                            ));  // mark all literals that are implied by output
                    // check whether all other complements in the clause are implied
                    bool allImplied = true;
                    for (int k = 0; k < c.size(); ++k) {
                        if (k == j) {
                            continue;    // do not check the current output literal!
                        }
                        constraintSteps ++;
                        if (!ma.isCurrentStep(toInt(~c[k]))) {
                            allImplied = false;
                            break;
                        }
                    }
                    if (!allImplied) {
                        // not a full AND gate, nor part of an EXO
                        hasAllANDS = false;
                        // check for blocked AND!
                        if (litToClsMap[toInt(output)].size() + 1 > c.size()) {
                            continue;    // cannot be a blocked AND gate
                        }

                        if (big.getSize(output)
                                != litToClsMap[toInt(output)].size()) {
                            continue;    // assume all the clauses with -output are binary, if fail, we do not consider that clause
                        }

                        // here, the candidate has the form (x, l1, ... ln), and all clauses with -x are binary, e.g. (-x,-l_i). Check whether all l_i occur in the candidate!
                        ma.nextStep();
                        for (int k = 0; k < c.size(); ++k)
                            if (k != j) {
                                ma.setCurrentStep(toInt(~c[k]));    // mark all literals!
                            }
                        bool missMatch = false; // check whether some l_i is not marked
                        for (int k = 0; k < big.getSize(output); ++k) {
                            if (!ma.isCurrentStep(
                                        toInt(big.getArray(output)[k]))) {
                                missMatch = true;
                                break;
                            }
                            constraintSteps ++;
                        }
                        if (!missMatch) {
                            // blocked AND gate found
                            double w = pow(2, -c.size() + 1);
                            for (int k = 0; k < c.size(); ++k) {
                                // TODO: add this information to the BLOCKED-AND graph!
                                if (c[k] == output) {
                                    continue;
                                }

                                // on a literal based graph, add the edge ( ~c[k] -> foundOutputs[j] to the BLOCKED-AND-gate graph, a weight could be related to the clause size (c.size()-1), that represents the number of inputs to the gate
                                blockedAndGraph.addDirectedEdge(toInt((~c[k]).x),
                                                                toInt(output), w); // add integers
                                constraintSteps ++;
                            }
                            // debug output
                            //                          cerr << "c found BLOCKED-AND gate " << output << " <- AND(";
                            //                          for (int k = 0; k < c.size(); ++k) {
                            //                              if (c[k] != output)
                            //                                  cerr << " " << ~c[k];
                            //                          }
                            //                          cerr << " )" << endl;
                        }
                    } else {
                        // add AND gate, handle it later!
                        foundAndOutputs.push_back(output);
                    }
                }
                // handle all output literals that have been found with that clause
                if (hasAllANDS) {
                    // this clause represents an exactly-one structure
                    // TODO: add this information to a exactly-one-graph!
                    for (int j = 0; j < c.size(); ++j) {
                        for (int k = j + 1; k < c.size(); ++k) {
                            // add the edge (c[j] <-> c[k]) to an exactly-one-graph;
                            exactlyOneLiterals.addUndirectedEdge(c[j].x,
                                                                 c[k].x);
                        }
                    }
                    // debug output
                    //                  cerr << "c found ExactlyOne( " << c << " )" << endl;
                } else {
                    for (int j = 0; j < foundAndOutputs.size(); ++j) {
                        double w = pow(2, -c.size() + 1);
                        for (int k = 0; k < c.size(); ++k) {
                            // TODO: add this information to the FULL-AND graph!
                            if (c[k] == foundAndOutputs[j]) {
                                continue;
                            }

                            // on a literal based graph, add the edge ( ~c[k] -> foundOutputs[j] to the FULL-AND-gate graph, a weight could be related to the clause size (c.size()-1), that represents the number of inputs to the gate
                            fullAndGraph.addDirectedEdge(toInt((~c[k]).x),
                                                         toInt(foundAndOutputs[j].x), w); // make sure to give integers
                        }
                        // debug output
                        //                      cerr << "c found FULL AND gate " << foundAndOutputs[j] << " <-> AND(";
                        //                      for (int k = 0; k < c.size(); ++k) {
                        //                          if (c[k] != foundAndOutputs[j])
                        //                              cerr << " " << ~c[k];
                        //                      }
                        //                      cerr << " )" << endl;
                    }
                }
            }
        }
        // Enrique, Compute stats and output them
        if (computeConstraints) {
            uint64_t exoSteps = constraintSteps, fandSteps = constraintSteps, bandSteps = constraintSteps;
            exoSteps += exactlyOneLiterals.computeOnlyStatistics(quantilesCount);
            fandSteps += fullAndGraph.computeOnlyStatistics(quantilesCount);
            bandSteps += blockedAndGraph.computeOnlyStatistics(quantilesCount);
            exactlyOneLiterals.getDegreeStatistics().infoToVector("Exactly1Lit",
                    featuresNames, ret);
            featuresNames.push_back("Exactly1Lit steps");
            ret.push_back(exoSteps);
            fullAndGraph.getDegreeStatistics().infoToVector(
                "Full AND gate degree", featuresNames, ret);
            fullAndGraph.getWeightStatistics().infoToVector(
                "Full AND gate weights", featuresNames, ret);
            featuresNames.push_back("Full AND gate steps");
            ret.push_back(fandSteps);
            blockedAndGraph.getDegreeStatistics().infoToVector(
                "Blocked AND gate degree", featuresNames, ret);
            blockedAndGraph.getWeightStatistics().infoToVector(
                "Blocked AND gate weights", featuresNames, ret);
            featuresNames.push_back("Blocked AND gate steps");
            ret.push_back(bandSteps);
            if (dumpingPlots) {
                exactlyOneLiterals.getDegreeStatistics().toStream(plotsFileName,
                        ".ex1l-deg");
                fullAndGraph.getDegreeStatistics().toStream(plotsFileName,
                        ".fand-deg");
                fullAndGraph.getWeightStatistics().toStream(plotsFileName,
                        ".fand-wgt");
                blockedAndGraph.getDegreeStatistics().toStream(plotsFileName,
                        ".band-deg");
                blockedAndGraph.getWeightStatistics().toStream(plotsFileName,
                        ".band-wgt");
            }
            time1 = (cpuTime() - time1);
            timeIndexes.push_back(ret.size());
            featuresNames.push_back("Constraints recognition time");
            ret.push_back(time1);
        }

        if (computeXor) {
            extractXorFeatures(litToClsMap, ret);
        }
    }
}

// calculate the number that is represented by the polarity of the given clause for an XOR
static uint64_t numberByPolarity(const Clause& clause)
{
    uint64_t nr = 0;
    for (uint32_t i = 0 ; i < clause.size(); ++ i) {
        const Lit literal = clause[i];
        nr = nr << 1;
        if (!sign(literal)) { nr += 1; }
    }
    return nr;
}

void CNFClassifier::extractXorFeatures(const vector<vector<CRef> >& litToClsMap, vector<double>& ret)
{
    // parameters to set
    const uint32_t maxXorSize = 7;
    const bool findSubsumed = false;  // too expensive to compute features

    // algorithm
    double xorTime = cpuTime() - 0;

    // sort clauses according to their size!
    int32_t n = clauses.size();
    int32_t m, s;
    // copy elements from vector
    CRef* tmpA = new CRef[ n ];
    CRef* a = tmpA;
    for (int32_t i = 0 ; i < n; i++) {
        a[i] = clauses[i];
    }
    CRef *tmpB = new CRef[n];
    CRef *b = tmpB;
    // size of work fields, power of 2
    for (s = 1; s < n; s += s) {
        m = n;
        do {
            m = m - 2 * s; // set begin of working field
            int32_t hi = (m + s > 0) ? m + s : 0; // set middle of working field

            int32_t i = (m > 0) ? m : 0;  // lowest position in field
            int32_t j = hi;

            int32_t stopb = m + 2 * s; // upper bound of current work area
            int32_t currentb = i;         // current position in field for copy

            // merge two sorted fields into one
            while (i < hi && j < stopb) {
                if ((ca[a[i]]) < (ca[a[j]])) {
                    b[currentb++] = a[i++];
                } else {
                    b[currentb++] = a[j++];
                }
            }
            // copy rest of the elements
            for (; i < hi;) {
                b[currentb++] = a[i++];
            }

            for (; j < stopb;) {
                b[currentb++] = a[j++];
            }

        } while (m > 0);

        // swap fields!
        CRef* tmp = a; a = b; b = tmp;
    }
    // write data back into vector
    for (int32_t i = 0 ; i < n; i++) { clauses[i] = a[i]; }

    delete [] tmpA;
    delete [] tmpB;
    // finished sorting

    uint32_t cL = 2;  // current length (below 5 there cannot be reduced anything)
    uint32_t cP = 0;  // current moveTo position
    uint32_t participatingXorClauses = 0;
    uint64_t findChecks = 0;
    uint32_t subsFound = 0, xors = 0;

    vector<CRef> clss, xorList;   // temporary clauses vector
    MarkArray markArray;
    markArray.create(nVars);
    const vec<CRef>& table = clauses; // another name, same vector#
    while (cP < table.size() && ca[ table[cP] ] .size() < cL) { cP ++; }   // find correct position

    vector<char> foundByIndex; // no need to do this on the stack!
    // handle only xor between 3 and 63 literals!!
    while (cP <  table.size() && cL < maxXorSize) {  // max xor size
        uint32_t start = cP;
        cL = ca[ table[cP] ] .size();
        while (cP < table.size() && ca[ table[cP] ] .size() == cL) { cP ++; }
        // check for each of the clauses, whether it and its successors could be an xor
        for (; start < cP - 1; ++start) {
            const CRef c = table[start];
            const Clause& cl = ca[c];
            if (cl.size() > maxXorSize) { break; }   // interrupt before too large size is recognized
            uint32_t stop = start + 1;
            // find last clause that contains the same variables as the first clause
            for (; stop < cP; ++stop) {
                const CRef c2 = table[stop];
                const Clause& cl2 = ca[c2];
                uint32_t k = 0;
                for (; k < cl2.size(); k ++) {
                    if (var(cl[k]) != var(cl2[k])) { break; }
                }
                if (k != cl2.size()) { break; }
            }
            uint64_t shift = 1;
            shift = shift << (cl.size() - 1);
            // check whether the first clause is odd or even, and go with this value first. Check the other value afterwards
            uint32_t count[2]; count[0] = 0; count[1] = 0;
            clss.clear(); // separate between the two clauses

            // check all clauses with the same variables
            for (uint32_t j = start ; j < stop; ++j) {
                const CRef c2 = table[j];
                const Clause& cl2 = ca[c2];
                uint32_t o = 0;
                for (uint32_t k = 0; k < cl2.size(); k ++)
                    if (!sign(cl2[k])) { o = o ^ 1; }     // count literals that are set to true!
                if (o == 1) {
                    clss.push_back(table[j]);
                    clss[ clss.size() - 1 ] = clss[ count[0] ];
                    clss[count[0]] = table[j];
                    count[0] ++;
                } else {
                    count[1] ++;
                    clss.push_back(table[j]);
                }
            }

            // try to recover odd and even XORs separately, if the limit can be reached
            for (uint32_t o = 0 ; o < 2; ++o) {
                uint32_t offset = o == 1 ? 0 : count[0];
                // check, whether there are clauses that are subsumed
                foundByIndex.assign(shift, 0);
                uint32_t foundCount = 0;
                // assign the clauses to the numbers they represent
                for (uint32_t j = 0 ; j < count[1 - o]; ++j) {
                    Clause& cl2 = ca[clss[j + offset]];
                    const uint32_t nrByPol = numberByPolarity(cl2);
                    const uint32_t index = nrByPol / 2;
                    foundCount = foundByIndex[ index ] == 0 ? foundCount + 1 : foundCount;
                    foundByIndex[ index ] = 1;
		    participatingXorClauses ++; // clause is covered
                    findChecks ++;
                }

                if (foundCount == 0) { continue; }
                // if( offset >= clss.size() ) continue;

                // try to find clauses, that subsume parts of the XOR
                if (foundCount < shift && findSubsumed) {
                    const Clause& firstClause = ca[ clss[offset] ];
                    assert(cL == firstClause.size() && "current clause needs to have the same size as the current candidate block");
                    Var variables [firstClause.size()]; // will be sorted!
                    markArray.nextStep();
                    for (uint32_t j = 0 ; j < cL; ++j) {
                        variables[ j ] = var(firstClause[j]);
                        markArray.setCurrentStep(variables[j]);
                    }

                    // check all smaller clauses (with limit!) whether they contain all the variables, if so, use them only, if the selected variable is the smallest one!
                    for (uint32_t j = 0 ; j < cL ; ++ j) {
                        Var cv = variables[j];
                        for (uint32_t p = 0 ; p < 2; ++ p) {   // polarity!
                            const Lit current = mkLit(cv, p != 0);   // 0 -> POS, 1 -> NEG
                            const vector<CRef>& cls = litToClsMap[ toInt(current) ]; // clause list of literal that is currently checked!
                            for (uint32_t k = 0 ; k < cls.size(); ++ k) {
                                const Clause& cclause = ca[cls[k]];
                                if (cls[k] == clss[offset]) { continue; }   // do not consider the same clause twice!
                                if (cclause.can_be_deleted()) { continue; }
                                if (cclause.size() > maxXorSize
                                        || (! false && cclause.size() >= cL)
                                        || (false && cclause.size() > cL)  // due to resolution, also bigger clauses are allowed!
                                   ) { continue; } // for performance, and it does not make sense to check clauses that are bigger than the current ones

                                Lit resolveLit = lit_Undef, foundLit = lit_Error; // if only one variable is wrong, the other literal could be resovled in?
                                for (uint32_t m = 0 ; m < cclause.size(); ++ m) {
                                    const Var ccv = var(cclause[m]);
                                    if (ccv < cv) {
                                        resolveLit = lit_Error; break;
                                    } // reject clause, if the current variable is not the smallest variable in the matching
                                    if (! markArray.isCurrentStep(ccv)) {
                                        resolveLit = lit_Error;
                                        break;
                                    }
                                }
                                if (resolveLit == lit_Error) { continue; }   // next clause!
                                int subsumeSize = (resolveLit == lit_Undef || foundLit != lit_Undef) ? cclause.size() : cclause.size() - 1;
                                if (subsumeSize == firstClause.size()) {
                                    int o0 = 0;
                                    for (int m = 0 ; m < cclause.size(); ++ m) {
                                        const Lit ccl = cclause[m] != resolveLit ? cclause[m] : foundLit; // use the resolvedIn literal, instead of the literal that is resolved
                                        assert(ccl != lit_Undef && "neither a clause literal, nor foundLit can be undefined!") ;
                                        if (!sign(ccl)) { o0 = (o0 ^ 1); }
                                    }
                                    if (o0 != o) { continue; }   // can consider only smaller clauses, or their odd value is the same as for for the first clause!
                                }

                                // generate all indexes it would subsume
                                uint32_t myNumber = 0;
                                uint32_t myMatch = 0;
                                bool subsumeOriginalClause = true; // if big clause is subsumed, working with this XOR is buggy!
                                for (uint32_t m = 0 ; m < cclause.size(); ++ m) {
                                    assert(cclause[m] != lit_Undef && "there should not be a lit_Undef inside a clause!");
                                    const Lit ccl = cclause[m] != resolveLit ? cclause[m] : foundLit; // use the resolvedIn literal, instead of the literal that is resolved

                                    if (ccl == lit_Undef) { continue; }   // nothing to do here (do not use same literal twice!)
                                    const Var ccv = var(ccl);
                                    bool hitVariable = false;
                                    for (uint32_t mv = 0 ; mv < cL; ++ mv) {
                                        if (variables[mv] == ccv) {
                                            hitVariable = true;
                                            if (ccl != firstClause[mv]) { subsumeOriginalClause = false; }
                                            myNumber = !sign(ccl) ? myNumber + (1 << (cL - mv - 1)) : myNumber;
                                            myMatch = myMatch + (1 << (cL - mv - 1));
                                            break;
                                        }
                                    }
                                    assert(hitVariable && "variable has to be in XOR!");
                                }
                                if (subsumeOriginalClause) {   // interrupt working on this XOR!
                                    // skip this xor? no, can still lead to improved reasoning!
                                }
                                assert(cclause.size() > 1 && "do not consider unit clauses here!");
                                assert(cL >= cclause.size() && "subsume clauses cannot be greater than the current XOR");
                                for (uint32_t match = 0 ; match < 2 * shift; ++match) {

                                    if (foundByIndex[ match / 2 ] == 1) { 
				      participatingXorClauses ++; // this clause is covered with this XOR!
				      continue; 
				    }

                                    if ((match & myMatch) == myNumber) {

                                        // reject matches that have the wrong polarity!
                                        uint32_t o1 = 0; // match has to be odd, as initial clause! and has to contain bits of myNumber!
                                        uint32_t matchBits = match;
                                        for (uint32_t km = 0; km < cL; km ++) {
                                            if ((matchBits & 1) == 1) {
                                                o1 = (o1 ^ 1);
                                            }
                                            matchBits = matchBits >> 1;
                                        }
                                        // wrong polarity -> reject
                                        if (o1 != o) { continue; }

                                        if (foundByIndex[ match / 2 ] == 0) { foundCount++; }   // only if this clause sets the value from 0 to 1!
                                        subsFound = resolveLit == lit_Undef ? subsFound + 1 : subsFound; // stats
                                        foundByIndex[ match / 2 ] = 1;
					participatingXorClauses ++; // this clause is covered with this XOR!
                                        if (foundCount == shift) { goto XorFoundAll; }
                                    }
                                }
                                if (foundCount == shift) { goto XorFoundAll; }
                            }

                        }
                    }
                }
            XorFoundAll:;
                // if the whole XOR is entailed by the formula, add the XOR to the recognized XORs
                if (foundCount == shift) {
                    xorList.push_back(clss[offset]);
                    xors ++;
                }
            }
            start = stop - 1;
        }
        // continue with clauses of a larger size!
        cL ++;
    }

    xorTime = cpuTime() - xorTime;
    if (verb > 0) {
        cerr << "c found " << xorList.size() << " xors (" << subsFound << " sub) in " << findChecks << " steps and " << xorTime << " s" << endl;
    }
    uint64_t xorSteps = 0; // TODO norbert here you should initiallize the variable with the number
    // of steps that took you to build the xorList

    // here I add cliques between the literals of the clauses in the xorList
    int nLiterals = nVars * 2;
    Graph xorGraph(nLiterals, computingDerivative);
    for (int i = 0; i < xorList.size(); ++i) {
        Clause& clause = ca[xorList[i]];
        int k = clause.size();
        double w = pow(2, -k);
        xorSteps += (k * (k - 1) / 2);

        for (int j = 0; j < k - 1; ++j) {
            for (int h = j + 1; h < k; ++h) {
                xorGraph.addUndirectedEdge(clause[j].x, clause[h].x, w);
            }
        }
    }

    // Print the features
    xorSteps += xorGraph.computeOnlyStatistics(quantilesCount);

    xorGraph.getDegreeStatistics().infoToVector("XOR gate degree",
            featuresNames, ret);
    xorGraph.getWeightStatistics().infoToVector("XOR gate weights",
            featuresNames, ret);
    
    const double clauseRatio = clauses.size() == 0 ? 1 : (double)participatingXorClauses / (double)(clauses.size());
    
    featuresNames.push_back("XOR used clauses");
    ret.push_back( participatingXorClauses );
    
    featuresNames.push_back("XOR used clauses ratio");
    ret.push_back( clauseRatio );
    
    featuresNames.push_back("XOR gate steps");
    ret.push_back(xorSteps);

    timeIndexes.push_back(ret.size());
    featuresNames.push_back("xor time");
    ret.push_back(xorTime);
}


void CNFClassifier::recursiveWeightHeuristic_code(const int maxClauseSize, vector<double>& ret)
{
    assert(maxClauseSize > 0 && "compute clauses degree before");
    double time = cpuTime();

    // iteration 0 - all h's are 1, muh is 1/(2 nVars) + \sum_nVars (h(v) + h(-v))
    double muh = 1;
    const double gamma = 5;
    const double maxDouble = 10e200;
    vector<double> thisData, lastData; // current value for each literal
    thisData.assign(2 * nVars, 0);
    lastData.assign(2 * nVars, 1);

    uint64_t globalSteps = 0;
    if (verb > 0) {
        cerr << "c calculate RWH with cls max size " << maxClauseSize << " and gamma " << gamma << endl;
    }

    for (int iter = 1; iter <= 3; iter ++) {
        double iterationTime = cpuTime();
        SequenceStatistics thisIterationSequence(computingDerivative);
        uint64_t iterationSteps = 0;

        for (int i = 0 ; i < clauses.size(); ++ i) {
            const Clause& c = ca[clauses[i]];
            if (c.size() == 1) { continue; }   // TODO: check: undefined behavior for |c| == 1 ?

            const double exponent = maxClauseSize < c.size() ? 0 : maxClauseSize - c.size(); // check again overflows!
            const double clauseConstant = pow(gamma, exponent) / pow(muh , c.size() - 1);

            // calculate the constant for the clause
            bool foundZero = false;
            double clauseValue = 1;
            for (int j = 0; j < c.size(); ++ j) {
                if ((lastData[ toInt(~c[j]) ] == 0)) {      // check whether a 0 has been found
                    foundZero = true; break;
                }
                clauseValue *= lastData[ toInt(~c[j]) ];
                iterationSteps ++;
            }

            if (! foundZero) {   // only if there is no non-zero literal inside, add the values!
                clauseValue *= clauseConstant;
                // for each literal, divide the clause value by the value for the corresponding complement to fit the calculation formula!
                for (int j = 0; j < c.size(); ++ j) {
                    thisData[ toInt(c[j]) ] += clauseValue / lastData[  toInt(~c[j]) ];
                }
            }
        }

        // this is the sequence for iteration "i"
        muh = 0;
        for (Var v = 0 ; v < nVars; ++ v) {
            for (int p = 0 ; p < 2; ++ p) {
                // assert( thisData[toInt( mkLit(v, p==0) ) ] != 0 && "value of literal cannot be 0" );
                thisIterationSequence.addValue(thisData[ toInt(mkLit(v, p == 0)) ] < maxDouble ? thisData[ toInt(mkLit(v, p == 0)) ] : maxDouble);       // add the value for each literal!
                muh += thisData[ toInt(mkLit(v, p == 0)) ];   // update the data for muh
            }
            iterationSteps ++;
        }
        iterationSteps += thisIterationSequence.compute(quantilesCount); // statistics with quantiles
        muh /= (2 * nVars);  // update muh

        if (muh <= 1) { muh = 1; }   // take care of small values!

        // add the features wrt number of the iteration // TODO: norbert: please check whether I do this right here!
        std::stringstream iterationFeatureName;
        iterationFeatureName << "RWH-" << iter;
        thisIterationSequence.infoToVector(iterationFeatureName.str(), featuresNames, ret);
        iterationTime = cpuTime() - iterationTime;
        // also handle iteration steps
//    thisIterationSequence.infoToVector(iterationFeatureName.str(), featuresNames, ret);
        featuresNames.push_back(iterationFeatureName.str() + "steps");
        ret.push_back(iterationSteps);   // TODO: this number is the same for each iteration -> have it only once?

        // after the iteration, swap the data!
        thisData.swap(lastData);
        thisData.assign(0, 2 * nVars);   // reset all values to 0! ...

        // FIXME TODO: handle steps here!
//     if( iterationSteps > localStepLimit ) break;
        globalSteps += iterationSteps;
//     if( globalSteps > globalStepLimit ) break;
    }

    time = cpuTime() - time;
    if (verb > 0) {
        cerr << "c computing all RWH features took " << time << " cpu seconds and " << globalSteps << " global steps"  << endl;
    }
}

void CNFClassifier::symmetrycode(vector<double>& ret)
{
    double time1 = cpuTime();
    // code for calculating symmetry features
    // I will add a method here, that produces more values
    // The data will not be a graph, but something like your initial sequencestatistics implementation.
    // The values represent how much symmetry there is inside the formula structure, by performing some appoximation
    // TODO I will try to implement this code next week.
    // how does it work: each clause gets an numeric "hash", that corresponds to the variables in that clause (for now, I do not use literals and polarities)
    // for each time step, the hash of all clauses of a variable are cumulated for that variable
    // then, based on these "new" variable hashes, the hashes for the clauses is generated again, so that new variable hashes can be build
    // based on the number of different hashes, and the number of times a certain hash occurs over all variables, conclusions on the symmetry of the problem can be drawn
    if (true) {
        time1 = cpuTime();
        uint64_t globalSymmSteps = 0;
        // TODO: have another parameter here!
        vector<double> data1(nVars, 1), data2(nVars, 1); // these field store the data of the current time-step, and the previous time step...
        vector<double> *thisData = &data1, *oldData = &data2; // these pointers are actually used (swapping time data is faster)
        vec<double> finalBins;
        SequenceStatistics seq0(computingDerivative), seq1(computingDerivative), seq2(computingDerivative);
        for (int timeStep = 0; timeStep < 3; ++timeStep) { // could have a parameter here, lets do it for 3 steps for now ...
            uint64_t iterationSteps = 0;
            // clear has for this time step
            thisData->assign(0, nVars); // set each variable hash to 0
            // go over all clauses
            for (int i = 0; i < clauses.size(); ++i) {
                double clsHash = 0; // calculate hash
                // TODO as soon as the first clsHash becomes INF, the loop should be stopped, because very soon all clause hashes will become INF! thus, limit the number of timesteps, or at least normalize them once in a while!
                const Clause& c = ca[clauses[i]];
                for (int j = 0; j < c.size(); ++j) {
                    clsHash += (*oldData)[var(c[j])];    // cumulate current hashes of variables
                }
                clsHash = clsHash / pow(2, c.size()); // 'normalize' hash for the current clause
                for (int j = 0; j < c.size(); ++j) {
                    (*thisData)[var(c[j])] += clsHash;    // update this timestamp with the current cls hash!
                }
                iterationSteps += c.size();
            }
            // here, the pointer thisData stores that hashes of the current step for each variable
            // TODO: analyze the data in thisData
            //   - copy in another vector, sort it
            //   - iterate over it once and store number of different hash values, and number of occurrence per value (bin)
            //   - with the number of bins, and the size of the bins, stats can be created (the actual distribution does not make sense, I think)
            //   - maybe, since we are calculating this for three steps, the differences between two time-steps might be interesting?

            finalBins.clear();
            for (Var v = 0; v < nVars; ++v) {
                finalBins.push((*thisData)[v]);
            }
            iterationSteps += finalBins.size();
            sort(finalBins);
            SequenceStatistics& thisTimeStat = (
                                                   timeStep == 0 ? seq0 : (timeStep == 1 ? seq1 : seq2)); // choose the right time step
            int cmpPos = 0; // position to compare to
            for (int i = 1; i < finalBins.size(); ++i) {
                if (finalBins[i] != finalBins[cmpPos]) {
                    thisTimeStat.addValue(i - cmpPos); // add the diff from cmp and the current position
                    cmpPos = i;
                    iterationSteps ++;
                }
            }
            thisTimeStat.addValue(finalBins.size() - cmpPos); // dont forget the final group!
            iterationSteps += thisTimeStat.compute(quantilesCount); // statistics with quantiles

            if (timeStep == 0) {
                thisTimeStat.infoToVector("SymmTime0", featuresNames, ret);
                featuresNames.push_back("Symmetry0 steps");
                ret.push_back(iterationSteps);
            } else if (timeStep == 1) {
                thisTimeStat.infoToVector("SymmTime1", featuresNames, ret);
                featuresNames.push_back("Symmetry1 steps");
                ret.push_back(iterationSteps);
            } else if (timeStep == 2) {
                thisTimeStat.infoToVector("SymmTime2", featuresNames, ret);
                featuresNames.push_back("Symmetry2 steps");
                ret.push_back(iterationSteps);
            }
            globalSymmSteps += iterationSteps;
            if (dumpingPlots) {
                if (timeStep == 0) {
                    thisTimeStat.toStream(plotsFileName, ".symm-time0");
                } else if (timeStep == 1) {
                    thisTimeStat.toStream(plotsFileName, ".symm-time1");
                } else if (timeStep == 2) {
                    thisTimeStat.toStream(plotsFileName, ".symm-time2");
                } else {
                    assert(false && "forgot to implement!");
                }
            }

            // debug output: show has for each variable:
            //

            // necessary for the algorithm again:
            // swap data pointers, so that current data becomes old data in the next step
            vector<double> *tmpData = thisData;
            thisData = oldData;
            oldData = tmpData;
            // thats it for this time step - continue with the next one
        }
        // thats it for symmetry calculation
        time1 = (cpuTime() - time1);
        timeIndexes.push_back(ret.size());
        featuresNames.push_back("Symmetry computation time");
        ret.push_back(time1);
        featuresNames.push_back("Symmetry computation steps");
        ret.push_back(globalSymmSteps);
        time1 = cpuTime();
    }

}

std::vector<double> CNFClassifier::extractFeatures(vector<double>& ret)
{
    uint64_t operations = 0; // number of operations to compute features;


    // TODO should return the vector of features. If possible, the features should range between 0 and 1 - all the graph features could be scaled down by number of variables, number of clauses or some other measure

    // some useful code snippets:

    // measure time in seconds
    double time1 = cpuTime(); // start timer
    // do some work here to measure the time for
    // TODO improve weights computation....

    // graphs for feature calculation
    BipartiteGraph clausesVariablesP(clauses.size(), nVars, computingDerivative); // FIXME: norbert: you might not need all graphs at the same time. Since they consume memory, it might be good to destroy them, as soon as you do not need them any more, and to set them up as late as possible!
    BipartiteGraph clausesVariablesN(clauses.size(), nVars, computingDerivative); // FIXME: norbert: you might not need all graphs at the same time. Since they consume memory, it might be good to destroy them, as soon as you do not need them any more, and to set them up as late as possible!

    operations += buildClausesAndVariablesGrapths(clausesVariablesP, clausesVariablesN, ret);

    fband(ret);

    symmetrycode(ret);

    // I will implement another feature here, the so called RWH, which is the "recursive weight heuristic", which kind of measures how much propagation a variable can do
    // depending on how well this can be implemented (have to check the method again), I might integrate it into the above loop, because its also iterating over the formula several times
    // TODO: I try to add this code next

    const int maxClauseSize = this->maxClauseSize; // TODO norbert: enrique, please return the maximum clause size from one of the above methods here to pass it to the next method!
    if (computingRWH) {
        recursiveWeightHeuristic_code(5, ret);    // cut off max size
    }

    // norbert ... end constraint calculation


    operations += buildResolutionAndClausesGrapths(clausesVariablesP, clausesVariablesN, ret);


    time1 = (cpuTime() - time1);
    timeIndexes.push_back(ret.size());
    featuresNames.push_back("features computation time");
    ret.push_back(time1);
    setCpuTime(time1);

    if (attrFileName != nullptr) {
        std::ofstream fnout;
        fnout.open(attrFileName, ios::out);
        fnout << "@relation cnf_features" << endl;
        fnout << "@attribute filename string" << endl;
        for (int i = 0; i < featuresNames.size(); ++i) {
            fnout << "@attribute " << replaceStr(featuresNames[i], ' ', '_') << " real" << endl;
        }
        fnout.close();
    }
    return ret;
}



static bool isInfinite(const double pV)
{
    return std::isinf(fabs(pV));
}

std::vector<double> CNFClassifier::outputFeatures(const char* formulaName)
{

    ostream* output = &cout;
    std::ofstream fout;
    if (outputFileName != nullptr) {
        fout.open(outputFileName, fstream::app | fstream::out);
        if (!fout) { cerr << "failed to open data file" << endl; }
        output = &fout;
    }
    vector<double> features;
    extractFeatures(features);
    if (precision > 0) {
        (*output).setf(ios::fixed);
        (*output).precision(precision);
    }
    *output << "\'" << formulaName << "\'"; // print the formula name
    for (int i = 0; i < features.size(); ++i) {
        stringstream s;
        if (isInfinite(features[i])) { s << ",?"; }     // weka cannot handle "inf"
        else { s << "," << features[i]; }
        if (s.str().find("inf") != string::npos) { cerr << "c there are infinite features (index " << i << " -- " << features[i] << " isInf: " << isInfinite(features[i]) <<  ")" << endl; }
        *output << s.str();
    }
    *output << endl;

    if (outputFileName != nullptr) {
        fout.close();
    }
    return features;
}
