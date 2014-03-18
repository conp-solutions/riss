/***************************************************************************************[PSolver.h]
Copyright (c) 2014,      Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "pfolio-src/PSolver.h"

#include <assert.h>

namespace Minisat {
  

/** main method that is executed by all worker threads */
static void* runWorkerSolver (void* data);
  
PSolver::PSolver(const int threadsToUse)
: initialized( false ), threads( threadsToUse )
, data(0)
, threadIDs( 0 )
, verbosity(3)
, verbEveryConflicts(0)
{
    // setup the default configuration for all the solvers!
    configs   = new CoreConfig        [ threads ];
    ppconfigs = new CP3Config         [ threads ];
    communicators = new Communicator* [ threads ];
    for( int i = 0 ; i < threads; ++ i ) {
      communicators[i] = 0;
    }

    // set preset configs here
    createThreadConfigs();
    
    // setup the first solver!
    solvers.push( new Solver( configs[0] ) ); // from this point on the address of this configuration is not allowed to be changed any more!
    solvers[0]->setPreprocessor( &ppconfigs[0] );
}

PSolver::~PSolver() 
{
  
  kill();
  
  sleep(0.2);
  
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    delete solvers[i]; // free all solvers
    if( communicators != 0 && communicators[i] != 0 ) delete communicators[i];
  }
  if( communicators != 0 ) { delete [] communicators; communicators = 0; }
  delete [] ppconfigs; ppconfigs = 0;
  delete [] configs; configs = 0;
  solvers.clear(true);
  
  if( threadIDs != 0 ) { delete [] threadIDs; threadIDs = 0; }
  if( data != 0 ) { delete data; data = 0; }
}

CoreConfig& PSolver::getConfig(const int solverID)
{
  return configs[solverID];
}

CP3Config& PSolver::getPPConfig(const int solverID)
{
  return ppconfigs[ solverID ];
}

int PSolver::nVars() const
{
  return solvers[0]->nVars();
}

int PSolver::nClauses() const
{
  return solvers[0]->nClauses();
}

int PSolver::nTotLits() const
{
  return solvers[0]->nTotLits();
}

Var PSolver::newVar(bool polarity, bool dvar, char type)
{
  return solvers[0]->newVar( polarity, dvar, type );
}

void PSolver::reserveVars(Var v)
{
  solvers[0]->reserveVars(v);
}

bool PSolver::simplify()
{
  return solvers[0]->simplify();
}

void PSolver::interrupt()
{
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    solvers[i]->interrupt();
  }
}

void PSolver::setConfBudget(int64_t x)
{
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    solvers[i]->setConfBudget(x);
  }
}

void PSolver::budgetOff()
{
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    solvers[i]->budgetOff();
  }
}


bool PSolver::addClause_(vec< Lit >& ps)
{
  bool ret = true;
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    for( int j = 0 ; j < ps.size(); ++ j ) {
      while( solvers[i]->nVars() <= var(ps[j]) ) solvers[i]->newVar();
    }
    bool ret2 = solvers[i]->addClause_(ps); // if a solver failed adding the clause, then the state for all solvers is bad as well
    ret = ret2 && ret; 
  }
  return ret;
}

lbool PSolver::solveLimited(const vec< Lit >& assumps)
{
//   // for now, simply let the first solver solve the formula!
//   lbool ret = solvers[0]->solveLimited( assumps );
//   
//   int winningSolver = 0;
//   if( ret == l_True ) {
//     model.clear();
//     model.capacity( solvers[winningSolver]->model.size() );
//     for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) model.push(solvers[winningSolver]->model[i]);
//   } else if ( ret == l_False ) {
//     conflict.clear();
//     conflict.capacity( solvers[winningSolver]->conflict.size() );
//     for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) conflict.push(solvers[winningSolver]->conflict[i]);    
//   } else {
//     
//   }
//   
//   return ret;
  
  int winningSolver = 0;
  lbool ret = l_Undef;
  /* TODO
   * 
   * preprocess the formula with the preprocessor of the first solver
   */
   ret = solvers[0]->preprocess();
   // solved by simplification?
   if( ret == l_False ){ return ret; }
   else if ( ret == l_True ) {
     winningSolver = 0;
     model.clear();
     model.capacity( solvers[winningSolver]->model.size() );
     for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) model.push(solvers[winningSolver]->model[i]);
   }
  
   if( ! initialized ) { // distribute the formula, if this is the first call to this method!

   /* setup all solvers
   * setup the communication system for the solvers, including the number of commonly known variables
   */
    if( initializeThreads () ) {
      cerr << "c initialization of " << threads << " threads failed" << endl;
      return l_Undef;
    }
   
   /* 
    * copy the formula from the first solver to all the other solvers
    */
   
    for( int i = 1; i < solvers.size(); ++ i ) {
      solvers[i]->reserveVars( solvers[0]->nVars() );
      while( solvers[i]->nVars() < solvers[0]->nVars() ) solvers[i]->newVar();
      communicators[i]->setFormulaVariables( solvers[0]->nVars() ); // tell which variables can be shared
      for( int j = 0 ; j < solvers[0]->clauses.size(); ++ j ) {
	solvers[i]->addClause( solvers[0]->ca[ solvers[0]->clauses[j] ] ); // import the clause of solver 0 into solver i
      }
      for( int j = 0 ; j < solvers[0]->learnts.size(); ++ j ) {
	solvers[i]->addClause( solvers[0]->ca[ solvers[0]->learnts[j] ] ); // import the learnt clause of solver 0 into solver i
      }
      solvers[i]->addUnitClauses( solvers[0]->trail ); // copy all the unit clauses
      cerr << "c Solver[" << i << "] has " << solvers[i]->nVars() << " vars, " << solvers[i]->clauses.size() << " cls, " << solvers[i]->learnts.size() << " learnts" << endl;
    }
    initialized = true;
   } else {
     // already initialized -- simply print the summary
     for( int i = 0; i < solvers.size(); ++ i ) {
      cerr << "c Solver[" << i << "] has " << solvers[i]->nVars() << " vars, " << solvers[i]->clauses.size() << " cls, " << solvers[i]->learnts.size() << " learnts" << endl;
     }
   }
   /*
    * reset solvers from previous call,
    * run the solvers on the formula
    */
   // add assumptions to all solvers
   for( int i = 0 ; i < threads; ++i ) {
     for( int j = 0; j < assumps.size(); ++ j ) { // make sure, everybody knows all the variables
      while( solvers[i]->nVars() <= var(assumps[j]) ) solvers[i]->newVar();
     }
     assumps.copyTo( communicators[i]->assumptions );
     communicators[i]->setFormulaVariables( solvers[i]->newVar() ); // for incremental calls, no ER is supported, so that everything should be fine until here! Note: be careful with this!
     communicators[i]->setWinner( false );
     assert( (communicators[i]->isFinished() || communicators[i]->isWaiting() ) && "all solvers should not touch anything!" );
   }
   start(); // allow all solvers to start, 
   waitFor( oneFinished ); // and wait until the first solver finishes
   
   /* interrupt all other solvers
   * clear all interrupts (for incremental solving)
   */
   for( int i = 0 ; i < threads; ++i ) solvers[i]->interrupt();
   
   // wait for the remaining threads to finish, and clear their interrupt again
   waitFor( allFinished );
   for( int i = 0 ; i < threads; ++i ) {
     assert( communicators[i]->isFinished() && "all solvers have to be finished" );
     solvers[i]->resetLastSolve(); // clear state of the solver (jump to level 0, clear interrupt)
   }
   
   /* 
   * determine the winning solver
   * store results of the winning solver into this solver
   */
   winningSolver = 0;
   for( int i = 0 ; i < threads; ++ i ) {
      if( communicators[i]->isWinner() && communicators[i]->getReturnValue() != l_Undef ) { winningSolver = i; break; }
   }
   
   cerr << "c MASTER found winning thread (" << communicators[winningSolver]->isWinner() << ") " << winningSolver << " / " << threads << " model= " << solvers[winningSolver]->model.size() << endl;

   // return model, if there is a winning thread!
   if( winningSolver < threads ) {
    /* 
    * return the result state
    */
      ret = communicators[ winningSolver ]->getReturnValue();
      if( ret == l_True ) {
	model.clear();
	model.capacity( solvers[winningSolver]->model.size() );
	for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) model.push(solvers[winningSolver]->model[i]);
	
	if( false && verbosity > 2 ) {
	  cerr << "c units: " << endl; for( int i = 0 ; i < solvers[winningSolver]->trail.size(); ++ i ) cerr << " " << solvers[winningSolver]->trail[i] << " 0" << endl; cerr << endl;
	  cerr << "c clauses: " << endl; for( int i = 0 ; i < solvers[winningSolver]->clauses.size(); ++ i ) cerr << "c " << solvers[winningSolver]->ca[solvers[winningSolver]->clauses[i]] << endl;
	  cerr << "c assumptions: " << endl; for ( int i = 0 ; i < assumps.size(); ++ i ) cerr << " " << assumps[i]; cerr << endl;
	}
	
      } else if ( ret == l_False ) {
	conflict.clear();
	conflict.capacity( solvers[winningSolver]->conflict.size() );
	for( int i = 0 ; i < solvers[winningSolver]->conflict.size(); ++ i ) conflict.push(solvers[winningSolver]->conflict[i]);    
      } else {
	cerr << "c winning thread returned UNKNOWN" << endl;
      }
   } else ret = l_Undef;
   
   if( verbosity > 0 ) {
     cerr << "c thread  S  \t|\t R   \t|\t sRej \t|\t lRej \t|" << endl;
     for( int i = 0 ; i < threads; ++ i ) {
	cerr << "c " << i << " : " << communicators[i]->nrSendCls 
	     <<  "  \t|\t" << communicators[i]->nrReceivedCls
	     <<  "  \t|\t" << communicators[i]->nrRejectSendSizeCls 
	     <<  "  \t|\t" << communicators[i]->nrRejectSendLbdCls <<  "  \t|"
	     << endl;
     }
     
// nrSendCls;           // how many clauses have been send via this communicator
//     unsigned nrRejectSendSizeCls; // how many clauses have been rejected to be send because of size
//     unsigned nrRejectSendLbdCls;  // how many clauses have been rejected to be send because of lbd
//     unsigned nrReceivedCls;
     
   }
   
   
   
   return ret;
}


void PSolver::createThreadConfigs()
{
  const char* Configs[] = { 
    "",			// 0
    "BIASSERTING",	// 1
    "LLA",		// 2
    "AUIP",		// 3
    "SUHD",		// 4
    "OTFSS",		// 5
    "SUHLE",		// 6
    "LHBR",		// 7
    "HACKTWO",		// 8
    "NOTRUST",		// 9
    "DECLEARN",		// 10
    "BIASSERTING",	// 11
    "LBD",		// 12
    "FASTRESTART",	// 13
    "AGILREJECT",	// 14
    "LIGHT",		// 15
    "","","","","","","","","","","","","","","","", // 16 - 31
    "","","","","","","","","","","","","","","","", // 32 - 47
    "","","","","","","","","","","","","","","","", // 48 - 63
  };

  for( int t = 0 ; t < threads; ++ t ) {
    configs[t].setPreset( Configs[t] );
    // configs[t].parseOptions("-solververb=2"); // set all solvers very verbose
  }
}


bool PSolver::initializeThreads()
{
  bool failed = false;
  // get space for thread ids
  threadIDs = new pthread_t [threads];

  data = new CommunicationData( 16000 ); // space for 16K clauses
  
  // create all solvers and threads
  for( unsigned i = 0 ; i < threads; ++ i )
  {
    communicators[i] = new Communicator(i, data);

    // create the solver
    if( i > 0 ) { 
      assert( solvers.size() == i && "next solver is not already created!" );
      solvers.push(  new Solver( configs[i] ) ); // solver 0 should exist already!
    }

    // tell the communication system about the solver
    communicators[i]->setSolver( solvers[i] );
    // tell solver about its communication interface
    solvers[i]->setCommunication( communicators[i] );
    
    if( verbosity > 0 ) cerr << "c [MASTER] setup thread " << i << " with data at " << std::hex << communicators[i]  << std::dec << endl;
    
    /* 
     * launch child threads here (will initially sleep in their run method)
     */
      pthread_attr_t attr;
      pthread_attr_init (&attr);
      pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);
    // create one thread
      int rc = pthread_create( &(threadIDs[i]), &attr, runWorkerSolver, (void *) communicators[i] );
      if (rc) {
	fprintf(stderr,"ERROR; return code from pthread_create() is %d for thread %d\n",rc,i);
	failed = failed || true;
	// set thread to abort if creation went wrong
	communicators[i]->setState( Communicator::aborted );
      }
      pthread_attr_destroy(&attr);
  }
  return failed;
}

void PSolver::start()
{
// set all threads to working (they'll have a look for new work on their own)
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    communicators[i]->ownLock->lock();
    if( verbosity > 1 ) cerr << "c [MASTER] set thread " << i << " state to working" << endl;
    communicators[i]->setState( Communicator::working );
    communicators[i]->ownLock->unlock();
    if( verbosity > 1 ) cerr << "c [MASTER] awake thread " << i << endl;
    communicators[i]->ownLock->awake();
  }
}

void PSolver::waitFor(const WaitState waitState)
{
 // evaluate the state of all the threads
  data->getMasterLock().lock();
  // repeat until there is some thread with the state finished
  int waitForIterations = 0;
  while( true )
  {
    waitForIterations ++;
    // check whether there are free threads
    unsigned threadOfInterest = threads;
    for( unsigned i = 0 ; i < threads; ++ i )
    {
      cerr << "c [WFI" << waitForIterations << "] MASTER checks thread " << i << " / " << threads << " finished: " << communicators[i]->isFinished() << endl;
      if( waitState == oneIdle ) {
	if( communicators[i]->isIdle() ) { threadOfInterest = i; break; }
      } else if ( waitState == oneFinished ) {
	if( communicators[i]->isFinished() ) { 
	  threadOfInterest = i; 
	  cerr << "c [WFI" << waitForIterations << "] Thread " << i << " finished" << endl;
	  // communicators[i]->setState( Communicator::waiting );
	  break;
	}
      } else if ( waitState == allFinished ) {
	if( ! communicators[i]->isFinished() && ! communicators[i]->isIdle() ) { // be careful with (! communicators[i]->isWaiting() &&)
	  cerr << "c [WFI" << waitForIterations << "] Thread " << i << " is not finished and not idle" << endl;
	  threadOfInterest = i; // notify that there is a thread that is not finished with this index
	  break; 
	}
      } else {
	assert(false && "Wait case for the given state has not been implemented" );
      }
    }
    
    if( waitState == allFinished ) {
      if( threadOfInterest == threads ) {
	data->getMasterLock().unlock(); // leave critical section
	return;                         // there is a free thread - use it!
      }
    } else {
      if( threadOfInterest != threads ) {
	data->getMasterLock().unlock(); // leave critical section
	return;                         // there is a free thread - use it!
      }
    }
    
    cerr << "c [WFI" << waitForIterations << "] MASTER sleeps" << endl;
    data->getMasterLock().sleep();  // wait until some thread notifies the master
  }
  data->getMasterLock().unlock(); // leave critical section
}

void PSolver::kill()
{
  if( !initialized ) return; // child threads have never been created - no need to kill them
  
  cerr << "c MASTER kills all child threads ..." << endl;
  // set all threads to working (they'll have a look for new work on their own)
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    communicators[i]->ownLock->lock();
    communicators[i]->setState( Communicator::aborted );
    communicators[i]->ownLock->unlock();
    communicators[i]->ownLock->awake();
  }
  
//   // interrupt all threads!
//   for( unsigned i = 0 ; i<threads; ++ i )
//   {
//     int* status = 0;
//     int err = 0;
//     err = pthread_kill(threadIDs[i], 15);
//     if( err != 0) cerr << "c killing a thread resulted in a failure" << endl;
//   }
  
  // join all threads!
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    int* status = 0;
    int err = 0;
    err = pthread_join(threadIDs[i], (void**)&status);
    if( err != 0) cerr << "c joining a thread resulted in a failure with status " << *status << endl;
  }
  cerr << "c finished killing" << endl;
}

void* runWorkerSolver(void* data)
{
  const bool verbose = true;
  
  /* Algorithm:
   */
  
  // tell thread that it can be interrupted
  pthread_attr_t attr;
  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);
  
  Communicator& info = * ( (Communicator*)data );
  
  if( verbose ) cerr << "c started thread with id " << info.getID() << endl;
  // initially, wait until master provides work!
  info.ownLock->lock();
  if( verbose ) cerr << "c thread id " << info.getID() << " state= " << ( info.isIdle() ? "idle" : "nonIdle" )<< endl;
  if( info.isIdle() || info.isWaiting() )
  {
    info.ownLock->sleep();
  }
  info.ownLock->unlock();
  vec<Lit> assumptions;

  // proceed with the current work item (group) as long as required
  while( ! info.isAborted() )
  {
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " start " <<  endl;
    
    // solve with assumptions!
    assumptions.clear();
    info.assumptions.copyTo( assumptions ); // get the current assumptions
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " solve task with " << assumptions.size() << " assumed literals:" << assumptions << endl;
      
    // do work
    lbool result l_Undef; 
    result = info.getSolver()->solveLimited( assumptions );

    info.setReturnValue( result );
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " result " << 
      (result == l_Undef ? "UNKNOWN" : ( result == l_True ? "SAT" : "UNSAT" ))
      << endl;
    
    if( result != l_Undef ) // solved the formula
    {
      info.setWinner(true); // indicate that this solver has a solution
    }
    
    // when done, tell master ...
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " change state to finished" << endl;
    // set own state to finished
    info.ownLock->lock();
      // depending on whether we did something useful, set the state
    info.setState( Communicator::finished );
    info.ownLock->unlock();
    
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " wake up master" << endl;
    // wake up master so that the master can do something with the solver
    info.awakeMaster();
    
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " wait for next round (sleep)" << endl;
    // wait until master changes the state again to working!
    info.ownLock->lock();
    while( ! info.isWorking() && ! info.isAborted()) { // wait for work or being aborted
      if( verbose ) cerr << "c [THREAD] " << info.getID() << " sleeps until next work ... " << endl;
      info.ownLock->sleep();
      if( verbose ) cerr << "c [THREAD] " << info.getID() << " awakes after work-sleep ... " << endl;
    }
    info.ownLock->unlock();
  }
  
  return 0;
}


};