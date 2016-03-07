/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_COPROCESSORTYPES_HH
#define RISS_COPROCESSORTYPES_HH

#include "riss/core/Solver.h"

#include "riss/utils/System.h"
#include "riss/mtl/Sort.h"

#include "riss/utils/LockCollection.h"
#include "riss/utils/AutoDelete.h"

#include <vector>
#include <ostream>

namespace Coprocessor
{

/** temporary Boolean flag to quickly enable debug output for the whole file */
const bool global_debug_out = false;

//forward declaration
class VarGraphUtils;

typedef std::vector<std::vector <Riss::CRef> > ComplOcc;

/** class that measures the time between creation and destruction of the object, and adds it*/
class MethodTimer
{
    double* pointer;
  public:
    MethodTimer(double* timer) : pointer(timer) { *pointer = Riss::cpuTime() - *pointer;}
    ~MethodTimer() { *pointer = Riss::cpuTime() - *pointer; }
};


/** class responsible for debug output */
class Logger
{
    int outputLevel; // level to output
    bool useStdErr;  // print to stderr, or to stdout?
  public:
    Logger(int level, bool err = true);

    void log(int level, const std::string& s);
    void log(int level, const std::string& s, const int i);
    void log(int level, const std::string& s, const Riss::Clause& c);
    void log(int level, const std::string& s, const Riss::Lit& l);
    void log(int level, const std::string& s, const Riss::Clause& c, const Riss::Lit& l);
};

struct VarOrderBVEHeapLt;

/** Data, that needs to be accessed by coprocessor and all the other classes
 */
class CoprocessorData
{
    // friend for VarGraph
    friend class Coprocessor::VarGraphUtils;

    Riss::ClauseAllocator& ca;
    Riss::Solver* solver;
    /* TODO to add here
     * occ list
     * counters
     * methods to update these structures
     * no statistical counters for each method, should be provided by each method!
     */
    uint32_t numberOfVars;                // number of variables
    uint32_t numberOfCls;                 // number of clauses during initialization
    uint32_t numberOfTotalLiterals;       // number of total literals in the formula during initialization
    ComplOcc occs;                        // list of clauses, per literal
    std::vector<int32_t> lit_occurrence_count; // number of literal occurrences in the formula

    bool hasLimit;                        // indicate whether techniques should be executed limited
    bool randomOrder;                     // perform preprocessing with random execution order within techniques
    bool currentlyInprocessing;           // current simplification is during search
    bool debugging;                       // current version works in debugging mode

    Lock dataLock;                        // lock for parallel algorithms to synchronize access to data structures

    Riss::MarkArray modTimer;             // store for each literal when (by which technique) it has been last modified

    std::vector<Riss::Lit> undo;          // store clauses that have to be undone for extending the model
    int lastCompressUndoLits;             // number of literals when the last compression took place
    // int decompressedUndoLits;             // number of literals on the decompressedUndoLits stack

  private:
    std::vector<Riss::Lit> equivalences;            // stack of literal classes that represent equivalent literals
    std::vector<Riss::CRef> subsume_queue;          // queue of clause references that potentially can subsume other clauses
    std::vector<Riss::CRef> strengthening_queue;    // vector of clausereferences, which potentially can strengthen

    int countLitOcc(Riss::Lit l)
    {
        int count = 0;
        std::vector<Riss::CRef>& list = occs[Riss::toInt(l)];
        for (int i = 0; i < list.size(); ++i) {
            Riss::CRef cr = list[i];
            if (cr == Riss::CRef_Undef) { continue; }
            Riss::Clause& c = ca[cr];
            if (c.can_be_deleted()) { continue; }
            bool occurs = false;
            for (int j = 0; j < c.size(); ++j)
                if (c[j] == l) {
                    occurs = true; break;
                }
            if (occurs) { ++count; }
            else { assert(false && "dirty occurrence list!!!"); }
        }
        return count;
    }

    // TODO decide whether a std::vector of active variables would be good!

  public:

    Logger& log;                           // responsible for logs

    Riss::MarkArray ma;                    // temporary markarray, that should be used only inside of methods
    std::vector<Riss::Lit> lits;           // temporary literal std::vector
    std::vector<Riss::CRef> clss;          // temporary literal std::vector

    CoprocessorData(Riss::ClauseAllocator& _ca, Riss::Solver* _solver, Coprocessor::Logger& _log, bool _limited = true, bool _randomized = false, bool _debug = false);

    // init all data structures for being used for nVars variables
    void init(uint32_t nVars);

    /** tell preprocessor to use randomized search now */
    void randomize() { randomOrder = true; }

    void preprocessing() { currentlyInprocessing = false; }
    void inprocessing() { currentlyInprocessing = true; }
    bool isInprocessing() const { return currentlyInprocessing; }

    // free all the resources that are used by this data object,
    void destroy();

    int32_t& operator[](const Riss::Lit& l);                            // return the number of occurrences of literal l
    int32_t operator[](const Riss::Var& v) const;                       // return the number of occurrences of variable v
    std::vector<Riss::CRef>& list(const Riss::Lit& l);                  // return the list of clauses, which have literal l
    const std::vector< Riss::CRef >& list(const Riss::Lit& l) const;    // return the list of clauses, which have literal l

    Riss::vec<Riss::CRef>& getClauses();   // return the std::vector of clauses in the solver object
    Riss::vec<Riss::CRef>& getLEarnts();   // return the std::vector of learnt clauses in the solver object
    Riss::vec<Riss::Lit>&  getTrail();     // return trail
    Riss::Compression& getCompression();   // return compression table of the solver
    void clearTrail();                     // remove all variables from the trail, and reset qhead in the solver
    void resetPPhead();                    // set the pointer to the next element to be propagated to 0

    uint32_t nCls()  const { return numberOfCls; }
    uint32_t nVars() const { return numberOfVars; }
    uint32_t nTotLits() const { return solver->nTotLits(); }
    Riss::Var nextFreshVariable(char type);

    /** overwrite data of variable to with data of variable from
     * Note: does not work on watches
     * @param final if true, decision heap will be re-build, and the set of variables will be shrinked to the given to variable
     */
    void moveVar(Riss::Var from, Riss::Var to, bool final = false);

    /** merge the solver data from one literal to another literal
     * two variants, one for two literals, one for many literals
     * @param final if true, decision heap will be re-build, and the set of variables will be shrinked to the given to variable
     */
    void mergeVar(Riss::Lit from, Riss::Lit to, bool final = false);
    void mergeVar(std::vector<Riss::Lit>& from, Riss::Lit to, bool final = false);

    /** notify about variable renaming */
    void didCompress()
    {
        //if (lastCompressUndoLits != -1 &&  // if there has been a  compression,
        //    decompressedUndoLits != undo.size()) {  // then the complete undo-stack has to be adopted
        //    std::cerr << "c variable renaming went wrong - abort. lastCom: "
        //              << lastCompressUndoLits
        //              << " decomp: " << decompressedUndoLits
        //              << " undo: " << undo.size() << std::endl;
        //    exit(14);
        //}

        lastCompressUndoLits = solver->compression.postvars();
    }

    /** return number of literals that have already been uncompressed
     * will return -1, if no compression took place yet
     */
    // int getLastCompressUndoLits() const { return lastCompressUndoLits; }
    // int getLastDecompressUndoLits() const { return decompressedUndoLits; }

// semantic:
    bool ok();                                             // return ok-state of solver
    void setFailed();                                      // found UNSAT, set ok state to false
    Riss::lbool enqueue(const Riss::Lit& l, const unsigned dependencyLevel = 0);  // enqueue literal l to current solver structures, adopt to extraInfo of solver, if needed
    Riss::lbool value(const Riss::Lit& l) const;           // return the assignment of a literal
    Riss::lbool value(const Riss::Var& v) const;           // return the assignment of a variable
    void resetAssignment(const Riss::Var v);               // set the polarity of a variable to l_Undef -- Note: be careful with this!

    Riss::Solver* getSolver();                             // return the pointer to the solver object
    bool hasToPropagate();                                 // signal whether there are new unprocessed units

    bool unlimited();                                      // do preprocessing without technique limits?
    bool randomized();                                     // use a random order for preprocessing techniques
    bool isInterupted();                    // has received signal from the outside

// adding, removing clauses and literals =======
    void addClause(const Riss::CRef& cr, bool check = false);                              // add clause to data structures, update counters
    void addClause(const Riss::CRef& cr, Riss::Heap< Coprocessor::VarOrderBVEHeapLt >* heap,
                   const bool update = false, const Riss::Var ignore = (-1),
                   SpinLock* data_lock = 0, SpinLock* heap_lock = 0);                       // add clause to data structures, update counters
    bool removeClauseFrom(const Riss::CRef& cr, const Riss::Lit& l);                        // remove clause reference from list of clauses for literal l, returns true, if successful
    void removeClauseFrom(const Riss::CRef& cr, const Riss::Lit& l, const int index);       // remove clause reference from list of clauses for literal l, returns true, if successful
    inline bool removeClauseFromThreadSafe(const Riss::CRef& cr, const Riss::Lit& l);       // replaces clause reference from clause list by Riss::CRef_Undef, returns true, if successful
    inline void cleanUpOccurrences(const Riss::MarkArray& dirtyOccs, const uint32_t timer); // removes Riss::CRef_Undef from all dirty occurrences
    void cleanOccurrences();                                                                // remove all clauses and set counters to 0

    // Garbage Collection
    void garbageCollect(std::vector<Riss::CRef> ** updateVectors = 0, int size = 0);
    void relocAll(Riss::ClauseAllocator& to, std::vector<Riss::CRef> ** updateVectors = 0, int size = 0);
    void checkGarbage(std::vector<Riss::CRef> ** updateVectors = 0, int size = 0) { return checkGarbage(solver->garbage_frac, updateVectors, size); }
    void checkGarbage(double gf, std::vector<Riss::CRef> ** updateVectors = 0, int size = 0) {  if (ca.wasted() > ca.size() * gf) { garbageCollect(updateVectors, size); }  }

    void updateClauseAfterDelLit(const Riss::Clause& clause)
    {
        if (global_debug_out) {
            std::cerr << "what to update in clause?! " << clause << std::endl;
        }
    }

    // sort
    void sortClauseLists(bool alsoLearnts = false);

    /** add all formulas back to the solver */
    void reSetupSolver();

// delete timers
    /** gives back the current times, increases for the next technique */
    uint32_t getMyModTimer();
    /** tell timer system that variable has been deleted (thread safe!) */
    void deletedVar(const Riss::Var v);
    /** fill the std::vector with all the literals that have been deleted after the given timer */
    void getActiveVariables(const uint32_t myTimer, std::vector<Riss::Var>& activeVariables, Riss::MarkArray* duplicateMarker = nullptr);
    /** fill the heap with all the literals that have been deleted afetr the given timer */
    template <class Comp>
    void getActiveVariables(const uint32_t myTimer, Riss::Heap<Comp>& heap, bool checkDuplicates = false);

    /** resets all delete timer */
    void resetModTimer();

// mark methods
    void mark1(Riss::Var x, Riss::MarkArray& array);
    void mark2(Riss::Var x, Riss::MarkArray& array, Riss::MarkArray& tmp);

// locking
    void lock()   { dataLock.lock();   } // lock and unlock the data structure
    void unlock() { dataLock.unlock(); } // lock and unlock data structure

// formula statistics with HeapUpdate and LockHandling

    void addedLiteral(const Riss::Lit& l, const int32_t diff = 1, Riss::Heap<VarOrderBVEHeapLt> * heap = nullptr, const bool update = false, const Riss::Var ignore = var_Undef, SpinLock * data_lock = nullptr, SpinLock * heap_lock = nullptr);   // update counter for literal
    void removedLiteral(const Riss::Lit& l, const int32_t diff = 1, Riss::Heap<VarOrderBVEHeapLt> * heap = nullptr, const bool update = false, const Riss::Var ignore = var_Undef, SpinLock * data_lock = nullptr, SpinLock * heap_lock = nullptr);   // update counter for literal
    void addedClause(const Riss::CRef& cr, Riss::Heap< Coprocessor::VarOrderBVEHeapLt >* heap = 0, const bool update = false, const Riss::Var ignore = (-1), SpinLock* data_lock = 0, SpinLock* heap_lock = 0);              // update counters for literals in the clause

    void removedClause(const Riss::CRef& cr, Riss::Heap<VarOrderBVEHeapLt> * heap = nullptr, const bool update = false, const Riss::Var ignore = var_Undef, SpinLock * data_lock = nullptr, SpinLock * heap_lock = nullptr);             // update counters for literals in the clause
    void removedClause(const Riss::Lit& l1, const Riss::Lit& l2);             // update counters for literals in the clause

    bool removeClauseThreadSafe(const Riss::CRef& cr);
    void correctCounters();

    // extending model after clause elimination procedures - l will be put first in list to be undone if necessary!
    void addToExtension(const Riss::CRef& cr, const Riss::Lit& l = Riss::lit_Error) { addToExtension(ca[cr], l); }
    void addToExtension(const Riss::Lit& dontTouch, const Riss::Lit& l = Riss::lit_Error);

    template<typename T>
    void addToExtension(const T& lits, const Riss::Lit& l);

    /** add already created vector to extension vector */
    template<typename T>
    void addExtensionToExtension(const T& lits);

    void extendModel(Riss::vec<Riss::lbool>& model);
    /** careful, should not be altered other than be the Dense object */
    std::vector<Riss::Lit>& getUndo() { return undo; }


    /** print formula (DIMACs), and dense, if another filename is given */
    void outputFormula(const char *file, const char *varMap = 0);

  private:
    /** write formula into file of file descriptor
     * @param clausesOnly: will not print the cnf header (e.g. to print something before)
     */
    void printFormula(FILE* fd, bool clausesOnly = false);
    inline void printClause(FILE * fd, Riss::CRef cr);
    inline void printLit(FILE * fd, int l);

  public:

    // for DRUP / DRAT proofs
    #ifdef DRATPROOF

    /** write the given clause/std::vector/Riss::vec to the output, if the output is enabled */
    template <class T>
    void addToProof(const T& clause, bool deleteFromProof = false, const Riss::Lit& remLit = Riss::lit_Undef);

    /** write a single unit clause to the proof */
    void addUnitToProof(const  Riss::Lit& l, bool deleteFromProof = false);
    void addCommentToProof(const char* text, bool deleteFromProof = false);

    /** return whether the solver outputs the drup proof! */
    bool outputsProof() const { return solver->outputsProof(); }

    #else // no DRAT proofs
    template <class T>
    void addToProof(const T& clause, bool deleteFromProof = false, const Riss::Lit& remLit = Riss::lit_Undef) const {};
    void addUnitToProof(const  Riss::Lit& l, bool deleteFromProof = false) const {};
    void addCommentToProof(const char* text, bool deleteFromProof = false) const {};
    bool outputsProof() const { return false; }
    #endif

    /// handling equivalent literals, not a constant list to be able to share it in priss (will not alter the list)
    void addEquivalences(const std::vector< Riss::Lit >& list);
    void addEquivalences(const Riss::Lit& l1, const Riss::Lit& l2);
    Riss::vec< Riss::Lit >& getEquivalences();
    Riss::vec< Riss::Lit >& replacedBy() { return solver->eqInfo.replacedBy; }

    /** add a clause to the queues, so that this clause will be checked by the next call to subsumeStrength
     * @return true, if clause has really been added and was not in both queues before
     */
    bool addSubStrengthClause(const Riss::CRef& cr, const bool& isNew = false);
    std::vector<Riss::CRef>& getSubsumeClauses();
    std::vector<Riss::CRef>& getStrengthClauses();

    // checking whether a literal can be altered - TODO: use the frozen information from the solver object!
    void setNotTouch(const Riss::Var& v);
    void unsetNotTouch(const Riss::Var& v);
    bool doNotTouch(const Riss::Var& v) const ;

    // TODO: remove after debug
    void printTrail(std::ostream& stream)
    {
        for (int i = 0 ; i < solver->trail.size(); ++ i) { std::cerr << " " << solver->trail[i]; }
    }

    /** return dependencyLevel of the given variable */
    unsigned dependencyLevel(const Riss::Var& v)
    {
        #ifdef PCASSO
        solver->vardata[ v ]. dependencyLevel;
        #else
        return 0;
        #endif
    }

    /** return the dependency level of the node the solver is operating on*/
    unsigned currentDependencyLevel() const
    {
        #ifdef PCASSO
        return solver->currentDependencyLevel();
        #else
        return 0;
        #endif
    }

    /**
     * Share units or a clause
     *
     * Note:
     *   equivalences are autoatically shared when they are added
     */
    template <typename T>
    #ifdef PCASSO
    void share(T* data, int dataSize, unsigned dependencyLevel, bool multiUnit = false)
    #else
    void share(T* data, int dataSize, bool multiUnit)
    #endif
    {
        #ifdef PCASSO
        solver->updateSleep(data, dataSize, dependencyLevel, multiUnit);
        #else
        solver->updateSleep(data, dataSize, multiUnit);
        #endif
    }

    /** convenient wrapper for the method share to share multiple unit clauses with other workers */
    void sendUnits(Riss::vec< Riss::Lit >& literalStorage, uint32_t beginIndex, int unitsToShare)
    {

        Riss::Lit* headPointer = & (literalStorage[beginIndex]);  // pointer to the actual element in the vector. as vectors use arrays to store data, the trick works
        #ifdef PCASSO
        share(&headPointer, unitsToShare , currentDependencyLevel(), true);  // give some value to the method, it will trace the dependencies for multi-units automatically
        #else
        share(&headPointer, unitsToShare , true);
        #endif
    }

};

/** class representing the binary implication graph of the formula */
class BIG
{
    // TODO implement a weak "implies" check based on the implication graph sampling!
    Riss::Lit* storage;
    int* sizes;
    Riss::Lit** big;

    /** these two arrays can be used to check whether a literal l implies another literal l'
     *  Note: this is not a complete check!
     */
    uint32_t *start; // when has to literal been touch when scanning the BIG
    uint32_t *stop;  // when has to literal been finished during scanning

    uint32_t duringCreationVariables; // number of variables for the last construction call

    uint32_t stampLiteral(const Riss::Lit& literal, uint32_t stamp, int32_t* index, std::deque< Riss::Lit >& stampQueue);
    void shuffle(Riss::Lit* adj, int size) const;

  public:
    BIG();
    ~BIG();

    /** adds binary clauses */
    void create(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list);
    void create(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list1, Riss::vec< Riss::CRef >& list2);

    /** recreate the big after the formula changed */
    void recreate(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list);
    void recreate(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list1, Riss::vec< Riss::CRef >& list2);

    /** return the number of variables that are known by the BIG */
    uint32_t getVars() const { return duringCreationVariables; }

    /** removes an edge from the graph again */
    void removeEdge(const Riss::Lit& l0, const Riss::Lit& l1);

    /** check all implication lists for duplicates and remove the duplicates
     * Note: side effect, the arrays are sorted
     */
    void removeDuplicateEdges(const uint32_t nVars);

    /** sort all the arrays */
    void sort(const uint32_t nVars);

    Riss::Lit* getArray(const Riss::Lit l);
    const Riss::Lit* getArray(const Riss::Lit& l) const;
    int getSize(const Riss::Lit& l) const;

    /** will travers the BIG and generate the start and stop indexes to check whether a literal implies another literal
     * @return false, if BIG is not initialized yet
     */
    void generateImplied(Coprocessor::CoprocessorData& data);
    void generateImplied(uint32_t nVars, Riss::vec<Riss::Lit>& tmpLits);   // alternative interface, to be able to use it during search!

    /** fill the literals in the order they would appear in a BFS in the big, starting with root nodes
     *  NOTE: will pollute the data.ma Riss::MarkArray
     * @param rootsOnly: fill the std::vector only with root literals
     */
    void fillSorted(std::vector< Riss::Lit >& literals, Coprocessor::CoprocessorData& data, bool rootsOnly = true, bool getAll = false);
    void fillSorted(std::vector<Riss::Var>& variables, CoprocessorData& data, bool rootsOnly = true, bool getAll = false);

    /** return true, if the condition "from -> to" holds, based on the stochastic scanned data */
    bool implies(const Riss::Lit& from, const Riss::Lit& to) const;

    /** return whether child occurs in the adjacence list of parent (and thus implied) */
    bool isChild(const Riss::Lit& parent, const Riss::Lit& child) const ;

    /** return whether one of the two literals is a direct child of parent (and thus implied)  */
    bool isOneChild(const Riss::Lit& parent, const Riss::Lit& child1, const Riss::Lit& child2) const ;

    /** get indexes of BIG scan algorithm */
    uint32_t getStart(const Riss::Lit& l) { return start != 0 && var(l) < duringCreationVariables ? start[ Riss::toInt(l) ] : 0; }
    /** get indexes of BIG scan algorithm */
    uint32_t getStop(const Riss::Lit& l) { return stop != 0 && var(l) < duringCreationVariables  ? stop[ Riss::toInt(l) ] : 0; }

};

/** Comperator for Variable Riss::Heap of BVE */
struct VarOrderBVEHeapLt {
    CoprocessorData& data;
    const int heapOption;
    bool operator()(Riss::Var x, Riss::Var y) const
    {
        /* assert (data != nullptr && "Please assign a valid data object before heap usage" );*/
        if (heapOption == 0) {
            return data[x] < data[y];
        } else if (heapOption == 1) {
            return data[x] > data[y];
        } else if (heapOption > 2 && heapOption < 11) {
            const double xp = data[ Riss::mkLit(x, false) ];
            const double xn = data[ Riss::mkLit(x, true)  ];
            const double yp = data[ Riss::mkLit(y, false) ];
            const double yn = data[ Riss::mkLit(y, true)  ];
            double rx = 0;
            if (xp != 0 || xn != 0) { rx = xp > xn ? (xn != 0 ? xp / xn : xp * 1000) : (xp != 0 ? xn  / xp : xn * 1000); }
            double ry = 0;
            if (yp != 0 || yn != 0) { ry = yp > yn ? (yn != 0 ? yp / yn : yp * 1000) : (yp != 0 ? yn  / yp : yn * 1000); }

            if (heapOption == 3) {
                return (rx < ry)
                       || (rx == ry && data[x] < data[y]);
            } else if (heapOption == 4)  {
                return (rx < ry)
                       || (rx == ry &&  data[x] > data[y]);
            } else if (heapOption == 5)  {
                return (rx > ry)
                       || (rx == ry &&  data[x] < data[y]);
            } else if (heapOption == 6)  {
                return (rx > ry)
                       || (rx == ry &&  data[x] > data[y]);
            } else if (heapOption == 7) {
                return (data[x] < data[y])
                       || (rx < ry && data[x] == data[y]);
            } else if (heapOption == 8)  {
                return (data[x] > data[y])
                       || (rx < ry &&  data[x] == data[y]);
            } else if (heapOption == 9)  {
                return (data[x] < data[y])
                       || (rx > ry &&  data[x] == data[y]);
            } else if (heapOption == 10)  {
                return (data[x] > data[y])
                       || (rx > ry &&  data[x] == data[y]);
            } else {
                assert(false && "forgot to update all parameter checks!");
                return false;
            }
        } else {
            assert(false && "In case of random order no heap should be used"); return false;
        }
        return false;
    }
    VarOrderBVEHeapLt(CoprocessorData& _data, int _heapOption) : data(_data), heapOption(_heapOption) { }
};

struct LitOrderHeapLt {
    CoprocessorData& data;
    const int heapOption;
    bool operator()(int ix, int iy) const
    {
        /* assert (data != nullptr && "Please assign a valid data object before heap usage" );*/
        const Riss::Lit x = Riss::toLit(ix); const Riss::Lit y = Riss::toLit(iy);
        if (heapOption == 0) {
            return data[x] < data[y];
        } else if (heapOption == 1) {
            return data[x] > data[y];
        } else if (heapOption > 2 && heapOption < 11) {
            const double xp = data[x];
            const double xn = data[~x];
            const double yp = data[y];
            const double yn = data[~y];
            double rx = 0;
            if (xp != 0 || xn != 0) { rx = xp > xn ? (xn != 0 ? xp / xn : xp * 1000) : (xp != 0 ? xn  / xp : xn * 1000); }
            double ry = 0;
            if (yp != 0 || yn != 0) { ry = yp > yn ? (yn != 0 ? yp / yn : yp * 1000) : (yp != 0 ? yn  / yp : yn * 1000); }

            if (heapOption == 3) {
                return (rx < ry)
                       || (rx == ry && data[x] < data[y]);
            } else if (heapOption == 4)  {
                return (rx < ry)
                       || (rx == ry &&  data[x] > data[y]);
            } else if (heapOption == 5)  {
                return (rx > ry)
                       || (rx == ry &&  data[x] < data[y]);
            } else if (heapOption == 6)  {
                return (rx > ry)
                       || (rx == ry &&  data[x] > data[y]);
            } else if (heapOption == 7) {
                return (data[x] < data[y])
                       || (rx < ry && data[x] == data[y]);
            } else if (heapOption == 8)  {
                return (data[x] > data[y])
                       || (rx < ry &&  data[x] == data[y]);
            } else if (heapOption == 9)  {
                return (data[x] < data[y])
                       || (rx > ry &&  data[x] == data[y]);
            } else if (heapOption == 10)  {
                return (data[x] > data[y])
                       || (rx > ry &&  data[x] == data[y]);
            } else {
                assert(false && "forgot to update all parameter checks!");
            }
        } else {
            assert(false && "In case of random order no heap should be used, or wrong parameter for heap comparison"); return false;
        }
        return false;
    }
    LitOrderHeapLt(CoprocessorData& _data, int _heapOption, bool allowRandom = true) : data(_data), heapOption(allowRandom ? _heapOption : (_heapOption > 1 ? _heapOption + 1 : _heapOption))
    {
        assert((allowRandom || heapOption != 2) && "only allow heap option 2 if random selection is allowed");
    }
};

inline CoprocessorData::CoprocessorData(Riss::ClauseAllocator& _ca, Riss::Solver* _solver, Coprocessor::Logger& _log, bool _limited, bool _randomized,  bool _debug)
    : ca(_ca)
    , solver(_solver)
    , numberOfVars(0)
    , numberOfCls(0)
    , numberOfTotalLiterals(_solver->tot_literals)
    , hasLimit(_limited)
    , randomOrder(_randomized)
    , currentlyInprocessing(false)
    , debugging(_debug)
    , lastCompressUndoLits(-1)
      //, decompressedUndoLits(-1)
    , log(_log)
{
}

inline void CoprocessorData::init(uint32_t nVars)
{
    occs.resize(nVars * 2);
    lit_occurrence_count.resize(nVars * 2, 0);
    numberOfVars = nVars;
    modTimer.create(nVars);

    //if there is still something in the queues, get rid of it!
    getStrengthClauses().clear();
    getSubsumeClauses().clear();
}

inline void CoprocessorData::destroy()
{
    ComplOcc().swap(occs); // free physical space of the std::vector
    std::vector<int32_t>().swap(lit_occurrence_count);
    modTimer.destroy();
}

inline Riss::vec< Riss::CRef >& CoprocessorData::getClauses()
{
    return solver->clauses;
}

inline Riss::vec< Riss::CRef >& CoprocessorData::getLEarnts()
{
    return solver->learnts;
}

inline Riss::vec< Riss::Lit >& CoprocessorData::getTrail()
{
    return solver->trail;
}

inline Riss::Compression& CoprocessorData::getCompression()
{
    return solver->compression;
}

inline void CoprocessorData::clearTrail()
{
    solver->trail.clear();
    solver->qhead = 0;
}

inline void CoprocessorData::resetPPhead()
{
    solver->qhead = 0;
}


inline Riss::Var CoprocessorData::nextFreshVariable(char type)
{
    // be careful here
    Riss::Var nextVar = solver->newVar(true, true, type);
    numberOfVars = solver->nVars();
    ma.resize(2 * nVars());

    modTimer.resize(2 * nVars());

    occs.resize(2 * nVars());
    // std::cerr << "c resize occs to " << occs.size() << std::endl;
    lit_occurrence_count.resize(2 * nVars());

    // std::cerr << "c new fresh variable: " << nextVar+1 << std::endl;
    return nextVar;
}

inline void CoprocessorData::moveVar(Riss::Var from, Riss::Var to, bool final)
{
    if (from != to) {   // move data only if necessary
        solver->varFlags[to].assigns = solver->varFlags[from].assigns; solver->varFlags[from].assigns = l_Undef;
        solver->vardata[to]  = solver->vardata[from];  solver->vardata[from] = Riss::Solver::VarData();
        solver->activity[to] = solver->activity[from]; solver->activity[from] = 0;
        solver->varFlags[to].seen = solver->varFlags[to].seen; solver->varFlags[to].seen = 0;
        solver->varFlags[to].polarity = solver->varFlags[from].polarity; solver->varFlags[from].polarity = 0;
        solver->varFlags[to].decision = solver->varFlags[from].decision; solver->varFlags[from].decision = false;
        solver->varFlags[to].frozen = solver->varFlags[from].frozen; solver->varFlags[from].frozen = false;

        solver->eqInfo.replacedBy[to] = solver->eqInfo.replacedBy[from]; solver->eqInfo.replacedBy[from] = Riss::mkLit(from, false);

        // cp3 structures
        lit_occurrence_count[Riss::toInt(Riss::mkLit(to, false))] = lit_occurrence_count[Riss::toInt(Riss::mkLit(from, false))];
        lit_occurrence_count[Riss::toInt(Riss::mkLit(to, true))] = lit_occurrence_count[Riss::toInt(Riss::mkLit(from, true))];
        occs[Riss::toInt(Riss::mkLit(to, false))].swap(occs[Riss::toInt(Riss::mkLit(from, false))]);
        occs[Riss::toInt(Riss::mkLit(to, true))].swap(occs[Riss::toInt(Riss::mkLit(from, true))]);
    }
    if (final == true) {

//         std::cerr << "c compress variables to " << to+1 << std::endl;
        solver->vardata.shrink_(solver->vardata.size() - to - 1);
        solver->activity.shrink_(solver->activity.size() - to - 1);
        solver->varFlags.shrink_(solver->varFlags.size() - to - 1);

        solver->rebuildOrderHeap();

        // resize the renaming vector
        solver->eqInfo.replacedBy.shrink_(solver->eqInfo.replacedBy.size() - solver->nVars());
	
        // set cp3 variable representation!
        numberOfVars = solver->nVars();
        lit_occurrence_count.resize(nVars() * 2);
        occs.resize(nVars() * 2);
    }
}

inline void CoprocessorData::mergeVar(Riss::Lit from, Riss::Lit to, bool final)
{

}

inline void CoprocessorData::mergeVar(std::vector<Riss::Lit>& from, Riss::Lit to, bool final)
{

}

inline bool CoprocessorData::ok()
{
    return solver->ok;
}

inline void CoprocessorData::setFailed()
{
    solver->ok = false;
}

inline bool CoprocessorData::unlimited()
{
    return !hasLimit;
}

inline bool CoprocessorData::randomized()
{
    return randomOrder;
}

inline bool CoprocessorData::isInterupted()
{
    return solver->asynch_interrupt || (0 != solver->terminationCallbackMethod && 0 != solver->terminationCallbackMethod(solver->terminationCallbackState));
}


inline Riss::lbool CoprocessorData::enqueue(const Riss::Lit& l, const unsigned int dependencyLevel)
{
    if (false || global_debug_out) { std::cerr << "c enqueue " << l << " with previous value " << (solver->value(l) == l_Undef ? "undef" : (solver->value(l) == l_False ? "unsat" : " sat ")) << std::endl; }
    if (solver->value(l) == l_False) {
        solver->ok = false; // set state to false
        return l_False;
    } else if (solver->value(l) == l_Undef) {
        #ifdef PCASSO
        solver->uncheckedEnqueue(l, dependencyLevel);
        #else
        solver->uncheckedEnqueue(l);
        #endif
        return l_True;
    }
    return l_Undef;
}

inline Riss::lbool CoprocessorData::value(const Riss::Lit& l) const
{
    return solver->value(l);
}

inline Riss::lbool CoprocessorData::value(const Riss::Var& v) const
{
    return solver->value(v);
}

inline void CoprocessorData::resetAssignment(const Riss::Var v)
{
    solver->varFlags[ v ].assigns = l_Undef;
}


inline Riss::Solver* CoprocessorData::getSolver()
{
    return solver;
}


inline bool CoprocessorData::hasToPropagate()
{
    return solver->trail.size() != solver->qhead;
}


inline void CoprocessorData::addClause(const Riss::CRef& cr, bool check)
{
    const Riss::Clause& c = ca[cr];
    if (c.can_be_deleted()) { return; }
    bool somePosInClause = false, somNegInClause = false;
    for (int l = 0; l < c.size(); ++l) {
        // std::cerr << "c add clause " << cr << " to list for " << c[l] << std::endl;
        if (check) {
            for (int i = 0 ; i < occs[Riss::toInt(c[l])].size(); ++ i) {
                if (occs[Riss::toInt(c[l])][i] == cr) {
                    std::cerr << "c clause " << cr << " is already in list for lit " << c[l] << " clause is: " << ca[cr] << std::endl;
                }
            }
        }
        occs[Riss::toInt(c[l])].push_back(cr);
        lit_occurrence_count[Riss::toInt(c[l])] += 1;
        somePosInClause = somePosInClause || !sign(c[l]);
        somNegInClause  = somNegInClause  || sign(c[l]);
    }
    if (c.size() > 1) { solver->updatePosNeg(somePosInClause, somNegInClause); }  // tell solver whether we can still use the polarity information, only for large clauses
    numberOfCls ++;
}

inline void CoprocessorData::addClause(const Riss::CRef& cr , Riss::Heap<VarOrderBVEHeapLt> * heap, const bool update, const Riss::Var ignore, SpinLock * data_lock, SpinLock * heap_lock)
{
    const Riss::Clause& c = ca[cr];
    if (c.can_be_deleted()) { return; }
    if (heap == nullptr && data_lock == nullptr && heap_lock == nullptr) {
        for (int l = 0; l < c.size(); ++l) {
            occs[Riss::toInt(c[l])].push_back(cr);
            lit_occurrence_count[Riss::toInt(c[l])] += 1;
        }
        numberOfCls ++;
    } else {
        if (data_lock != nullptr) { data_lock->lock(); }
        if (heap_lock != nullptr) { heap_lock->lock(); }
        for (int l = 0; l < c.size(); ++l) {
            occs[Riss::toInt(c[l])].push_back(cr);
            lit_occurrence_count[Riss::toInt(c[l])] += 1;
            if (heap != nullptr) {
                if (heap->inHeap(var(c[l]))) {
                    heap->increase(var(c[l]));
                } else {
                    if (update && var(c[l]) != ignore) {
                        heap->update(var(c[l]));
                    }
                }
            }
        }
        if (heap_lock != nullptr) { heap_lock->unlock(); }
        numberOfCls ++;
        if (data_lock != nullptr) { data_lock->unlock(); }
    }
}

inline bool CoprocessorData::removeClauseFrom(const Riss::CRef& cr, const Riss::Lit& l)
{
    std::vector<Riss::CRef>& list = occs[Riss::toInt(l)];
    for (int i = 0 ; i < list.size(); ++ i) {
        if (list[i] == cr) {
            list[i] = list[ list.size() - 1 ];
            list.pop_back();
            return true;
        }
    }
    return false;
}

inline void CoprocessorData::removeClauseFrom(const Riss::CRef& cr, const Riss::Lit& l, const int index)
{
    std::vector<Riss::CRef>& list = occs[Riss::toInt(l)];
    assert(list[index] == cr);
    list[index] = list[ list.size() - 1 ];
    list.pop_back();
}

/** replaces clause reference from clause list by Riss::CRef_Undef, returns true, if successful
 *  asynchronous list modification
 */
inline bool CoprocessorData::removeClauseFromThreadSafe(const Riss::CRef& cr, const Riss::Lit& l)
{
    assert(cr != Riss::CRef_Undef);
    std::vector<Riss::CRef>& list = occs[Riss::toInt(l)];
    for (int i = 0 ; i < list.size(); ++ i) {
        if (list[i] == cr) {
            list[i] = Riss::CRef_Undef;
            return true;
        }
    }
    return false;
}

/** removes Riss::CRef_Undef from all dirty occurrences
 *  should be used sequentiell or with exclusive occ-access
 *
 *  @param dirtyOccs (on Lits !)
 */
inline void CoprocessorData::cleanUpOccurrences(const Riss::MarkArray& dirtyOccs, const uint32_t timer)
{
    for (int l = 0 ; l < dirtyOccs.size() ; ++ l) {
        if (dirtyOccs.getIndex(l) >= timer) {
            std::vector<Riss::CRef>& list = occs[l];
            int i = 0;
            while (i < list.size()) {
                if (list[i] == Riss::CRef_Undef) {
                    list[i] = list[list.size() - 1];
                    list.pop_back();
                    continue;
                }
                ++i;
            }
        }
    }
}

inline void CoprocessorData::cleanOccurrences()
{
    for (Riss::Var v = 0; v < nVars(); ++v) {
        list(Riss::mkLit(v, false)).clear();
        list(Riss::mkLit(v, true)).clear();
    }
    lit_occurrence_count.assign(0, nVars() * 2);
}

inline
void CoprocessorData::reSetupSolver()
{
    assert(solver->decisionLevel() == 0 && "can re-setup solver only if it is at decision level 0!");
    int kept_clauses = 0;

    // check whether reasons of top level literals are marked as deleted. in this case, set reason to CRef_Undef!
    if (solver->trail_lim.size() > 0)
        for (int i = 0 ; i < solver->trail_lim[0]; ++ i)
            if (!solver->reason(var(solver->trail[i])).isBinaryClause() && solver->reason(var(solver->trail[i])).getReasonC() != CRef_Undef)
                if (ca[ solver->reason(var(solver->trail[i])).getReasonC() ].can_be_deleted()) {
                    solver->vardata[ var(solver->trail[i]) ].reason.setReason(CRef_Undef);
                }

    // give back into solver
    for (int i = 0; i < solver->clauses.size(); ++i) {
        const CRef cr = solver->clauses[i];
        Clause& c = ca[cr];
        assert(c.size() != 0 && "empty clauses should be recognized before re-setup");
        if (c.can_be_deleted()) {
            c.mark(1);
            ca.free(cr);
        } else {
            assert(c.mark() == 0 && "only clauses without a mark should be passed back to the solver!");
            if (c.size() > 1) {
                if (solver->qhead == 0) {    // do not change the clause, if nothing has been propagated yet
                    solver->attachClause(cr);
                    solver->clauses[kept_clauses++] = cr; // add original clauss back!
                    continue;
                }

                // do not watch literals that are false!
                int j = 1;
                for (int k = 0 ; k < 2; ++ k) {   // ensure that the first two literals are undefined!
                    if (solver->value(c[k]) == l_False) {
                        for (; j < c.size() ; ++j)
                            if (solver->value(c[j]) != l_False)
                            { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
                    }
                }

                // reduct of clause is empty, or unit
                if (solver->value(c[0]) == l_False) { setFailed(); return; }
                else if (solver->value(c[1]) == l_False) {
                    if (enqueue(c[0]) == l_False) {
                        addCommentToProof("learnt unit during resetup solver");
                        addUnitToProof(c[0]);   // tell drup about this unit (whereever it came from)
                    } else {
                        c.set_delete(true);
                    }
                    if (solver->propagate() != CRef_Undef) { setFailed(); return; }
                    c.set_delete(true);
                } else {
                    solver->attachClause(cr);
                    solver->clauses[kept_clauses++] = cr; // add original clauss back!
                }
            } else {
                if (solver->value(c[0]) == l_Undef) {
                    if (enqueue(c[0]) == l_False) {
                        addCommentToProof("learnt unit during resetup solver");
                        addUnitToProof(c[0]);   // tell drup about this unit (whereever it came from)
                        return;
                    } else if (solver->value(c[0]) == l_False) {
                        // assert( false && "This UNSAT case should be recognized before re-setup" );
                        setFailed();
                    }
                }
                c.set_delete(true);
            }
        }
    }
    int c_old = solver->clauses.size();
    solver->clauses.shrink_(solver->clauses.size() - kept_clauses);

    int learntToClause = 0;
    kept_clauses = 0;
    for (int i = 0; i < solver->learnts.size(); ++i) {
        const CRef cr = solver->learnts[i];
        Clause& c = ca[cr];
        assert(c.size() != 0 && "empty clauses should be recognized before re-setup");
        if (c.can_be_deleted()) {
            c.mark(1);
            ca.free(cr);
            continue;
        }
        assert(c.mark() == 0 && "only clauses without a mark should be passed back to the solver!");
        if (c.learnt()) {
            if (c.size() > 1) {
                solver->learnts[kept_clauses++] = solver->learnts[i];
            }
        } else { // move subsuming clause from learnts to clauses
            c.set_has_extra(true);
            c.calcAbstraction();
            learntToClause ++;
            if (c.size() > 1) {
                solver->clauses.push(cr);
            }
        }
        assert(c.mark() == 0 && "only clauses without a mark should be passed back to the solver!");
        if (c.size() > 1) {
            // do not watch literals that are false!
            int j = 1;
            for (int k = 0 ; k < 2; ++ k) {   // ensure that the first two literals are undefined!
                if (solver->value(c[k]) == l_False) {
                    for (; j < c.size() ; ++j)
                        if (solver->value(c[j]) != l_False)
                        { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
                }
            }
            // assert( (solver->value( c[0] ) != l_False || solver->value( c[1] ) != l_False) && "Cannot watch falsified literals" );

            // reduct of clause is empty, or unit
            if (solver->value(c[0]) == l_False) { setFailed(); return; }
            else if (solver->value(c[1]) == l_False) {
                addCommentToProof("learnt unit during resetup solver");
                addUnitToProof(c[0]);   // tell drup about this unit (whereever it came from)
                if (enqueue(c[0]) == l_False) {
                    return;
                }
                if (solver->propagate() != CRef_Undef) { setFailed(); return; }
                c.set_delete(true);
            } else { solver->attachClause(cr); }
        } else if (solver->value(c[0]) == l_Undef) {
            if (enqueue(c[0]) == l_False) {
                addCommentToProof("learnt unit during resetup solver");
                addUnitToProof(c[0]);   // tell drup about this unit (whereever it came from)
                return;
            }
        } else if (solver->value(c[0]) == l_False) {
            // assert( false && "This UNSAT case should be recognized before re-setup" );
            setFailed();
        }

    }
    solver->learnts.shrink_(solver->learnts.size() - kept_clauses);
}


inline void CoprocessorData::sortClauseLists(bool alsoLearnts)
{
    for (int p = 0 ; p < (alsoLearnts ? 2 : 1); ++ p) {
        Riss::vec<Riss::CRef>& clauseList = (p == 0 ? getClauses() : getLEarnts());
        int32_t n = clauseList.size();
        int32_t m, s;
        // copy elements from std::vector
        Riss::CRef* tmpA = new Riss::CRef[ n ];
        Riss::CRef* a = tmpA;
        for (int32_t i = 0 ; i < n; i++) {
            a[i] = clauseList[i];
        }
        Riss::CRef *tmpB = new Riss::CRef[n];
        Riss::CRef *b = tmpB;

        // size of work fields, power of 2
        for (s = 1; s < n; s += s) {
            m = n;
            do {
                m = m - 2 * s;  // set begin of working field
                int32_t hi = (m + s > 0) ? m + s : 0; // set middle of working field

                int32_t i = (m > 0) ? m : 0;    // lowest position in field
                int32_t j = hi;

                int32_t stopb = m + 2 * s;  // upper bound of current work area
                int32_t currentb = i;           // current position in field for copy

                // merge two sorted fields into one
                while (i < hi && j < stopb) {
                    if ((ca[a[i]]) < (ca[a[j]])) {
                        b[currentb++] = a[i++];
                    } else {
                        b[currentb++] = a[j++];
                    }
                }
                // copy rest of the elements
                for (; i < hi;) {
                    b[currentb++] = a[i++];
                }

                for (; j < stopb;) {
                    b[currentb++] = a[j++];
                }

            } while (m > 0);

            // swap fields!
            Riss::CRef* tmp = a; a = b; b = tmp;
        }
        // write data back into std::vector
        for (int32_t i = 0 ; i < n; i++) { clauseList[i] = a[i]; }

        delete [] tmpA;
        delete [] tmpB;
    }
}


inline uint32_t CoprocessorData::getMyModTimer()
{
    // if an overflow occurs, the step will be reset to 0, but also the whole
    // array is memsetted to 0. So, there is no problem with an overflow and
    // the modification time.
    return modTimer.nextStep();
}

inline void CoprocessorData::deletedVar(const Riss::Var v)
{
    modTimer.setCurrentStep(v);
}

inline void CoprocessorData::getActiveVariables(const uint32_t myTimer, std::vector< Riss::Var >& activeVariables,
        Riss::MarkArray* duplicateMarker)
{
    // check for duplicate variables
    if (duplicateMarker != nullptr) {
        for (Riss::Var v = 0 ; v < solver->nVars(); ++ v) {
            if (modTimer.getIndex(v) >= myTimer && !duplicateMarker->isCurrentStep(v)) {
                activeVariables.push_back(v);
            }
        }
    }
    // no check for duplicates
    else {
        for (Riss::Var v = 0 ; v < solver->nVars(); ++ v) {
            if (modTimer.getIndex(v) >= myTimer) {
                activeVariables.push_back(v);
            }
        }
    }

}

template<class Comp>
inline void CoprocessorData::getActiveVariables(const uint32_t myTimer, Riss::Heap< Comp >& heap, bool checkDuplicates)
{
    // check for duplicate variables
    if (checkDuplicates) {
        for (Riss::Var v = 0 ; v < solver->nVars(); ++ v) {
            if (modTimer.getIndex(v) >= myTimer && !heap.inHeap(v)) {
                heap.insert(v);
            }
        }
    }
    // no check for duplicates
    else {
        for (Riss::Var v = 0 ; v < solver->nVars(); ++ v) {
            if (modTimer.getIndex(v) >= myTimer) {
                heap.insert(v);
            }
        }
    }
}


inline void CoprocessorData::resetModTimer()
{
    modTimer.reset();
}

inline void CoprocessorData::removedClause(const Riss::Lit& l1, const Riss::Lit& l2)
{
    removedLiteral(l1);
    removedLiteral(l2);

    const Riss::Lit searchLit = lit_occurrence_count[Riss::toInt(l1)] < lit_occurrence_count[Riss::toInt(l2)] ? l1 : l2;
    const Riss::Lit secondLit = Riss::toLit(Riss::toInt(l1) ^ Riss::toInt(l2) ^ Riss::toInt(searchLit));

    // find the right binary clause and remove it!
    for (int i = 0 ; i < list(searchLit).size(); ++ i) {
        Riss::Clause& cl = ca[list(searchLit)[i]];
        if (cl.can_be_deleted() || cl.size() != 2) { continue; }
        if (cl[0] == secondLit || cl[1] == secondLit) {
            cl.set_delete(true);
            addCommentToProof("remove binary clause");
            addToProof(cl, true);
            break;
        }
    }
}

inline void CoprocessorData::addedLiteral(const Riss::Lit& l, const int32_t diff, Riss::Heap<VarOrderBVEHeapLt> * heap, const bool update, const Riss::Var ignore, SpinLock * data_lock, SpinLock * heap_lock)
{
    if (heap == nullptr && data_lock == nullptr && heap_lock == nullptr) {
        lit_occurrence_count[Riss::toInt(l)] += diff;
    } else {
        if (data_lock != nullptr) {
            data_lock->lock();
        }
        if (heap_lock != nullptr) {
            heap_lock->lock();
        }
        lit_occurrence_count[Riss::toInt(l)] += diff;
        if (heap != nullptr) {
            if (heap->inHeap(var(l))) {
                heap->increase(var(l));
            } else if (update && var(l) != ignore) {
                heap->update(var(l));
            }
        }
        if (heap_lock != nullptr) {
            heap_lock->unlock();
        }
        if (data_lock != nullptr) {
            data_lock->unlock();
        }
    }
}
inline void CoprocessorData::removedLiteral(const Riss::Lit& l, const int32_t diff, Riss::Heap<VarOrderBVEHeapLt> * heap, const bool update, const Riss::Var ignore, SpinLock * data_lock, SpinLock * heap_lock)   // update counter for literal
{
    if (heap == nullptr && data_lock == nullptr && heap_lock == nullptr) {
        deletedVar(var(l));
        lit_occurrence_count[Riss::toInt(l)] -= diff;
        ca.freeLit();
        //assert(lit_occurrence_count[Riss::toInt(l)] == countLitOcc(l));
    } else {
        if (data_lock != nullptr) {
            data_lock->lock();
        }
        if (heap_lock != nullptr) {
            heap_lock->lock();
        }
        deletedVar(var(l));
        lit_occurrence_count[Riss::toInt(l)] -= diff;
        ca.freeLit();
        //assert(lit_occurrence_count[Riss::toInt(l)] == countLitOcc(l));
        if (heap != nullptr) {
            if (heap->inHeap(var(l))) {
                heap->decrease(var(l));
            } else if (update && var(l) != ignore) {
                heap->update(var(l));
            }
        }
        if (heap_lock != nullptr) {
            heap_lock->unlock();
        }
        if (data_lock != nullptr) {
            data_lock->unlock();
        }
    }
}
inline void CoprocessorData::addedClause(const Riss::CRef& cr, Riss::Heap<VarOrderBVEHeapLt> * heap, const bool update , const Riss::Var ignore, SpinLock * data_lock, SpinLock * heap_lock)              // update counters for literals in the clause
{
    const Riss::Clause& c = ca[cr];
    if (heap == nullptr && data_lock == nullptr && heap_lock == nullptr) {
        for (int l = 0; l < c.size(); ++l) {
            lit_occurrence_count[Riss::toInt(c[l])] += 1;
        }
        numberOfCls++;
    } else {
        if (data_lock != nullptr) {
            data_lock->lock();
        }
        if (heap_lock != nullptr) {
            heap_lock->lock();
        }
        for (int l = 0; l < c.size(); ++l) {
            lit_occurrence_count[Riss::toInt(c[l])] += 1;
            if (heap != nullptr) {
                if (heap->inHeap(var(c[l]))) {
                    heap->increase(var(c[l]));
                } else if (update && var(c[l]) != ignore) {
                    heap->update(var(c[l]));
                }
            }
        }
        numberOfCls++;
        if (heap_lock != nullptr) {
            heap_lock->unlock();
        }
        if (data_lock != nullptr) {
            data_lock->unlock();
        }
    }
}
inline void CoprocessorData::removedClause(const Riss::CRef& cr, Riss::Heap< Coprocessor::VarOrderBVEHeapLt >* heap, const bool update, const Riss::Var ignore, SpinLock* data_lock, SpinLock* heap_lock)             // update counters for literals in the clause
{
    const Riss::Clause& c = ca[cr];
    if (heap == nullptr && data_lock == nullptr && heap_lock == nullptr) {
        for (int l = 0; l < c.size(); ++l) {
            deletedVar(var(c[l]));
            --lit_occurrence_count[Riss::toInt(c[l])];
            //assert(lit_occurrence_count[Riss::toInt(c[l])] == countLitOcc(c[l]));
        }
        numberOfCls --;
        ca.free(cr);
    } else {
        if (data_lock != nullptr) {
            data_lock->lock();
        }
        if (heap_lock != nullptr) {
            heap_lock->lock();
        }
        for (int l = 0; l < c.size(); ++l) {
            deletedVar(var(c[l]));
            --lit_occurrence_count[Riss::toInt(c[l])];
            //assert(lit_occurrence_count[Riss::toInt(c[l])] == countLitOcc(c[l]));

            if (heap != nullptr) {
                if (heap->inHeap(var(c[l]))) {
                    heap->decrease(var(c[l]));
                } else if (update && var(c[l]) != ignore) {
                    heap->update(var(c[l]));
                }
            }
        }
        numberOfCls --;
        ca.free(cr);
        if (heap_lock != nullptr) {
            heap_lock->unlock();
        }
        if (data_lock != nullptr) {
            data_lock->unlock();
        }
    }
}

inline int32_t& CoprocessorData::operator[](const Riss::Lit& l)
{
    return lit_occurrence_count[Riss::toInt(l)];
}

inline int32_t CoprocessorData::operator[](const Riss::Var& v) const
{
    return lit_occurrence_count[Riss::toInt(Riss::mkLit(v, 0))] + lit_occurrence_count[Riss::toInt(Riss::mkLit(v, 1))];
}

inline std::vector< Riss::CRef >& CoprocessorData::list(const Riss::Lit& l)
{
    return occs[ Riss::toInt(l) ];
}

inline const std::vector< Riss::CRef >& CoprocessorData::list(const Riss::Lit& l) const
{
    return occs[ Riss::toInt(l) ];
}

inline void CoprocessorData::correctCounters()
{
    numberOfVars = solver->nVars();
    numberOfCls = 0;
    // reset to 0
    for (int v = 0; v < solver->nVars(); v++)
        for (int s = 0; s < 2; s++) {
            lit_occurrence_count[ Riss::toInt(Riss::mkLit(v, s)) ] = 0;
        }
    // re-calculate counters!
    for (int i = 0 ; i < solver->clauses.size(); ++ i) {
        const Riss::Clause& c = ca[ solver->clauses[i] ];
        if (c.can_be_deleted()) { continue; }
        numberOfCls ++;
        for (int j = 0 ; j < c.size(); j++) { lit_occurrence_count[ Riss::toInt(c[j]) ] ++; }  // increment all literal counters accordingly
    }
    for (int i = 0 ; i < solver->learnts.size(); ++ i) {
        const Riss::Clause& c = ca[ solver->learnts[i] ];
        if (c.can_be_deleted()) { continue; }
        numberOfCls ++;
        for (int j = 0 ; j < c.size(); j++) { lit_occurrence_count[ Riss::toInt(c[j]) ] ++; }  // increment all literal counters accordingly
    }
}

inline void CoprocessorData::garbageCollect(std::vector<Riss::CRef> ** updateVectors, int size)
{
    if (debugging) {
        std::cerr << "c check garbage collection [REJECTED DUE TO DEBUGGING] " << std::endl;
        return;
    }
    Riss::ClauseAllocator to((ca.size() >= ca.wasted()) ? ca.size() - ca.wasted() : 0);  //FIXME just a workaround
    // correct add / remove would be nicer

    DOUT(std::cerr << "c garbage collection ... " << std::endl;);
    relocAll(to, updateVectors);
    DOUT(std::cerr << "c Garbage collection: " << ca.size()*Riss::ClauseAllocator::Unit_Size
         << " bytes => " << to.size()*Riss::ClauseAllocator::Unit_Size <<  " bytes " << std::endl;);

    to.moveTo(ca);
}

inline void CoprocessorData::relocAll(Riss::ClauseAllocator& to, std::vector<Riss::CRef> ** updateVectors, int size)
{
    // Update Vectors
    if (size > 0 && updateVectors != 0) {
        for (int v_ix = 0; v_ix < size; ++v_ix) {
            if (updateVectors[v_ix] == 0) {
                continue;
            }
            std::vector<Riss::CRef>& list = *(updateVectors[v_ix]);
            int i, j;
            for (i = j = 0; i < list.size(); ++i) {
                Riss::Clause& c = ca[list[i]];
                if (c.can_be_deleted()) {
                    // removeClause(list[i]);
                } else {
                    ca.reloc(list[i], to);
                    list[j++] = list[i];
                }
            }
            list.resize(j);
        }
    }

    // Subsume Queue
    {
        int i, j;
        for (i = j = 0; i < subsume_queue.size(); ++i) {
            Riss::Clause& c = ca[subsume_queue[i]];
            if (c.can_be_deleted()) {
                // removeClause(subsume_queue[i]);
            } else {
                ca.reloc(subsume_queue[i], to);
                subsume_queue[j++] = subsume_queue[i];
            }
        }
        subsume_queue.resize(j);
    }
    // Strength Queue
    {
        int i, j;
        for (i = j = 0; i < strengthening_queue.size(); ++i) {
            Riss::Clause& c = ca[strengthening_queue[i]];
            if (c.can_be_deleted()) {
                // removeClause(strength_queue[i]);
            } else {
                ca.reloc(strengthening_queue[i], to);
                strengthening_queue[j++] = strengthening_queue[i];
            }
        }
        strengthening_queue.resize(j);
    }

    // All Occurrences
    for (int v = 0 ; v < nVars(); ++ v) {
        for (int i = 0 ; i < 2; ++i) {
            std::vector<Riss::CRef>& litOccs = list(Riss::mkLit(v, ((i == 0) ? false : true)));
            int j, k;
            for (j = k = 0; j < litOccs.size(); ++j) {
                if (litOccs[j] == Riss::CRef_Undef) {
                    continue;
                }
                Riss::Clause& c = ca[litOccs[j]];
                if (c.can_be_deleted()) {
                    //removeClause(litOccs[j]);
                } else {
                    ca.reloc(litOccs[j], to);
                    litOccs[k++] = litOccs[j];
                }
            }
            litOccs.resize(k);
        }
    }
    // Watches are clean!

    // All reasons:
    //
    for (int i = 0; i < solver->trail.size(); i++) {
        Riss::Var v = var(solver->trail[i]);
        // FIXME TODO: there needs to be a better workaround for this!!
        if (solver->level(v) == 0) { solver->vardata[v].reason = Riss::CRef_Undef; }   // take care of reason clauses for literals at top-level
        else if (
            ! solver->reason(v).isBinaryClause()
            && solver->reason(v).getReasonC() != Riss::CRef_Undef
            && (ca[solver->reason(v).getReasonC() ].reloced() || solver->locked(ca[solver->reason(v).getReasonC() ]))
        ) {
            solver->vardata[v].reason.setReason(ca.relocC(solver->vardata[v].reason.getReasonC() , to));   // update reason
        }
    }

    Riss::vec<Riss::CRef>& clauses = solver->clauses;
    Riss::vec<Riss::CRef>& learnts = solver->learnts;

    // All original:
    //
    {
        int i, j;
        for (i = j = 0; i < clauses.size(); ++i) {
            Riss::Clause& c = ca[clauses[i]];
            if (c.can_be_deleted()) {
                // removeClause(clauses[i]);
            } else {
                ca.reloc(clauses[i], to);
                clauses[j++] = clauses[i];
            }
        }
        clauses.shrink_(i - j);
    }
    // All learnt:
    //
    {
        int i, j;
        for (i = j = 0; i < learnts.size(); ++i) {
            Riss::Clause& c = ca[learnts[i]];
            if (c.can_be_deleted()) {
                // removeClause(learnts[i]);
            } else if (!c.learnt()) {
                ca.reloc(learnts[i], to);
                clauses.push(learnts[i]);
            } else {
                ca.reloc(learnts[i], to);
                learnts[j++] = learnts[i];
            }
        }
        learnts.shrink_(i - j);
    }

    // handle all clause pointers from OTFSS
    int keptClauses = 0;
    for (int i = 0 ; i < solver->otfss.info.size(); ++ i) {
        if (!ca[solver->otfss.info[i].cr].can_be_deleted()) { // keep only relevant clauses (checks mark() != 0 )
            ca.reloc(solver->otfss.info[i].cr, to);
            if (!to[ solver->otfss.info[i].cr ].mark()) {
                solver->otfss.info[keptClauses++] = solver->otfss.info[i]; // keep the clause only if its not marked!
            }
        }
    }
    solver->otfss.info.shrink_(solver->otfss.info.size() - keptClauses);
}

/** Mark all variables that occure together with _x_.
 *
 * @param x the variable to start with
 * @param array the mark array in which the marks are set
 */
inline void CoprocessorData::mark1(Riss::Var x, Riss::MarkArray& array)
{
    std::vector<Riss::CRef>& clauses = occs[Riss::toInt(Riss::mkLit(x, true))];
    for (int i = 0; i < clauses.size(); ++i) {
        Riss::CRef cr = clauses[i];
        Riss::Clause& c = ca[cr];
        for (int j = 0; j < c.size(); ++j) {
            array.setCurrentStep(var(c[j]));
        }
    }
    clauses = occs[Riss::toInt(Riss::mkLit(x, false))];
    for (int i = 0; i < clauses.size(); ++i) {
        Riss::CRef cr = clauses[i];
        Riss::Clause& c = ca[cr];
        for (int j = 0; j < c.size(); ++j) {
            array.setCurrentStep(var(c[j]));
        }
    }
}

/** Marks all variables that occure together with x or with one of x's direct neighbors.
 *
 * mark2 marks all variables in two steps from x. That means, all variables that can be reched
 * in the adjacency graph of variables within two steps.
 *
 * @param x the variable to start from
 * @param array the mark array which contains the marks as result
 * @param tmp an array used for internal compution (temporary)
 */
inline void CoprocessorData::mark2(Riss::Var x, Riss::MarkArray& array, Riss::MarkArray& tmp)
{
    tmp.nextStep();
    // for negative literal
    std::vector<Riss::CRef>& clauses = occs[Riss::toInt(Riss::mkLit(x, true))];
    for (int i = 0; i < clauses.size(); ++i) {
        Riss::Clause& c = ca[clauses[i]];
        // for l in C
        for (int l = 0; l < c.size(); ++l) {
            if (!tmp.isCurrentStep(var(c[l]))) {
                mark1(var(c[l]), array);
            }
            tmp.setCurrentStep(var(c[l]));
        }
    }
    // for positive literal
    clauses = occs[Riss::toInt(Riss::mkLit(x, false))];
    for (int i = 0; i < clauses.size(); ++i) {
        Riss::Clause& c = ca[clauses[i]];
        for (int l = 0; l < c.size(); ++l) {
            if (!tmp.isCurrentStep(var(c[l]))) {
                mark1(var(c[l]), array);
            }
            tmp.setCurrentStep(var(c[l]));
        }
    }
}

template<typename T>
inline void CoprocessorData::addToExtension(const T& lits, const Riss::Lit& l)
{
    // if the last element in the undo stack is lit_Undef, no other literal was
    // add to the stack. That means, there was the attempt to add the empty
    // clause on the stack
    if (undo.size() > 0) {
        assert(undo[undo.size() - 1] != Riss::lit_Undef && "an empty clause should not be put on the undo stack");
    }

    // separator for the different clauses
    undo.push_back(Riss::lit_Undef);

    if (l != Riss::lit_Error) {
        // the undo stack always operates on the orginal formula
        // therefore we have to translate the literals back
        undo.push_back(getCompression().exportLit(l));
    }

    // add all
    for (int i = 0 ; i < lits.size(); ++ i) {
        if (lits[i] != l) {
            undo.push_back(getCompression().exportLit(lits[i]));
        }
    }
}

inline void CoprocessorData::addToExtension(const Riss::Lit& dontTouch, const Riss::Lit& l)
{
    // for comments take a look at the above function
    if (undo.size() > 0) {
        assert(undo[undo.size() - 1] != Riss::lit_Undef && "an empty clause should not be put on the undo stack");
    }

    undo.push_back(Riss::lit_Undef);

    if (l != Riss::lit_Error) {
        undo.push_back(getCompression().exportLit(l));
    }

    undo.push_back(getCompression().exportLit(dontTouch));
}

template<typename T>
inline void CoprocessorData::addExtensionToExtension(const T& lits)
{
    for (int i = 0 ; i < lits.size(); ++ i) {
        undo.push_back(getCompression().exportLit(lits[i]));
    }
}

inline void CoprocessorData::extendModel(Riss::vec< Riss::lbool >& model)
{
    //if (lastCompressUndoLits != -1 &&           // if there has been a  compression,
    //    decompressedUndoLits != undo.size()) {  // then the complete undo-stack has to be adopted
    //    std::cerr << "c variable renaming went wrong - abort. lastCom: " << lastCompressUndoLits
    //              << " decomp: " << decompressedUndoLits
    //              << " undo: " << undo.size() << std::endl;
    //    exit(13);
    //}

    for (int j = 0 ; j < model.size(); ++ j) {
        if (model[j] == l_Undef) { model[j] = l_True; }   // set free variables to some value
    }

    const bool local_debug = false;
    if (true && (global_debug_out || local_debug)) {
        std::cerr << "c extend model of size " << model.size() << " with undo information of size " << undo.size() << std::endl;
        std::cerr << "c in model: ";
        for (int j = 0 ; j < model.size(); ++ j) {
            if (model[j] == l_Undef) { std::cerr << "? "; }
            else {
                const Riss::Lit satLit = Riss::mkLit(j, model[j] == l_True ? false : true);
                std::cerr << satLit << " ";
            }
        }
        std::cerr << std::endl;
    }

    if (false && local_debug) {
        std::cerr << "extend Stack: " << std::endl;
        for (int i = undo.size() - 1; i >= 0 ; --i) {
            if (undo[i] == Riss::lit_Undef) { std::cerr << std::endl; }
            else { std::cerr << " " << undo[i]; }
        }


        std::cerr << "next clause: ";
        for (int j = undo.size() - 1; j >= 0 ; --j) if (undo[j] == Riss::lit_Undef) { break; }  else { std::cerr << " " << undo[j]; }
        std::cerr << std::endl;

    }

    // check current clause for being satisfied
    bool isSat = false; // FIXME: this bool is redundant!
    for (int i = undo.size() - 1; i >= 0 ; --i) {

        isSat = false; // init next clause - redundant!
        const Riss::Lit c = undo[i]; // check current literal
        if (global_debug_out  || local_debug) { std::cerr << "c read literal " << c << std::endl; }
        if (c == Riss::lit_Undef) {   // found clause delimiter, without jumping over it in the SAT case (below)
            if (!isSat) {         // this condition is always satisfied -- the current clause has to be unsatisfied (otherwise, would have been ignored below!)
                // if clause is not satisfied, satisfy last literal!
                const Riss::Lit& satLit = undo[i + 1];
                assert(satLit != Riss::lit_Undef && "there should not be an empty clause on the undo stack");
                log.log(1, "set literal to true", satLit);
                if (local_debug) { std::cerr << "c set literal " << undo[i + 1] << " to true " << std::endl; }
                model[ var(satLit) ] = sign(satLit) ? l_False : l_True;
            }

            // finished this clause!
            if (local_debug) {   // print intermediate state!
                std::cerr << "c current model: ";
                for (int j = 0 ; j < model.size(); ++ j) {
                    if (model[j] == l_Undef) { std::cerr << "? "; }
                    else {
                        const Riss::Lit satLit = Riss::mkLit(j, model[j] == l_True ? false : true);
                        std::cerr << satLit << " ";
                    }
                }
                std::cerr << std::endl;
                std::cerr << "next clause: ";
                for (int j = i - 1; j >= 0 ; --j) if (undo[j] == Riss::lit_Undef) { break; }  else { std::cerr << " " << undo[j]; }
                std::cerr << std::endl;
            }
            continue;
        }
        if (var(c) >= model.size()) { model.growTo(var(c) + 1, l_True); }     // model is too small? this will also take care of extended resolution variables!
        if (model[var(c)] == (sign(c) ? l_False : l_True)) {  // satisfied
            isSat = true; // redundant -- will be reset in the next loop iteration immediately
            while (undo[i] != Riss::lit_Undef) {   // skip literal until hitting the delimiter - for loop will decrease i once more
                if (global_debug_out  || local_debug) { std::cerr << "c skip because SAT: " << undo[i] << std::endl; }
                --i;
            }
            if (local_debug) {   // print intermediate state!
                std::cerr << "next clause: ";
                for (int j = i - 1; j >= 0 ; --j) if (undo[j] == Riss::lit_Undef) { break; }  else { std::cerr << " " << undo[j]; }
                std::cerr << std::endl;
            }
        }
    }

    if (global_debug_out  || local_debug) {
        std::cerr << "c out model: ";
        for (int i = 0 ; i < model.size(); ++ i) {
            const Riss::Lit satLit = Riss::mkLit(i, model[i] == l_True ? false : true);
            std::cerr << satLit << " ";
        }
        std::cerr << std::endl;
    }
}

#ifdef DRATPROOF
template <class T>
inline void CoprocessorData::addToProof(const T& clause, bool deleteFromProof, const Riss::Lit& remLit)
{
    solver->addToProof(clause, deleteFromProof, remLit);
}

inline void CoprocessorData::addUnitToProof(const Riss::Lit& l, bool deleteFromProof)
{
    solver->addUnitToProof(l, deleteFromProof);
}

inline void CoprocessorData::addCommentToProof(const char* text, bool deleteFromProof)
{
    solver->addCommentToProof(text, deleteFromProof);
}
#endif

inline void CoprocessorData::addEquivalences(const std::vector< Riss::Lit >& list)
{
    assert((list.size() != 2 || list[0] != list[1]) && "do not allow to add a std::pair of the same literals");
    solver->eqInfo.addEquivalenceClass(list); // will also share the SCC
}

inline void CoprocessorData::addEquivalences(const Riss::Lit& l1, const Riss::Lit& l2)
{
    assert(l1 != l2 && "do not state that the same literal is equivalent to itself");
    if (global_debug_out) { std::cerr << "c [DATA] set equi: " << l1 << " == " << l2 << std::endl; }
    solver->eqInfo.addEquivalenceClass(l1, l2);
}

inline Riss::vec< Riss::Lit >& CoprocessorData::getEquivalences()
{
    return solver->eqInfo.getEquivalenceStack();
}

inline bool CoprocessorData::addSubStrengthClause(const Riss::CRef& cr, const bool& isNew)
{
    bool ret = false;
    Riss::Clause& c = ca[cr];
    if (!c.can_strengthen() || isNew) {
        c.set_strengthen(true);
        strengthening_queue.push_back(cr);
        ret = true;
    }
    if (!c.can_subsume() || isNew) {
        c.set_subsume(true);
        subsume_queue.push_back(cr);
        ret = true;
    }
    return ret;
}

inline std::vector< Riss::CRef >& CoprocessorData::getSubsumeClauses()
{
    return subsume_queue;
}


inline std::vector<Riss::CRef>& CoprocessorData::getStrengthClauses()
{
    return strengthening_queue;
}


inline void CoprocessorData::setNotTouch(const Riss::Var& v)
{
    solver->freezeVariable(v, true);
}

inline void CoprocessorData::unsetNotTouch(const Riss::Var& v)
{
    solver->freezeVariable(v, false);
}

inline bool CoprocessorData::doNotTouch(const Riss::Var& v) const
{
    return solver->isFrozen(v);
}

bool inline CoprocessorData::removeClauseThreadSafe(const Riss::CRef& cr)
{
    Riss::Clause& c = ca[cr];
    c.spinlock();
    if (!c.can_be_deleted()) {
        c.set_delete(true);
        while (__sync_bool_compare_and_swap(&numberOfCls, numberOfCls, numberOfCls - 1) == false) {};
        for (int l = 0; l < c.size(); ++l) {
            int32_t old_count, new_count;
            Riss::Lit lit = c[l];
            do {
                old_count = lit_occurrence_count[Riss::toInt(lit)];
                new_count = old_count - 1;
            } while (__sync_bool_compare_and_swap(&lit_occurrence_count[Riss::toInt(lit)], old_count, new_count) == false);
        }
        c.unlock();
        return true;
    } else {
        c.unlock();
        return false;
    }
}

inline BIG::BIG()
    : storage(0), sizes(0), big(0), start(0), stop(0), duringCreationVariables(0)
{}

inline BIG::~BIG()
{
    if (big != 0)    { free(big); big = 0; }
    if (storage != 0) { free(storage); storage = 0; }
    if (sizes != 0)  { free(sizes); sizes = 0 ; }
    if (start != 0) { free(start); start = 0; }
    if (stop != 0) { free(stop); stop = 0; }

}

inline void BIG::create(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list)
{
    duringCreationVariables = nVars; // memorize the number of present variables
    sizes = (int*) malloc(sizeof(int) * nVars * 2);
    memset(sizes, 0, sizeof(int) * nVars * 2);

    int sum = 0;
    // count occurrences of literals in binary clauses of the given list
    for (int i = 0 ; i < list.size(); ++i) {
        const Riss::Clause& c = ca[list[i]];
        if (c.size() != 2 || c.can_be_deleted()) { continue; }
        sizes[ Riss::toInt(~c[0])  ] ++;
        sizes[ Riss::toInt(~c[1])  ] ++;
        sum += 2;
    }
    storage = (Riss::Lit*) malloc(sizeof(Riss::Lit) * sum);
    big = (Riss::Lit**)malloc(sizeof(Riss::Lit*) * nVars * 2);
    // memset(sizes,0, sizeof(Riss::Lit*) * nVars * 2 );
    // set the pointers to the right location and clear the size
    sum = 0 ;
    for (int i = 0 ; i < nVars * 2; ++ i) {
        big[i] = &(storage[sum]);
        sum += sizes[i];
        sizes[i] = 0;
    }

    // add all binary clauses to graph
    for (int i = 0 ; i < list.size(); ++i) {
        const Riss::Clause& c = ca[list[i]];
        if (c.size() != 2 || c.can_be_deleted()) { continue; }
        const Riss::Lit l0 = c[0]; const Riss::Lit l1 = c[1];

        (big[ Riss::toInt(~l0) ])[ sizes[Riss::toInt(~l0)] ] = l1;
        (big[ Riss::toInt(~l1) ])[ sizes[Riss::toInt(~l1)] ] = l0;
        sizes[Riss::toInt(~l0)] ++;
        sizes[Riss::toInt(~l1)] ++;
    }
}

inline void BIG::create(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list1, Riss::vec< Riss::CRef >& list2)
{
    duringCreationVariables = nVars; // memorize the number of present variables
    sizes = (int*) malloc(sizeof(int) * nVars * 2);
    memset(sizes, 0, sizeof(int) * nVars * 2);

    int sum = 0;
    // count occurrences of literals in binary clauses of the given list
    for (int p = 0 ; p < 2; ++ p) {
        const Riss::vec<Riss::CRef>& list = (p == 0 ? list1 : list2);
        for (int i = 0 ; i < list.size(); ++i) {
            const Riss::Clause& c = ca[list[i]];
            if (c.size() != 2 || c.can_be_deleted()) { continue; }
            sizes[ Riss::toInt(~c[0])  ] ++;
            sizes[ Riss::toInt(~c[1])  ] ++;
            sum += 2;
        }
    }
    storage = (Riss::Lit*) malloc(sizeof(Riss::Lit) * sum);
    big = (Riss::Lit**)malloc(sizeof(Riss::Lit*) * nVars * 2);
    // memset(sizes,0, sizeof(Riss::Lit*) * nVars * 2 );
    // set the pointers to the right location and clear the size
    sum = 0 ;
    for (int i = 0 ; i < nVars * 2; ++ i) {
        big[i] = &(storage[sum]);
        sum += sizes[i];
        sizes[i] = 0;
    }

    // add all binary clauses to graph
    for (int p = 0 ; p < 2; ++ p) {
        const Riss::vec<Riss::CRef>& list = (p == 0 ? list1 : list2);
        for (int i = 0 ; i < list.size(); ++i) {
            const Riss::Clause& c = ca[list[i]];
            if (c.size() != 2 || c.can_be_deleted()) { continue; }
            const Riss::Lit l0 = c[0]; const Riss::Lit l1 = c[1];
            (big[ Riss::toInt(~l0) ])[ sizes[Riss::toInt(~l0)] ] = l1;
            (big[ Riss::toInt(~l1) ])[ sizes[Riss::toInt(~l1)] ] = l0;
            sizes[Riss::toInt(~l0)] ++;
            sizes[Riss::toInt(~l1)] ++;
        }
    }
}


inline void BIG::recreate(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list)
{
    duringCreationVariables = nVars; // memorize the number of present variables
    sizes = sizes == 0 ? (int*) malloc(sizeof(int) * nVars * 2) : (int*) realloc(sizes, sizeof(int) * nVars * 2);
    memset(sizes, 0, sizeof(int) * nVars * 2);

    int sum = 0;
    // count occurrences of literals in binary clauses of the given list
    for (int i = 0 ; i < list.size(); ++i) {
        const Riss::Clause& c = ca[list[i]];
        if (c.size() != 2 || c.can_be_deleted()) { continue; }
        sizes[ Riss::toInt(~c[0])  ] ++;
        sizes[ Riss::toInt(~c[1])  ] ++;
        sum += 2;
    }
    storage = storage == 0 ? (Riss::Lit*) malloc(sizeof(Riss::Lit) * sum) : (Riss::Lit*) realloc(storage, sizeof(Riss::Lit) * sum)  ;
    big = big == 0 ? (Riss::Lit**)malloc(sizeof(Riss::Lit*) * nVars * 2) : (Riss::Lit**)realloc(big, sizeof(Riss::Lit*) * nVars * 2);
    // should not be necessary!
    memset(storage, 0, sizeof(Riss::Lit) * sum);
    memset(big, 0, sizeof(Riss::Lit*) * nVars * 2);

    // set the pointers to the right location and clear the size
    sum = 0 ;
    for (int i = 0 ; i < nVars * 2; ++ i) {
        big[i] = &(storage[sum]);
        sum += sizes[i];
        sizes[i] = 0;
    }

    // add all binary clauses to graph
    for (int i = 0 ; i < list.size(); ++i) {
        const Riss::Clause& c = ca[list[i]];
        if (c.size() != 2 || c.can_be_deleted()) { continue; }
        const Riss::Lit l0 = c[0]; const Riss::Lit l1 = c[1];
        (big[ Riss::toInt(~l0) ])[ sizes[Riss::toInt(~l0)] ] = l1;
        (big[ Riss::toInt(~l1) ])[ sizes[Riss::toInt(~l1)] ] = l0;
        sizes[Riss::toInt(~l0)] ++;
        sizes[Riss::toInt(~l1)] ++;
    }
}

inline void BIG::recreate(Riss::ClauseAllocator& ca, uint32_t nVars, Riss::vec< Riss::CRef >& list1, Riss::vec< Riss::CRef >& list2)
{
    duringCreationVariables = nVars; // memorize the number of present variables
    sizes = sizes == 0 ? (int*) malloc(sizeof(int) * nVars * 2) : (int*) realloc(sizes, sizeof(int) * nVars * 2);
    memset(sizes, 0, sizeof(int) * nVars * 2);

    int sum = 0;
    // count occurrences of literals in binary clauses of the given list
    for (int p = 0 ; p < 2; ++ p) {
        const Riss::vec<Riss::CRef>& list = (p == 0 ? list1 : list2);
        for (int i = 0 ; i < list.size(); ++i) {
            const Riss::Clause& c = ca[list[i]];
            if (c.size() != 2 || c.can_be_deleted()) { continue; }
            sizes[ Riss::toInt(~c[0])  ] ++;
            sizes[ Riss::toInt(~c[1])  ] ++;
            sum += 2;
        }
    }
    storage = storage == 0 ? (Riss::Lit*) malloc(sizeof(Riss::Lit) * sum) : (Riss::Lit*) realloc(storage, sizeof(Riss::Lit) * sum)  ;
    big = big == 0 ? (Riss::Lit**)malloc(sizeof(Riss::Lit*) * nVars * 2) : (Riss::Lit**)realloc(big, sizeof(Riss::Lit*) * nVars * 2);
    // should not be necessary!
    memset(storage, 0, sizeof(Riss::Lit) * sum);
    memset(big, 0, sizeof(Riss::Lit*) * nVars * 2);

    // set the pointers to the right location and clear the size
    sum = 0 ;
    for (int i = 0 ; i < nVars * 2; ++ i) {
        big[i] = &(storage[sum]);
        sum += sizes[i];
        sizes[i] = 0;
    }

    // add all binary clauses to graph
    for (int p = 0 ; p < 2; ++ p) {
        const Riss::vec<Riss::CRef>& list = (p == 0 ? list1 : list2);
        for (int i = 0 ; i < list.size(); ++i) {
            const Riss::Clause& c = ca[list[i]];
            if (c.size() != 2 || c.can_be_deleted()) { continue; }
            const Riss::Lit l0 = c[0]; const Riss::Lit l1 = c[1];
            (big[ Riss::toInt(~l0) ])[ sizes[Riss::toInt(~l0)] ] = l1;
            (big[ Riss::toInt(~l1) ])[ sizes[Riss::toInt(~l1)] ] = l0;
            sizes[Riss::toInt(~l0)] ++;
            sizes[Riss::toInt(~l1)] ++;
        }
    }
}

inline void BIG::removeDuplicateEdges(const uint32_t nVars)
{
    const uint32_t maxVar = duringCreationVariables < nVars ? duringCreationVariables : nVars; // use only known variables
    for (Riss::Var v = 0 ; v < maxVar; ++v) {
        for (int p = 0 ; p < 2 ; ++ p) {
            const Riss::Lit l = Riss::mkLit(v, p == 1);
            if (getSize(l) == 0) { continue; }   // not for empty lists!
            Riss::sort(getArray(l), getSize(l));
            int j = 0;
            for (int i = 1; i < getSize(l); ++i) {
                assert(getArray(l)[i - 1] <= getArray(l)[i] && "implication list should be ordered");
                if (getArray(l)[i] != getArray(l)[j]) { getArray(l)[++j] = getArray(l)[i]; }   // keep elements, if they are not equal to the last element!
            }
            sizes[ Riss::toInt(l) ] = j + 1; // update size information
        }
    }
}

inline void BIG::sort(const uint32_t nVars)
{
    const uint32_t maxVar = duringCreationVariables < nVars ? duringCreationVariables : nVars; // use only known variables
    for (Riss::Var v = 0 ; v < maxVar; ++v) {
        for (int p = 0 ; p < 2 ; ++ p) {
            const Riss::Lit l = Riss::mkLit(v, p == 1);
            if (getSize(l) == 0) { continue; }   // not for empty lists!
            Riss::sort(getArray(l), getSize(l));
        }
    }
}

inline void BIG::removeEdge(const Riss::Lit& l0, const Riss::Lit& l1)
{
    // remove literal from the two lists
    Riss::Lit* list = getArray(~l0);
    const uint32_t size = getSize(~l0);
    for (int i = 0 ; i < size; ++i) {
        if (list[i] == l1) {
            list[i] = list[ size - 1 ];
            sizes[ Riss::toInt(~l0) ] --;
            //std::cerr << "c removed edge " << ~l0 << " -> " << l1 << std::endl;
            break;
        }
    }
    Riss::Lit* list2 = getArray(~l1);
    const uint32_t size2 = getSize(~l1);
    for (int i = 0 ; i < size2; ++i) {
        if (list2[i] == l0) {
            list2[i] = list2[ size2 - 1 ];
            sizes[ Riss::toInt(~l1) ] --;
//        //std::cerr << "c removed edge " << ~l1 << " -> " << l0 << std::endl;
            break;
        }
    }
}

inline Riss::Lit* BIG::getArray(const Riss::Lit l)
{
    return var(l) < duringCreationVariables ? big[ Riss::toInt(l) ] : 0;
}

inline const Riss::Lit* BIG::getArray(const Riss::Lit& l) const
{
    return var(l) < duringCreationVariables ? big[ Riss::toInt(l) ] : 0;
}

inline int BIG::getSize(const Riss::Lit& l) const
{
    return var(l) < duringCreationVariables ? sizes[ Riss::toInt(l) ] : 0;
}

inline void BIG::generateImplied(CoprocessorData& data)
{
    uint32_t stamp = 1 ;
    const uint32_t maxVar = duringCreationVariables < data.nVars() ? duringCreationVariables : data.nVars(); // use only known variables

    if (maxVar == 0) { return; }

    if (start == 0) { start = (uint32_t*) malloc(maxVar * sizeof(uint32_t) * 2); }
    else {
        uint32_t* oldPtr = start;
        start = (uint32_t*)realloc(start, maxVar * sizeof(uint32_t) * 2);
        if (start == 0) { if (oldPtr != 0) { free(oldPtr); } }
    }

    if (stop == 0) { stop = (uint32_t*) malloc(maxVar * sizeof(uint32_t) * 2); }
    else {
        uint32_t* oldPtr = stop;
        stop = (uint32_t*)realloc(stop, maxVar * sizeof(int32_t) * 2);
        if (stop == 0) { if (oldPtr != 0) { free(oldPtr); } }
    }

    int32_t* index = (int32_t*)malloc(maxVar * sizeof(int32_t) * 2);

    // set everything to 0!
    memset(start, 0, maxVar * sizeof(uint32_t) * 2);
    memset(stop, 0, maxVar * sizeof(uint32_t) * 2);
    memset(index, 0, maxVar * sizeof(int32_t) * 2);


    std::deque< Riss::Lit > stampQueue;

    data.lits.clear();
    // reset all present variables, collect all roots in binary implication graph
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        const Riss::Lit pos =  Riss::mkLit(v, false);
        // a literal is a root, if its complement does not imply a literal
        if (getSize(pos) == 0) { data.lits.push_back(~pos); }
        if (getSize(~pos) == 0) { data.lits.push_back(pos); }
    }

    // do stamping for all roots, shuffle first
    const uint32_t ts = data.lits.size();
    for (uint32_t i = 0 ; i < ts; i++) { const uint32_t rnd = rand() % ts; const Riss::Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd] = tmp; }
    // stamp shuffled data.lits
    for (uint32_t i = 0 ; i < ts; ++ i) {
        stamp = stampLiteral(data.lits[i], stamp, index, stampQueue);
    }

    // stamp all remaining literals, after shuffling
    data.lits.clear();
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        const Riss::Lit pos =  Riss::mkLit(v, false);
        if (start[ Riss::toInt(pos) ] == 0) { data.lits.push_back(pos); }
        if (start[ Riss::toInt(~pos) ] == 0) { data.lits.push_back(~pos); }
    }
    // stamp shuffled data.lits
    const uint32_t ts2 = data.lits.size();
    for (uint32_t i = 0 ; i < ts2; i++) { const uint32_t rnd = rand() % ts2; const Riss::Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd] = tmp; }
    for (uint32_t i = 0 ; i < ts2; ++ i) {
        stamp = stampLiteral(data.lits[i], stamp, index, stampQueue);
    }
    free(index);
}

inline void BIG::generateImplied(uint32_t nVars, Riss::vec<Riss::Lit>& tmpLits)
{
    uint32_t stamp = 1 ;
    const uint32_t maxVar = duringCreationVariables < nVars ? duringCreationVariables : nVars; // use only known variables
    if (maxVar == 0) { return; }

    if (start == 0) { start = (uint32_t*) malloc(maxVar * sizeof(uint32_t) * 2); }
    else { uint32_t* oldPtr = start; start = (uint32_t*)realloc(start, maxVar * sizeof(uint32_t) * 2); if (start == 0) { free(oldPtr); exit(-1); } }

    if (stop == 0) { stop = (uint32_t*) malloc(maxVar * sizeof(uint32_t) * 2); }
    else { uint32_t* oldPtr = stop; stop = (uint32_t*)realloc(stop, maxVar * sizeof(int32_t) * 2); if (stop == 0) { free(oldPtr); exit(-1); } }

    int32_t* index = (int32_t*)malloc(maxVar * sizeof(int32_t) * 2);

    // set everything to 0!
    memset(start, 0, maxVar * sizeof(uint32_t) * 2);
    memset(stop, 0, maxVar * sizeof(uint32_t) * 2);
    memset(index, 0, maxVar * sizeof(int32_t) * 2);


    std::deque< Riss::Lit > stampQueue;

    tmpLits.clear();
    // reset all present variables, collect all roots in binary implication graph
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        const Riss::Lit pos =  Riss::mkLit(v, false);
        // a literal is a root, if its complement does not imply a literal
        if (getSize(pos) == 0) { tmpLits.push(~pos); }
        if (getSize(~pos) == 0) { tmpLits.push(pos); }
    }

    // do stamping for all roots, shuffle first
    const uint32_t ts = tmpLits.size();
    for (uint32_t i = 0 ; i < ts; i++) { const uint32_t rnd = rand() % ts; const Riss::Lit tmp = tmpLits[i]; tmpLits[i] = tmpLits[rnd]; tmpLits[rnd] = tmp; }
    // stamp shuffled tmpLits
    for (uint32_t i = 0 ; i < ts; ++ i) {
        stamp = stampLiteral(tmpLits[i], stamp, index, stampQueue);
    }

    // stamp all remaining literals, after shuffling
    tmpLits.clear();
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        const Riss::Lit pos =  Riss::mkLit(v, false);
        if (start[ Riss::toInt(pos) ] == 0) { tmpLits.push(pos); }
        if (start[ Riss::toInt(~pos) ] == 0) { tmpLits.push(~pos); }
    }
    // stamp shuffled tmpLits
    const uint32_t ts2 = tmpLits.size();
    for (uint32_t i = 0 ; i < ts2; i++) { const uint32_t rnd = rand() % ts2; const Riss::Lit tmp = tmpLits[i]; tmpLits[i] = tmpLits[rnd]; tmpLits[rnd] = tmp; }
    for (uint32_t i = 0 ; i < ts2; ++ i) {
        stamp = stampLiteral(tmpLits[i], stamp, index, stampQueue);
    }

    tmpLits.clear(); // clean up
    free(index);
}

inline void BIG::fillSorted(std::vector<Riss::Lit>& literals, CoprocessorData& data, bool rootsOnly, bool getAll)
{
    literals.clear();
    const uint32_t maxVar = duringCreationVariables < data.nVars() ? duringCreationVariables : data.nVars(); // use only known variables
    data.ma.resize(maxVar * 2);
    data.ma.nextStep();

    // put root nodes in queue
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        if (getSize(Riss::mkLit(v, false)) == 0)
            if (getSize(Riss::mkLit(v, true)) == 0) { continue; }
            else {
                data.ma.setCurrentStep(Riss::toInt(Riss::mkLit(v, true)));
                literals.push_back(Riss::mkLit(v, true));   // tthis is a root node
            }
        else if (getSize(Riss::mkLit(v, true)) == 0) {
            data.ma.setCurrentStep(Riss::toInt(Riss::mkLit(v, false)));
            literals.push_back(Riss::mkLit(v, false));   // tthis is a root node
        }
    }

    // shuffle root nodes
    for (int i = 0 ; i + 1 < literals.size(); ++ i) {
        const Riss::Lit tmp = literals[i];
        const int rndInd = rand() % literals.size();
        literals[i] = literals[ rndInd ];
        literals[ rndInd ] = tmp;
    }

    if (rootsOnly) { return; }

    // perform BFS
    data.ma.nextStep();
    for (int i = 0 ; i < literals.size(); ++ i) {
        const Riss::Lit l = literals[i];
        Riss::Lit* lits = getArray(l);
        int s = getSize(l);
        for (int j = 0 ; j < s; ++ j) {
            const Riss::Lit l2 = lits[j];
            // each literal only once!
            if (data.ma.isCurrentStep(Riss::toInt(l2))) { continue; }
            data.ma.setCurrentStep(Riss::toInt(l2));
            literals.push_back(l2);
        }
    }

    if (!getAll) { return; }

    unsigned seenSoFar = literals.size();
    for (Riss::Var v = 0 ; v < maxVar; ++ v) {
        for (int p = 0 ; p < 2; ++ p) {
            const Riss::Lit l = Riss::mkLit(v, p == 1);
            if (data.ma.isCurrentStep(Riss::toInt(l))) { continue; }   // literal already in heap
            else { literals.push_back(l); }
        }
    }
    // shuffle these variables!
    const unsigned diff = literals.size() - seenSoFar;
    for (int i =  seenSoFar; i < literals.size(); ++ i) {
        const Riss::Lit tmp = literals[i];
        const int rndInd = (rand() % diff) + seenSoFar;
        literals[i] = literals[ rndInd ];
        literals[ rndInd ] = tmp;
    }
}

inline void BIG::fillSorted(std::vector< Riss::Var >& variables, Coprocessor::CoprocessorData& data, bool rootsOnly, bool getAll)
{
    // get sorted list of lits
    data.lits.clear();
    fillSorted(data.lits, data, rootsOnly, getAll);
    variables.clear();

    // store variables in std::vector, according to occurrence of first literal in literal std::vector
    data.ma.nextStep();
    for (int i = 0 ; i < data.lits.size(); ++ i) {
        const Riss::Lit l = data.lits[i];
        if (!data.ma.isCurrentStep(var(l))) {
            variables.push_back(var(l));
            data.ma.setCurrentStep(var(l));
        }
    }
}

inline void BIG::shuffle(Riss::Lit* adj, int size) const
{
    for (uint32_t i = 0 ;  i + 1 < size; ++i) {
        const uint32_t rnd = rand() % size;
        const Riss::Lit tmp = adj[i];
        adj[i] = adj[rnd];
        adj[rnd] = tmp;
    }
}

inline uint32_t BIG::stampLiteral(const Riss::Lit& literal, uint32_t stamp, int32_t* index, std::deque<Riss::Lit>& stampQueue)
{
    // do not stamp a literal twice!
    if (start[ Riss::toInt(literal) ] != 0) { return stamp; }

    if (global_debug_out) { std::cerr << "c call STAMP for " << literal << std::endl; }

    stampQueue.clear();
    // linearized algorithm from paper
    stamp++;
    // handle initial literal before putting it on queue
    assert(Riss::var(literal) < duringCreationVariables && "write only into valid range");
    start[Riss::toInt(literal)] = stamp; // parent and root are already set to literal
    if (global_debug_out) { std::cerr << "c start[" << literal << "] = " << stamp << std::endl; }
    stampQueue.push_back(literal);

    shuffle(getArray(literal), getSize(literal));
    index[Riss::toInt(literal)] = 0;

    while (! stampQueue.empty()) {
        const Riss::Lit current = stampQueue.back();
        const int adjSize = getSize(current);

        if (index[Riss::toInt(current)] == adjSize) {
            stampQueue.pop_back();
            stamp++;
            stop[Riss::toInt(current)] = stamp;
            if (global_debug_out) { std::cerr << "c stop[" << current << "] = " << stamp << std::endl; }
        } else {
            int32_t& ind = index[ Riss::toInt(current) ]; // store number of processed elements
            const Riss::Lit impliedLit = getArray(current)[ind];   // get next implied literal
            ind ++;
            if (start[ Riss::toInt(impliedLit) ] != 0) { continue; }
            stamp ++;
            assert(Riss::var(impliedLit) < duringCreationVariables && "write only into valid range");
            start[ Riss::toInt(impliedLit) ] = stamp;
            if (global_debug_out) { std::cerr << "c start[" << impliedLit << "] = " << stamp << std::endl; }
            index[ Riss::toInt(impliedLit) ] = 0;
            stampQueue.push_back(impliedLit);
            shuffle(getArray(impliedLit), getSize(impliedLit));
        }

    }
    return stamp;
}

inline bool BIG::implies(const Riss::Lit& from, const Riss::Lit& to) const
{
    if (start == 0 || stop == 0 || var(from) >= duringCreationVariables || var(to) >= duringCreationVariables) { return false; }
    return (start[ Riss::toInt(from) ] < start[ Riss::toInt(to) ] && stop[ Riss::toInt(from) ] > stop[ Riss::toInt(to) ])
           || (start[ Riss::toInt(~to) ] < start[ Riss::toInt(~from) ] && stop[ Riss::toInt(~to) ] > stop[ Riss::toInt(~from) ]);
}

inline bool BIG::isChild(const Riss::Lit& parent, const Riss::Lit& child) const
{
    const Riss::Lit * list = getArray(parent);
    const int listSize = getSize(parent);
    for (int j = 0 ; j < listSize; ++ j) {
        if (list[j] == child) {
            return true;
        }
    }
    return false;
}

inline bool BIG::isOneChild(const Riss::Lit& parent, const Riss::Lit& child1, const Riss::Lit& child2) const
{
    const Riss::Lit * list = getArray(parent);
    const int listSize = getSize(parent);
    for (int j = 0 ; j < listSize; ++ j) {
        if (list[j] == child1 || list[j] == child2) { return true; }
    }
    return false;
}


inline Logger::Logger(int level, bool err)
    : outputLevel(level), useStdErr(err)
{}

inline void Logger::log(int level, const std::string& s)
{
    if (level > outputLevel) { return; }
    (useStdErr ? std::cerr : std::cout)
            << "c [" << level << "] " << s << std::endl;
}

inline void Logger::log(int level, const std::string& s, const Riss::Clause& c)
{
    if (level > outputLevel) { return; }
    (useStdErr ? std::cerr : std::cout)
            << "c [" << level << "] " << s << " : " ;
    for (int i = 0 ; i < c.size(); ++i) {
        const Riss::Lit& l = c[i];
        (useStdErr ? std::cerr : std::cout)
                << " " << (sign(l) ? "-" : "") << var(l) + 1;
    }
    (useStdErr ? std::cerr : std::cout)
            << std::endl;
}

inline void Logger::log(int level, const std::string& s, const Riss::Lit& l)
{
    if (level > outputLevel) { return; }
    (useStdErr ? std::cerr : std::cout)
            << "c [" << level << "] " << s << " : "
            << (sign(l) ? "-" : "") << var(l) + 1
            << std::endl;
}

inline void Logger::log(int level, const std::string& s, const int i)
{
    if (level > outputLevel) { return; }
    (useStdErr ? std::cerr : std::cout)
            << "c [" << level << "] " << s << " " << i << std::endl;
}


inline void Logger::log(int level, const std::string& s, const Riss::Clause& c, const Riss::Lit& l)
{
    if (level > outputLevel) { return; }
    (useStdErr ? std::cerr : std::cout)
            << "c [" << level << "] " << s << " : "
            << (sign(l) ? "-" : "") << var(l) + 1 << " with clause ";
    for (int i = 0 ; i < c.size(); ++i) {
        const Riss::Lit& l = c[i];
        (useStdErr ? std::cerr : std::cout)
                << " " << (sign(l) ? "-" : "") << var(l) + 1;
    }
    (useStdErr ? std::cerr : std::cout)
            << std::endl;
}

}

#endif
