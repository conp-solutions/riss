/*********************************************************************************[Communication.h]
Copyright (c) 2012, All rights reserved, Norbert Manthey

 **************************************************************************************************/

#ifndef RISS_COMMUNICATION_H
#define RISS_COMMUNICATION_H

#include <cmath>
#include <deque>
#include <vector>
#include <iostream>

// own files
#include "riss/utils/LockCollection.h"
//#include "Controller.h"
// minisat files
#include "riss/core/SolverTypes.h"
#include "riss/core/Solver.h"

#include "proofcheck/ProofMaster.h"

namespace Riss
{

/** collection of some wait states */
enum WaitState {
    oneIdle = 0,
    oneFinished = 1,
    allFinished = 2,
};

/** ringbuffer that can be used to share clauses among multiple solver incarnations
 * note: is build based on MiniSATs Lit and Vec structures
 */
class ClauseRingBuffer
{
public:
  
    /** author ID that can be used for special sharing, received by all receivers (there should not be a thread with this ID) */
    const int specialAuthor () { return 1073741823; /* 2^30 - 1 */ }
    
    /** to be used to receive only (do not use for sending) */
    const int allReceivAuthor () { return 1073741822; /* 2^30 - 2 */ }
  
    /** author ID that can be used for special sharing (there should not be a thread with this ID) */
    const int maxRegularAuthor () { return 1073741821; /* 2^30 - 3 */ }
  
private:
    /** item for the pool, remembers the sender so that own clauses are not received again, the type (and the dependencies)
     */
    struct poolItem {
        std::vector<Lit> data;      /** the actual clause, equivalence class or multiple units */
        unsigned author : 30;       /** the author of the clause */
        unsigned multiunits : 1;    /** is a multiple unit clauses */
        unsigned equivalence: 1;    /** is a set of equivalent literals */
        #ifdef PCASSO
        int dependencyLevel;        /** store depth in the partition tree where this share-element depends on */
        #endif
        poolItem() : author(1073741823), multiunits(0), equivalence(0)     /** the initial author is invalid, so that it can be seen whether a clause in the ringbuffer has been added by solver */
            #ifdef PCASSO
            , dependencyLevel(0)
            #endif
        {}
    };

    Lock dataLock;                  /** lock that protects the access to the task data structures */
    poolItem* pool;                 /** ringbuffer for the clauses */
    unsigned poolSize;              /** size of the pool */
    unsigned addHereNext;           /** index of the position where the next clause will be added in the buffer */

    ProofMaster* proofMaster;       /** handle to the proof master, to handle shared clauses of the shared clauses pool */

    /** get the author of the clause of the given position in the pool
     * @param position index of the clause that should be received
     * note: this method should be locked
     */
    int getAuthor(const unsigned position) const
    {
        return pool[position].author;
    }

    int getMultiUnit(const unsigned position) const
    {
        return pool[position].multiunits;
    }

    int getEquivalence(const unsigned position) const
    {
        return pool[position].equivalence;
    }

    /** return actual vector to data */
    const std::vector<Lit>& getData(const unsigned position) const
    {
        return pool[position].data;
    }

    #ifdef PCASSO
    int getDependency(const unsigned position) const
    {
        return pool[position].dependencyLevel;
    }
    #endif

    /** get the clause of the given position to the pool
     * @param position index of the clause that should be received
     * @param allocator clause allocator of the solver that receives clauses (clauses are copied directly into the allocator
     * note: this method should be locked
     */
    Riss::CRef getClause(const unsigned position, Riss::ClauseAllocator& allocator)
    {
        std::vector<Lit>& poolClause = pool[position].data;
        return allocator.alloc(poolClause, true); // create as learned clause!
    }

    /** lock the whole data set
     */
    void lock()
    {
        dataLock.lock();
    }

    /** unlock the whole data set
     */
    void unlock()
    {
        dataLock.unlock();
    }

  public:

    /** create the data that is needed for adding enough clauses
     */
    ClauseRingBuffer(const unsigned size)
        :
        poolSize(size)
        , addHereNext(1)   // initially, last seen is set to 0 for each thread. to not incorporate non-initialized clauses, start at 1
        , proofMaster(0)
    {
        pool = new poolItem [size];
        poolSize = size;
    }

    ~ClauseRingBuffer()
    {
        if (pool != 0) { delete [] pool; pool = 0; }
    }

    /** set the handle for the proof master */
    void setProofMaster(ProofMaster *pm) { proofMaster = pm; }

    unsigned size() const { return poolSize; }

    /** return the position of the clause that has been deleted last
     */
    unsigned getCurrentPosition() const { return ((addHereNext == 0) ? poolSize - 1 : addHereNext - 1); }

    /** adds a clause to the next position of the pool
     * used template type should be Clause, vec<Lit> or Lit*
     * Note: in case of multiple units make sure that all assingments have the same dependency level!
     * @param authorID id of the author thread, to be stored with the clause
     * @param clause std::vector that stores the clause to be added
     * @param clauseSize number of elements in clause container
     * @param dependencyLevel dependency of currently shared object
     * @param multiUnits container represents multiple unit clauses
     * @param equivalence container represents equivalence class
     */

    #ifdef PCASSO
    template<typename T> // can be either clause or vector
    void addClause(int authorID, const T& clause, const int& clauseSize, const int& dependencyLevel, bool multiUnits = false, bool equivalence = false)
    #else
    template<typename T> // can be either clause or vector
    void addClause(int authorID, const T& clause, const int& clauseSize, bool multiUnits = false, bool equivalence = false)
    #endif
    {
        lock();

        // std::cerr << "[COMM] thread " << authorID << " adds clause to " << addHereNext << std::endl;
        // overwrite current position (starts with 0)
        std::vector<Lit>& poolClause = pool[addHereNext].data;
        // if there has been a clause at this position before, then this clause is removed right now ...
        if (pool[addHereNext].author != -1 && proofMaster != 0) { proofMaster->delFromProof(poolClause, lit_Undef, -1, false); }     // can work only on the global proof

        assert((!multiUnits || !equivalence) && "cannot have both properties");
        pool[addHereNext].author = authorID;
        pool[addHereNext].multiunits = multiUnits;
        pool[addHereNext].equivalence = equivalence;

        poolClause.resize(clauseSize);
        for (int i = 0 ; i < clauseSize; ++i) { poolClause[i] = clause[i]; }

        if (proofMaster != 0) {  // can work only on the global proof
            if (multiUnits) {
                for (int i = 0 ; i < clauseSize; ++ i) {
                    proofMaster->addUnitToProof(clause[i], -1, false);
                }
            } else if (equivalence) {
                for (int i = 1 ; i < clauseSize; ++ i) {
                    proofMaster->addEquivalenceToProof(clause[0], clause[i], -1, false);
                }
            } else {
                proofMaster->addToProof(poolClause, lit_Undef, -1, false);
            }
        }

        // push pointer to the next position
        // stay in the pool!
        addHereNext ++;
        addHereNext = (addHereNext == poolSize ? 0 : addHereNext);

        unlock();
    }

    /** adds a set of unit clauses to the pool
     * @param authorID id of the author thread, to be stored with the clause
     * @param units std::vector that stores all the literals that are inside the unit clauses to share
     */
    void addUnitClauses(int authorID, const std::vector<Lit>& units)
    {
        lock(); // coarse lock, do not lock for each unit, but for all!

        for (size_t i = 0 ; i < units.size(); ++ i) {
            // std::cerr << "[COMM] thread " << authorID << " adds clause to " << addHereNext << std::endl;
            // overwrite current position (starts with 0)
            std::vector<Lit>& poolClause = pool[addHereNext].data;
            if (pool[addHereNext].author != -1 && proofMaster != 0) { proofMaster->delFromProof(poolClause, lit_Undef, -1, false); }     // can work only on the global proof
            pool[addHereNext].author = authorID;
            poolClause.resize(1);
            poolClause[0] = units[i];
            if (proofMaster != 0) { proofMaster->addToProof(poolClause, lit_Undef, -1, false); }     // can work only on the global proof

            // push pointer to the next position
            // stay in the pool!
            addHereNext ++;
            addHereNext = (addHereNext == poolSize ? 0 : addHereNext);
        }
        unlock();
    }


    /** copy shared element into local receive data structure (sort type, handle variable info (and dependency for Pcasso)
     * @param position of the element that is currently received
     * @param allocator allocator object of calling solver
     * @param clauses vector to clause references of newly added clauses
     * @param receivedUnits vector of unit clauses that are received
     * @param receivedUnitsDependencies dependencylevel for each unit clause
     * @param receivedEquivalences vector of equivalent literal classes (separated by lit_Undef)
     * @param receivedEquivalencesDependencies dependencyLevel for each received equivalence class (one dependency per lit_Undef)
     * @param receiveData object that knows dependencies per variable, and can tell whether variable is allowed for receiving
     * Note: should be run when read-locked
     */
    template <typename T>
    #ifdef PCASSO
    void incorporateReceiveItem(unsigned position, Riss::ClauseAllocator& allocator, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<int>& receivedUnitsDependencies, vec<Lit>& receivedEquivalences, vec<int>& receivedEquivalencesDependencies,  T& receiveData)
    {
    #else
    void incorporateReceiveItem(unsigned position, Riss::ClauseAllocator& allocator, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<Lit>& receivedEquivalences, T& receiveData)
    {
    #endif
        if (getMultiUnit(position)) {
            const std::vector<Lit>& units = getData(position);
            for (int j = 0 ; j < units.size(); ++ j) {
                if (receiveData.canBeReceived(var(units[j]))) {      // we are allowed to receive that unit clause due to simplification
                    receivedUnits.push(units[j]);    // receive unit
                    #ifdef PCASSO
                    receivedUnitsDependencies. push(getDependency(position));   // store dependency level
                    #endif
                }
            }
        } else if (getEquivalence(position)) {
            const std::vector<Lit>& eeSCC = getData(position);
            int usedSCCliterals = 0;
            const int oldSize = receivedEquivalences.size();
            for (int j = 0 ; j < eeSCC.size(); ++ j) {
                if (receiveData.canBeReceived(var(eeSCC[j]))) {      // we are allowed to receive that unit clause due to simplification
                    receivedEquivalences.push(eeSCC[j]);    // receive unit
                    usedSCCliterals ++;
                    #ifdef PCASSO
                    if (usedSCCliterals > 1) { receiveData.setDependency(var(eeSCC[j]), getDependency(position)); }    // store dependency level
                    #endif
                }
            }
            if (usedSCCliterals == 1) { receivedEquivalences.pop(); }  // remove the single literal again, as its a trivial SCC
            else {
                receivedEquivalences.push(lit_Undef);   // add a terminal symbol, so that next class can be added
                #ifdef PCASSO
                receivedEquivalencesDependencies.push(getDependency(position));    // set dependency for equivalence class, if there are at least 2 literals
                #endif
            }
        } else {
            // usual clause
            const std::vector<Lit>& lits = getData(position);
            for (int i = 0 ; i < lits.size(); ++ i) {                     // check soundness of receiving
                if (! receiveData.canBeReceived(var(lits[i]))) { return; }      // if a literal in the clause is locked, do not receive it
            }
            // otherwise, receiving is fine at the moment
            clauses.push_back(getClause(position, allocator));                 // create clause directly in clause allocator
            #ifdef PCASSO
            allocator[ clauses[clauses.size() - 1] ].setPTLevel(getDependency(position));   // set dependency of this clause
            #endif
        }
    }

    /** copy all clauses into the clauses std::vector that have been received since the last call to this method
     * @param authorID id of the author thread, to be stored with the clause
     * note: only an approximation
     */
    template <typename T>
    #ifdef PCASSO
    unsigned receiveClauses(int authorID, unsigned lastSeenIndex, Riss::ClauseAllocator& allocator, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<int>& receivedUnitsDependencies, vec<Lit>& receivedEquivalences, vec<int>& receivedEquivalencesDependencies, T& receiveData)
    #else
    unsigned receiveClauses(int authorID, unsigned lastSeenIndex, Riss::ClauseAllocator& allocator, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<Lit>& receivedEquivalences, T& receiveData)
    #endif
    {
        //std::cerr << "c [COMM] thread " << authorID << " called receive with last seen " << lastSeenIndex << ", addHere: " << addHereNext << std::endl;
        clauses.clear();
#warning use read- and write-lock here!
        lock();
        // incorporate all clauses that are stored BEFORE addHereNext
        unsigned returnIndex = addHereNext == 0 ? poolSize - 1 : addHereNext - 1;

        const unsigned startIndex = lastSeenIndex == poolSize - 1 ? 0 : lastSeenIndex + 1; // first clause that needs to be copied
        const unsigned stopIndex = addHereNext;     // last clause that needs to be copied

        //std::cerr << "c [COMM] thread " << authorID << " start:" << startIndex << " stop:" << stopIndex << " return: " << returnIndex << std::endl;

        // do not copy anything, if the next position is the one where the next clause would be added
        if (startIndex != addHereNext) {
            if (startIndex < stopIndex) {
                for (unsigned i = startIndex; i < stopIndex; ++ i) {   // do copy the last clause!
                    // receive only, if calling thread was not the author
                    if (getAuthor(i) != authorID) {
                        #ifdef PCASSO
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);  // receive one element, and its dependencies
                        #else
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedEquivalences, receiveData); // receive one element, add info to collecting data strucutures
                        #endif
                    }
                }
            } else { // startIndex > stopIndex
                for (unsigned i = startIndex; i < poolSize; ++i) {
                    // receive only, if calling thread was not the author
                    if (getAuthor(i) != authorID) {
                        #ifdef PCASSO
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);  // receive one element, and its dependencies
                        #else
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedEquivalences, receiveData); // receive one element, add info to collecting data strucutures
                        #endif
                    }
                }
                for (unsigned i = 0 ; i < stopIndex; ++ i) {
                    // receive only, if calling thread was not the author
                    if (getAuthor(i) != authorID) {
                        #ifdef PCASSO
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);  // receive one element, and its dependencies
                        #else
                        incorporateReceiveItem(i, allocator, clauses, receivedUnits, receivedEquivalences, receiveData); // receive one element, add info to collecting data strucutures
                        #endif
                    }
                }
            }
        } else { // end if (something to share)
            //std::cerr << "[COMM] c thread " << authorID << " nothing new to import" << std::endl;
        }
        unlock();
        return returnIndex;
    }

};

/** object that takes care which data is shared among the threads, handles
 */
class CommunicationData
{
    ClauseRingBuffer clauseBuffer;  /** buffer that stores the shared clauses */
    ClauseRingBuffer specialBuffer; /** buffer for multiunits and equivalences (should not be missed, and not overwritten too regularly) */

    // enable communication between global psolver and pcasso in pcasso
    ClauseRingBuffer* extraClauseBuffer;  /** buffer that should be filled by add clause as well (author will be Ringbuffer::externAuthor) */
    ClauseRingBuffer* extraSpecialBuffer; /** buffer that should be filled by add clause as well (author will be Ringbuffer::externAuthor) */
    
    Lock dataLock;               /** lock that protects the access to the task data structures */
    SleepLock masterLock;        /** lock that enables the master thread to sleep during waiting for child threads */

    vec <Lit> sendUnits;         /** std::vector that stores the unit clauses that should be send to all clients as clauses (not learned!) */

  public:

    /** @param buffersize sets up a buffer with the given number of elements, and another buffer with quarter the number of elements */
    CommunicationData(const int buffersize) :
        clauseBuffer(buffersize),
        specialBuffer(buffersize / 4),
        extraClauseBuffer(nullptr),
        extraSpecialBuffer(nullptr)
    {
    }

    SleepLock& getMasterLock() { return masterLock; };

    /** set the handle for the proof master in the ringbuffer */
    void setProofMaster(ProofMaster *pm) { clauseBuffer.setProofMaster(pm); }


    /** return a reference to the ringbuffer
     */
    ClauseRingBuffer& getBuffer() { return clauseBuffer; }

    /** return a reference to the ringbuffer
     */
    ClauseRingBuffer& getSpecialBuffer() { return specialBuffer; }

    /** return a reference to the ringbuffer
     */
    ClauseRingBuffer* getExtraBuffer() { return extraClauseBuffer; }

    /** return a reference to the ringbuffer
     */
    ClauseRingBuffer* getExtraSpecialBuffer() { return extraSpecialBuffer; }
    
    /** set pointers to extra buffers */
    void setExternBuffers(ClauseRingBuffer* externClauseBuffer, ClauseRingBuffer* externSpecialBuffer ) {
      extraClauseBuffer  = externClauseBuffer;
      extraSpecialBuffer = externSpecialBuffer;
    }
    
    
    /** clears the std::vector of units to send
     * should be called by the master thread only!
     */
    void clearToSend()
    {
        sendUnits.clear();
    }

    /** adds the given literal to the std::vector of literals that should be sent
     * should be called by the master thread only
     */
    void addToSendThisUnit(int unitLiteral)
    {
        // convert into literal, push to std::vector
        sendUnits.push(Riss::mkLit(abs(unitLiteral), unitLiteral < 0));
    }

    /** receive clauses
     * should be called by worker threads
     * @param fillMe std::vector of the client that should store the literals that have been send recently
     */
    void receiveUnits(vec<Lit>& fillMe)
    {
        fillMe.clear();
        for (int i = 0 ; i < sendUnits.size(); ++i) {
            fillMe.push(sendUnits[i]);
        }
    }
};

/** during receive, also receive from parent tree receivers */
class TreeReceiver {
  TreeReceiver* parent;
  CommunicationData* data;
  
  unsigned lastSeenIndex;         // position of the last clause that has been incorporated
  unsigned lastSeenSpecialIndex;  // position of the last clause that has been incorporated
  
public:
  TreeReceiver() : 
    parent(nullptr)
  , data(nullptr)
  , lastSeenIndex(0)
  , lastSeenSpecialIndex(0)
  {}
  
  /** calls delete to its parent */
  ~TreeReceiver() {
    if ( parent != nullptr ) delete parent;
    parent = nullptr;
  }
  
  void setParent( TreeReceiver* outerParent ) { parent = outerParent; }
  
  void setData( CommunicationData* outerData ) { data = outerData; }
  
    /** receive clauses from current data, as well as from parents data recursively
     * follow recursion, even if there is no data element in the middle 
     * copy all clauses into the clauses std::vector that have been received since the last call to this method
     * note: only an approximation, can happen that ringbuffer overflows!
     * note: should be called by the worker solver only!
     * @param ca clause allocator, to avoid unnecessary copies
     * @param clauses vector to new clause references of received clauses
     * @param receivedUnits vector of received units
     * @param receivedEquivalences vector of received equivalence classes, classes are separated by lit_Undef
     * @param receiveData object that can tell per variable whether receiving is ok, get set dependency value per variable (for Pcasso, clauses have their value stored already)
     */
    template <typename T>
    #ifdef PCASSO
    void receiveClauses(Riss::ClauseAllocator& ca, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<int>& receivedUnitsDependencies, vec<Lit>& receivedEquivalences, vec<int>& receivedEquivalencesDependencies, T& receiveData)
    #else
    void receiveClauses(Riss::ClauseAllocator& ca, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<Lit>& receivedEquivalences, T& receiveData)
    #endif
    {
        // receive from special buffer first
        #ifdef PCASSO
	if( data != nullptr ) {
	  lastSeenSpecialIndex = data->getSpecialBuffer().receiveClauses(data->getSpecialBuffer().allReceivAuthor(), lastSeenSpecialIndex, ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);
	  lastSeenIndex        = data->getBuffer().receiveClauses(data->getBuffer().allReceivAuthor(), lastSeenIndex, ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);
	}
	if( parent != nullptr ) parent->receiveClauses(ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);  // receive from parent, if activated
        #else
	if( data != nullptr ) {
	  lastSeenSpecialIndex = data->getSpecialBuffer().receiveClauses(data->getSpecialBuffer().allReceivAuthor(), lastSeenSpecialIndex, ca, clauses, receivedUnits, receivedEquivalences, receiveData);
	  lastSeenIndex        = data->getBuffer().receiveClauses(data->getSpecialBuffer().allReceivAuthor(), lastSeenIndex, ca, clauses, receivedUnits, receivedEquivalences, receiveData);
	}
	if( parent != nullptr ) parent->receiveClauses(ca, clauses, receivedUnits, receivedEquivalences, receiveData);
        #endif
    }
  
};

/** provide the major communication between thread and master!
 */
class Communicator
{
  public:
    // attributes

    SleepLock* ownLock;     // sleep lock of this thread to not waste cpu time

    CommunicationData* data;    // pointer to the data, that is shared among all threads

    ProofMaster* proofMaster;   // class to take care of the proof

    // TODO: think about read and write. master writes, client polls, could set back to poll again
    enum State {
        idle, // has no work at the moment
        working,// is simply working
        interrupted,// interrupt current run, proceed with the run afterwards!
        interruptedForce, // interrupted with force (to perform a restart immedeately)
        aborted,// abort current solving run and wait for next work item
        doExit, // at next interruption thread is canceled
        sleeping, // the thread currently sleeps
        finished, // thread finished its work on the current group
        waiting,  // thread waits and master does something with it
        doReceiveFromMaster, // thread should receive shared unit clauses (from master)
        finishedReceiving,   // thread is finished with receiving!
    };

  private:

    bool winner;    // this thread solved the problem
    int originalVars;   // number of variables that is present (without new ER variables)
    Riss::lbool returnValue; // value that is returned by the solver after a solving call

    Riss::Solver * solver;  // pointer to the used solver object
    int id;                    // id of this thread

    State state;

    int myLastTaskID;
    unsigned lastSeenIndex;         // position of the last clause that has been incorporated
    unsigned lastSeenSpecialIndex;  // position of the last clause that has been incorporated
    bool doSend;               // should this thread send clauses
    bool doReceive;            // should this thread receive clauses

    vec<char> protect;         // if char in std::vector is 0, the variable has to be considered for calculating limits

    char dummy[64]; // to separate this data on extra cache lines (avoids false sharing)

    TreeReceiver* parent; // handle to communcation of parent node

    // methods
  public:

    int hardwareCore; // core on which this thread should be pinned (if -1, do not use pinning)
    
    vec<Lit> assumptions;

    // seet default values, ownLock is set to initial sleep
    Communicator(const int id, CommunicationData* communicationData) :
        ownLock(new SleepLock())
        , data(communicationData)
        , proofMaster(0)
        , winner(false)
        , originalVars(-1)
        , returnValue(l_Undef)
        , solver(0)
        , id(id)
        , state(waiting)
        , myLastTaskID(-1)
        , lastSeenIndex(0)
        , lastSeenSpecialIndex(0)
        , doSend(true)              // should this thread send clauses
        , doReceive(true)
	
	, parent( nullptr )
	
	, hardwareCore(-1) // so far, do not use a core
	
        , protectAssumptions(false) // should the size limit check also consider assumed variables?
        , sendSize(10)    // initial value, also minimum limit (smaller clauses can be shared if LBD is also accepted)
        , sendLbd(5)      // initial value, also minimum limit (smaller clauses can be shared if size is also accepted)
        , sendMaxSize(128) // upper bound for clause size (larger clause is never shared)
        , sendMaxLbd(32)  // upper bound for clause lbd (larger lbd is never shared)
        , sizeChange(0.0)  // TODO: set to value greater than 0 to see dynamic limit changes! (e.g. 0.05)
        , lbdChange(0.0)   // TODO: set to value greater than 0 to see dynamic limit changes! (e.g. 0.02)
        , sendRatio(0.1)   // how many of the learned clauses should be shared? 10%?
        , doBumpClauseActivity(false)
        , sendIncModel(true)           // allow sending with variables where the number of models potentially increased
        , sendDecModel(false)          // allow sending with variables where the number of models potentially deecreased
        , useDynamicLimits(true)       // update sharing limits dynamically
        , sendEquivalences(true)       // share equivalence information
        , receiveEqiuvalences(false)   // receive equivalence information

        , vivifiedLiterals(0)

        , nrSendCls(0)
        , nrRejectSendSizeCls(0)
        , nrRejectSendLbdCls(0)
        , nrReceivedCls(0)
        , nrSendMultiUnits(0)
        , nrReceivedMultiUnits(0)
        , nrSendEEs(0)
        , nrReceivedEEs(0)
        , nrReceiveAttempts(0)
        , nrSendCattempt(0)
        , nrSendMattempt(0)
        , nrSendEattempt(0)
    {
        // do create the solver here, or from the outside?
        // solver = new Solver();
    }

    // destructor
    ~Communicator()
    {
        if (ownLock != 0) { delete ownLock; }
        ownLock = 0;
        if( parent  != nullptr ) delete parent;
	parent = nullptr;
    }

    void setSolver(Riss::Solver* s)
    {
        assert(solver == 0 && "will not overwrite handle to another solver");
        solver = s;
    }

    /** tell the communicator about the proof master, so that it can be used */
    void setProofMaster(ProofMaster* pm)
    {
        assert(proofMaster == 0 && "will not overwrite handle to another proof master");
        proofMaster = pm;
    }
    
    /** a parallel proof is constructed, if there is a proof master */
    bool generateProof() const { return proofMaster != 0; }

    /** forward the API of the proof master to the solver (or other callers) */
    ProofMaster* getPM() { return proofMaster ; }
    
    /** set object to receive clauses from, this object will be deleted during destruction of the called object*/
    void setParentReceiver( TreeReceiver* receiver ) {
      parent = receiver;
    }
    
    State getState() const
    {
        return state;
    }

    bool getDoSend() const  { return doSend; }
    void setDoSend(bool ds) { doSend = ds; }
    bool getDoReceive() const  { return doReceive; }
    void setDoReceive(bool dr) { doReceive = dr; }

    /** tell the number of shared variables */
    void setFormulaVariables(const int formulaVariables) { originalVars = formulaVariables; }

    /** set whether this thread is the winner */
    bool setWinner(const bool newWinner)
    {
        return winner = newWinner;
    }
    /** check whether this thread solved the problem */
    bool isWinner() const
    {
        return winner;
    }

    /** tell return value of solver */
    Riss::lbool getReturnValue() const { return returnValue; }
    /** set return value of this thread (should be done by the solver, or by the solving thread!) */
    Riss::lbool setReturnValue(const Riss::lbool newReturnValue)
    {
        return returnValue = newReturnValue;
    }

    /** update the state of the thread (define what will happen next)
     * @param s future state
     * note: when this method is called, the lock ownlock should be locked first!
     * @return true, if state transition is valid, false otherwise (in this case the state is not changed)
     */
    bool setState(const State s)
    {
        // TODO: take care of state transitions!
        // is the ownlock-lock locked right now?
        state = s;
        return true;
    }

    int getID() const { return id; }

    bool isIdle() const { return state == idle; }
    bool isWorking() const { return state == working; }
    bool isAborted() const { return state == aborted; }
    bool isSleeping() const { return state == sleeping; }
    bool isFinished() const { return state == finished; }
    bool isInterrupted() const { return state == interrupted; }
    bool isInterruptForced() const { return state == interruptedForce; }
    bool isWaiting() const { return state == waiting; }
    bool isDoReceive() const { return state == doReceiveFromMaster; }
    bool isFinishedReceiving() const { return state == finishedReceiving; }

    /** update the solver at the current state
     * @param s pointer to the solver that just entered the update method TODO: necessary? Should be equal to ptr in this object!
     * @return true, if something has been done (e.g. a clause has been added), false otherwise
     * note: this method should be called by the solver only if it will be doing a decision next
     */
    bool update(Riss::Solver* s)
    {
        // implement update code here!
        return true;
    }

    /** wake up master after some notification
     */
    void awakeMaster()
    {
        data->getMasterLock().awake();
    }

    /** return a handle to the solver of this communicator
     */
    Riss::Solver* getSolver() { return solver; }

    unsigned currentDependencyLevel() const
    {
        // FIXME to be adapted to the actual tree
#warning USE TREE INFORMATION HERE!
//         assert(false && "this method should do something useful");
        return INT32_MAX; // return highest possible value
    };

    /** adds a clause to the next position of the pool
     * @param clause std::vector that stores the clause to be added (could be Clause, vec<Lit> or Lit*
     * @param toSendSize number of literals in the parameter clause
     * @param dependencyLevel dependencylevel of currently shared object
     * @param multiUnits we do not add one clause, but multiple unit clauses
     * @param equivalences we share a class of equivalent literals
     */
    #ifdef PCASSO
    template<typename T, typename V> // can be either clause or vector, do not name variable information explicitely
    void addClause(const T& clause, const int& toSendSize, const int& dependencyLevel, const V& variableInformation, bool multiUnits = false, bool equivalences = false)
    #else
    template<typename T> // can be either clause or vector
    void addClause(const T& clause, const int& toSendSize, bool multiUnits = false, bool equivalences = false)
    #endif
    {
        #ifdef PCASSO
#warning enable assertion again later
//        assert(!multiUnits && "remove this assertion when method makes sure that all units have the same dependency");   // either set the highest vor all, or sort and add multiple items
        if (!multiUnits && !equivalences) { 
	  data->getBuffer().addClause(id, clause, toSendSize, dependencyLevel);     // usual buffer
	  if( data->getExtraBuffer() != nullptr ) {
	    data->getExtraBuffer()->addClause(data->getExtraBuffer()->specialAuthor(), clause, toSendSize, dependencyLevel);     // usual special buffer
	  }
	} else {
	  data->getSpecialBuffer().addClause(id, clause, toSendSize, dependencyLevel, multiUnits, equivalences);  // special buffer
	  if( data->getExtraSpecialBuffer() != nullptr ) {
	    data->getExtraSpecialBuffer()->addClause(data->getExtraSpecialBuffer()->specialAuthor(), clause, toSendSize, dependencyLevel, multiUnits, equivalences);  // special buffer
	  }
	} 
        #else
        if (!multiUnits && !equivalences) { 
	  data->getBuffer().addClause(id, clause, toSendSize); // usual buffer
	  if( data->getExtraBuffer() != nullptr ) {
	    data->getExtraBuffer()->addClause(data->getExtraBuffer()->specialAuthor(), clause, toSendSize);     // usual special buffer
	  }
	} else { 
	  data->getSpecialBuffer().addClause(id, clause, toSendSize, multiUnits, equivalences);  // special buffer
	  if( data->getExtraSpecialBuffer() != nullptr ) {
	    data->getExtraSpecialBuffer()->addClause(data->getExtraSpecialBuffer()->specialAuthor(), clause, toSendSize, multiUnits, equivalences);  // special buffer
	  }
	} 
        #endif
    }

    /** copy all clauses into the clauses std::vector that have been received since the last call to this method
     * note: only an approximation, can happen that ringbuffer overflows!
     * note: should be called by the worker solver only!
     * @param ca clause allocator, to avoid unnecessary copies
     * @param clauses vector to new clause references of received clauses
     * @param receivedUnits vector of received units
     * @param receivedEquivalences vector of received equivalence classes, classes are separated by lit_Undef
     * @param receiveData object that can tell per variable whether receiving is ok, get set dependency value per variable (for Pcasso, clauses have their value stored already)
     */
    template <typename T>
    #ifdef PCASSO
    void receiveClauses(Riss::ClauseAllocator& ca, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<int>& receivedUnitsDependencies, vec<Lit>& receivedEquivalences, vec<int>& receivedEquivalencesDependencies, T& receiveData)
    #else
    void receiveClauses(Riss::ClauseAllocator& ca, std::vector< Riss::CRef >& clauses, vec<Lit>& receivedUnits, vec<Lit>& receivedEquivalences, T& receiveData)
    #endif
    {
        if (!doReceive) { return; }
        // receive from special buffer first
        #ifdef PCASSO
        lastSeenSpecialIndex = data->getSpecialBuffer().receiveClauses(id, lastSeenSpecialIndex, ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);
        lastSeenIndex        = data->getBuffer().receiveClauses(id, lastSeenIndex, ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);
	
	if( parent != nullptr ) parent->receiveClauses(ca, clauses, receivedUnits, receivedUnitsDependencies, receivedEquivalences, receivedEquivalencesDependencies, receiveData);  // receive from parent, if activated
        #else
        lastSeenSpecialIndex = data->getSpecialBuffer().receiveClauses(id, lastSeenSpecialIndex, ca, clauses, receivedUnits, receivedEquivalences, receiveData);
        lastSeenIndex        = data->getBuffer().receiveClauses(id, lastSeenIndex, ca, clauses, receivedUnits, receivedEquivalences, receiveData);
	
	if( parent != nullptr ) parent->receiveClauses(ca, clauses, receivedUnits, receivedEquivalences, receiveData); // receive from parent, if activated
        #endif
    }

    void initProtect(const vec<Lit>& assumptions, const int vars)
    {
        protect.clear();
        protect.growTo(vars, 0);
        for (int i = 0 ; i < assumptions.size(); ++ i) {
            protect[ Riss::var(assumptions[i]) ] = 1;
        }
    }

    // literal is only protected, if this option is enabled
    bool isProtected(const Lit& l) const
    {
        return protectAssumptions && protect[ Riss::var(l) ];
    }

    bool variableProtection() const { return protectAssumptions; }
    
    
    /** return whether this solver works on the original formula (independently, maybe later with communication)*/
    bool independent() const { return solver->independent(); }

  public:       // this probably is not a good idea, but ah well ...

    bool protectAssumptions;      // should assumption variables not be considered for calculating send-limits?
    float sendSize;               // Minimum Lbd of clauses to send  (also start value)
    float sendLbd;                // Minimum size of clauses to send (also start value)
    float sendMaxSize;            // Maximum size of clauses to send
    float sendMaxLbd;             // Maximum Lbd of clauses to send
    float sizeChange;             // How fast should size send limit be adopted?
    float lbdChange;              // How fast should lbd send limit be adopted?
    float sendRatio;              // How big should the ratio of send clauses be?
    bool  doBumpClauseActivity;   // Should the activity of a received clause be increased from 0 to current activity

    bool sendIncModel;            // allow sending with variables where the number of models potentially increased
    bool sendDecModel;            // allow sending with variables where the number of models potentially deecreased
    bool useDynamicLimits;        // update sharing limits dynamically
    bool sendEquivalences;        // share equivalence information
    bool receiveEqiuvalences;     // receive equivalences

    int vivifiedLiterals;         // number of literals that have been eliminated by vivification of received clause

    unsigned nrSendCls;           // how many clauses have been send via this communicator
    unsigned nrRejectSendSizeCls; // how many clauses have been rejected to be send because of size
    unsigned nrRejectSendLbdCls;  // how many clauses have been rejected to be send because of lbd
    unsigned nrReceivedCls;       // how many clauses have been received (there is no filter yet!!)
    unsigned nrSendMultiUnits;   // number of shared multi units
    unsigned nrReceivedMultiUnits; // how many multi-unit packages have been sent
    unsigned nrSendEEs;           // number of shared EEs
    unsigned nrReceivedEEs;       // how many equivalence SCC have been sent
    unsigned nrReceiveAttempts, nrSendCattempt, nrSendMattempt, nrSendEattempt; // number of tries to receive/send certain data types

};

} // namespace Riss

#endif
