/******************************************************************************************[hbr.cc]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor/techniques/HBR.h"

using namespace Riss;
using namespace std;

namespace Coprocessor
{

HyperBinaryResolution::HyperBinaryResolution(CP3Config& _config, ClauseAllocator& _ca,
        ThreadController& _controller, CoprocessorData& _data,
        Coprocessor::Propagation& _propagation)
    : Technique(_config, _ca, _controller), data(_data), propagation(_propagation)
    , hbrSteps(0)
    , foundMultiHBR(0)
    , foundHBR(0)
{

}


void HyperBinaryResolution::reset()
{

}

/** run blocked clause elimination, and covered literal elimination */
bool HyperBinaryResolution::hyperBinaryResolution()
{

    bool addedClauses = false, addedSomeClause = false;

    // collect all clauses that are not binary (larger clauses might shrink once we found additional units!)
    data.clss.clear(); // clear list of clauses
    for (int i = 0 ; i < data.getClauses().size(); ++i) {
        const Clause& c = ca[ data.getClauses()[i] ];
        if (c.size() >= 3 && !c.can_be_deleted()) { data.clss.push_back(data.getClauses()[i]); }
    }

    // mark all literals
    data.ma.create(2 * data.nVars());

    nextRound.create(2 * data.nVars());
    data.lits.clear();
    for (int i = 0; i < 2 * data.nVars(); ++ i) { nextRound.setCurrentStep(i); data.lits.push_back(toLit(i)); }
    MarkArray useThisRound; // memorize all literals that have already been tested as candidates for the current clause
    useThisRound.create(2 * data.nVars());

    int iteration = 0;
    do {
        iteration ++;
        addedClauses = false;
        toBeAdded.clear();

        // initialize literals for this iteration
        data.ma.nextStep();
        for (int i = 0 ; i < data.lits.size(); ++ i) {
            data.ma.setCurrentStep(toInt(data.lits[i]));
        }
        data.lits.clear();
        nextRound.nextStep();


        // add new clauses into BIG
        big.recreate(ca, data.nVars(), data.getClauses(), data.getLEarnts());
        big.generateImplied(data);
        hbrSteps += data.getClauses().size() + data.getLEarnts().size();


        DOUT(if (config.opt_hbr_debug) {
        cerr << "c full formula: " << endl;
        for (int i = 0 ; i < data.getClauses().size(); ++ i) {
                cerr << "c " << ca[ data.getClauses()[i] ] << endl;
            }
        }
            );

        // loop over clauses and check for HBRs, stay in steps, stop if interrupted
        int keptClauses = 0;
        for (int i = 0; i < data.clss.size() && (data.unlimited() || config.hbrLimit > hbrSteps) && !data.isInterupted(); ++i) {
            const Clause& c = ca[ data.clss[i] ];
            hbrSteps ++;
            if (c.size() < 3 || c.can_be_deleted()) { continue; }    // delete this pointer from the current version of the clss vector, if the clause is binary now, of can be deleted
            data.clss [keptClauses ++ ] = data.clss[i];              // move the current pointer forward, if there is a gap
            if (c.size() > config.opt_hbr_maxCsize) { continue; }

            assert(c.size() > 2 && "there cannot be binary or unit clauses in the list");
            DOUT(if (config.opt_hbr_debug) cerr << "c HBR work on clause: [ " << data.clss[i] << " ]: " << c << endl;);

            // use next clause, if not all literals in this clause are marked
            bool skipClause = false;
            bool oneMissing = false;
            for (int j = 0 ; j < c.size(); ++ j) {
                hbrSteps ++;
                if (!data.ma.isCurrentStep(toInt(c[j]))) {
                    if (! oneMissing) { oneMissing = true; }
                    else { skipClause = true; break; }
                }
            }
            if (skipClause) {
                DOUT(if (config.opt_hbr_debug) cerr << "c HBR skip clause" << endl;);
                continue;
            }

            useThisRound.nextStep();

            // find out which two literals should be scanned (the two with the fewest binary clauses)
            assert(c.size() > 2 && "apply HBR only to clauses larger than 2 literals");
            int pos [2]; // the two literals to scan, the ones with the fewest binary clauses (no need to scan more literals!)
            pos[0] = big.getSize(c[0]) <= big.getSize(c[1]) ? 0 : 1;       //
            pos[1] = big.getSize(c[0]) >  big.getSize(c[1]) ?  0 : 1;
            assert(pos[0] + pos[1] == 1 && "assign each number exactly once");
            for (int j = 2 ; j < c.size(); ++ j) {
                hbrSteps ++;
                const int& s = big.getSize(c[j]);
                if (s < big.getSize(c[pos[0]])) {     // found new smallest value
                    pos[1] = pos[0]; pos[0] = j;
                } else if (s < big.getSize(c[pos[1]])) {     // found new second smallest value
                    pos[1] = j;
                }
            }
            DOUT(if (config.opt_hbr_debug) {
            cerr << "c HBR selected lits to scan binaries: " << c[pos[0]] << " and " << c[pos[1]] << endl;
                for (int j = 0 ; j < c.size(); ++ j) {
                    const int& s = big.getSize(c[j]);
                    cerr << "c    " << c[j] << " -> " << s << " binaries" << endl;
                }
            }
                );

            // actual scan for binary clauses
            for (int j = 0 ; j < 2; ++ j) {

                const Lit x = c[ pos[j] ]; // current literal to iterate over binary clauses
                const Lit* xList = big.getArray(x);   // list of implied literals x->a ... ( ~x \lor a)
                const int xSize = big.getSize(x);

                // loop over all binary clauses that contain the literal x
                for (int k = 0 ; k < xSize; ++ k) {
                    const Lit l = xList[k]; // candidate literal for lhbr clause
                    if (useThisRound.isCurrentStep(toInt(l))) { continue; }  // already tested for this clause?
                    useThisRound.setCurrentStep(toInt(l));               // memorize for this clause
                    DOUT(if (config.opt_hbr_debug) cerr << "c HBR test literal " << l << " from " << x << endl;);

                    hbrSteps ++;

                    Lit otherLit = lit_Undef; // literal that will be the other partner in the generated binary clause
                    for (int m = 0 ; m < c.size(); ++ m) {
                        if (m == pos[j]) { continue ; }  // do not process same literal with itself!
                        const Lit lm = c[m];
                        if (var(lm) == var(l)) {  // will not find tautologic binary clause, or binary clause with duplicate literals
                            DOUT(if (config.opt_hbr_debug) cerr << "c HBR literal " << c[m] << " and " << l << " are the same variable - abort scan for " << l << endl;);
                            otherLit = lit_Error; break;
                        }
                        if (!big.implies(lm, l) && !big.isChild(lm, l)) { // check whether the clause (l \lor c[m]) is present in the formula
                            DOUT(if (config.opt_hbr_debug) cerr << "c HBR literal " << c[m] << " does not match " << l << " in a binary clause (" << ~c[m] << " or " << l << ")" << endl;);
                            // the clause (l \lor c[m]) does not exist in the formula, and is not implied with the current stochastic data
                            if (otherLit == lit_Undef) { otherLit = c[m]; }  // this literal is not present as binary clause, hence it might be left after HBR
                            else { otherLit = lit_Error; break; }        // there are multiple literals that fail, hence the hyper resolvent will not be binary
                        } else {
                            DOUT(if (config.opt_hbr_debug) cerr << "c HBR hit (" << ~c[m] << " or " << l << ") with imply: " << big.implies(lm, l) << " child: " << big.isChild(lm, l) << endl;);
                        }
                    }

                    if (otherLit != lit_Error) {  // we found another binary HBR resolvent
                        if (otherLit == lit_Undef) {  // HBR works for all literals of the clause
                            DOUT(if (config.opt_hbr_debug) cerr << "c HBR multi HBR by " << l << " from " << x << " -> add unit " << l << endl;);
                            foundMultiHBR ++;
                            // in case of multiLHBR, e.g. (1,2,3),(-1,l),(-2,l),(-3,l), the literal l is implied
                            data.enqueue(l);
                            data.addCommentToProof("implied unit as result of multi-HBR"); // tell unsat proof about new clause
                            data.addUnitToProof(l);                        // add clause to proof
                        } else { // we found one candidate where it works

                            // limit adding the binary clause based on the given parameter
                            if (config.opt_hbr_addBinaries == 0 || (iteration == 1 && config.opt_hbr_addBinaries)) {
                                DOUT(if (config.opt_hbr_debug) cerr << "c HBR HBR by " << l << " from " << x << endl;);
                                addHBR(l, otherLit);
                                hbrSteps++;
                            }
                        }
                    }
                }

            }

        }

        // close the gap in the vector
        data.clss.resize(keptClauses);

        // we might have found a unit via multiHBR - check!
        propagation.process(data, true); // propagate, if there's something to be propagated
        addedSomeClause = addedSomeClause || propagation.appliedSomething();
        if (!data.ok()) { return addedSomeClause; }

        if (toBeAdded.size() > 0) {
            DOUT(if (config.opt_hbr_debug) cerr << "c HBR process " << toBeAdded.size() << " temporarily added pairs" << endl;);
            // have we found some HBR clause?
            addedSomeClause = addedSomeClause || toBeAdded.size() > 0; // update whether some clause has been added at all
            addedClauses = addedClauses || toBeAdded.size() > 0; // update whether some clause has been added at all
            // sort pairs to be added to not add duplicate clauses multiple times
            sort(toBeAdded);

            // add new clauses, avoid duplicates
            LitPair lastPair = toBeAdded[0];
            Lit lits[2];
            for (int i = 0 ; i < toBeAdded.size(); ++ i) {
                hbrSteps ++;
                if (i == 0 || lastPair != toBeAdded[i]) {  // only add the clause if it does not match another clause in the list
                    lastPair = toBeAdded[i];
                    lits[0] = lastPair.getX();
                    lits[1] = lastPair.getY();
                    if (data.value(lits[0]) != l_Undef || data.value(lits[1]) != l_Undef) { continue; }      // propagation already set the values accordingly, no need to add the clause
                    CRef cr = ca.alloc(lits, 2, false); // allocate clause as part of the formula
                    DOUT(if (config.opt_hbr_debug) cerr << "c added clause by HBR: [" << cr << "]: " << ca[cr] << endl;);    // print created clause
                    data.addClause(cr);               // add to coprocessor data structures
                    data.getClauses().push(cr);
                    data.addCommentToProof("created during HBR"); // tell unsat proof about new clause
                    data.addToProof(ca[cr]);                      // add clause to proof
                    foundHBR ++; // count each clause that has been added
                }
            }
        }

    } while (addedClauses);


    // TODO: nextRound mark array might survive multiple calls with original variable names, but then also store the literals in the list data.lits!
    data.lits.clear();
    nextRound.destroy();

    return addedSomeClause;
}

bool HyperBinaryResolution::addHBR(const Lit& otherLit, const Lit& clauseLit)
{
    if (big.isChild(~otherLit, clauseLit)) { return false; }    // sufficient to scan one direction, as BIG is directed graph and each binary clause adds both edges
    toBeAdded.push_back(LitPair(otherLit, clauseLit));     // temporarily store pair
    DOUT(if (config.opt_hbr_debug) cerr << "c added temporary HBR pair: " << otherLit << ", " << clauseLit << endl;);
    if (!nextRound.isCurrentStep(toInt(otherLit))) {
        nextRound.setCurrentStep(toInt(otherLit));
        data.lits.push_back(otherLit);
    }
    if (!nextRound.isCurrentStep(toInt(clauseLit))) {
        nextRound.setCurrentStep(toInt(clauseLit));
        data.lits.push_back(clauseLit);
    }
    return true;
}


bool HyperBinaryResolution::process()
{
    MethodClock mc(hbrTime);

    if (!performSimplification()) { return false; } // do not do anything?!
    modifiedFormula = false;

    // do not simplify, if the formula is considered to be too large!
    if (!data.unlimited() && (data.nVars() > config.opt_hbr_vars &&
                              data.getClauses().size() + data.getLEarnts().size() > config.opt_hbr_cls &&
                              data.nTotLits() > config.opt_hbr_lits)) {
        return false;
    }

    // run UP first!
    DOUT(if (config.opt_hbr_debug) cerr << "c HBR run unit propagation" << endl;);
    propagation.process(data, true); // propagate, if there's something to be propagated
    modifiedFormula = modifiedFormula || propagation.appliedSomething();
    if (!data.ok()) { return modifiedFormula; }

    modifiedFormula = hyperBinaryResolution() || modifiedFormula;

    // run HBR here again to remove the new blocked clauses, if there have been any!
    return modifiedFormula;
}

void HyperBinaryResolution::printStatistics(ostream& stream)
{
    cerr << "c [STAT] HBR " << hbrTime.getCpuTime() << " seconds, " << hbrSteps << " steps, " << foundHBR << " addedBinaries, " << foundMultiHBR << " multiHBR-units, " << endl;
}

void HyperBinaryResolution::giveMoreSteps()
{
    hbrSteps = hbrSteps < config.opt_hbrInpStepInc ? 0 : hbrSteps - config.opt_hbrInpStepInc;
}

void HyperBinaryResolution::destroy()
{

}

}
