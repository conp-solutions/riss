/****************************************************************************[CommunicatorClient.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COMMUNICATORCLIENT_H
#define COMMUNICATORCLIENT_H

#include "riss/core/Communication.h"
#include "riss/core/Solver.h"
#include "riss/mtl/Sort.h"


namespace Riss {

class CommunicatorClient {
     Solver* solver; /// pointer to the instance of the solver with which we work

     Communicator* communication; /// communication with the outside, and control of this solver
     
public:
  
    /** sets up the communication client */
    CommunicatorClient( Solver* _solver, Communicator* _communicator=0);
  
      /** setup the communication object
     * @param comm pointer to the communication object that should be used by this thread
     */
    void setCommunication( Communicator* comm );
    
    /** goto sleep, wait until master did updates, wakeup, process the updates
     * @param toSend if not 0, send the (learned) clause, if 0, receive shared clauses
     * note: can also interrupt search and incorporate new assumptions etc.
     * @return -1 = abort, 0=everythings nice, 1=conflict/false result
     */
    int updateSleep(vec<Lit>* toSend);
    
private: 
    
    /** add a learned clause to the solver
     * @param bump increases the activity of the current clause to the last seen value
     * note: behaves like the addClause structure
     */
    bool addLearnedClause(Riss::vec< Riss::Lit >& ps, bool bump);
    
    /** update the send limits based on whether a current clause could have been send or not
     * @param failed sending current clause failed because of limits
     * @param sizeOnly update only size information
     */
    void updateDynamicLimits( bool failed, bool sizeOnly = false );
    
    /** inits the protection environment for variables
     * Note: should not be necessary in this project ;)
     */
    void initVariableProtection();
    
    /** set send limits once!
     */
    void initLimits();
    
    /*
     * temporary structures
     */
    vec<Lit> receiveClause;			/// temporary placeholder for receiving clause
    std::vector< CRef > receiveClauses;	/// temporary placeholder indexes of the clauses that have been received by communication
    int currentTries;                          /// current number of waits
    int receiveEvery;                          /// do receive every n tries
    
    float currentSendSizeLimit;                /// dynamic limit to control send size
    float currentSendLbdLimit;                 /// dynamic limit to control send lbd
    int succesfullySend;                       /// number of clauses that have been sucessfully transmitted
    float sendSize;                            /// Minimum Lbd of clauses to send  (also start value)
    float sendLbd;                             /// Minimum size of clauses to send (also start value)
    float sendMaxSize;                         /// Maximum size of clauses to send
    float sendMaxLbd;                          /// Maximum Lbd of clauses to send
    float sizeChange;                          /// How fast should size send limit be adopted?
    float lbdChange;                           /// How fast should lbd send limit be adopted?
    float sendRatio;                           /// How big should the ratio of send clauses be?
    
// [END] modifications for parallel assumption based solver
};

CommunicatorClient::CommunicatorClient(Solver* _solver, Communicator* _communicator)
:   solver(_solver)
  , communication(_communicator)
  , currentTries (0)
  , receiveEvery (128)
  , currentSendSizeLimit(0)                /// dynamic limit to control send size
  , currentSendLbdLimit(0)
  , succesfullySend(0)
  , sendSize(10)
  , sendLbd( 5)
  , sendMaxSize( 128)
  , sendMaxLbd(32)
  , sizeChange(0.15)
  , lbdChange(0.2)
  , sendRatio (0.1) 

{

}

inline void CommunicatorClient::setCommunication(Communicator* comm)
{
  assert ( communication == 0 && "Cannot overwrite previously set communicator!" );
  communication = comm;
  
  // TODO: receive parameters here?
}



/** add a learned clause to the solver
 */
bool CommunicatorClient::addLearnedClause(vec<Lit>& ps, bool bump)
{
    assert(decisionLevel() == 0);
    if (!solver->ok) return false;

    sort(ps);
    Lit p; int i, j;
    for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        if (solver->value(ps[i]) == l_True || ps[i] == ~p)
            return true;
        else if (solver->value(ps[i]) != l_False && ps[i] != p)
            ps[j++] = p = ps[i];
    ps.shrink(i - j);

    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1){
        solver->uncheckedEnqueue(ps[0]);
        return ok = (solver->propagate() == CRef_Undef);
    }else{
        CRef cr = solver->ca.alloc(ps, true);
	if( bump ) solver->ca[cr].activity() += cla_inc; // same as in claBumpActivity
        solver->learnts.push(cr);
        solver->attachClause(cr);
    }
    return true;
}


int CommunicatorClient::updateSleep(vec<Lit>* toSend)
{
  if( communication == 0 ) return 0;
  
  // nothing to send, do only receive every reveiceEvery tries!
  if( toSend == 0 && currentTries++ < receiveEvery) return 0;
  currentTries = 0;
  
  // check current state, e.g. wait for master to do something!
  if( !communication->isWorking() )
  {
    if( communication->isAborted() ) {
      solver->interrupt();
      if (verbosity > 0)
        cerr << "c [THREAD] " << communication->getID()
             << " aborted current search due to flag by master" << endl;
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

    cerr << "c [THREAD] " << communication->getID() << " wait for master to do something (sleep)" << endl;
    // wait until master changes the state again to working!

    while( ! communication->isWorking() ){
      if( communication->isDoReceive() ) {
        // goto level 0
        if( solver->decisionLevel() != 0 ) solver->cancelUntil(0);
        // add unit clauses from master as clauses of the formula!!
        communication->data->receiveUnits( receiveClause );
        for( int i = 0 ; i < receiveClause.size(); ++ i ) {
          if( !solver->addClause (receiveClause[i]) ) {
            assert( false && "case that send unit clause makes the whole formula unsatisfiable is not handled - can this happen?");
            break;                                  // Add a unit clause to the solver.
          }
        }
        // sleep again, so that master can make sure everybody saw the units!
        communication->setState(Communicator::finishedReceiving);
      }
      if( communication->isAborted() ) break;
      communication->ownLock->sleep();
    }
    communication->ownLock->unlock();
    if( communication->isAborted() ) {
      solver->interrupt();
      return -1;
    }
  }

  // if there should not be communication
  if( ! communication->getDoSend() && ! communication->getDoReceive() ) return 0;

  if( communication->getDoSend() && toSend != 0 )      // send
  {
    // clause: 
    int levels [ toSend->size() ];

    // calculated size of the send clause    
    int s = 0;
    if( communication->variableProtection() ) {  // check whether there are protected literals inside the clause!
      for( int i = 0 ; i < toSend->size(); ++ i )
        s = communication->isProtected( (*toSend)[i] ) ? s : s+1;
    } else s = toSend->size();
      
    // filter sending:
    if( toSend->size() > currentSendSizeLimit ) {
      updateDynamicLimits ( true ); // update failed limits!
      communication->nrRejectSendSizeCls++;
      return 0; // TODO: parameter, adaptive?
    }

    if( s > 0 ) {
      // calculate LBD value
      if( communication->variableProtection() ) {
        int j = 0;
	for( int i = 0 ; i < toSend->size(); ++ i )
	  if( !communication->isProtected( (*toSend)[i] ) )
            levels[j++] = solver->level( var((*toSend)[i]));  // just consider the variable that are not protected!
      } else {
        for( int i = 0 ; i < toSend->size(); ++ i )
          levels[i] = solver->level( var((*toSend)[i]));
      }


      // insertionsort
      for( int i = 1; i < s; ++i ) {
        unsigned p = i; int l = levels[i];
        for( int j = i+1; j < s; ++ j ) {
          if( levels[j] > l ) {
            p = j; l = levels[j];
          }
        }
        int ltmp = levels[i]; levels[i] = levels[p]; levels[p] = ltmp;
      }

      int lbd = 1;
      for( int i = 1; i < s; ++ i ) {
        lbd = ( levels[i-1] == levels[i] ) ? lbd : lbd + 1;
      }
      if( lbd > currentSendLbdLimit ) {
	updateDynamicLimits ( true ); // update failed limits!
	communication->nrRejectSendLbdCls++;
	return 0; //TODO: parameter (adaptive?)
      }
    }
    communication->addClause(*toSend);
    updateDynamicLimits(false); // a clause could be send
    communication->nrSendCls++;

  } else if( communication->getDoReceive() ) {        // receive (only at level 0)

    // TODO: add parameter that forces to restart!
    // if( communication->

    // not at level 0? nothing to do
    if( solver->decisionLevel() != 0 ) return 0;

    receiveClauses.clear();
    communication->receiveClauses( ca, receiveClauses );
    
    // if( receiveClauses.size()  > 5 ) cerr << "c [THREAD] " << communication->getID() << " received " << receiveClauses.size() << " clauses." << endl;

    for( unsigned i = 0 ; i < receiveClauses.size(); ++ i ) {
      Clause& c = ca[ receiveClauses[i] ]; // take the clause and propagate / enqueue it
      
      if (c.size() < 2) {
	if( c.size() == 0 ) {
	  cerr << "c empty clause has been shared!" << endl;
	  ok = false; return 1; 
	}
        solver->uncheckedEnqueue(c[0]);
	c.mark();
        ok = (solver->propagate() == CRef_Undef);
	if( !ok ) { // adding this clause failed?
          cerr << "c adding received clause failed" << endl;
          return 1; 
	}
      } else {
	for( int j = 0 ; j < c.size(); ++ j ) { // propagate inside the clause!
          if( var(c[j]) > solver->nVars() ) {
	    cerr << "c shared variable " << var( c[j] ) << "[" << j << "] is greater than " << nVars() << endl;
	   assert( false && "received variables have to be smaller than maximum!" );
	  }
	  if( solver->value( c[j] ) == l_True ) { c.mark(1); break; } // this clause is already satisfied -> remove it! (we are on level 0!)
	  else if ( solver->value( c[j] ) == l_False ) { c[j] = c[ c.size() -1 ]; c.shrink(1); } // this literal is mapped to false -> remove it!
	}
	// TODO: here could be more filters for received clauses to be rejected (e.g. PSM?!)
	if( !c.mark() ) communication->nrReceivedCls ++;
        if( c.size() == 0 ) { ok = false; return 1; }
        else if ( c.size() == 1 ) {
          solver->uncheckedEnqueue(c[0]);
	  c.mark();
          ok = (solver->propagate() == CRef_Undef);
	  if( !ok ) return 1; 
	} else { // attach the clause, if its not a unit clause!
          solver->learnts.push(receiveClauses[i]);
	  if( communication->doBumpClauseActivity )
	    solver->ca[receiveClauses[i]].activity() += solver->cla_inc; // increase activity of clause
          solver->attachClause(receiveClauses[i]);
	}
      }
    }
  }
  // everything worked nicely
  return 0;
}

void CommunicatorClient::updateDynamicLimits( bool failed, bool sizeOnly )
{
  if( ! failed ) succesfullySend ++;
  
  bool fulfillRatio = (double) conflicts * sendRatio < succesfullySend; // send more than ratio clauses?
  
  // fail -> increase geometrically, success, decrease geometrically!
  currentSendSizeLimit = (failed ? currentSendSizeLimit * (1.0 + sizeChange) : currentSendSizeLimit - currentSendSizeLimit * sizeChange );
  // check bound
  currentSendSizeLimit = currentSendSizeLimit < sendSize    ? sendSize    : currentSendSizeLimit;
  currentSendSizeLimit = currentSendSizeLimit > sendMaxSize ? sendMaxSize : currentSendSizeLimit;
  
  if( fulfillRatio ) initLimits(); // we have hit the ratio, tigthen limits again!
  
//   if( sizeOnly ) {
//     currentSendLbdLimit = currentSendLbdLimit < sendLbd    ? sendLbd    : currentSendLbdLimit;  
//     currentSendLbdLimit = currentSendLbdLimit > sendMaxLbd ? sendMaxLbd : currentSendLbdLimit;  
//     return;
//   }
  
  // fail -> increase geometrically, success, decrease geometrically!
  currentSendLbdLimit = (failed ? currentSendLbdLimit * (1.0 + lbdChange) : currentSendLbdLimit - currentSendLbdLimit * lbdChange );
  // check bound
  currentSendLbdLimit = currentSendLbdLimit < sendLbd    ? sendLbd    : currentSendLbdLimit;  
  currentSendLbdLimit = currentSendLbdLimit > sendMaxLbd ? sendMaxLbd : currentSendLbdLimit;  
  
  return;
}

void CommunicatorClient::initVariableProtection() {
  if( communication != 0 )
      communication->initProtect( assumptions, nVars() );
}

void CommunicatorClient::initLimits() {
  currentSendSizeLimit = communication->sendSize;
  currentSendLbdLimit  = communication->sendLbd;
}

};

#endif