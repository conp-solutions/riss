#ifndef RWLOCK_H
#define RWLOCK_H

#include <cstdio>
#include <pthread.h>
#include <semaphore.h>

namespace Pcasso
{

class RWLock
{

  private:
    pthread_rwlock_t _lock;   // actual semaphore
  public:

    /** create an unlocked lock
     *
     * @param max specify number of maximal threads that have entered the semaphore
     */
    RWLock()
    {
        // create semaphore with no space in it
        pthread_rwlock_init(&_lock, NULL);
    }

    /** release all used resources (nothing to do -> semaphore becomes invalid)
     */
    ~RWLock()
    {
        pthread_rwlock_destroy(&_lock);
    }

    /** releases one lock again
     *
     * should only be called by the thread that is currently owns the lock
     */
    void unlock()
    {
        pthread_rwlock_unlock(&_lock);
    }

    /** reader Lock
     */
    void rLock()
    {
        pthread_rwlock_rdlock(&_lock);
    }

    /** writer Lock
     */
    void wLock()
    {
        pthread_rwlock_wrlock(&_lock);
    }

    /** try to lock for reader
     */
    bool try_rLock()
    {
        int err = pthread_rwlock_tryrdlock(&_lock);
        return err == 0;
    }

    /** try to lock for writer
     */
    bool try_wLock()
    {
        int err = pthread_rwlock_trywrlock(&_lock);
        return err == 0;
    }
};

} // namespace Pcasso

#endif
