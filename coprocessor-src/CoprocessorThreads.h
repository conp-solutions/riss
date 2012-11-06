/****************************************************************************[CoprocessorThreads.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTHREADS_HH
#define COPROCESSORTHREADS_HH

#include "coprocessor-src/CoprocessorTypes.h"

#include "coprocessor-src/LockCollection.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

/** Data Structures that are required to control parallel simplification
 */

/** Object that represents a single job that could be executed by a thread
 */
class Job{
public:

	void* (*function)(void *argument);
	void* argument;
	
	/** constructor, gives the job a unique (32bit) id
	 */
	Job(){
		function = 0;
		argument = 0;
	};
	
	Job( void* (*f)(void *), void* a)
	: function( f ), argument(a)
	{};
};

/** represent the states that a thread can have */
enum ThreadState {
  initializing,  // thread is still setting up, creating the environment
  waiting,       // thread waits for the master to receive a new task
  working,       // thread is currently handling the last job it has been assigned
  finished,      // thread finished its current job and is now waiting for the master to acknowledge
  aborting,      // thread will abort its execution and finish its execution
};

/** main class for thread data, store the data a thread can have */
class ThreadData {
  Job job;           // current / last job of the thread
  ThreadState state; // current state of the thread
  SleepLock ownLock; // lock to synchronize with the mast in a sleep state
  
public:
  ThreadData() : state(initializing) {}
  
  /** main thread method */
  void run();
  
  /** state getter and setter */
  bool isWorking() const { return state == working; }
  bool isWaiting() const { return state == waiting; }
  bool isFinished() const { return state == finished; }

  /** give a new job to the thread, thread has to be in waiting state */
  void performTask( Job newJob ); 
  
  /** method to start thread
   * @param threadData pointer to the threadData object for the current thread
   */
  static void* executeThread(void* threadData);
};

/** main class to controll the execution of multiple threads */
class ThreadController
{
  int32_t threads;          // number of threads that should be used by this object
  SleepLock masterLock;     // lock to be able to wait for threads in a sleep state
  
  pthread_t* threadHandles; // handler to each thread
  
public:
  /** set up the threads, wait for them to finish initialization */
  ThreadController(int _threads);
  
  /** pass jobs to threads, size of vector has to be number of threads! */
  void runJobs( vector<Job>& jobs );
};

void* ThreadData::executeThread(void* threadData)
{
  ThreadData* tData = (ThreadData*) threadData;
  return tData->run();
}

void ThreadData::performTask(Job newJob)
{
  assert( isWaiting() && "thread has to be waiting before a new job can be assigned" );
  job = newJob;    // assign new job
  state = working; // set state to working
  ownLock.awake(); // awake thread so that it can execute its task
}

void ThreadData::run()
{
  // copy from run later!
}



}

#endif