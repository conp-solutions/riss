/*********************************************************************************[Experimental.cc]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "Experimental.h"

using namespace Riss;
using namespace std;

namespace Coprocessor
{

ExperimentalTechniques::ExperimentalTechniques(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver)
    : Technique(_config, _ca, _controller)
    , data(_data)
    , solver(_solver)
    , processTime(0)
    , subsumed(0)
    , removedClauses(0)
    , extraSubs(0)
{

}

void ExperimentalTechniques::reset()
{

}

bool ExperimentalTechniques::process()
{
    DOUT(if (config.entailed_debug > 0) cerr << "c run ENT process" << endl;);
    MethodTimer mt(&processTime);

    if (! performSimplification()) { return false; }   // do not do anything?!
    modifiedFormula = false;

    data.ma.resize(2 * data.nVars());
    data.lits.clear();
    data.clss.clear();

    reSetupSolver();

    vector<int> iterationPures(21);
    vector<int> iterationVanish(21);
    int failedLiteral = 0;

    int totalRemoves = 0;

    for (int i = 0 ; i < data.getClauses().size() && !data.isInterupted(); ++ i) {

        const CRef cr = data.getClauses()[i];
        Clause& c = ca[ cr ];
        if (c.can_be_deleted() || c.size() != 2) { continue; }   // clause does not fulfill criteria, here we want to delete binary clauses

        solver.cancelUntil(0);     // make sure we're back on level 0
        solver.detachClause(cr);
        solver.newDecisionLevel();

        for (int j = 0 ; j < c.size(); ++ j) {
            // work on implication x->y
            const Lit x = ~c[j];
            const Lit y = c[1 - j];


            solver.newDecisionLevel();
            if (solver.value(x) != l_Undef) { continue; }     // do not perform this check, clause is satisfied anyways

//       cerr << "c enqueue " << x << " from " << c << " on level " << solver.decisionLevel() << endl;

            solver.uncheckedEnqueue(x);   // add x, then propagate
            if (CRef_Undef == solver.propagate()) {
                failedLiteral ++;
                solver.cancelUntil(1);
                continue;
            }


            {
                bool yIsPure = false;
                // expensive pure check
                for (int iteration = 0 ; iteration < 20 && !data.isInterupted(); ++ iteration) {
                    data.ma.nextStep();
                    for (int k = 0 ; k < solver.clauses.size(); ++ k) {
                        if (solver.clauses[k] == cr) { continue; }
                        const Clause& d = ca[ solver.clauses[k] ];
                        if (d.can_be_deleted()) { continue; }   // ignore deleted clauses

                        // if clause is satisfied, skip it
                        bool dIsSat = false;
                        for (int l = 0 ; l < d.size(); ++ l) {
                            if (solver.value(d[l]) == l_True) {dIsSat = true; break;}
                        }
                        if (dIsSat) { continue; }

                        // else store occurrences of literals of d
                        for (int l = 0 ; l < d.size(); ++ l) {
                            if (solver.value(d[l]) == l_Undef) {
                                data.ma.setCurrentStep(toInt(d[l]));   // saw this literal in this iteration
                            }
                        }
                    }

                    // collect all pure literals (if there are none, break, if y is among them, also stop )
                    if (! data.ma.isCurrentStep(toInt(y)) &&  ! data.ma.isCurrentStep(toInt(~y)) && solver.value(y) != l_False) {
                        // y vanished completely
                        iterationVanish[ iteration ] ++;
                        c.set_delete(true);   // c can be removed
                        break;
                    } else if (data.ma.isCurrentStep(toInt(y)) &&  ! data.ma.isCurrentStep(toInt(~y))) {
                        // y became pure
                        iterationPures[ iteration ] ++;
                        c.set_delete(true);   // c can be removed
                        break;
                    }

                    // check each variable for being pure
                    bool foundNewPures = false;
                    for (Var v = 0; v <= solver.nVars(); ++ v) {
                        for (int p = 0 ; p < 2; ++ p) {
                            const Lit l = mkLit(v, p == 0);
                            if (data.ma.isCurrentStep(toInt(l)) &&  ! data.ma.isCurrentStep(toInt(~l))) {
                                assert(solver.value(l) == l_Undef && "variable cannot be assigned a truth value");
                                solver.uncheckedEnqueue(l);   // enqueue the pure variable
                                foundNewPures = true;
                            }
                        }
                    }
                    CRef ref = solver.propagate();
                    assert(ref == CRef_Undef && "propagating pure literals cannot result in a conflict");
                    if (!foundNewPures) { break; }   // stop here, because no new pure literals have been added
                }

            }

            // if clause could not be removed, add it back
            if (!c.can_be_deleted()) {
                solver.cancelUntil(0);     // make sure we're back on level 0
                solver.attachClause(cr);
            } else {
                totalRemoves ++;
            }
            solver.cancelUntil(1);
        }

    }

    cerr << "c [EXP] total-removes:  " << totalRemoves << endl;
    cerr << "c [EXP] pure-removes:   " << iterationPures  << endl;
    cerr << "c [EXP] vanish-removes: " << iterationVanish << endl;
    cerr << "c [EXP] failed: " << failedLiteral << endl;
    solver.cancelUntil(0);
    cleanSolver();

    return false;
}

void ExperimentalTechniques::printStatistics(ostream& stream)
{
    stream << "c [STAT] ENT " << processTime << " s, " << removedClauses << " cls, " << subsumed << " subs, " << extraSubs << " extraSubs, " << endl;
}

void ExperimentalTechniques::giveMoreSteps()
{

}

void ExperimentalTechniques::destroy()
{

}

void ExperimentalTechniques::cleanSolver()
{
    // clear all watches!
    solver.watches.cleanAll();

    // clear all watches!
    for (int v = 0; v < solver.nVars(); v++)
        for (int s = 0; s < 2; s++) {
            solver.watches[ mkLit(v, s) ].clear();
        }

    solver.learnts_literals = 0;
    solver.clauses_literals = 0;
    solver.watches.cleanAll();

    for (int i = 0 ; i < solver.learnts.size(); ++ i) {
        ca[ solver.learnts[i] ].sort();
    }
    for (int i = 0 ; i < solver.clauses.size(); ++ i) {
        ca[ solver.clauses[i] ].sort();
    }
}

void ExperimentalTechniques::reSetupSolver()
{
    assert(solver.decisionLevel() == 0 && "solver can be re-setup only at level 0!");
    // check whether reasons of top level literals are marked as deleted. in this case, set reason to CRef_Undef!
    if (solver.trail_lim.size() > 0)
        for (int i = 0 ; i < solver.trail_lim[0]; ++ i) {
            Solver::ReasonStruct& reason = solver.reason(var(solver.trail[i]));
            if (! reason.isBinaryClause() && reason.getReasonC() != CRef_Undef)
                if (ca[ reason.getReasonC() ].can_be_deleted()) {
                    reason.setReason(CRef_Undef);
                }
        }

    // give back into solver
    for (int p = 0 ; p < 1; ++ p) {   // do not use learned clauses, because they might be dropped without any notice later again
        vec<CRef>& clauses = (p == 0 ? solver.clauses : solver.learnts);
        for (int i = 0; i < clauses.size(); ++i) {
            const CRef cr = clauses[i];
            Clause& c = ca[cr];
            assert(c.size() != 0 && "empty clauses should be recognized before re-setup");
            if (!c.can_be_deleted()) {   // all clauses are neccesary for re-setup!
                assert(c.mark() == 0 && "only clauses without a mark should be passed back to the solver!");
                if (c.size() > 1) {
                    // do not watch literals that are false!
                    int j = 1;
                    for (int k = 0 ; k < 2; ++ k) {   // ensure that the first two literals are undefined!
                        if (solver.value(c[k]) == l_False) {
                            for (; j < c.size() ; ++j)
                                if (solver.value(c[j]) != l_False)
                                { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
                        }
                    }
                    // assert( (solver.value( c[0] ) != l_False || solver.value( c[1] ) != l_False) && "Cannot watch falsified literals" );

                    // reduct of clause is empty, or unit
                    if (solver.value(c[0]) == l_False) { data.setFailed(); return; }
                    else if (solver.value(c[1]) == l_False) {
                        if (data.enqueue(c[0]) == l_False) { return; }
                        else {
                            c.set_delete(true);
                        }
                        if (solver.propagate() != CRef_Undef) { data.setFailed(); return; }
                        c.set_delete(true);
                    } else { solver.attachClause(cr); }
                } else if (solver.value(c[0]) == l_Undef)
                    if (data.enqueue(c[0]) == l_False) { return; }
                    else if (solver.value(c[0]) == l_False) {
                        // assert( false && "This UNSAT case should be recognized before re-setup" );
                        data.setFailed();
                    }
            }
        }
    }
}

} // namespace Coprocessor
