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
    communicationClient.useDynamicLimits = communication->useDynamicLimits;
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
        learnts.push(cr);
        attachClause(cr);
    }
    return true;
}

template<typename T> // can be either clause or vector
inline
int Solver::updateSleep(const T* toSend, bool multiUnits)
// int Solver::updateSleep(vec< Lit >* toSend, bool multiUnits)
{
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
    if (! communication->getDoSend() && ! communication->getDoReceive()) { return 0; }

    if (communication->getDoSend() && toSend != 0) {     // send
 
        // check, whether the clause is allowed to be sent
        bool rejectSend = false;
        for (int i = 0 ; !rejectSend && i < toSend->size(); ++ i) { // repeat until allowed, stay in clause
	  const Var v = var( (*toSend)[i] ); // get variable to analyze
	  rejectSend = (!communicationClient.sendDecModel && varFlags[v].delModels) || (!communicationClient.sendIncModel && varFlags[v].addModels);
#warning: check here, whether the clause contains a variable that is too high, and hence should not be sent (could be done via above flags)
	}
	if( rejectSend ) return 0; // do nothing here, as there are variables that should not be sent
      
        // calculated size of the send clause
        int s = 0;
        if (communication->variableProtection()) {   // check whether there are protected literals inside the clause!
            for (int i = 0 ; i < toSend->size(); ++ i) {
                s = communication->isProtected((*toSend)[i]) ? s : s + 1;
            }
        } else { s = toSend->size(); }

        // filter sending:
        if (toSend->size() > communicationClient.currentSendSizeLimit) {
            updateDynamicLimits(true);    // update failed limits!
            communication->nrRejectSendSizeCls++;
            return 0; // TODO: parameter, adaptive?
        }

        if (s > 0) {
            // calculate LBD value
            int lbd = computeLBD(*toSend);

            if (lbd > communicationClient.currentSendLbdLimit) {
                updateDynamicLimits(true);    // update failed limits!
                communication->nrRejectSendLbdCls++;
                return 0; //TODO: parameter (adaptive?)
            }
        }
        communication->addClause(*toSend);
        updateDynamicLimits(false); // a clause could be send
        communication->nrSendCls++;

    } else if (communication->getDoReceive()) {         // receive (only at level 0)

        // TODO: add parameter that forces to restart!
        // if( communication->

        // not at level 0? nothing to do
        if (decisionLevel() != 0) { return 0; }   // receive clauses only at level 0!

        communicationClient.receiveClauses.clear();
        communication->receiveClauses(ca, communicationClient.receiveClauses);
        // if( communicationClient.receiveClauses.size()  > 5 ) std::cerr << "c [THREAD] " << communication->getID() << " received " << communicationClient.receiveClauses.size() << " clauses." << std::endl;
        communicationClient.succesfullySend += communicationClient.receiveClauses.size();
        for (unsigned i = 0 ; i < communicationClient.receiveClauses.size(); ++ i) {
            Clause& c = ca[ communicationClient.receiveClauses[i] ]; // take the clause and propagate / enqueue it

            if (c.size() < 2) {
                if (c.size() == 0) {
                    std::cerr << "c empty clause has been shared!" << std::endl;
                    ok = false; return 1;
                }
                // has to be unit clause!
                addUnitToProof(c[0]); // add the clause to the proof
                if (value(c[0]) == l_Undef) { uncheckedEnqueue(c[0]); }
                else if (value(c[0]) == l_False) {
                    ok = false; return 1;
                }
                c.mark();
                ok = (propagate() == CRef_Undef);
                if (!ok) {   // adding this clause failed?
                    std::cerr << "c adding received clause failed" << std::endl;
                    return 1;
                }
            } else {
                for (int j = 0 ; j < c.size(); ++ j) {   // propagate inside the clause!
                    if (var(c[j]) > nVars()) {
                        std::cerr << "c shared variable " << var(c[j]) << "[" << j << "] is greater than " << nVars() << std::endl;
                        assert(false && "received variables have to be smaller than maximum!");
                    }
                    if (value(c[j]) == l_True) { c.mark(1); break; }     // this clause is already satisfied -> remove it! (we are on level 0!)
                    else if (value(c[j]) == l_False) { c[j] = c[ c.size() - 1 ]; c.shrink(1); }     // this literal is mapped to false -> remove it!
                }
                // TODO: here could be more filters for received clauses to be rejected (e.g. PSM?!)
                if (!c.mark()) {
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
                        attachClause(communicationClient.receiveClauses[i]);
                    }
                }
            }
        }
    }
    // everything worked nicely
    return 0;
}

inline
void Solver::updateDynamicLimits(bool failed, bool sizeOnly)
{
    if( !communicationClient.useDynamicLimits ) return; // do not do anything with the ratios
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
