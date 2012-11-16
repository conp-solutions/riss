/******************************************************************************[CoprocessorTypes.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef COPROCESSORTYPES_HH
#define COPROCESSORTYPES_HH

#include "core/Solver.h"

#include "coprocessor-src/LockCollection.h"

#include <vector>

using namespace Minisat;
using namespace std;

namespace Coprocessor {

typedef std::vector<std::vector <CRef> > ComplOcc;

/** this object frees a pointer before a method /statementblock is left */
class MethodFree {
  void* pointer;
public:
  MethodFree( void* ptr ) : pointer( ptr ) {}
  ~MethodFree() { free( pointer ); }
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
	bool hasSameIndex( const uint32_t index, const uint32_t comparestep ) const {
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
  void log( int level, const string& s, const Clause& c);
  void log( int level, const string& s, const Lit& l);
  void log( int level, const string& s, const Clause& c, const Lit& l);
};

/** Data, that needs to be accessed by coprocessor and all the other classes
 */
class CoprocessorData
{
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
  char* untouchable;                    // store all variables that should not be touched (altering presence in models)

  vector<Lit> undo;                     // store clauses that have to be undone for extending the model
  
  // TODO decide whether a vector of active variables would be good!
  
public:
  
  Logger& log;                           // responsible for logs
  
  CoprocessorData(ClauseAllocator& _ca, Solver* _solver, Logger& _log, bool _limited = true, bool _randomized = false);

  // init all data structures for being used for nVars variables
  void init( uint32_t nVars );

  // free all the resources that are used by this data object,
  void destroy();

  int32_t& operator[] (const Lit l ); // return the number of occurrences of literal l
  int32_t operator[] (const Var v ); // return the number of occurrences of variable v
  vector<CRef>& list( const Lit l ); // return the list of clauses, which have literal l

  vec<CRef>& getClauses();           // return the vector of clauses in the solver object
  vec<CRef>& getLEarnts();           // return the vector of learnt clauses in the solver object

  uint32_t nCls()  const { return numberOfCls; }
  uint32_t nVars() const { return numberOfVars; }

// semantic:
  bool ok();                                             // return ok-state of solver
  void setFailed();                                      // found UNSAT, set ok state to false
  lbool enqueue( const Lit l );                          // enqueue literal l to current solver structures
  bool unlimited();                                      // do preprocessing without technique limits?
  bool randomized();                                     // use a random order for preprocessing techniques

// adding, removing clauses and literals =======
  void addClause (      const CRef cr );                 // add clause to data structures, update counters
  bool removeClauseFrom (const Minisat::CRef cr, const Lit l); // remove clause reference from list of clauses for literal l, returns true, if successful

// delete timers
  /** gives back the current times, increases for the next technique */
  uint32_t getMyDeleteTimer();
  /** tell timer system that variable has been deleted (thread safe!) */
  uint32_t deletedVar( const Var v );
  /** fill the vector with all the literals that have been deleted after the given timer */
  void getActiveVariables(const uint32_t myTimer, vector< Var >& activeVariables );
  /** resets all delete timer */
  void resetDeleteTimer();

// mark methods
  void mark1(Var x, Coprocessor::MarkArray& array);
  void mark2(Var x, Coprocessor::MarkArray& array, MarkArray& tmp);

// locking
  void lock()   { dataLock.lock();   } // lock and unlock the data structure
  void unlock() { dataLock.unlock(); } // lock and unlock data structure

// formula statistics ==========================
  void addedLiteral(   const Lit l, const int32_t diff = 1);  // update counter for literal
  void removedLiteral( const Lit l, const int32_t diff = 1);  // update counter for literal
  void addedClause (   const CRef cr );                   // update counters for literals in the clause
  void removedClause ( const CRef cr );                 // update counters for literals in the clause

  void correctCounters();

  // extending model after clause elimination procedures - l will be put first in list to be undone if necessary!
  void addToExtension( const Minisat::CRef cr, const Lit l = lit_Error );
  void addToExtension( vec< Lit >& lits, const Lit l = lit_Error );
  void addToExtension( vector< Lit >& lits, const Lit l = lit_Error );
  
  void extendModel(vec<lbool>& model);
  
  // checking whether a literal can be altered
  void setNotTouch(const Var v);
  bool doNotTouch (const Var v) const ;
};

/** class representing the binary implication graph of the formula */
class BIG {
  Lit* storage;
  int* sizes;
  Lit** big;
public:
  BIG();
  ~BIG();

  /** adds binary clauses */
  void create( ClauseAllocator& ca, Coprocessor::CoprocessorData& data, vec< Minisat::CRef >& list);

  /** removes an edge from the graph again */
  void removeEdge(const Lit l0, const Lit l1 );

  Lit* getArray(const Lit l);
  const int getSize(const Lit l);


};

inline CoprocessorData::CoprocessorData(ClauseAllocator& _ca, Solver* _solver, Coprocessor::Logger& _log, bool _limited, bool _randomized)
: ca ( _ca )
, solver( _solver )
, hasLimit( _limited )
, randomOrder(_randomized)
, log(_log)
, untouchable(0)
{
}

inline void CoprocessorData::init(uint32_t nVars)
{
  occs.resize( nVars * 2 );
  lit_occurrence_count.resize( nVars * 2, 0 );
  numberOfVars = nVars;
  deleteTimer.create( nVars );
  untouchable = (char*) malloc( sizeof(char) * nVars);
  memset( untouchable, 0, sizeof(char) * nVars );
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

inline lbool CoprocessorData::enqueue(const Lit l)
{
  if( solver->value( l ) == l_False) {
    solver->ok = false; // set state to false
    return l_False;
  } else if( solver->value( l ) == l_Undef ) solver->uncheckedEnqueue(l);
    return l_True;
  return l_Undef;
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

inline uint32_t CoprocessorData::getMyDeleteTimer()
{
  return deleteTimer.nextStep();
}

inline uint32_t CoprocessorData::deletedVar(const Var v)
{
  deleteTimer.setCurrentStep(v);
}

inline void CoprocessorData::getActiveVariables(const uint32_t myTimer, vector< Var >& activeVariables)
{
  for( Var v = 0 ; v < solver->nVars(); ++ v ) {
    if( deleteTimer.getIndex(v) > myTimer ) activeVariables.push_back(v);
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

inline void CoprocessorData::removedLiteral(Lit l, int32_t diff)
{
  lit_occurrence_count[toInt(l)] -= diff;
}

inline int32_t& CoprocessorData::operator[](const Lit l)
{
  return lit_occurrence_count[toInt(l)];
}

inline int32_t CoprocessorData::operator[](const Var v)
{
  return lit_occurrence_count[toInt(mkLit(v,0))] + lit_occurrence_count[toInt(mkLit(v,1))];
}

inline vector< Minisat::CRef >& CoprocessorData::list(const Lit l)
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

inline void CoprocessorData::mark2(Var x, MarkArray& array, MarkArray& tmp)
{
  tmp.nextStep();
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

inline void CoprocessorData::extendModel(vec< lbool >& model)
{
  // check current clause for being satisfied
  bool isSat = false;
  for( int i = undo.size() - 1; i >= 0 ; --i ) {
     isSat = false; // init next clause!
     Lit c = undo[i];
     
     if( c == lit_Undef ) {
       // if clause is not satisfied, satisfy last literal!
       const Lit& satLit = undo[i+1];
       model[ var(satLit) ] = sign(satLit) ? l_False : l_True;
     }
     if( var(c) > model.size() ) model.growTo( var(c), l_True ); // model is too small?
     if (model[var(c)] == (sign(c) ? l_False : l_True) ) // satisfied
     {
       isSat = true;
       while( undo[i] != lit_Undef ) --i;
     }
  }
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
: big(0), storage(0), sizes(0)
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

inline const int BIG::getSize(const Lit l)
{
  return sizes[ toInt(l) ];
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
