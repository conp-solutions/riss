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
, verbosity(0)
, verbEveryConflicts(0)
{
    // setup the default configuration for all the solvers!
    configs   = new CoreConfig [ threads ];
    ppconfigs = new CP3Config  [ threads ];
    
    // setup the first solver!
    solvers.push( new Solver( configs[0] ) ); // from this point on the address of this configuration is not allowed to be changed any more!
    solvers[0]->setPreprocessor( &ppconfigs[0] );
}

PSolver::~PSolver() {
  kill();
  for( int i = 0 ; i < solvers.size(); ++ i ) delete solvers[i]; // free all solvers
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

bool PSolver::addClause_(vec< Lit >& ps)
{
  bool ret = false;
  for( int i = 0 ; i < solvers.size(); ++ i ) {
    ret = ret || solvers[i]->addClause(ps); // if a solver failed adding the clause, then the state for all solvers is bad as well
  }
  return ret;
}

lbool PSolver::solveLimited(const vec< Lit >& assumps)
{
  // for now, simply let the first solver solve the formula!
  lbool ret = solvers[0]->solveLimited( assumps );
  
  int winningSolver = 0;
  if( ret == l_True ) {
    model.clear();
    model.capacity( solvers[winningSolver]->model.size() );
    for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) model.push(solvers[winningSolver]->model[i]);
  } else if ( ret == l_False ) {
    conflict.clear();
    conflict.capacity( solvers[winningSolver]->conflict.size() );
    for( int i = 0 ; i < solvers[winningSolver]->model.size(); ++ i ) conflict.push(solvers[winningSolver]->conflict[i]);    
  } else {
    
  }
  
  return ret;
  
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
  
   /* setup all solvers
   * setup the communication system for the solvers, including the number of commonly known variables
   */
   if( !initializeThreads () ) {
    cerr << "c initialization of " << threads << " threads failed" << endl;
   }
   
   /* 
    * copy the formula from the first solver to all the other solvers
    */
   if( ! initialized ) { // distribute the formula, if this is the first call to this method!
    for( int i = 1; i < solvers.size(); ++ i ) {
      solvers[i]->reserveVars( solvers[0]->nVars() );
      communicators[i]->setFormulaVariables( solvers[0]->nVars() ); // tell which variables can be shared
      for( int j = 0 ; j < solvers[0]->trail.size(); ++ j ) solvers[i]->trail.push( solvers[0]->trail[j] ); // copy all the unit clauses
      for( int j = 0 ; j < solvers[0]->clauses.size(); ++ j ) {
	solvers[i].addClause( solvers[0]->ca[ solvers[0]->clauses[j] ] ); // import the clause of solver 0 into solver i
      }
      for( int j = 0 ; j < solvers[0]->learnts.size(); ++ j ) {
	solvers[i].addClause( solvers[0]->ca[ solvers[0]->learnts[j] ] ); // import the learnt clause of solver 0 into solver i
      }
    }
    initialized = true;
   }

   /*
    * run the solvers on the formula
    */
   // add assumptions to all solvers
   for( int i = 0 ; i < threads; ++i ) {
      solvers[i]->clearAssumptions();
      solvers[i]->addAssumptions( assumps );
   }
   start(true); // allow all solvers to start, and wait until the first solver finishes
   
   /* interrupt all other solvers
   * clear all interrupts (for incremental solving)
   */
   
   for( int i = 0 ; i < threads; ++i ) {
      solvers[i]->interrupt();
   }
   
   /* 
   * determine the winning solver
   * store results of the winning solver into this solver
   */
   winningSolver = 0;
   for( int i = 0 ; i < threads; ++ i ) {
      
   }
   
   /* 
   * return the result state
   */
   
}


};


bool PSolver::initializeThreads()
{
  bool failed = false;
  // get space for thread ids
  threadIDs = new pthread_t [threads];

  // create all solvers and threads
  for( unsigned i = 0 ; i < threads; ++ i )
  {
    communicators[i] = new Communicator(i, data);

    // create the solver
    if( i > 0 ) solvers[i] = new Solver( configs[i] ); // solver 0 should exist already!

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

void PSolver::kill()
{
  // set all threads to working (they'll have a look for new work on their own)
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    communicators[i]->ownLock->lock();
    communicators[i]->setState( Communicator::aborted );
    communicators[i]->ownLock->unlock();
    communicators[i]->ownLock->awake();
  }
  
  // interrupt all threads!
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    int* status = 0;
    int err = 0;
    err = pthread_kill(threadIDs[i], 15);
    if( err != 0) cerr << "c killing a thread resulted in a failure" << endl;
  }
  
  // join all threads!
  for( unsigned i = 0 ; i<threads; ++ i )
  {
    int* status = 0;
    int err = 0;
    err = pthread_join(threadIDs[i], (void**)&status);
    if( err != 0) cerr << "c joining a thread resulted in a failure" << endl;
  }
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
  if( info.isIdle() )
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
    info.assumptions->copyTo( assumptions ); // get the current assumptions
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " solve task with " << assumptions.size() << " assumed literals" << endl;
      
    // do work
    lbool result = info.getSolver()->solveLimited( assumptions );
    if( verbose ) cerr << "c [THREAD] " << info.getID() << " result " << toInt(result) << endl;
      
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
    while( ! info.isWorking() ) info.ownLock->sleep();
    info.ownLock->unlock();
  }
  
  return 0;
}


/*
  
  


        CoreConfig coreConfig;
	Coprocessor::CP3Config cp3config;
	bool foundHelp = coreConfig.parseOptions(argc, argv);
	foundHelp = cp3config.parseOptions(argc, argv) || foundHelp;
  
  */