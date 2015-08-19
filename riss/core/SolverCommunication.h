/**************************************************************************[SolverCommunication.cc]
Copyright (c) 2012, All rights reserved, Norbert Manthey

This file implements the methods that are necessary for communicating among solver threads based
on the Communicator implementation.

**************************************************************************************************/

#ifndef RISS_Minisat_SolverCommunication_h
#define RISS_Minisat_SolverCommunication_h

#include <cmath>

#include "riss/mtl/Sort.h"
// #include "riss/core/Solver.h"
#include "riss/utils/System.h"

// to avoid cyclic dependencies
#include "riss/core/Communication.h"

// for test_cancel()
#include <pthread.h>

// pick one to enable/disable some debug messages
#define DBG(x)
// #define DBG(x) x

namespace Riss
{

inline
void Solver::setCommunication(Communicator* comm)
{
    assert(communication == 0 && "Will not overwrite already set communication interface");
    communication = comm;
    initLimits();  // set communication limits
    // copy values from communicator object
    communicationClient.sendSize = communication->sendSize;
    communicationClient.sendLbd = communication->sendLbd;
    communicationClient.sendMaxSize = communication->sendMaxSize;
    communicationClient.sendMaxLbd = communication->sendMaxLbd;
    communicationClient.sizeChange = communication->sizeChange;
    communicationClient.lbdChange = communication->lbdChange;
    communicationClient.sendRatio = communication->sendRatio;
    communicationClient.sendIncModel = communication->sendIncModel; // allow sending with variables where the number of models potentially increased
    communicationClient.sendDecModel = communication->sendDecModel; // allow sending with variables where the number of models potentially decreased (soundness, be careful here!)
    communicationClient.useDynamicLimits = communicationClient.useDynamicLimits || communication->useDynamicLimits; // one of the two overwrites the other
    communicationClient.receiveEE = communication->receiveEqiuvalences;

    assert(communication->nrSendCls == 0 && "cannot send clauses before initialization");
}

inline
void Solver::resetLastSolve()
{
    clearInterrupt();
    cancelUntil(0);
    budgetOff();
}


/** add a learned clause to the solver
 */
inline
bool Solver::addLearnedClause(vec<Lit>& ps, bool bump)
{
    assert(decisionLevel() == 0);
    if (!ok) { return false; }

    sort(ps);
    Lit p; int i, j;
    for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        if (value(ps[i]) == l_True || ps[i] == ~p) {
            return true;
        } else if (value(ps[i]) != l_False && ps[i] != p) {
            ps[j++] = p = ps[i];
        }
    ps.shrink_(i - j);

    if (ps.size() == 0) {
        return ok = false;
    } else if (ps.size() == 1) {
        uncheckedEnqueue(ps[0]);
        return ok = (propagate() == CRef_Undef);
    } else {
        CRef cr = ca.alloc(ps, true);
        if (bump) { ca[cr].activity() += cla_inc; }   // same as in claBumpActivity
        if (communicationClient.keepLonger) { ca[cr].resetCanBeDel(); }  // keep this clause for at least one removal round
        ca[cr].setLBD(communicationClient.lbdFactor > 0 ?
                      (double)ca[cr].size() * communicationClient.lbdFactor
                      : - communicationClient.lbdFactor * ((double)sumLearnedClauseLBD / (double)sumLearnedClauseSize) * (double)ca[cr].size()); // set LBD for received clause based on heuristics (TODO: also consider level cache)
        learnts.push(cr);
        attachClause(cr);
    }
    return true;
}

inline
unsigned Solver::currentDependencyLevel() const
{
    #ifdef PCASSO
    return communication == 0 ? 0 : communication->currentDependencyLevel();
    #else
    return 0;
    #endif
}

template<typename T> // can be either clause or vector
inline
#ifdef PCASSO
    int Solver::updateSleep(T* toSend, int toSendSize, int dependencyLevel, bool multiUnits, bool equivalences)
#else
    int Solver::updateSleep(T* toSend, int toSendSize, bool multiUnits, bool equivalences)
#endif
// int Solver::updateSleep(vec< Lit >* toSend, bool multiUnits)
{

    const bool localDebug = false; // for temporarly debugging this method

if (communication == 0) { return 0; }     // no communication -> do nothing!

// nothing to send, do only receive every reveiceEvery tries!
if (toSend == 0 && communicationClient.currentTries++ < communicationClient.receiveEvery) { return 0; }
communicationClient.currentTries = 0;

                                   // check current state, e.g. wait for master to do something!
if (!communication->isWorking()) {
if (communication->isAborted()) {
        interrupt();
        if (verbosity > 0)
            std::cerr << "c [THREAD] " << communication->getID()
                      << " aborted current search due to flag by master" << std::endl;
        return -1;
    }

    /* not working -> master interrupted thread
     * tell master that we reached this state
     * sleep until master changed something
     * wake up afterwards
     */
    communication->ownLock->lock();
    communication->setState(Communicator::waiting);
    // not unlock (avoid same error as in master!)

    communication->awakeMaster();

    std::cerr << "c [THREAD] " << communication->getID() << " wait for master to do something (sleep)" << std::endl;
    // wait until master changes the state again to working!

    while (! communication->isWorking()) {
        if (communication->isDoReceive()) {
            // goto level 0
            if (decisionLevel() != 0) { cancelUntil(0); }
            // add unit clauses from master as clauses of the formula!!
            communication->data->receiveUnits(communicationClient.receiveClause);
            for (int i = 0 ; i < communicationClient.receiveClause.size(); ++ i) {
                if (!addClause(communicationClient.receiveClause[i])) {    // this methods adds the units to the proof as well!
                    assert(false && "case that send unit clause makes the whole formula unsatisfiable is not handled - can this happen?");
                    break;                                  // Add a unit clause to the solver.
                }
            }
            // sleep again, so that master can make sure everybody saw the units!
            communication->setState(Communicator::finishedReceiving);
        }
        if (communication->isAborted()) { break; }
        communication->ownLock->sleep();
    }
    communication->ownLock->unlock();
    if (communication->isAborted()) {
        interrupt();
        return -1;
    }
}

// if there should not be communication
if (! communication->getDoSend() && (! communication->getDoReceive() || !communicationClient.doReceive)) { return 0; }

if (communication->getDoSend() && toSend != 0) {     // send

// count attempts
communication->nrSendCattempt = (!multiUnits && !equivalences) ? communication->nrSendCattempt + 1 : communication->nrSendCattempt;
    communication->nrSendMattempt = multiUnits ? communication->nrSendMattempt + 1 : communication->nrSendMattempt;
    communication->nrSendEattempt = equivalences ? communication->nrSendEattempt + 1 : communication->nrSendEattempt ;

    if (! communication->sendEquivalences && equivalences) {
// #warning remove after debug
//         cerr << "c reject equivalences due to disabled option" << endl;
        return 0;
    }  // do not share equivalences

    // check, whether the clause is allowed to be sent
    bool rejectSend = false;
    int keep = 0;
    for (int i = 0 ; i < toSendSize; ++ i) { // repeat until allowed, stay in clause
        const Var v = var((*toSend)[i]);   // get variable to analyze
        rejectSend = (!communicationClient.sendDecModel && varFlags[v].delModels) || (!communicationClient.sendIncModel && varFlags[v].addModels);

        if (!rejectSend) { (*toSend)[keep++] = (*toSend)[i]; } // keep literal
        else { // otherwise check how to proceed with variable that is
            if (multiUnits || equivalences) { continue; } // jump over this literal, so that it is not shared
            else { break; }   // do not share a clause with that literal
        }
#warning: check here, whether the clause contains a variable that is too high, and hence should not be sent (could be done via above flags)
    }
    if (rejectSend && !multiUnits && !equivalences) {
        return 0;
    }  // do nothing here, as there are variables in the clause that should not be sent
    else {
        if (keep <= 1 && equivalences) { return 0; }      // do not share equivalence of one literal
        else if (keep == 0 && multiUnits) {    // do not share "no" units
            return 0;
        }
    }

    toSendSize = keep; // set to number of elements that can be shared

    if (!multiUnits && !equivalences && !communicationClient.sendAll) {   // check sharing limits, if its not units, and no equivalences
        // calculated size of the send clause
        int s = 0;
        if (communication->variableProtection()) {   // check whether there are protected literals inside the clause!
            for (int i = 0 ; i < toSendSize; ++ i) {
                s = communication->isProtected((*toSend)[i]) ? s : s + 1;
            }
        } else { s = toSendSize; }

        // filter sending:
        if (toSendSize > communicationClient.currentSendSizeLimit) {
            updateDynamicLimits(true);    // update failed limits! (only happens if we share a clause)
            communication->nrRejectSendSizeCls++;
            return 0; // TODO: parameter, adaptive?
        }

        if (s > 0) {
            // calculate LBD value
            int lbd = computeLBD(*toSend, toSendSize);

            if (lbd > communicationClient.currentSendLbdLimit) {
                updateDynamicLimits(true);    // update failed limits! (only happens if we share a clause)
                communication->nrRejectSendLbdCls++;
                return 0; //TODO: parameter (adaptive?)
            }
        }
    }

    communication->nrSendMultiUnits = (multiUnits ? communication->nrSendMultiUnits + toSendSize : communication->nrSendMultiUnits);
    communication->nrSendEEs = (equivalences ? communication->nrSendEEs + toSendSize : communication->nrSendEEs);


    #ifdef PCASSO
    VariableInformation vi(varFlags, communicationClient.sendIncModel, communicationClient.sendDecModel);    // setup variable information object
    communication->addClause(*toSend, toSendSize, dependencyLevel, vi, multiUnits, equivalences);
    #else
    communication->addClause(*toSend, toSendSize, multiUnits, equivalences);
    #endif
    if (! equivalences && !multiUnits) {  // update limits only if a clause was sent
        updateDynamicLimits(false); // a clause could be send
        communication->nrSendCls++;
    }

} else if (communication->getDoReceive() && communicationClient.doReceive) {           // receive (only at level 0)

// TODO: add parameter that forces to restart!
communication->nrReceiveAttempts ++;

// not at level 0? nothing to do
if (decisionLevel() != 0) { return 0; }   // receive clauses only at level 0!

    VariableInformation vi(varFlags, communicationClient.sendIncModel, communicationClient.sendDecModel);    // setup variable information object
    communicationClient.receiveClauses.clear();  // prepare for receive
    communicationClient.receivedUnits.clear();
    communicationClient.receivedEquivalences.clear();
    #ifdef PCASSO
    communicationClient.unitDependencies.clear();
    communicationClient.eeDependencies.clear();
    communication->receiveClauses(ca, communicationClient.receiveClauses, communicationClient.receivedUnits, communicationClient.unitDependencies,
                                  communicationClient.receivedEquivalences, communicationClient.eeDependencies, vi); // receive multiple things!
    #else
    communication->receiveClauses(ca, communicationClient.receiveClauses, communicationClient.receivedUnits, communicationClient.receivedEquivalences, vi); // receive multiple things!
    #endif
    // if( communicationClient.receiveClauses.size()  > 5 ) std::cerr << "c [THREAD] " << communication->getID() << " received " << communicationClient.receiveClauses.size() << " clauses." << std::endl;
    communicationClient.succesfullyReceived += communicationClient.receiveClauses.size();

    if (communicationClient.receivedUnits.size() > 0) { // prcess all unit clauses
        cancelUntil(0);   // jump to level 0!
        for (int i = 0 ; i < communicationClient.receivedUnits.size() ; ++ i) {
            const Lit& unit = communicationClient.receivedUnits[i];
            if (value(unit) == l_False) { // we found a conflict
                #ifdef PCASSO
//                 assert(false && "take care to set the unsatPTlevel correctly!");
#warning take care to set the unsatPTlevel correctly! for upward sharing!
                #endif
                return 1;
            } else if (value(unit) == l_Undef) {
                #ifdef PCASSO
                uncheckedEnqueue(unit, communicationClient.unitDependencies[i]);  // enqueue unit with dependency
                #else
                uncheckedEnqueue(unit); // enqueue unit
                #endif
                if (propagate() != CRef_Undef) { return 1; }  // report conflict, if necessary
            }
        }
        communication->nrReceivedMultiUnits += communicationClient.receivedUnits.size();
    } // finished processing units

    if (communicationClient.receivedEquivalences.size() > 0) {  // process all equivalent literals
        if (communicationClient.receiveEE) {
            #ifdef PCASSO
            eqInfo.addEquivalenceClass(communicationClient.receivedEquivalences,  communicationClient.eeDependencies, false);  // use fast-import for pcasso, do not share again
            #else
            eqInfo.addEquivalenceClass(communicationClient.receivedEquivalences, false);  // tell object about these equivalences, do not share again, ignore setting a dependency
            #endif
            communication->nrReceivedEEs += communicationClient.receivedEquivalences.size();
        } else {  // do not receive EE, hence, simply clear the vectors again
            communicationClient.receivedEquivalences.clear();
            communicationClient.eeDependencies.clear();
        }
    }

    if (qhead < trail.size()) {                 // if we have something to be propagated
        if (propagate() != CRef_Undef) { return 1; }  // return the error, if there was an error
    }

    for (unsigned i = 0 ; i < communicationClient.receiveClauses.size(); ++ i) {
        Clause& c = ca[ communicationClient.receiveClauses[i] ]; // take the clause and propagate / enqueue it

        if (c.size() < 2) {
            if (c.size() == 0) {
//                 std::cerr << "c empty clause has been shared!" << std::endl;
                ok = false; return 1;
            }
            // has to be unit clause!
            addUnitToProof(c[0]); // add the clause to the proof
            if (value(c[0]) == l_Undef) {
                uncheckedEnqueue(c[0]);
                ok = (propagate() == CRef_Undef);
                if (!ok) {   // adding this clause failed?
//        std::cerr << "c adding received clause failed" << std::endl;
                    return 1;
                }
            } else if (value(c[0]) == l_False) {
                ok = false; return 1;
            }
            // in case of SAT simply mark the clause as being irrelevant
            c.mark(1);

        } else {
            for (int j = 0 ; j < c.size(); ++ j) {   // propagate inside the clause!
                if (var(c[j]) > nVars()) {
//                     std::cerr << "c shared variable " << var(c[j]) << "[" << j << "] is greater than " << nVars() << std::endl;
                    assert(false && "received variables have to be smaller than maximum!");
                }
//                 cerr << "c look at literal " << c[j] << " sat: " << (value(c[j]) == l_True) << " unsat: " << (value(c[j]) == l_False) << endl;
                if (value(c[j]) == l_True) { c.mark(1); break; }     // this clause is already satisfied -> remove it! (we are on level 0!)
                else if (value(c[j]) == l_False) { c[j] = c[ c.size() - 1 ]; --j; c.shrink(1); }     // this literal is mapped to false -> remove it! repeat check for this position
                else {
                    assert(level(var(c[j])) != 0 && "all level 0 variables have to be removed here already");
                }
            }
            // TODO: here could be more filters for received clauses to be rejected (e.g. PSM?!)
            if (c.mark() == 0) {
                // set isAnalzyed and isPropagated, such that this clause is not sent again
                c.setPropagated();
                c.setUsedInAnalyze();

                // perform vivification only with clauses that are at least binary, afterwards handle the shortened clause correctly
                if (c.size() > 1 && communicationClient.refineReceived) {
                    if (! reverseMinimization.enabled) {  // enable reverseMinimization to be able to use it
//                         DOUT( cerr << "c initialize reverseMinimization during receiving" << endl; );
                        reverseMinimization.enabled = true;
                        reverseMinimization.assigns.growTo(nVars() + 1, l_Undef); // grow assignment
                        reverseMinimization.trail.capacity(nVars()  + 1);       // allocate trail
                    }
                    unsigned int lbd = c.size();
                    const unsigned int beforeSize = c.size();
                    bool shrinkedClause = false;
                    #ifdef PCASSO
                    unsigned dependency = c.getPTLevel();
                    shrinkedClause = reverseLearntClause(c, lbd, dependency);
                    if (shrinkedClause) {
                        c.setPTLevel(dependency);   // update dependency accordingly
                    }
                    #else
                    unsigned dependency = currentDependencyLevel();
                    DOUT(if (localDebug) {
                    cerr << "c received clause(" << i << "): " << c << endl;
                    {
                        stringstream s;
                        s << "c sat: ";
                        for (int k  = 0 ; k < c.size(); ++ k) { s << " " << (value(c[k]) == l_True) << "/" << (value(c[k]) == l_False); }
                            s << endl;
                            cerr << s.str();
                        }
                    });
                    shrinkedClause = reverseLearntClause(c, lbd, dependency);
                    #endif
//            cerr << "c shrinked received clause(" << i << "): " << c << " first sat: " << (value(c[0]) == l_True) << endl;
                    communication->vivifiedLiterals += beforeSize - c.size();
                    if (shrinkedClause && communicationClient.resendRefined) {  // re-share clause (should be performed only by one thread per group
//          cerr << "c recend received clause(" << i << "): " << c << endl;
                        #ifdef PCASSO
                        updateSleep(&(c), c.size(), dependency);
                        #else
                        updateSleep(&(c), c.size());
                        #endif
                    }
                    // could re-share clause, but another thread might also shorten the clause, and hence this would be useless
                }

                communication->nrReceivedCls ++;
                if (c.size() == 0) { ok = false; return 1; }
                else if (c.size() == 1) {
                    addUnitToProof(c[0]); // add the clause to the proof
                    if (value(c[0]) == l_Undef) { uncheckedEnqueue(c[0]); }
                    else if (value(c[0]) == l_False) {
                        ok = false; return 1;
                    }
                    c.mark();
                    ok = (propagate() == CRef_Undef);
                    if (!ok) { return 1; }
                } else { // attach the clause, if its not a unit clause!
                    addToProof(ca[communicationClient.receiveClauses[i]]);   // the shared clause stays in the solver, hence add this clause to the proof!
                    learnts.push(communicationClient.receiveClauses[i]);
                    if (communication->doBumpClauseActivity) {
                        ca[communicationClient.receiveClauses[i]].activity() += cla_inc;    // increase activity of clause
                    }

                    if (communicationClient.keepLonger) { ca[communicationClient.receiveClauses[i]].resetCanBeDel(); }  // keep this clause for at least one removal round
                    ca[communicationClient.receiveClauses[i]].setLBD(communicationClient.lbdFactor > 0 ?
                            (double)ca[communicationClient.receiveClauses[i]].size() * communicationClient.lbdFactor
                            : - communicationClient.lbdFactor * ((double)sumLearnedClauseLBD / (double)sumLearnedClauseSize) * (double)ca[communicationClient.receiveClauses[i]].size()); // set LBD for received clause based on heuristics (TODO: also consider level cache)

                    attachClause(communicationClient.receiveClauses[i]);
                }
            }
        }
        if (qhead < trail.size()) {                 // if we have something to be propagated
            if (propagate() != CRef_Undef) { return 1; }  // return the error, if there was an error
        }
    }
}
// everything worked nicely
return 0;
}

inline
void Solver::updateDynamicLimits(bool failed, bool sizeOnly)
{
    if (!communicationClient.useDynamicLimits) { return; }  // do not do anything with the ratios
    if (! failed) { communicationClient.succesfullySend ++; }

    bool fulfillRatio = (double) conflicts * communicationClient.sendRatio < communicationClient.succesfullySend; // send more than ratio clauses?

    // fail -> increase geometrically, success, decrease geometrically!
    communicationClient.currentSendSizeLimit = (failed ? communicationClient.currentSendSizeLimit * (1.0 + communicationClient.sizeChange) : communicationClient.currentSendSizeLimit - communicationClient.currentSendSizeLimit * communicationClient.sizeChange);
    // check bound
    communicationClient.currentSendSizeLimit = communicationClient.currentSendSizeLimit < communicationClient.sendSize    ? communicationClient.sendSize    : communicationClient.currentSendSizeLimit;
    communicationClient.currentSendSizeLimit = communicationClient.currentSendSizeLimit > communicationClient.sendMaxSize ? communicationClient.sendMaxSize : communicationClient.currentSendSizeLimit;

    if (fulfillRatio) { initLimits(); }   // we have hit the ratio, tigthen limits again!

//   if( sizeOnly ) {
//     communicationClient.currentSendLbdLimit = communicationClient.currentSendLbdLimit < communicationClient.sendLbd    ? communicationClient.sendLbd    : communicationClient.currentSendLbdLimit;
//     communicationClient.currentSendLbdLimit = communicationClient.currentSendLbdLimit > communicationClient.sendMaxLbd ? communicationClient.sendMaxLbd : communicationClient.currentSendLbdLimit;
//     return;
//   }

    // fail -> increase geometrically, success, decrease geometrically!
    communicationClient.currentSendLbdLimit = (failed ? communicationClient.currentSendLbdLimit * (1.0 + communicationClient.lbdChange) : communicationClient.currentSendLbdLimit - communicationClient.currentSendLbdLimit * communicationClient.lbdChange);
    // check bound
    communicationClient.currentSendLbdLimit = communicationClient.currentSendLbdLimit < communicationClient.sendLbd    ? communicationClient.sendLbd    : communicationClient.currentSendLbdLimit;
    communicationClient.currentSendLbdLimit = communicationClient.currentSendLbdLimit > communicationClient.sendMaxLbd ? communicationClient.sendMaxLbd : communicationClient.currentSendLbdLimit;

    return;
}

inline
void Solver::initVariableProtection()
{
    if (communication != 0) {
        communication->initProtect(assumptions, nVars());
    }
}

inline
void Solver::initLimits()
{
    communicationClient.currentSendSizeLimit = communication->sendSize;
    communicationClient.currentSendLbdLimit  = communication->sendLbd;
}

}

#endif
