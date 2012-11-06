/*************************************************************************************[SleepLock.h]
Copyright (c) 2012, All rights reserved, Norbert Manthey

**************************************************************************************************/


#ifndef PARALLEL_HH
#define PARALLEL_HH

#include <cstdio>
#include <iostream>
#include <queue>
#include <signal.h>
#include <stack>
#include <unistd.h>


// for parallel stuff
#include <pthread.h>

/** usual lock
 */
class Lock
{
  pthread_mutex_t m;
public:
  /// initially, the lock can be grabbed
  Lock()
  {
    pthread_mutex_init(&m, 0);
  }
  
  /// get the lock
  void lock(){
    pthread_mutex_lock (&m); 
  }
  
  /// release the lock
  void unlock(){
    pthread_mutex_unlock (&m); 
  }
  
  // give the method lock access to the mutex that is inside of a lock class
  friend class MethodLock;
  
};

/** This class can be created the begin of each method and will be automatically destroyed before the method is left
 */
class MethodLock
{
  pthread_mutex_t& m;
public:
  // when the object is created, the lock is grabbed
  MethodLock( pthread_mutex_t& mutex ) : m(mutex)
  {
    pthread_mutex_lock (&m);  
  }
  MethodLock( Lock& lock ) : m(lock.m)
  {
    pthread_mutex_lock (&m);  
  }
  
  // when the object is destroyed, the lock is released
  ~MethodLock()
  {
    pthread_mutex_unlock (&m);
  }
};


/** locking class, also for waiting 
 * class that offers a mutex combibed with a conditional variable and a boolean variable
 */
class SleepLock
{
  // bool sleeps;               /// is set to true, iff last time somebody called sleep() before awake was called
  pthread_mutex_t mutex;     /// mutex for the lock
  pthread_cond_t master_cv;  /// conditional variable for the lock
  
  // do not allow the outside to copy this lock
  explicit SleepLock(const SleepLock& l )
  {};
  SleepLock& operator=(const SleepLock& l)
  {return *this;}
  
public:
  /** setup the lock
   * @param initialSleep first call to sleep will lead to sleep if the parameter is true (if awake is not called inbetween)
   */
  SleepLock() // : sleeps( initialSleep )
  {
    pthread_mutex_init(&mutex,     0);
    pthread_cond_init (&master_cv, 0);
  }
  
  ~SleepLock()
  {
    // sleeps = false;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy (&master_cv);
  }
  
  /// get the lock
  void lock(){
    pthread_mutex_lock (&mutex); 
  }
  
  /// release the lock
  void unlock(){
    pthread_mutex_unlock (&mutex); 
  }
    
  
  /** sleep until somebody calls awake()
   *  Note: should only be called between the lock() and unlock() command!
   *        After waking up again, unlock() has to be called again!
   */
  void sleep(){
    pthread_cond_wait (&master_cv,&mutex); // otherwise sleep now!
  }
  
  /** wakeup all sleeping threads!
   *  Note: waits until it can get the lock
   *        wakes up threads afterwards (cannot run, because calling thread still has the lock)
   *        releases the lock
   */
  void awake()
  {
    pthread_mutex_lock (&mutex);
    pthread_cond_broadcast (&master_cv); // initial attempt will fail!
    pthread_mutex_unlock (&mutex); 
  }
};


#endif