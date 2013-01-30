/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTYPES_HH
#define COPROCESSORTYPES_HH

#include "core/Solver.h"

#include "utils/System.h"

#include "coprocessor-src/LockCollection.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

  /// temporary Boolean flag to quickly enable debug output for the whole file
  const bool global_debug_out = false;
  
  //forward declaration
  class VarGraphUtils;

/// print literals into a stream
inline ostream& operator<<(ostream& other, const Lit& l ) {
  other << (sign(l) ? "-" : "") << var(l) + 1;
  return other;
}

/// print a clause into a stream
inline ostream& operator<<(ostream& other, const Clause& c ) {
  other << "[";
  for( int i = 0 ; i < c.size(); ++ i )
    other << " " << c[i];
  other << "]";
  return other;
}

typedef std::vector<std::vector <CRef> > ComplOcc;

/** this object frees a pointer before a method /statementblock is left */
class MethodFree {
  void* pointer;
public:
  MethodFree( void* ptr ) : pointer( ptr ) {}
  ~MethodFree() { free( pointer ); }
};

/** class that measures the time between creation and destruction of the object, and adds it*/
class MethodTimer {
  double* pointer;
public:
  MethodTimer( double* timer ) : pointer( timer ) { *pointer = cpuTime() - *pointer;}
  ~MethodTimer() { *pointer = cpuTime() - *pointer; }
};  


/** class that is used as mark array */
class MarkArray {
private:
	uint32_t* array;
	uint32_t step;
	uint32_t mySize;

public:
	MarkArray ():
	 array(0),
	 step(0),
	 mySize(0)
	 {}

	~MarkArray ()
	{
	  destroy();
	}

	void destroy() {
	  if( array != 0 ) free( array );
	  mySize = 0; step = 0; array = 0;
	}

	void create(const uint32_t newSize){
	  assert( array == 0 );
	  array = (uint32_t * ) malloc( sizeof( uint32_t) * newSize );
	  memset( array, 0 , sizeof( uint32_t) * newSize );
	  mySize = newSize;
	}

	void resize(const uint32_t newSize) {
	  if( newSize > mySize ) {
	    array = (uint32_t * ) realloc( array, sizeof( uint32_t) * newSize );
	    memset( &(array[mySize]), 0 , sizeof( uint32_t) * (newSize - mySize) );
	    mySize = newSize;
	  }
	}

	/** reset the mark of a single item
	 */
	void reset( const uint32_t index ) {
	  array[index] = 0;
	}

	/** reset the marks of the whole array
	 */
	void reset() {
	  memset( array, 0 , sizeof( uint32_t) * mySize );
	  step = 0;
	}

	/** give number of next step. if value becomes critical, array will be reset
	 */
	uint32_t nextStep() {
	  if( step >= 1 << 30 ) reset();
	  return ++step;
	}

	/** returns whether an item has been marked at the current step
	 */
	bool isCurrentStep( const uint32_t index ) const {
	  return array[index] == step;
	}

	/** set an item to the current step value
	 */
	void setCurrentStep( const uint32_t index ) {
	  array[index] = step;
	}

	/** check whether a given index has the wanted index */ 
	bool hasSameIndex( const uint32_t index, const uint32_t comparestep ) const { //TODO name is confusing hasSameStep ??
	  return array[index] == comparestep;
	}

	uint32_t size() const {
	  return mySize;
	}

	const uint32_t getIndex(uint32_t index) const { return array[index]; }

};

/** class responsible for debug output */
class Logger
{
  int outputLevel; // level to output
  bool useStdErr;  // print to stderr, or to stdout?
public:
  Logger(int level, bool err = true);

  void log( int level, const string& s );
  void log( int level, const string& s, const int i);
  void log( int level, const string& s, const Clause& c);
  void log( int level, const string& s, const Lit& l);
  void log( int level, const string& s, const Clause& c, const Lit& l);
};

/** Data, that needs to be accessed by coprocessor and all the other classes
 */
class CoprocessorData
{
  // friend for VarGraph
  friend class Coprocessor::VarGraphUtils;

  ClauseAllocator& ca;
  Solver* solver;
  /* TODO to add here
   * occ list
   * counters
   * methods to update these structures
   * no statistical counters for each method, should be provided by each method!
   */
  uint32_t numberOfVars;                // number of variables
  uint32_t numberOfCls;                 // number of clauses
  ComplOcc occs;                        // list of clauses, per literal
  vector<int32_t> lit_occurrence_count; // number of literal occurrences in the formula

  bool hasLimit;                        // indicate whether techniques should be executed limited
  bool randomOrder;                     // perform preprocessing with random execution order within techniques

  Lock dataLock;                        // lock for parallel algorithms to synchronize access to data structures

  MarkArray deleteTimer;                // store for each literal when (by which technique) it has been deleted
  vector<char> untouchable;             // store all variables that should not be touched (altering presence in models)

  vector<Lit> undo;                     // store clauses that have to be undone for extending the model
  vector<Lit> equivalences;             // stack of literal classes that represent equivalent literals

  // TODO decide whether a vector of active variables would be good!

public:

  Logger& log;                           // responsible for logs

  MarkArray ma;                          // temporary markarray, that should be used only inside of methods
  vector<Lit> lits;                      // temporary literal vector
  vector<CRef> clss;                     // temporary literal vector

  CoprocessorData(ClauseAllocator& _ca, Solver* _solver, Logger& _log, bool _limited = true, bool _randomized = false);

  // init all data structures for being used for nVars variables
  void init( uint32_t nVars );

  // free all the resources that are used by this data object,
  void destroy();

  int32_t& operator[] (const Lit l ); // return the number of occurrences of literal l
  int32_t operator[] (const Var v ) const; // return the number of occurrences of variable v
  vector<CRef>& list( const Lit l ); // return the list of clauses, which have literal l
  const vector< Minisat::CRef >& list( const Lit l ) const; // return the list of clauses, which have literal l

  vec<CRef>& getClauses();           // return the vector of clauses in the solver object
  vec<CRef>& getLEarnts();           // return the vector of learnt clauses in the solver object

  uint32_t nCls()  const { return numberOfCls; }
  uint32_t nVars() const { return numberOfVars; }
  Var nextFreshVariable();

// semantic:
  bool ok();                                             // return ok-state of solver
  void setFailed();                                      // found UNSAT, set ok state to false
  lbool enqueue( const Lit l );                          // enqueue literal l to current solver structures
  lbool value( const Lit l ) const ;                     // return the assignment of a literal
  
  Solver* getSolver();                                   // return the pointer to the solver object
  bool hasToPropagate();                                 // signal whether there are new unprocessed units
  
  bool unlimited();                                      // do preprocessing without technique limits?
  bool randomized();                                     // use a random order for preprocessing techniques
  bool isInterupted();					  // has received signal from the outside

// adding, removing clauses and literals =======
  void addClause (      const CRef cr );                 // add clause to data structures, update counters
  bool removeClauseFrom (const Minisat::CRef cr, const Lit l); // remove clause reference from list of clauses for literal l, returns true, if successful
  void removeClauseFrom (const Minisat::CRef cr, const Lit l, const int index); // remove clause reference from list of clauses for literal l, returns true, if successful

  void updateClauseAfterDelLit(const Minisat::Clause& clause)
  { if( global_debug_out ) cerr << "what to update in clause?!" << endl; }
  
// delete timers
  /** gives back the current times, increases for the next technique */
  uint32_t getMyDeleteTimer();
  /** tell timer system that variable has been deleted (thread safe!) */
  void deletedVar( const Var v );
  /** fill the vector with all the literals that have been deleted after the given timer */
  void getActiveVariables(const uint32_t myTimer, vector< Var >& activeVariables );
  /** fill the heap with all the literals that have been deleted afetr the given timer */
  
  template<class Comp>
  void getActiveVariables(const uint32_t myTimer, Heap < Comp > & heap );

  /** resets all delete timer */
  void resetDeleteTimer();

// mark methods
  void mark1(Var x, Coprocessor::MarkArray& array);
  void mark2(Var x, Coprocessor::MarkArray& array, MarkArray& tmp);

// locking
  void lock()   { dataLock.lock();   } // lock and unlock the data structure
  void unlock() { dataLock.unlock(); } // lock and unlock data structure

// formula statistics ==========================
  void addedLiteral(   const Lit l, const int32_t diff = 1);	// update counter for literal
  void removedLiteral( const Lit l, const int32_t diff = 1);	// update counter for literal
  void addedClause (   const CRef cr );			// update counters for literals in the clause
  void removedClause ( const CRef cr );			// update counters for literals in the clause
  void removedClause ( const Lit l1, const Lit l2 );		// update counters for literals in the clause
  
  void correctCounters();

  // extending model after clause elimination procedures - l will be put first in list to be undone if necessary!
  void addToExtension( const Minisat::CRef cr, const Lit l = lit_Error );
  void addToExtension( vec< Lit >& lits, const Lit l = lit_Error );
  void addToExtension( vector< Lit >& lits, const Lit l = lit_Error );
  void addToExtension( const Lit dontTouch, const Lit l = lit_Error );

  void extendModel(vec<lbool>& model);

  // handling equivalent literals
  void addEquivalences( const std::vector<Lit>& list );
  void addEquivalences( const Lit& l1, const Lit& l2 );
  vector<Lit>& getEquivalences();

  // checking whether a literal can be altered - TODO: use the frozen information from the solver object!
  void setNotTouch(const Var v);
  bool doNotTouch (const Var v) const ;
  
  // TODO: remove after debug
  void printTrail(ostream& stream) {
    for( int i = 0 ; i < solver->trail.size(); ++ i ) cerr << " " << solver->trail[i]; 
  }
};

/** class representing the binary implication graph of the formula */
class BIG {
  // TODO implement a weak "implies" check based on the implication graph sampling!
  Lit* storage;
  int* sizes;
  Lit** big;

  /** these two arrays can be used to check whether a literal l implies another literal l'
   *  Note: this is not a complete check!
   */
  uint32_t *start; // when has to literal been touch when scanning the BIG
  uint32_t *stop;  // when has to literal been finished during scanning

  uint32_t stampLiteral( const Lit literal, uint32_t stamp, int32_t* index, deque< Lit >& stampQueue );
  void shuffle( Lit* adj, int size ) const;

public:
  BIG();
  ~BIG();

  /** adds binary clauses */
  void create( ClauseAllocator& ca, Coprocessor::CoprocessorData& data, vec< Minisat::CRef >& list);
  void create( ClauseAllocator& ca, Coprocessor::CoprocessorData& data, vec< Minisat::CRef >& list1, vec< Minisat::CRef >& list2);

  /** removes an edge from the graph again */
  void removeEdge(const Lit l0, const Lit l1 );

  Lit* getArray(const Lit l);
  const Lit* getArray(const Lit l) const;
  const int getSize(const Lit l) const;

  /** will travers the BIG and generate the start and stop indexes to check whether a literal implies another literal
   * @return false, if BIG is not initialized yet
   */
  void generateImplied(Coprocessor::CoprocessorData& data);

  /** return true, if the condition "from -> to" holds, based on the stochstic scanned data */
  bool implies(const Lit& from, const Lit& to) const;

  /** return whether child occurs in the adjacence list of parent (and thus implied) */
  bool isChild(const Lit& parent, const Lit& child) const ;

  /** return whether one of the two literals is a direct child of parent (and thus implied)  */
  bool isOneChild( const Lit& parent, const Lit& child1, const Lit& child2 ) const ;
  
  /** get indexes of BIG scan algorithm */
  uint32_t getStart( const Lit& l ) { return start != 0 ? start[ toInt(l) ] : 0; }
  /** get indexes of BIG scan algorithm */
  uint32_t getStop( const Lit& l ) { return stop != 0 ? stop[ toInt(l) ] : 0; }
};

inline CoprocessorData::CoprocessorData(ClauseAllocator& _ca, Solver* _solver, Coprocessor::Logger& _log, bool _limited, bool _randomized)
: ca ( _ca )
, solver( _solver )
, hasLimit( _limited )
, randomOrder(_randomized)
, log(_log)
{
}

inline void CoprocessorData::init(uint32_t nVars)
{
  occs.resize( nVars * 2 );
  lit_occurrence_count.resize( nVars * 2, 0 );
  numberOfVars = nVars;
  deleteTimer.create( nVars );
  untouchable.resize(nVars);
}

inline void CoprocessorData::destroy()
{
  ComplOcc().swap(occs); // free physical space of the vector
  vector<int32_t>().swap(lit_occurrence_count);
  deleteTimer.destroy();
}

inline vec< Minisat::CRef >& CoprocessorData::getClauses()
{
  return solver->clauses;
}

inline vec< Minisat::CRef >& CoprocessorData::getLEarnts()
{
  return solver->learnts;
}

inline Var CoprocessorData::nextFreshVariable()
{
  // be careful
  Var nextVar = solver->newVar();
  numberOfVars = solver->nVars();
  ma.resize( 2*nVars() );
  
  deleteTimer.resize( 2*nVars() );
  
  occs.resize( 2*nVars() );
  // cerr << "c resize occs to " << occs.size() << endl;
  lit_occurrence_count.resize( 2*	nVars() );
  
  untouchable.push_back(0);
  // cerr << "c new fresh variable: " << nextVar+1 << endl;
  return nextVar;
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
  return solver->asynch_interrupt;
}


inline lbool CoprocessorData::enqueue(const Lit l)
{
  if( global_debug_out ) cerr << "c enqueue " << l << " with previous value " << (solver->value( l ) == l_Undef ? "undef" : (solver->value( l ) == l_False ? "unsat" : " sat ") ) << endl;
  if( solver->value( l ) == l_False) {
    solver->ok = false; // set state to false
    return l_False;
  } else if( solver->value( l ) == l_Undef ) solver->uncheckedEnqueue(l);
    return l_True;
  return l_Undef;
}

inline lbool CoprocessorData::value(const Lit l) const
{
 return solver->value( l );
}

inline Solver* CoprocessorData::getSolver()
{
  return solver;
}


inline bool CoprocessorData::hasToPropagate()
{
  return solver->trail.size() != solver->qhead;
}


inline void CoprocessorData::addClause(const Minisat::CRef cr)
{
  const Clause & c = ca[cr];
  if( c.can_be_deleted() ) return;
  for (int l = 0; l < c.size(); ++l)
  {
    occs[toInt(c[l])].push_back(cr);
    addedLiteral( c[l] );
  }
  numberOfCls ++;
}

inline bool CoprocessorData::removeClauseFrom(const Minisat::CRef cr, const Lit l)
{
  vector<CRef>& list = occs[toInt(l)];
  for( int i = 0 ; i < list.size(); ++ i )
  {
    if( list[i] == cr ) {
      list[i] = list[ list.size() - 1 ];
      list.pop_back();
      return true;
    }
  }
  return false;
}

inline void CoprocessorData::removeClauseFrom(const Minisat::CRef cr, const Lit l, const int index)
{
  vector<CRef>& list = occs[toInt(l)];
  assert( list[index] == cr );
  list[index] = list[ list.size() -1 ];
}


inline uint32_t CoprocessorData::getMyDeleteTimer()
{
  return deleteTimer.nextStep();
}

inline void CoprocessorData::deletedVar(const Var v)
{
  deleteTimer.setCurrentStep(v);
}

inline void CoprocessorData::getActiveVariables(const uint32_t myTimer, vector< Var >& activeVariables)
{
  for( Var v = 0 ; v < solver->nVars(); ++ v ) {
    if( deleteTimer.getIndex(v) >= myTimer ) activeVariables.push_back(v);
  }
}


template<class Comp>
inline void CoprocessorData::getActiveVariables(const uint32_t myTimer, Heap< Comp > & heap)
{
  for( Var v = 0 ; v < solver->nVars(); ++ v ) {
    if( deleteTimer.getIndex(v) >= myTimer ) heap.insert(v);
  } 
}

inline void CoprocessorData::resetDeleteTimer()
{
  deleteTimer.reset();
}


inline void CoprocessorData::addedClause(const Minisat::CRef cr)
{
  const Clause & c = ca[cr];
  for (int l = 0; l < c.size(); ++l)
  {
    addedLiteral( c[l] );
  }
}

inline void CoprocessorData::addedLiteral(const Lit l, const  int32_t diff)
{
  lit_occurrence_count[toInt(l)] += diff;
}

inline void CoprocessorData::removedClause(const Minisat::CRef cr)
{
  const Clause & c = ca[cr];
  for (int l = 0; l < c.size(); ++l)
  {
    removedLiteral( c[l] );
  }
  numberOfCls --;
}

inline void CoprocessorData::removedClause(const Lit l1, const Lit l2)
{
  removedLiteral(l1);
  removedLiteral(l2);
  
  const Lit searchLit = lit_occurrence_count[toInt(l1)] < lit_occurrence_count[toInt(l2)] ? l1 : l2;
  const Lit secondLit = toLit(  toInt(l1) ^ toInt(l2) ^ toInt(searchLit) );

  // find the right binary clause and remove it!
  for( int i = 0 ; i < list(searchLit).size(); ++ i ) {
    Clause& cl = ca[list(searchLit)[i]];
    if( cl.can_be_deleted() || cl.size() != 2 ) continue;
    if( cl[0] == secondLit || cl[1] == secondLit ) {
      cl.set_delete(true);
      break;
    }
  }
}

inline void CoprocessorData::removedLiteral(Lit l, int32_t diff)
{
  deletedVar(var(l));
  lit_occurrence_count[toInt(l)] -= diff;
}

inline int32_t& CoprocessorData::operator[](const Lit l)
{
  return lit_occurrence_count[toInt(l)];
}

inline int32_t CoprocessorData::operator[](const Var v) const
{
  return lit_occurrence_count[toInt(mkLit(v,0))] + lit_occurrence_count[toInt(mkLit(v,1))];
}

inline vector< Minisat::CRef >& CoprocessorData::list(const Lit l)
{
   return occs[ toInt(l) ];
}

inline const vector< Minisat::CRef >& CoprocessorData::list(const Lit l) const
{
   return occs[ toInt(l) ];
}

inline void CoprocessorData::correctCounters()
{
  numberOfVars = solver->nVars();
  numberOfCls = 0;
  // reset to 0
  for (int v = 0; v < solver->nVars(); v++)
    for (int s = 0; s < 2; s++)
      lit_occurrence_count[ toInt(mkLit(v,s)) ] = 0;
  // re-calculate counters!
  for( int i = 0 ; i < solver->clauses.size(); ++ i ) {
    const Clause & c = ca[ solver->clauses[i] ];
    if( c.can_be_deleted() ) continue;
    numberOfCls ++;
    for( int j = 0 ; j < c.size(); j++) lit_occurrence_count[ toInt(c[j]) ] ++; // increment all literal counters accordingly
  }
  for( int i = 0 ; i < solver->learnts.size(); ++ i ) {
    const Clause & c = ca[ solver->learnts[i] ];
    if( c.can_be_deleted() ) continue;
    numberOfCls ++;
    for( int j = 0 ; j < c.size(); j++) lit_occurrence_count[ toInt(c[j]) ] ++; // increment all literal counters accordingly
  }
}

/** Mark all variables that occure together with _x_.
 *
 * @param x the variable to start with
 * @param array the mark array in which the marks are set
 */
inline void CoprocessorData::mark1(Var x, MarkArray& array)
{
  std::vector<CRef> & clauses = occs[Minisat::toInt( mkLit(x,true))];
  for( int i = 0; i < clauses.size(); ++i)
  {
    CRef cr = clauses[i];
    Clause &c = ca[cr];
    for (int j = 0; j < c.size(); ++j)
    {
      array.setCurrentStep(var(c[j]));
    }
  }
  clauses = occs[Minisat::toInt( mkLit(x,false) )];
  for( int i = 0; i < clauses.size(); ++i)
  {
    CRef cr = clauses[i];
    Clause &c = ca[cr];
    for (int j = 0; j < c.size(); ++j)
    {
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
inline void CoprocessorData::mark2(Var x, MarkArray& array, MarkArray& tmp)
{
  tmp.nextStep();
  // for negative literal
  std::vector<CRef> & clauses = occs[Minisat::toInt( mkLit(x,true))];
  for( int i = 0; i < clauses.size(); ++i)
  {
    Clause &c = ca[clauses[i]];
    // for l in C
    for (int l = 0; l < c.size(); ++l)
    {
      if( !tmp.isCurrentStep(var(c[l])) )
      {
        mark1(var(c[l]), array);
      }
      tmp.setCurrentStep(var(c[l]));
    }
  }
  // for positive literal
  clauses = occs[Minisat::toInt( mkLit(x,false))];
  for( int i = 0; i < clauses.size(); ++i)
  {
    Clause &c = ca[clauses[i]];
    for (int l = 0; l < c.size(); ++l)
    {
      if( !tmp.isCurrentStep(var(c[l])) )
      {
        mark1(var(c[l]), array);
      }
      tmp.setCurrentStep(var(c[l]));
    }
  }
}

inline void CoprocessorData::addToExtension(const Minisat::CRef cr, const Lit l)
{
  const Clause& c = ca[cr];
  undo.push_back(lit_Undef);
  if( l != lit_Error ) undo.push_back(l);
  for( int i = 0 ; i < c.size(); ++ i ) {
    if( c[i] != l ) undo.push_back(c[i]);
  }
}

inline void CoprocessorData::addToExtension(vec< Lit >& lits, const Lit l)
{
  undo.push_back(lit_Undef);
  if( l != lit_Error ) undo.push_back(l);
  for( int i = 0 ; i < lits.size(); ++ i ) {
    if( lits[i] != l ) undo.push_back(lits[i]);
  }
}

inline void CoprocessorData::addToExtension(vector< Lit >& lits, const Lit l)
{
  undo.push_back(lit_Undef);
  if( l != lit_Error ) undo.push_back(l);
  for( int i = 0 ; i < lits.size(); ++ i ) {
    if( lits[i] != l ) undo.push_back(lits[i]);
  }
}

inline void CoprocessorData::addToExtension(const Lit dontTouch, const Lit l)
{
  undo.push_back(lit_Undef);
  if( l != lit_Error) undo.push_back(l);
  undo.push_back(dontTouch);

}


inline void CoprocessorData::extendModel(vec< lbool >& model)
{
  const bool local_debug = false;
  if( global_debug_out || local_debug) {
    cerr << "c extend model of size " << model.size() << " with undo information of size " << undo.size() << endl;
    cerr << "c  in model: ";
    for( int i = 0 ; i < model.size(); ++ i ) {
      const Lit satLit = mkLit( i, model[i] == l_True ? false : true );
      cerr << satLit << " ";
    }
    cerr << endl;
  }

  // check current clause for being satisfied
  bool isSat = false;
  for( int i = undo.size() - 1; i >= 0 ; --i ) {
     isSat = false; // init next clause!
     const Lit c = undo[i];
     if( global_debug_out  || local_debug) cerr << "c read literal " << c << endl;
     if( c == lit_Undef ) {
       if( !isSat ) {
         // if clause is not satisfied, satisfy last literal!
         const Lit& satLit = undo[i+1];
         log.log(1, "set literal to true",satLit);
         model[ var(satLit) ] = sign(satLit) ? l_False : l_True;
       }
     }
     if( var(c) > model.size() ) model.growTo( var(c), l_True ); // model is too small?
     if (model[var(c)] == (sign(c) ? l_False : l_True) ) // satisfied
     {
       isSat = true;
       while( undo[i] != lit_Undef ){
	 if( global_debug_out  || local_debug) cerr << "c skip because SAT: " << undo[i] << endl; 
	 --i;
       }
     }
  }

  if( global_debug_out  || local_debug) {
    cerr << "c out model: ";
    for( int i = 0 ; i < model.size(); ++ i ) {
      const Lit satLit = mkLit( i, model[i] == l_True ? false : true );
      cerr << satLit << " ";
    }
    cerr << endl;
  }
}

inline void CoprocessorData::addEquivalences(const vector< Lit >& list)
{
  for( int i = 0 ; i < list.size(); ++ i ) equivalences.push_back(list[i]);
  equivalences.push_back( lit_Undef ); // termination symbol!
}

inline void CoprocessorData::addEquivalences(const Lit& l1, const Lit& l2)
{
  if( global_debug_out ) cerr << "c [DATA] set equi: " << l1 << " == " << l2 << endl;
  equivalences.push_back(l1);
  equivalences.push_back(l2);
  equivalences.push_back( lit_Undef ); // termination symbol!
}

inline vector< Lit >& CoprocessorData::getEquivalences()
{
  return equivalences;
}


inline void CoprocessorData::setNotTouch(const Var v)
{
  untouchable[v] = 1;
}

inline bool CoprocessorData::doNotTouch(const Var v) const
{
  return untouchable[v] == 1;
}


inline BIG::BIG()
: big(0), storage(0), sizes(0), start(0), stop(0)
{}

inline BIG::~BIG()
{
 if( big != 0 )     free( big );
 if( storage != 0 ) free( storage );
 if( sizes != 0 )   free( sizes );
}

inline void BIG::create(ClauseAllocator& ca, CoprocessorData& data, vec<CRef>& list)
{
  sizes = (int*) malloc( sizeof(int) * data.nVars() * 2 );
  memset(sizes,0, sizeof(int) * data.nVars() * 2 );

  int sum = 0;
  // count occurrences of literals in binary clauses of the given list
  for( int i = 0 ; i < list.size(); ++i ) {
    const Clause& c = ca[list[i]];
    if(c.size() != 2 || c.can_be_deleted() ) continue;
    sizes[ toInt( ~c[0] )  ] ++;
    sizes[ toInt( ~c[1] )  ] ++;
    sum += 2;
  }
  storage = (Lit*) malloc( sizeof(Lit) * sum );
  big =  (Lit**)malloc ( sizeof(Lit*) * data.nVars() * 2 );
  // memset(sizes,0, sizeof(Lit*) * data.nVars() * 2 );
  // set the pointers to the right location and clear the size
  sum = 0 ;
  for ( int i = 0 ; i < data.nVars() * 2; ++ i )
  {
    big[i] = &(storage[sum]);
    sum += sizes[i];
    sizes[i] = 0;
  }

  // add all binary clauses to graph
  for( int i = 0 ; i < list.size(); ++i ) {
    const Clause& c = ca[list[i]];
    if(c.size() != 2 || c.can_be_deleted() ) continue;
    const Lit l0 = c[0]; const Lit l1 = c[1];
    ( big[ toInt(~l0) ] )[ sizes[toInt(~l0)] ] = l1;
    ( big[ toInt(~l1) ] )[ sizes[toInt(~l1)] ] = l0;
    sizes[toInt(~l0)] ++;
    sizes[toInt(~l1)] ++;
  }
}

inline void BIG::create(ClauseAllocator& ca, CoprocessorData& data, vec< Minisat::CRef >& list1, vec< Minisat::CRef >& list2)
{
  sizes = (int*) malloc( sizeof(int) * data.nVars() * 2 );
  memset(sizes,0, sizeof(int) * data.nVars() * 2 );

  int sum = 0;
  // count occurrences of literals in binary clauses of the given list
  for( int p = 0 ; p < 2; ++ p ) {
    const vec<CRef>& list = (p==0 ? list1 : list2 );
    for( int i = 0 ; i < list.size(); ++i ) {
      const Clause& c = ca[list[i]];
      if(c.size() != 2 || c.can_be_deleted() ) continue;
      sizes[ toInt( ~c[0] )  ] ++;
      sizes[ toInt( ~c[1] )  ] ++;
      sum += 2;
    }
  }
  storage = (Lit*) malloc( sizeof(Lit) * sum );
  big =  (Lit**)malloc ( sizeof(Lit*) * data.nVars() * 2 );
  // memset(sizes,0, sizeof(Lit*) * data.nVars() * 2 );
  // set the pointers to the right location and clear the size
  sum = 0 ;
  for ( int i = 0 ; i < data.nVars() * 2; ++ i )
  {
    big[i] = &(storage[sum]);
    sum += sizes[i];
    sizes[i] = 0;
  }

  // add all binary clauses to graph
  for( int p = 0 ; p < 2; ++ p ) {
    const vec<CRef>& list = (p==0 ? list1 : list2 );
    for( int i = 0 ; i < list.size(); ++i ) {
      const Clause& c = ca[list[i]];
      if(c.size() != 2 || c.can_be_deleted() ) continue;
      const Lit l0 = c[0]; const Lit l1 = c[1];
      ( big[ toInt(~l0) ] )[ sizes[toInt(~l0)] ] = l1;
      ( big[ toInt(~l1) ] )[ sizes[toInt(~l1)] ] = l0;
      sizes[toInt(~l0)] ++;
      sizes[toInt(~l1)] ++;
    }
  }
}


inline void BIG::removeEdge(const Lit l0, const Lit l1)
{
  // remove literal from the two lists
  Lit* list = getArray( ~l0 );
  const uint32_t size = getSize( ~l0 );
  for( int i = 0 ; i < size; ++i )
  {
    if( list[i] == l1 ) {
      list[i] = list[ size - 1 ];
      sizes[ toInt(~l0) ] --;
    }
  }
  Lit* list2 = getArray( ~l0 );
  const uint32_t size2 = getSize( ~l0 );
  for( int i = 0 ; i < size2; ++i )
  {
    if( list2[i] == l0 ) {
      list2[i] = list2[ size - 1 ];
      sizes[ toInt(~l1) ] --;
    }
  }
}

inline Lit* BIG::getArray(const Lit l)
{
  return big[ toInt(l) ];
}

inline const Lit* BIG::getArray(const Lit l) const
{
  return big[ toInt(l) ];
}

inline const int BIG::getSize(const Lit l) const
{
  return sizes[ toInt(l) ];
}

inline void BIG::generateImplied( CoprocessorData& data )
{
    uint32_t stamp = 1 ;

    if( start == 0 ) start = (uint32_t*) malloc( data.nVars() * sizeof(uint32_t) * 2 );
    else start = (uint32_t*)realloc( start, data.nVars() * sizeof(uint32_t) * 2 );

    if( stop == 0 ) stop = (uint32_t*) malloc( data.nVars() * sizeof(uint32_t) * 2 );
    else stop = (uint32_t*)realloc( stop, data.nVars() * sizeof(int32_t) * 2 );

    int32_t* index = (int32_t*)malloc( data.nVars() * sizeof(int32_t) * 2 );

    // set everything to 0!
    memset( start, 0, data.nVars() * sizeof(uint32_t) * 2 );
    memset( stop, 0, data.nVars() * sizeof(uint32_t) * 2 );
    memset( index, 0, data.nVars() * sizeof(int32_t) * 2 );


    deque< Lit > stampQueue;

    data.lits.clear();
    // reset all present variables, collect all roots in binary implication graph
    for ( Var v = 0 ; v < data.nVars(); ++ v )
    {
      const Lit pos =  mkLit(v,false);
      // a literal is a root, if its complement does not imply a literal
      if( getSize(pos) == 0 ) data.lits.push_back(~pos);
      if( getSize(~pos) == 0 ) data.lits.push_back(pos);
    }

    // do stamping for all roots, shuffle first
    const uint32_t ts = data.lits.size();
    for( uint32_t i = 0 ; i < ts; i++ ) { const uint32_t rnd=rand()%ts; const Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd]=tmp; }
    // stamp shuffled data.lits
    for ( uint32_t i = 0 ; i < ts; ++ i )
      stamp = stampLiteral(data.lits[i],stamp,index,stampQueue);

    // stamp all remaining literals, after shuffling
    data.lits.clear();
    for ( Var v = 0 ; v < data.nVars(); ++ v )
    {
      const Lit pos =  mkLit(v,false);
      if( start[ toInt(pos) ] == 0 ) data.lits.push_back(pos);
      if( start[ toInt(~pos) ] == 0 ) data.lits.push_back(~pos);
    }
    // stamp shuffled data.lits
    const uint32_t ts2 = data.lits.size();
    for( uint32_t i = 0 ; i < ts2; i++ ) { const uint32_t rnd=rand()%ts2; const Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd]=tmp; }
    for ( uint32_t i = 0 ; i < ts2; ++ i )
      stamp = stampLiteral(data.lits[i],stamp,index,stampQueue);
}

inline void BIG::shuffle( Lit* adj, int size ) const
{
  for( uint32_t i = 0 ;  i+1<size; ++i ) {
    const uint32_t rnd = rand() % size;
    const Lit tmp = adj[i];
    adj[i] = adj[rnd];
    adj[rnd] = tmp;
  }
}

inline uint32_t BIG::stampLiteral( const Lit literal, uint32_t stamp, int32_t* index, deque<Lit>& stampQueue)
{
  // do not stamp a literal twice!
  if( start[ toInt(literal) ] != 0 ) return stamp;

  if( global_debug_out ) cerr << "c call STAMP for " << literal << endl;
  
  stampQueue.clear();
  // linearized algorithm from paper
  stamp++;
  // handle initial literal before putting it on queue
  start[toInt(literal)] = stamp; // parent and root are already set to literal
  if( global_debug_out ) cerr << "c start[" << literal << "] = " << stamp << endl;
  stampQueue.push_back(literal);

  shuffle( getArray(literal), getSize(literal) );
  index[toInt(literal)] = 0;

  while( ! stampQueue.empty() )
    {
      const Lit current = stampQueue.back();
      const Lit* adj =   getArray(current);
      const int adjSize = getSize(current);

      if( index[toInt(current)] == adjSize ) {
	stampQueue.pop_back();
	stamp++;
	stop[toInt(current)] = stamp;
	if( global_debug_out ) cerr << "c stop[" << current << "] = " << stamp << endl;
      } else {
	int32_t& ind = index[ toInt(current) ]; // store number of processed elements
	const Lit impliedLit = getArray( current )[ind]; // get next implied literal
	ind ++;
	if( start[ toInt(impliedLit) ] != 0 ) continue;
	stamp ++;
	start[ toInt(impliedLit) ] = stamp;
	if( global_debug_out ) cerr << "c start[" << impliedLit << "] = " << stamp << endl;
	index[ toInt(impliedLit) ] = 0;
	stampQueue.push_back( impliedLit );
	shuffle( getArray(impliedLit), getSize(impliedLit) );
      }

    }
    return stamp;
}

inline bool BIG::implies(const Lit& from, const Lit& to) const
{
  if( start == 0 || stop == 0 ) return false;
  return ( start[ toInt(from) ] < start[ toInt(to) ] && stop[ toInt(from) ] > stop[ toInt(to) ] )
   || ( start[ toInt(~to) ] < start[ toInt(~from) ] && stop[ toInt(~to) ] > stop[ toInt(~from) ] );
}

inline bool BIG::isChild(const Lit& parent, const Lit& child) const
{
  const Lit * list = getArray(parent);
  const int listSize = getSize(parent);
  for( int j = 0 ; j < listSize; ++ j ) {
    if( list[j] == child )
      return true;
  }
  return false;
}

inline bool BIG::isOneChild(const Lit& parent, const Lit& child1, const Lit& child2) const
{
  const Lit * list = getArray(parent);
  const int listSize = getSize(parent);
  for( int j = 0 ; j < listSize; ++ j ) {
    if( list[j] == child1 || list[j] == child2 ) return true;
  }
  return false;
}


inline Logger::Logger(int level, bool err)
: outputLevel(level), useStdErr(err)
{}

inline void Logger::log(int level, const string& s)
{
  if( level > outputLevel ) return;
  (useStdErr ? std::cerr : std::cout )
    << "c [" << level << "] " << s << endl;
}

inline void Logger::log(int level, const string& s, const Clause& c)
{
  if( level > outputLevel ) return;
  (useStdErr ? std::cerr : std::cout )
    << "c [" << level << "] " << s << " : " ;
  for( int i = 0 ; i< c.size(); ++i ) {
    const Lit& l = c[i];
    (useStdErr ? std::cerr : std::cout )
      << " " << (sign(l) ? "-" : "") << var(l)+1;
  }
  (useStdErr ? std::cerr : std::cout )
    << endl;
}

inline void Logger::log(int level, const string& s, const Lit& l)
{
  if( level > outputLevel ) return;
  (useStdErr ? std::cerr : std::cout )
    << "c [" << level << "] " << s << " : "
    << (sign(l) ? "-" : "") << var(l)+1
    << endl;
}

inline void Logger::log(int level, const string& s, const int i)
{
  if( level > outputLevel ) return;
  (useStdErr ? std::cerr : std::cout )
    << "c [" << level << "] " << s << " " << i << endl;
}


inline void Logger::log(int level, const string& s, const Clause& c, const Lit& l)
{
  if( level > outputLevel ) return;
  (useStdErr ? std::cerr : std::cout )
    << "c [" << level << "] " << s << " : "
    << (sign(l) ? "-" : "") << var(l)+1 << " with clause ";
  for( int i = 0 ; i< c.size(); ++i ) {
    const Lit& l = c[i];
    (useStdErr ? std::cerr : std::cout )
      << " " << (sign(l) ? "-" : "") << var(l)+1;
  }
  (useStdErr ? std::cerr : std::cout )
    << endl;
}


}

#endif
