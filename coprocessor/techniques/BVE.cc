/*******************************************************************[BoundedVariableElimination.cc]
Copyright (c) 2012, Kilian Gebhardt, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "BVE.h"
#include "Propagation.h"
#include "Subsumption.h"
#include "riss/mtl/Heap.h"

using namespace std;
using namespace Riss;

namespace Coprocessor
{

BoundedVariableElimination::BoundedVariableElimination(CP3Config& _config, Riss::ClauseAllocator& _ca,
        Riss::ThreadController& _controller,
        Coprocessor::Propagation& _propagation,
        Coprocessor::Subsumption& _subsumption)
    : Technique(_config, _ca, _controller)
    , propagation(_propagation)
    , subsumption(_subsumption)
    , stepper(config.opt_bve_limit)
    , variable_heap(nullptr)
    , removedClauses(0)
    , removedLiterals(0)
    , createdClauses(0)
    , createdLiterals(0)
    , removedLearnts(0)
    , learntLits(0)
    , newLearnts(0)
    , newLearntLits(0)
    , testedVars(0)
    , anticipations(0)
    , eliminatedVars(0)
    , removedBC(0)
    , blockedLits(0)
    , removedBlockedLearnt(0)
    , learntBlockedLit(0)
    , skippedVars(0)
    , unitsEnqueued(0)
    , foundGates(0)
    , usedGates(0)
    , initialClauses(0)
    , initialLits(0)
    , clauseCount(0)
    , litCount(0)
    , unitCount(0)
    , elimCount(0)
    , restarts(0)
    , nClsIncreases(0)
    , nClsDecreases(0)
    , nClsKeep(0)
    , totallyAddedClauses(0)
    , processTime(0)
    , subsimpTime(0)
    , gateTime(0)
{
}

BoundedVariableElimination::~BoundedVariableElimination()
{
    // free variable heap
    if (variable_heap != nullptr) {
        delete variable_heap;
    }
}

void BoundedVariableElimination::giveMoreSteps()
{
    stepper.increaseLimit(config.opt_bveInpStepInc);
}


void BoundedVariableElimination::printStatistics(ostream& stream)
{
    stream << "c [STAT] BVE(1) " << processTime << " s, "
           << subsimpTime << " s spent on subsimp, "
           << testedVars << " vars tested, "
           << anticipations << " anticipations, "  //  = tested vars?
           << skippedVars << " vars skipped, "
           << stepper.getCurrentSteps() << " checks, "
           << endl;
    stream << "c [STAT] BVE(2) " << removedClauses << " rem cls, "
           << "with " << removedLiterals << " lits, "
           << removedLearnts << " learnts rem, "
           << "with " << learntLits << " lits, "
           << createdClauses << " new cls, "
           << "with " << createdLiterals << " lits, "
           << newLearnts << " new learnts, "
           << "with " << newLearntLits << " lits, "
           << endl;
    stream << "c [STAT] BVE(3) " << eliminatedVars << " vars eliminated, "
           << unitsEnqueued << " units enqueued, "
           << removedBC << " BC removed, "
           << "with " << blockedLits << " lits, "
           << removedBlockedLearnt << " blocked learnts removed, "
           << "with " << learntBlockedLit << " lits, "
           << endl;
    stream << "c [STAT] BVE(4) " << foundGates << " gateDefs, "
           << usedGates << " usedGates, "
           << gateTime << " gateSeconds, "
           << endl;
    stream << "c [STAT] BVE(5) " << nClsIncreases << " incElims, "
           << nClsKeep << " keepElims, "
           << nClsDecreases << " decElims, "
           << totallyAddedClauses << " totalAdds,"
           << endl;
    for (int i = 0; i < parStats.size(); ++i) {
        ParBVEStats& s = parStats[i];
        stream << "c [STAT] BVE(1)-T" << i << " "
               << s.processTime << " s, "
               << s.subsimpTime << " s spent on subsimp, "
               << s.upTime << " s spent on up, "
               << s.lockNeighborTime << " s locking & neighb-calc, "
               << s.mereLockingTime << " s mere locking, "
               << s.testedVars << " vars tested, "
               << s.anticipations << " anticipations, "  //  = tested vars?
               << s.skippedVars << " vars skipped, "
               << s.parBveChecks << " checks,"
               << endl;
        stream << "c [STAT] BVE(2)-T" << i << " "
               << s.removedClauses << " rem cls, "
               << "with " << s.removedLiterals << " lits, "
               << s.removedLearnts << " learnts rem, "
               << "with " << s.learntLits << " lits, "
               << s.createdClauses << " new cls, "
               << "with " << s.createdLiterals << " lits, "
               << s.newLearnts << " new learnts, "
               << "with " << s.newLearntLits << " lits, "
               << endl;
        stream << "c [STAT] BVE(3)-T" << i << " "
               << s.eliminatedVars << " vars eliminated, "
               << s.unitsEnqueued << " units enqueued, "
               << s.removedBC << " BC removed, "
               << "with " << s.blockedLits << " lits, "
               << s.removedBlockedLearnt << " blocked learnts removed, "
               << "with " << s.learntBlockedLit << " lits, "
               << endl;
        stream << "c [STAT] BVE(4)-T" << i << " "
               << s.foundGates << " gateDefs, "
               << s.usedGates << " usedGates, "
               << s.gateTime << " gateSeconds, "
               << endl;
    }
}

void BoundedVariableElimination::progressStats(CoprocessorData& data, const bool cputime)
{
    if (!config.opt_print_progress || !config.opt_printStats) { return; }
    clauseCount = data.nCls();
    unitCount = data.getSolver()->trail.size();
    elimCount = eliminatedVars;
    for (int i = 0; i < parStats.size(); ++i) {
        elimCount += parStats[i].eliminatedVars;
    }
    cerr << "c [STAT] BVEprogress: restarts: " << restarts
         << ", clauses: " << clauseCount << " (" << ((double) clauseCount / (double) initialClauses * 100) << " %), "
         << "units: " << unitCount << " (" << ((double) unitCount / (double) data.nVars() * 100) << " %), "
         << "elim:  " << elimCount << " (" << ((double) elimCount / (double) data.nVars() * 100) << " %), "
         << "bve-time: " << (cputime ? Riss::cpuTime() : Riss::wallClockTime()) - processTime << endl;
    restarts++;
}

bool BoundedVariableElimination::hasToEliminate()   // TODO if heap is used, this will not work, since the heap depends on the changing data-object
{
    return (variable_queue.size() > 0);
}

lbool BoundedVariableElimination::process(CoprocessorData& data, const bool doStatistics)
{
    assert(isInitialized && "Technique must be initialized before processing");

    // do not do anything?!
    if (!performSimplification()) {
        return l_Undef;
    }

    // do not simplify, if the formula is considered to be too large!
    if (!data.unlimited() && (data.nVars() > config.opt_bve_vars &&
                              data.getClauses().size() + data.getLEarnts().size() > config.opt_bve_cls &&
                              data.nTotLits() > config.opt_bve_lits)) {
        return l_Undef;
    }

    initialClauses = data.nCls();
    restarts = 0;

    // run the parallel BVE
    if (controller.size() > 0) {
        parallelBVE(data);

        if (data.ok()) {
            return l_Undef;
        } else {
            return l_False;
        }
    }
    // sequential BVE here

    if (doStatistics) {
        processTime = cpuTime() - processTime;
    }

    // We use a heap, if the option is not set to "random"
    // otherwise use a plain vector.
    //
    // Because we do not clear the heap / queue after a process step to reuse the remaining variables in the next
    // process step, we have to check for duplicates when we insert new variables into the heap / queue
    if (config.opt_bve_heap != 2) {
        // initialize variable heap only once
        assert(variable_heap != nullptr && "Variable heap must be initialzied");
        data.getActiveVariables(lastModTime(), *variable_heap, true);
    } else {
        // initialze mark array the first time process() gets called
        if (duplicateMarker.size() == 0) {
            duplicateMarker.create(data.nVars());
            duplicateMarker.nextStep(); // create initial step
        }
        // if new variables are created by other techniques, reserve some space in the marker
        else if (duplicateMarker.size() < data.nVars()) {
            duplicateMarker.resize(data.nVars());
        }
        data.getActiveVariables(lastModTime(), variable_queue, &duplicateMarker);
    }

    if (propagation.process(data, true) == l_False) {
        return l_False;
    }
    bool propagatedSomething = propagation.appliedSomething();


    if (false) {
        cerr << "formula after propagation: " << endl;
        for (int i = 0; i < data.getClauses().size(); ++i)
            if (!ca[data.getClauses()[i]].can_be_deleted()) { cerr << ca[data.getClauses()[i]] << endl; }
        for (int i = 0; i < data.getLEarnts().size(); ++i)
            if (!ca[data.getClauses()[i]].can_be_deleted()) { cerr << ca[data.getLEarnts()[i]] << endl; }
    }

    data.ma.resize(data.nVars() * 2);

    sequentiellBVE(data, false);

    // Note:
    //  We do not clear the variable queue / heap, because we want to reuse the variables in the next
    //  process step.
    if (config.opt_bve_heap != 2) {
        duplicateMarker.nextStep();
        // Mark all variables with the current step so that they are recognized the next time
        // process() gets called
        for (int i = 0; i < variable_queue.size(); ++i) {
            duplicateMarker.setCurrentStep(variable_queue[i]);
        }
    }

    if (doStatistics) {
        processTime = cpuTime() - processTime;
    }
    if (!appliedSomething()) {
        unsuccessfulSimplification();
    }

    if (appliedSomething() || propagatedSomething) {
        successfulSimplification();
    }

    if (data.getSolver()->okay()) {
        return l_Undef;
    } else {
        return l_False;
    }
}


static void printLitErr(const Lit l)
{
    if (toInt(l) % 2) {
        cerr << "-";
    }
    cerr << (toInt(l) / 2) + 1 << " ";
}


static void printClause(const Clause& c)
{
    cerr << "c ";
    for (int i = 0; i < c.size(); ++i) {
        printLitErr(c[i]);
    }
    cerr << (c.can_be_deleted() ? " delete" : " valid") << endl;

}

static void printLitVec(const vec<Lit>& litvec)
{
    cerr << "c ";
    for (int i = 0; i < litvec.size(); ++i) {
        printLitErr(litvec[i]);
    }
    cerr << endl;

}

static void printClauses(ClauseAllocator& ca, vector<CRef>& list, bool skipDeleted)
{
    for (unsigned i = 0; i < list.size(); ++i) {
        if (skipDeleted && ca[list[i]].can_be_deleted()) {
            continue;
        }
        printClause(ca[list[i]]);
    }

}

void BoundedVariableElimination::sequentiellBVE(CoprocessorData& data, const bool force, const bool doStatistics)
{
    //Subsumption / Strengthening
    if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }
    subsumption.process(config.opt_bve_strength); // here, use penalty!
    modifiedFormula = modifiedFormula || subsumption.appliedSomething();
    if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }

    if (!data.ok()) { return; }

    touched_variables.clear();


    while (// repeat loop only, if not already interrupted
        !data.isInterupted()
        // variable heap or list is not empty
        && ((config.opt_bve_heap != 2 && variable_heap->size() > 0) || (config.opt_bve_heap == 2 && variable_queue.size() > 0))
        // check if we are inside the limit
        && (stepper.inLimit() || data.unlimited())
    ) {

        updateModTime(data.getMyModTimer());

        dirtyOccs.nextStep();
        progressStats(data, true);

        if (config.opt_bve_verbose > 0)
            cerr << "c sequentiel bve on "
                 << ((config.opt_bve_heap != 2) ? variable_heap->size() : variable_queue.size()) << " variables" << endl;

        bve_worker(data, stepper, force, doStatistics);

        if (!data.ok()) { return; }

        //propagate units
        if (data.hasToPropagate()) {
            if (l_False == propagation.process(data, true)) { return; }
            modifiedFormula = modifiedFormula || propagation.appliedSomething();
        }
        // perform garbage collection
        data.checkGarbage();

        // add active variables and clauses to variable heap and subsumption queues
        data.getActiveVariables(lastModTime(), touched_variables);
        touchedVarsForSubsumption(data, touched_variables);

        if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }
        subsumption.process(config.opt_bve_strength); // have an option to avoid penalty!
        if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }
        modifiedFormula = modifiedFormula || subsumption.appliedSomething();

        if (config.opt_bve_heap != 2) { variable_heap->clear(); }
        else { variable_queue.clear(); }

        for (int i = 0; i < touched_variables.size(); ++i) {
            if (config.opt_bve_heap != 2) {
                variable_heap->insert(touched_variables[i]);
            } else {
                variable_queue.push_back(touched_variables[i]);
            }
        }
        touched_variables.clear();
    }

    progressStats(data, true);
}

/**
 * Expects filled variable processing queue
 *
 * force -> forces resolution
 */
void BoundedVariableElimination::bve_worker(CoprocessorData& data, Stepper& workerStepper, const bool force, const bool doStatistics)
{
    while (// repeat loop only until being interrupted
        !data.isInterupted()
        // variable queue / heap is not empty
        && ((config.opt_bve_heap != 2 && variable_heap->size() > 0) || (config.opt_bve_heap == 2 && variable_queue.size() > 0))
        // step limit not reached
        && (workerStepper.inLimit() || data.unlimited())
    ) {
        Var v = var_Undef;
        if (config.opt_bve_heap != 2) {
            v = variable_heap->removeMin();
        } else {
            int rand = data.getSolver()->irand(data.getSolver()->random_seed, variable_queue.size());
            v = variable_queue[rand];
            if (variable_queue.size() > 1) {
                variable_queue[rand] = variable_queue[variable_queue.size() - 2];
            }
            variable_queue.pop_back();
        }
        assert(v != var_Undef && "variable heap or queue failed");

        // do not work on this variable, if it will be unit-propagated! if all units are eagerly propagated, this is not necessary
        if (data.value(mkLit(v, true)) != l_Undef || data.value(mkLit(v, false)) != l_Undef) { continue; }
        // do not work on this variable, if it does not occur in the formula (any more)
        if (data.list(mkLit(v, false)).size() == 0 && data.list(mkLit(v, true)).size() == 0) { continue; }
        // do not work with this variable, if it is frozen!
        if (data.doNotTouch(v)) { continue; }

        // Heuristic Cutoff Gate-Search
        if (!config.opt_force_gates && !config.opt_unlimited_bve &&
                (data[mkLit(v, true)] > 10 && data[mkLit(v, false)] > 10 ||
                 data[v] > 15 && (data[mkLit(v, true)] > 5 || data[mkLit(v, false)] > 5))) {
            if (doStatistics) { ++skippedVars; }
            continue;
        }

        // Search for Gates
        int p_limit = data.list(mkLit(v, false)).size();
        int n_limit = data.list(mkLit(v, true)).size();
        bool foundGate = false;
        if (config.opt_bve_findGate) {
            foundGate = findGates(data, v, p_limit, n_limit, gateTime);
            if (doStatistics && foundGate) { foundGates++; }
        }

        // only eliminate if there is a gate
        if (!foundGate && config.bve_funcDepOnly) { continue; }

        // Heuristic Cutoff Anticipation (if no Gate Found)
        if (!config.opt_unlimited_bve && !foundGate &&
                (data[mkLit(v, true)] > 10 && data[mkLit(v, false)] > 10
                 || data[v] > 15 && (data[mkLit(v, true)] > 5 || data[mkLit(v, false)] > 5)
                )
           ) {
            if (doStatistics) { ++skippedVars; }
            continue;
        }

        if (doStatistics) { ++testedVars; }

        // if( data.value( mkLit(v,true) ) != l_Undef ) continue;
        vector<CRef>& pos = data.list(mkLit(v, false));
        vector<CRef>& neg = data.list(mkLit(v, true));

        // ---Printing all Clauses with v --------------------------//
        if (config.opt_bve_verbose > 2) {
            cerr << "c Variable: " << v + 1 << endl;
            cerr << "c Clauses with Literal  " << v + 1 << ":" << endl;
            printClauses(ca, pos, false);
            cerr << "c Clauses with Literal ¬" << v + 1 << ":" << endl;
            printClauses(ca, neg, false);
        }
        // ---------------------------------------------------------//
        int pos_count = 0;
        int neg_count = 0;
        int lit_clauses_old = 0;
        int lit_learnts_old = 0;

        if (config.opt_bve_verbose > 1) {
            cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
            cerr << "c Counting Clauses" << endl;
        }

        for (int i = 0; i < pos.size(); ++i) {
            Clause& c = ca[pos[i]];
            if (c.can_be_deleted()) { // remove from the list!
                pos[i] = pos.back();
                pos.pop_back();
                --i;
                continue;
            }
            if (c.learnt()) { lit_learnts_old += c.size(); }
            else { lit_clauses_old += c.size(); }
            ++pos_count;
        }
        for (int i = 0; i < neg.size(); ++i) {
            Clause& c = ca[neg[i]];
            if (c.can_be_deleted()) {
                neg[i] = neg.back();
                neg.pop_back();
                --i;
                continue;
            }
            if (c.learnt()) { lit_learnts_old += c.size(); }
            else { lit_clauses_old += c.size(); }
            ++neg_count;
        }
        if (config.opt_bve_verbose > 2) { cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl; }

        pos_stats.growTo(pos.size(), 0);
        neg_stats.growTo(neg.size(), 0);
        int lit_clauses = 0;
        int lit_learnts = 0;
        int resolvents = 0;

        lbool anticipateResult = l_Undef; // usual end

        if (!force) {
            // anticipate only, if there are positiv and negative occurrences of var
            if (pos_count != 0 && neg_count != 0) {
                if (doStatistics) { ++anticipations; }
                anticipateResult = anticipateElimination(data, pos, neg, v, p_limit, n_limit, pos_stats, neg_stats,
                                   lit_clauses, lit_learnts, resolvents, workerStepper);
                if (anticipateResult == l_False) { return; }  // level 0 conflict found while anticipation TODO ABORT
            }


            // use BCE only, if no early abort
            if (anticipateResult == l_Undef && config.opt_bve_bc) {
                //mark Clauses without resolvents for deletion
                if (config.opt_bve_verbose > 2) { cerr << "c ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl; }
                if (config.opt_bve_verbose > 1) { cerr << "c removing blocked clauses from F_" << v + 1 << endl; }
                removeBlockedClauses(data, pos, pos_stats, mkLit(v, false), p_limit, doStatistics);
                if (config.opt_bve_verbose > 1) { cerr << "c removing blocked clauses from F_¬" << v + 1 << endl; }
                removeBlockedClauses(data, neg, neg_stats, mkLit(v, true), n_limit, doStatistics);
            }
        }

        // if resolving reduces number of literals in clauses:
        //    add resolvents
        //    mark old clauses for deletion
        bool doResolve = false; // TODO: do we need the lit_clauses > 0 or resolvents > 0 check? Assumption: no!

        int compareNumber = pos_count + neg_count + config.opt_bve_grow;
        if (config.opt_bve_growTotal != INT32_MAX && config.opt_bve_growTotal > config.opt_bve_grow) {
            compareNumber += (config.opt_bve_growTotal -
                              totallyAddedClauses); // have one stable reference value
        }

        // we do allow growth
        bool reducedClss = resolvents <= compareNumber; // && resolvents > 0 ;
        DOUT(if (config.opt_bve_verbose > 2) cerr << "c BVE reduced clauses " << reducedClss << " resolvents: " << resolvents << " posC: " << pos_count << " negC: " << neg_count << " grow: " << config.opt_bve_grow << endl;);

        if (totallyAddedClauses > config.opt_bve_growTotal) {
            DOUT(if (config.opt_bve_verbose > 2) cerr << "c BVE block increase due to totally added clauses " << totallyAddedClauses << " vs " << config.opt_bve_growTotal << endl;);
            reducedClss = false;
        } // stop increasing!
        if (reducedClss) {
            totallyAddedClauses += resolvents - (pos_count + neg_count);
        }


        doResolve = reducedClss;    // number of clauses decreasesd
        // anticipateResult == l_True -> !reducedClss,
        // anticipateResult == l_Undef -> ?
        assert((anticipateResult != l_True || !reducedClss) && "in case of an early abort, we have to resolve");

        if ((force || doResolve) // clauses or literals should be reduced and we did
                && !config.opt_bce_only // only if bve should be done
           ) {
            if (resolvents < pos_count + neg_count) {
                nClsDecreases++;
            } else if (resolvents > pos_count + neg_count) {
                nClsIncreases++;
            } else if (resolvents == pos_count + neg_count) {
                nClsKeep++;
            }

            if (doStatistics) { usedGates = (foundGate ? usedGates + 1 : usedGates); } // statistics
            if (config.opt_bve_verbose > 1) { cerr << "c resolveSet" << endl; }
            data.addCommentToProof("perform BVE");
            if (resolveSet(data, pos, neg, v, p_limit, n_limit, workerStepper) == l_False) {
                assert(!data.ok() && "here, the data object should already know that nothing can be done");
                return;
            }
            if (doStatistics) { ++eliminatedVars; }

            // if the clause does not already have a value,
            // add only a unit clause to the extension stack for undo simplification, and all clauses for the other polarity before
            removeClauses(data, pos, lit_Undef, p_limit,
                          doStatistics);      // do not add these clauses to the undo stack!
            if (data.value(mkLit(v, false)) == l_Undef) {
                removeClauses(data, neg, mkLit(v, true), n_limit,
                              doStatistics); // add these clauses to the undo stack
                data.addToExtension(mkLit(v,
                                          false));                                 // make this variable false by default on the undo stack, the other clauses with take care afterwards
            } else {
                removeClauses(data, neg, lit_Undef, n_limit, doStatistics); // add these clauses to the undo stack
            }

            vector<CRef>().swap(pos); //free physical memory of occs
            vector<CRef>().swap(neg); //free physical memory of occs
            if (config.opt_bve_verbose > 0) { cerr << "c Resolved " << v + 1 << endl; }
            //subsumption with new clauses!!
            if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }
            if (config.heap_updates > 0 && config.opt_bve_heap != 2) {
                subsumption.process(config.opt_bve_strength, variable_heap, v);
            } else {
                subsumption.process(config.opt_bve_strength);
            }
            if (doStatistics) { subsimpTime = cpuTime() - subsimpTime; }
            modifiedFormula = modifiedFormula || subsumption.appliedSomething();

            if (!data.ok()) {
                return;
            }
        } else {
            if (config.opt_bve_verbose > 2) {
                cerr << "c current formula: " << endl;
                for (int t = 0; t < data.getClauses().size(); ++t) {
                    if (!ca[data.getClauses()[t]].can_be_deleted()) {
                        cerr << "[" << data.getClauses()[t] << "] " << ca[data.getClauses()[t]] << endl;
                    }
                }
                for (Var v = 0; v < data.nVars(); ++v) {
                    for (int p = 0; p < 2; ++p) {
                        const Lit l = mkLit(v, p == 0);
                        if (data.list(l).size() > 0) {
                            cerr << "c list(" << l << "): " << endl;
                            for (int t = 0; t < data.list(l).size(); ++t)
                                if (!ca[data.list(l)[t]].can_be_deleted()) {
                                    cerr << "[" << data.list(l)[t] << "] " << ca[data.list(l)[t]] << endl;
                                }
                            cerr << endl;
                        }
                    }
                }
            }
        }
        pos_stats.clear();
        neg_stats.clear();

        if (config.opt_bve_verbose > 1) {
            cerr << "c =============================================================================" << endl;
        }

    }

}

/*
 * on every clause, that is not yet marked for deletion:
 *      remove it from data-Objects statistics
 *      mark it for deletion
 */
inline void BoundedVariableElimination::removeClauses(Coprocessor::CoprocessorData& data, const vector< Riss::CRef >& list,
        const Lit& l, const int limit, const bool doStatistics)
{
    for (int cr_i = 0; cr_i < list.size(); ++cr_i) {
        Clause& c = ca[list[cr_i]];
        CRef cr = list[cr_i];
        if (!c.can_be_deleted()) {
            data.addToProof(c, true);
            // also updated modTimer
            if (config.heap_updates > 0 && config.opt_bve_heap != 2) {
                data.removedClause(cr, variable_heap);
            } else {
                data.removedClause(cr);
            }
            successfulSimplification();
            c.set_delete(true);
            if (l != lit_Undef && !c.learnt() /*&& cr < limit*/) { data.addToExtension(cr, l); }
            if (doStatistics) {
                if (c.learnt()) {
                    ++removedLearnts;
                    learntLits += c.size();
                } else {
                    ++removedClauses;
                    removedLiterals += c.size();
                }
            }
            if (config.opt_bve_verbose > 1) {
                cerr << "c removed clause: ";
                printClause(c);
            }
        }

        //Delete Clause from all Occ-Lists TODO too much overhead?
        /*for (int j = 0; j < c.size(); ++j)
            if (c[j] != l)
                data.removeClauseFrom(cr,c[j]);
        list[cr_i] = list[list.size() - 1];
        list.pop_back();*/
    }

}

/** simulate elimination of one variable
 *  anticipates following numbers:
 *  -> number of resolvents derived from specific clause:       pos_stats / neg_stats
 *  -> total number of literals in clauses after resolution:    lit_clauses
 *  -> total number of literals in learnts after resolution:    lit_learnts
 *
 *  @return l_False if the empty clause was produced, l_Undef, if the procedure reached its end, l_True, if we had an
 *                  early abort due to too many resolvents
 */
inline lbool BoundedVariableElimination::anticipateElimination(CoprocessorData& data, vector<CRef>& positive,
        vector<CRef>& negative, const int v, const int p_limit,
        const int n_limit, vec<int32_t>& pos_stats,
        vec<int32_t>& neg_stats, int& lit_clauses,
        int& lit_learnts, int& resolvents, Stepper& bveStepper,
        const bool doStatistics)
{
    if (config.opt_bve_verbose > 2) {
        cerr << "c starting anticipate BVE -- with pLim= " << p_limit << " nLim= " << n_limit << endl;
    }
    // Clean the stats
    lit_clauses = 0;
    lit_learnts = 0;
    // vec <Lit > resolvent;
    const bool hasDefinition = (p_limit < positive.size() || n_limit < negative.size());


    assert(resolvents == 0 && "before resolution there are no resolvents");
//     config.opt_bve_growTotal
    int clausesToUse = 0;
    if (config.opt_bve_earlyAbort) {
        for (int cr_p = 0; cr_p < positive.size(); ++cr_p) {
            clausesToUse = (ca[positive[cr_p]].can_be_deleted()) ? clausesToUse : clausesToUse + 1;
        }
        DOUT(if (config.opt_bve_verbose > 2) cerr << "c limit after positives: " << clausesToUse << endl;);
        for (int cr_p = 0; cr_p < negative.size(); ++cr_p) {
            clausesToUse = (ca[negative[cr_p]].can_be_deleted()) ? clausesToUse : clausesToUse + 1;
        }
        DOUT(if (config.opt_bve_verbose > 2) cerr << "c limit after negtaives: " << clausesToUse << endl;);
        clausesToUse = clausesToUse + config.opt_bve_grow;
        DOUT(if (config.opt_bve_verbose > 2) cerr << "c limit after grow: " << clausesToUse << endl;);
        if (config.opt_bve_growTotal != INT32_MAX && config.opt_bve_growTotal > config.opt_bve_grow) {
            clausesToUse += (config.opt_bve_growTotal -
                             totallyAddedClauses); // have one stable reference value
            if (config.opt_bve_verbose > 2) { cerr << "c adapt limit due to total grow factor: " << clausesToUse << " by adding " << (config.opt_bve_growTotal - totallyAddedClauses) << endl; }
        }
    }
    bool binariesOnly = false; // process only binary clauses? use if we already reached the clause limit with bve early

    for (int cr_p = 0; cr_p < positive.size(); ++cr_p) {
        Clause& p = ca[positive[cr_p]];
        if (p.can_be_deleted()) {
            if (config.opt_bve_verbose > 2) {
                cerr << "c    skipped p " << p << endl;
            }
            continue;
        }
        // we are already over the clause limit, but we are still looking for units
        if (binariesOnly && p.size() > 2) {
            continue;
        }
        if (config.opt_bve_verbose > 2) { cerr << "c [" << cr_p << "/" << positive.size() << "] : " << p << endl; }
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n) {
            // do not check resolvents, which would touch two clauses out of the variable definition
            if (cr_p >= p_limit && cr_n >= n_limit) { continue; }
            if (hasDefinition && cr_p < p_limit && cr_n < n_limit) {
                // no need to resolve the definition clauses with each other
                // NOTE: assumes that these clauses result in tautologies
                continue;
            }

            bveStepper.increaseSteps(1); // count number of clause dereferrences!

            Clause& n = ca[negative[cr_n]];
            if (n.can_be_deleted()) {
                if (config.opt_bve_verbose > 2) {
                    cerr << "c    skipped n " << n << endl;
                }
                continue;
            }
            if (binariesOnly && n.size() > 2) { continue; }   // we are already over the clause limit, but we are still looking for units
            int newLits = tryResolve(p, n, v);
            if (config.opt_bve_verbose > 2) { cerr << "c vs [" << cr_n << "/" << negative.size() << "] : " << n << endl; }

            if (config.opt_bve_verbose > 2) { cerr << "c    resolvent size " << newLits << endl; }

            if (newLits > 1) {
                if (config.opt_bve_verbose > 2) {
                    cerr << "c    Clause P: " << p << endl;
                    cerr << "c    Clause N: " << n << endl;
                    cerr << "c    Resolvent: ";
                    vec<Lit> resolvent;
                    resolve(p, n, v, resolvent);
                    printLitVec(resolvent);
                }
                if (p.learnt() || !n.learnt()) { // don't increment blocking-stats for p,
                    ++pos_stats[cr_p];    // if p is original and n learnt
                }
                // goal: detect that p is blocked by all orig. n-clauses
                if (n.learnt() || !p.learnt()) { // vice versa
                    ++neg_stats[cr_n];
                }
                if (p.learnt() || n.learnt()) {
                    lit_learnts += newLits;
                } else {
                    lit_clauses += newLits;
                }
                resolvents++; // count number of produced clauses!

                // check for an early abort -- we already have more resolvents than we are allowed to create
                if (config.opt_bve_earlyAbort && resolvents > clausesToUse) { // indicate that we interrupted the process, hence, the BCE information is not valid, furthermore, we continue only on binary clauses
                    binariesOnly = true;
                    if (config.opt_bve_verbose > 2) { cerr << "c reached clauses to use limit (" << clausesToUse << ") with resolvents: " << resolvents << endl; }
                }

            }

            // empty Clause
            else if (newLits == 0) {
                successfulSimplification();
                if (config.opt_bve_verbose > 2) {
                    cerr << "c    empty resolvent" << endl;
                    cerr << "c    Clause P: ";
                    printClause(p);
                    cerr << "c    Clause N: ";
                    printClause(n);
                    cerr << "c    finished anticipate_bve by finding empty clause" << endl;
                }
                data.setFailed();
                return l_False;
            }

            // unit Clause
            else if (newLits == 1) {
                resolvent.clear();
                resolve(p, n, v, resolvent);
                assert(resolvent.size() == 1);
                if (config.opt_bve_verbose > 0) {
                    cerr << "c    Unit Resolvent: ";
                    printLitVec(resolvent);
                }
                if (config.opt_bve_verbose > 2) {
                    cerr << "c    Clause P: ";
                    printClause(p);
                    cerr << "c     Clause N: ";
                    printClause(n);
                }

                data.addToProof(resolvent);
                uint64_t extraInfo = Clause::updateExtraInformation(p.extraInformation(), n.extraInformation());

                lbool status = data.enqueue(resolvent[0], extraInfo); //check for level 0 conflict

                if (status == l_False) {
                    successfulSimplification();
                    if (config.opt_bve_verbose > 2) { cerr << "c finished anticipate_bve with conflict" << endl; }
                    return l_False;
                } else if (status == l_Undef); // variable already assigned
                else if (status == l_True) {
                    successfulSimplification();
                    //assert(false && "all units should be discovered before (while strengthening)!");
                    if (doStatistics) { ++unitsEnqueued; }
                    if (propagation.process(data, true) == l_False) {
                        return l_False;
                    }
                    modifiedFormula = modifiedFormula || propagation.appliedSomething();
                    if (p.can_be_deleted()) {
                        if (config.opt_bve_verbose > 1) {
                            cerr << "c stop working on clause " << p << ", because it can be deleted" << endl;
                        }
                        break;
                    }
                } else {
                    assert(0);    //something went wrong
                }
            }

            if (config.opt_bve_verbose > 2) { cerr << "c ------------------------------------------" << endl; }
        }
    }
    if (config.opt_bve_verbose > 2) {
        for (int i = 0; i < positive.size(); ++i) {
            cerr << "c pos stat(" << i << "): " << (unsigned) pos_stats[i] << endl;
        } ;
        for (int i = 0; i < negative.size(); ++i) {
            cerr << "c neg stat(" << i << "): " << (unsigned) neg_stats[i] << endl;
        } ;

        cerr << "c finished anticipate_bve normally" << endl;
    }
    assert((!binariesOnly || resolvents > clausesToUse) && "binariesOnly can only be set if the limit was reached");
    DOUT(if (config.opt_bve_verbose > 2) {
    cerr << "c binariesOnly: " << binariesOnly << " resolvents: " << resolvents << " clausesToUseLimit: " << clausesToUse << endl;
});
    return binariesOnly ? l_True : l_Undef; // return l_True, if only binary clauses are tested any longer (limit reached)
}

/*
 *   performes resolution and
 *   allocates resolvents c, with |c| > 1, in ClauseAllocator
 *   as learnts or clauses respectively
 *   and adds them in data object
 *
 *   - resolvents that are tautologies are skipped
 *   - unit clauses and empty clauses are not handeled here
 *          -> this is already done in anticipateElimination
 */
lbool BoundedVariableElimination::resolveSet(CoprocessorData& data,
        vector<CRef>& positive, vector<CRef>& negative, const int v,
        const int p_limit, const int n_limit, Stepper& bveStepper,
        const bool keepLearntResolvents, const bool force,
        const bool doStatistics)
{
    vec<Lit>& ps = resolvent;
    const bool hasDefinition = (p_limit < positive.size() || n_limit < negative.size());

    for (int cr_p = 0; cr_p < positive.size(); ++cr_p) {
        Clause& p = ca[positive[cr_p]];
        if (p.can_be_deleted()) { continue; }
        for (int cr_n = 0; cr_n < negative.size(); ++cr_n) {
            // no need to resolve two clauses that are both not within the variable definition
            if (cr_p >= p_limit && cr_n >= n_limit) { continue; }
            if (hasDefinition && cr_p < p_limit && cr_n < n_limit) {
                continue;    // no need to resolve the definition clauses with each other NOTE: assumes that these clauses result in tautologies
            }
            bveStepper.increaseSteps(1); // count number of clause dereferrences!
            Clause& p = ca[positive[cr_p]]; // renew reference as it could got invalid while clause allocation
            Clause& n = ca[negative[cr_n]];
            if (n.can_be_deleted()) {
                continue;
            }
            // Dont compute resolvents for learnt clauses (this is done in case of force,
            // since blocked clauses and units have not been computed by anticipate)
            if (!force) {
                if (config.opt_resolve_learnts == 0 && (p.learnt() || n.learnt())) {
                    continue;
                }
                if (config.opt_resolve_learnts == 1 && (p.learnt() && n.learnt())) {
                    continue;
                }
            }
            ps.clear();
            if (!resolve(p, n, v, ps)) {
                if (ps.size() == 1) {
                    data.addToProof(ps); // tell proof about the new clause!
                    uint64_t extraInfo = Clause::updateExtraInformation(p.extraInformation(), n.extraInformation());

                    lbool status = data.enqueue(ps[0], extraInfo); //check for level 0 conflict
                    if (status == l_False) {
                        return l_False;
                    } else if (status == l_Undef); // variable already assigned
                    else if (status == l_True) {
                        if (doStatistics) { ++unitsEnqueued; }
                        if (propagation.process(data, true) == l_False) { //TODO propagate own lits only (parallel)
                            return l_False;
                        }
                        modifiedFormula = modifiedFormula || propagation.appliedSomething();
                        if (p.can_be_deleted()) { // now, the next clause might be redundant!
                            break;
                        }
                    }
                } else if (ps.size() > 1) {
                    // Depending on config.opt_resovle_learnts, skip clause creation
                    if (force) {
                        if (config.opt_resolve_learnts == 0 && (p.learnt() || n.learnt())) {
                            continue;
                        }
                        if (config.opt_resolve_learnts == 1 && (p.learnt() && n.learnt())) {
                            continue;
                        }
                    }
                    if ((p.learnt() || n.learnt()) && ps.size() > max(p.size(), n.size()) + config.opt_learnt_growth) {
                        continue;
                    }

                    if (config.opt_bve_verbose > 2) { // expensive!
                        for (int i = 0; i < ps.size(); ++i) {
                            for (int j = i + 1; j < ps.size(); ++j) {
                                if (ps[i] == ps[j]) {
                                    cerr << "c resolvent of clauses " << p << " and " << n <<
                                         " result in duplicate lits clause! (" << i << "," << j << "): " << endl;
                                    for (i = 0; i < ps.size(); ++i) { cerr << " " << ps[i]; }
                                    cerr << endl;
                                    break;
                                }
                            }
                        }
                    }


                    if (config.opt_bve_verbose > 1) { cerr << "c from resolution with" << p << " and " << n << endl; }
                    const uint64_t pinfo = p.extraInformation(), ninfo = n.extraInformation();
                    CRef cr = ca.alloc(ps, p.learnt() || n.learnt());
                    // IMPORTANT! dont use p and n in this block, as they could got invalid
                    Clause& resolvent = ca[cr];
                    if (config.opt_bve_verbose > 1) { cerr << "c add resolvent [" << cr << "] " << resolvent << endl; }
                    data.addToProof(ca[cr]); // tell proof about the new clause!
                    ca[cr].setExtraInformation(pinfo); // setup extra information for this clause!
                    ca[cr].updateExtraInformation(ninfo);

                    if (config.heap_updates > 0 && config.opt_bve_heap != 2) {
                        data.addClause(cr, variable_heap);
                    } else {
                        data.addClause(cr);
                    }

                    if (resolvent.learnt()) {
                        data.getLEarnts().push(cr);
                    } else {
                        data.getClauses().push(cr);
                    }
                    // push Clause on subsumption-queue
                    subsumption.initClause(cr);

                    if (doStatistics) {
                        if (resolvent.learnt()) {
                            ++newLearnts;
                            ++newLearntLits;

                        } else {
                            ++createdClauses;
                            createdLiterals += resolvent.size();
                        }
                    }
                } else if (force) {
                    // | resolvent | == 0  => UNSAT
                    if (ps.size() == 0) {
                        data.setFailed();
                        return l_False;
                    }

                    // | resolvent | == 1  => unit Clause
                    else if (ps.size() == 1) {
                        data.addToProof(ps); // tell proof about the new clause!
                        uint64_t extraInfo = Clause::updateExtraInformation(p.extraInformation(), n.extraInformation());

                        //assert(false && "all units should be discovered before (while strengthening)!");
                        lbool status = data.enqueue(ps[0], extraInfo); //check for level 0 conflict
                        if (status == l_False) {
                            return l_False;
                        } else if (status == l_Undef); // variable already assigned
                        else if (status == l_True) {
                            if (doStatistics) { ++unitsEnqueued; }
                            if (propagation.process(data, true) == l_False) { //TODO propagate own lits only (parallel)
                                return l_False;
                            }
                            modifiedFormula = modifiedFormula || propagation.appliedSomething();
                            if (p.can_be_deleted()) { // now, the next clause might be redundant!
                                break;
                            }
                        } else {
                            assert(false && "a case that should not be happen occurred!");    //something went wrong
                        }
                    }
                }

            }
        }

    }
    return l_Undef;
}


/**
 * this function removes Clauses that have no resolvents
 * i.e. all resolvents are tautologies
 */
inline void BoundedVariableElimination::removeBlockedClauses(CoprocessorData& data, const vector< Riss::CRef >& list,
        const int32_t stats[], const Lit& l, const int limit,
        const bool doStatistics)
{
    // FIXME limit parameter is not used!
    for (unsigned ci = 0; ci < list.size(); ++ci) {
        Clause& c = ca[list[ci]];
        if (c.can_be_deleted()) { continue; }
        if (stats[ci] == 0) {
            c.set_delete(true);
            data.addCommentToProof("blocked clause during BVE");
            data.addToProof(c, true);
            if (config.heap_updates > 0 && config.opt_bve_heap != 2) {
                data.removedClause(list[ci], variable_heap);
            } else {
                data.removedClause(list[ci]);
            }
            successfulSimplification();
            if (!c.learnt() /*&& ci < limit*/) {
                if (config.opt_bve_verbose > 2) { cerr << "c remove blocked clause " << ca[list[ci]] << endl; }
                data.addToExtension(list[ci], l);
            }
            if (doStatistics) {
                if (c.learnt()) {
                    ++removedBlockedLearnt;
                    learntBlockedLit += c.size();
                } else {
                    ++removedBC;
                    blockedLits += c.size();
                }
            }
            if (config.opt_bve_verbose > 1 || (config.opt_bve_verbose > 0 && !c.learnt())) {
                cerr << "c removed clause: " << ca[list[ci]] << endl;
                cerr << "c added to extension with Lit " << l << endl;;
            }
        }
    }
}

// All clauses that have been modified, can possibly be subsumed by clauses
// that share a subset of their literals
// Therefore we add those clauses to the processing list (if they are not contained in it already).
/*void BoundedVariableElimination :: append_modified (CoprocessorData & data, std::vector<CRef> & modified_list)
{
    for (int i = 0; i < modified_list.size(); ++i)
    {
        Clause & c = ca[modified_list[i]];
        for (int l = 0; l < c.size(); l++)
        {
            vector<CRef> & clauses = data.list(c[l]);
            for (int j = 0; j < clauses.size(); ++j)
            {
                Clause & d = ca[clauses[j]];
                if (! d.can_subsume())
                {
                    d.set_subsume(true);
                    subsumption.initClause(d);
                    //subsumption_processing_queue.push_back(occs[l][j]);
                }
            }

        }
        //c.set_modified(false);
    }
}*/
void BoundedVariableElimination::touchedVarsForSubsumption(CoprocessorData& data,
        const std::vector<Var>& touched_vars)
{
    for (int i = 0; i < touched_vars.size(); ++i) {
        Var v = touched_vars[i];
        addClausesToSubsumption(data.list(mkLit(v, false)));
        addClausesToSubsumption(data.list(mkLit(v, true)));

    }
}

inline void BoundedVariableElimination::addClausesToSubsumption(const vector<CRef>& clauses)
{
    for (int j = 0; j < clauses.size(); ++j) {
        assert(clauses[j] != CRef_Undef);
        Clause& d = ca[clauses[j]];
        if (!d.can_be_deleted() && !d.can_subsume()) {
            d.set_subsume(true);
            d.set_strengthen(true);
            subsumption.initClause(clauses[j]);
            //subsumption_processing_queue.push_back(occs[l][j]);
        }
    }
}

bool BoundedVariableElimination::findGates(CoprocessorData& data, const Var v, int& p_limit, int& n_limit,
        double& _gateTime, MarkArray *helper)
{
    // do not touch lists that are too small for benefit
    if (data.list(mkLit(v, false)).size() < 3 && data.list(mkLit(v, true)).size() < 3) { return false; }
    if (data.list(mkLit(v, false)).size() < 2 || data.list(mkLit(v, true)).size() < 2) { return false; }

    MethodTimer gateTimer(&_gateTime); // measure time spend in this method
    MarkArray& markArray = (helper == 0 ? data.ma : *helper);

    for (uint32_t pn = 0; pn < 2; ++pn) {
        vector<CRef>& pList = data.list(mkLit(v, pn != 0));
        vector<CRef>& nList = data.list(mkLit(v, pn == 0));
        const Lit pLit = mkLit(v, pn != 0);
        const Lit nLit = mkLit(v, pn == 0);
        int& pClauses = pn == 0 ? p_limit : n_limit;
        int& nClauses = pn == 0 ? n_limit : p_limit;

        // check binary of pos variable
        markArray.nextStep();
        for (uint32_t i = 0; i < pList.size(); ++i) {
            const CRef cr = pList[i];
            if (CRef_Undef == cr) { continue; }
            const Clause& clause = ca[cr];
            if (clause.can_be_deleted() || clause.learnt() || clause.size() != 2) {
                continue;    // NOTE: do not use learned clauses for gate detection!
            }
            Lit other = clause[0] == pLit ? clause[1] : clause[0];
            markArray.setCurrentStep(toInt(~other));
            if (config.opt_bve_verbose > 0) { cerr << "c mark " << ~other << " by " << clause << endl; }
        }
        for (uint32_t i = 0; i < nList.size(); ++i) {
            CRef cr = nList[i];
            if (CRef_Undef == cr) { continue; }
            const Clause& clause = ca[cr];
            if (clause.can_be_deleted() || clause.learnt()) { continue; }
            uint32_t j = 0;
            for (; j < clause.size(); ++j) {
                const Lit cLit = clause[j];
                if (cLit == nLit) { continue; } // do not consider variable that is to eliminate
                if (!markArray.isCurrentStep(toInt(cLit))) { break; }
            }
            if (j == clause.size()) {
                assert(!clause.can_be_deleted() &&
                       "a participating clause of the gate cannot be learned, because learned clauses will be removed completely during BVE");
                if (config.opt_bve_verbose > 0) {
                    cerr << "c [BVE] found " << (pn == 0 ? "pos" : "neg") << " gate with size " << j << " p: " <<
                         pList.size() << " n:" << nList.size() << " :=" << clause << endl;
                }
                // setup values
                pClauses = clause.size() - 1;
                nClauses = 1;
                // do not add unnecessary clauses
                if (config.opt_bve_verbose > 0) { cerr << "unmark literals of clause " << clause << endl; }
                for (uint32_t k = 0; k < clause.size(); ++k) {
                    if (config.opt_bve_verbose > 0) { cerr << "unmark " << clause[k] << endl; }
                    markArray.reset(toInt(clause[k]));
                }
                CRef tmp = nList[0];
                nList[0] = nList[i];
                nList[i] = tmp; // swap responsible clause in list to front
                // swap responsible clauses in list to front
                uint32_t placedClauses = 0;
                for (uint32_t k = 0; k < pList.size(); ++k) {
                    CRef cr = pList[k];
                    if (CRef_Undef == cr) { continue; }
                    const Clause& clause = ca[cr];
                    if (clause.learnt() || clause.can_be_deleted() || clause.size() != 2) { continue; }
                    Lit other = clause[0] == pLit ? clause[1] : clause[0];
                    if (!markArray.isCurrentStep(toInt(~other))) {
                        CRef tmp = pList[placedClauses];
                        pList[placedClauses] = pList[k];
                        pList[k] = tmp;
                        placedClauses++;
                        markArray.setCurrentStep(toInt(~other)); // no need to add the same binary twice
                        if (config.opt_bve_verbose > 0) {
                            cerr << "c move binary clause with " << other << " and clause " << clause << endl;
                        }
                    } else {
                        if (config.opt_bve_verbose > 0)
                            cerr << "c do not work with " << other << " and clause " << clause <<
                                 ", because already re-marked" << endl;
                    }
                }

                bool failPrint = false;
                if (pClauses != placedClauses) {
                    cerr << "c [BVE-G] placed: " << placedClauses << ", participating: " << pClauses << endl;
                    failPrint = true;
                }

                if (failPrint || config.opt_bve_verbose > 1) {
                    if (nList[0] != CRef_Undef) {
                        cerr << "c [BVE] GATE clause: " << ca[nList[0]] << " placed clauses: " << placedClauses << endl;
                    }
                    for (uint32_t k = 0; k < placedClauses; ++k) {
                        if (pList[k] != CRef_Undef) { cerr << "c [BVE] bin clause[" << k << "]: " << ca[pList[k]] << endl; }
                    }
                    cerr << "c return parameter: pos:" << p_limit << " neg: " << n_limit << endl;

                    cerr << "c clause lists: " << endl;
                    cerr << "for " << mkLit(v, false) << endl;
                    for (int i = 0; i < data.list(mkLit(v, false)).size(); ++i) {
                        if (data.list(mkLit(v, false))[i] == CRef_Undef) { continue; }
                        if (ca[data.list(mkLit(v, false))[i]].can_be_deleted()) { continue; }
                        else { cerr << i << " : " << ca[data.list(mkLit(v, false))[i]] << endl; }
                    }
                    cerr << "for " << mkLit(v, true) << endl;
                    for (int i = 0; i < data.list(mkLit(v, true)).size(); ++i) {
                        if (data.list(mkLit(v, true))[i] == CRef_Undef) { continue; }
                        if (ca[data.list(mkLit(v, true))[i]].can_be_deleted()) { continue; }
                        else { cerr << i << " : " << ca[data.list(mkLit(v, true))[i]] << endl; }
                    }
                }


                assert(pClauses == placedClauses &&
                       "number of moved binary clauses and number of participating clauses has to be the same");
                return true;
            }
        }
    }

    return false;
}

/**
 * Performs clear and minimize on all member vars to release memory
 */
void BoundedVariableElimination::destroy()
{
    vector<Var>().swap(touched_variables);
    vector<Var>().swap(variable_queue);

    // if we use a variable heap, clear it -- we do not delete the whole object
    // (this is done in the destructor of BVE), because it can be used for
    // the next simplification run
    if (variable_heap != nullptr) {
        variable_heap->clear();
    }

    resolvent.clear(true); // vector for sequential resolution
    pos_stats.clear(true);
    neg_stats.clear(true);

    // parallel member variables
    lastTouched.destroy();                    //MarkArray to track modifications of parallel BVE-Threads
    dirtyOccs.destroy();                      // tracks occs that contain CRef_Undef
    vector<Job>().swap(jobs);
    vector<SpinLock>().swap(variableLocks);         // 3 extra SpinLock for data, heap, ca
    vector<deque<CRef> >().swap(subsumeQueues);
    vector<deque<CRef> >().swap(strengthQueues);
    vector<MarkArray>().swap(gateMarkArrays);
    deque<CRef>().swap(sharedStrengthQueue);
}

} // namespace Coprocessor
