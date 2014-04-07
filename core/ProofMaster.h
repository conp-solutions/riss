/*********************************************************************************[Communication.h]
Copyright (c) 2014, All rights reserved, Norbert Manthey

 **************************************************************************************************/

#ifndef PROOFMASTER_H
#define PROOFMASTER_H


#include "mtl/Vec.h"
#include "core/SolverTypes.h"
#include "utils/LockCollection.h"
#include <mprocessor-src/open-wbo/open-wbo/solvers/glucored/utils/ParseUtils.h>

#include <cstdio>
#include <iostream>

using namespace Minisat;
using namespace std;

/** class that takes care of constructing a DRUP proof for a portfolio solver
 *  (yet, only DRUP is supported, and each solver is assumed to not introduce new variables)
 */
class ProofMaster 
{
  SleepLock ownLock;	// lock for the proof file
  FILE* drupProofFile; // Output for DRUP unsat proof
  int threads;		// number of used threads
  int HASHMAX;		// constant used for hashing
  
  
  /** temporal storage for clauses for the proof for each thread until a clause is shared (one more for the proof master
   *  lit_Undef is used to separate clauses
   *  lit_Error is used to indicate deletion of clauses
   */
  vector< vector<Lit> > localClauses;	
  
  // for each clause, re-use the LBD as a counter how often that clause is theoretically present in the proof
  
  vector< vector<CRef> > hashTable; // for each hash have a list of clauses, the LBD of the clause is re-used as the counter for the number of occurrences
  ClauseAllocator ca;	// storage for clauses, all active clauses represent the clauses that are currently present in the proof
  
  MarkArray matchArray;	// array to match two clauses
  
  vec<Lit> tmpCls;	// helper vector to create unit clauses in allocator
  
public:

  /** set up a proof master class with a handle to a file, and a number of threads */
  ProofMaster( FILE* drupFile, const int nrOfThreads, const int nVars, const int hashTableSize = 100000);
  
  /** add clauses to the proof (if not local, a lock is used before the global proof is updated)
   * Note: that added clause should not be an original clause from the CNF formula! this has to be ensured by the calling thread!
   * @param clause clause that should be added (can be vector or clause)
   * @param extraClauseLit additional literal of the clause that is written to the proof at the first position (and might be presend in the clause again)
   * @param ownerID id of the calling thread, for the clause sharing pool the ID has to match the thread number
   * @param local add the clauses only to the local storage, and do not submit them to the global proof (can be done later, when the next clauses are sent)
   */
  template <class T>
  void addToProof(   const T& clause, const Lit& extraClauseLit , int ownerID, bool local = true); 
  /** add a set of unit clauses to the proof */
  void addUnitsToProof( const vec<Lit>& units, int ownerID, bool local = true); 
  /** add a single unit clause to the proof */
  void addUnitToProof( const Lit& unit, int ownerID, bool local = true); 

  /** add the clause directly to the proof-data without writing to FILE, and without a check. Sets the number of occurrences to the given number */
  template <class T>
  void addInputToProof(const T& clause, int numberOfOccurrence = 1); 
  
  /** delete a clause from the proof (if not local, a lock is used before the global proof is updated)
   * @param clause clause that should be added (can be vector or clause)
   * @param ownerID id of the calling thread, for the clause sharing pool the ID has to match the thread number
   * @param local delete the clauses only from the local storage, and do not ubdate the global proof (can be done later, when the next clauses are sent)
   */
  template <class T>
  void delFromProof( const T& clause, const Lit& extraClauseLit, int ownerID, bool local = true); 
  /** delete a single unit clause from the proof */
  void delFromProof( const Lit& unit, int ownerID, bool local = true); 
  
  /** add all clauses from the local proof of a thread to the global proof 
   *  @param ownerID id of the calling thread
   *  @param useExtraLock lock the global pool before updating
   */
  void updateGlobalProof( int ownerID , bool useExtraLock = false);

private:
  
  template <class T>
  unsigned long long getHash (const T& clause, const Lit& extraClauseLit = lit_Undef); // hash function for a single clause, extraLit might be present in clause again
  unsigned long long getHash (const Lit& unit); // hash function for a unit literal (speciallizes the above method)
  
  /** add a unit clause to the proof without locking (locking has been done by the caller) */
  void addUnitToGlobalProof( const Lit& unit );
  
};

inline ProofMaster::ProofMaster(FILE* drupFile, const int nrOfThreads, const int nVars, const int hashTableSize)
: drupProofFile( drupFile )
, threads( nrOfThreads )
, HASHMAX( hashTableSize )
{
  localClauses.resize( nrOfThreads+1 ); // one for each thread, and the last one for the clause sharing pool
  matchArray.create( nVars * 2 ); // one for each literal
}

inline void ProofMaster::addUnitToGlobalProof(const Lit& unit)
{
    unsigned long long thisHash = getHash( unit );
    vector<CRef>& list = hashTable[ thisHash ];
    CRef hashClause = CRef_Undef;
    if( list.size() > 0 ) { // there are clauses that have to be matched
      matchArray.nextStep();
      for( int i = 0 ; i < list.size(); ++ i ) {
	const Clause& c = ca[ list[i] ];
	if( c.size() != 1 || c[0] != unit ) continue; // not the same clause, if the size check fails
	hashClause = list[i]; break;	// store current reference, stop loop
      }
    }
    // if not present, write clause to proof
    if( hashClause == CRef_Undef ) {
      // write to file
      fprintf(drupProofFile, "%i 0\n", (var(unit) + 1) * (-2 * sign(unit) + 1));
      
      // add to clause storage
      tmpCls.clear(); tmpCls.push(unit);
      CRef ref = ca.alloc( tmpCls , false );
      ca[ref].setLBD(1); // the clause is currently present once in the proof
      // add to hash table
      hashTable[thisHash].push_back( ref );
    } else { // else increase the counter of that clause by 1
      ca[ hashClause ].setLBD(ca[ hashClause ].lbd() + 1); // re-use LBD
    }
}


inline void ProofMaster::addUnitToProof(const Lit& unit, int ownerID, bool local)
{
  if( local ) { // simply extend the local pool
    localClauses[ownerID].push_back( unit ); // do not add the extra lit twice!
    localClauses[ownerID].push_back( lit_Undef ); // mark the end of the clause with "lit_Undef"
  } else {
    // lock
    ownLock.lock();
    // add the clause to the global proof
    addUnitToGlobalProof(unit);
    // unlock
    ownLock.unlock();
  }
}


inline void ProofMaster::addUnitsToProof(const vec< Lit >& units, int ownerID, bool local)
{
  if( local ) { // simply extend the local pool
    for( int i = 0 ; i < units.size(); ++ i ) {
      localClauses[ownerID].push_back( units[i] ); // do not add the extra lit twice!
      localClauses[ownerID].push_back( lit_Undef ); // mark the end of the clause with "lit_Undef"
    }
  } else {
    // lock
    ownLock.lock();
    // add the clauses to the global proof
    for( int i = 0 ; i < units.size(); ++ i ) {
      addUnitToGlobalProof(units[i]);
    }
    // unlock
    ownLock.unlock();
  }
}

template <class T>
inline void ProofMaster::addToProof(const T& clause, const Lit& extraClauseLit,  int ownerID, bool local)
{
  if( local ) { // simply extend the local pool
    if( extraClauseLit != lit_Undef ) localClauses[ownerID].push_back( extraClauseLit ); // have the extra literal first!
    for( int i = 0 ; i < clause.size(); ++ i ) 
      if( clause[i] != extraClauseLit ) localClauses[ownerID].push_back( clause[i] ); // do not add the extra lit twice!
    localClauses[ownerID].push_back( lit_Undef ); // mark the end of the clause with "lit_Undef"
  } else { // interesting part, write to global proof
    // lock
    ownLock.lock();
    unsigned long long thisHash = getHash( clause, extraClauseLit );
    vector<CRef>& list = hashTable[ thisHash ];
    CRef hashClause = CRef_Undef;
    if( list.size() > 0 ) { // there are clauses that have to be matched
      matchArray.nextStep();
      int clauseSize = 0 ;
      if( extraClauseLit != lit_Undef ) { clauseSize ++; matchArray.setCurrentStep( toInt ( extraClauseLit ) ); } // mark all literals of the current clause
      for( int i = 0 ; i < clause.size(); ++ i ) {
	if( extraClauseLit != clause[i] ) {
	  clauseSize ++;
	  matchArray.setCurrentStep( toInt ( clause[i] ) ); // mark all literals of the current clause
	}
      }
      for( int i = 0 ; i < list.size(); ++ i ) {
	const Clause& c = ca[ list[i] ];
	if( c.size() != clauseSize ) continue; // not the same clause, if the size check fails
	int j = 0;
	for( ; j < c.size(); ++ j ) { // try to find all literals of the current clause in the matchArray
	  if( ! matchArray.isCurrentStep( toInt(c[j] ) ) ) {
	    break;
	  }
	}
	if( j == c.size() ) { // found all literals -> same clause! (because also same size)
	  hashClause = list[i]; break;	// store current reference, stop loop
	}
      }
    }
    // if not present, write clause to proof
    if( hashClause == CRef_Undef ) {
      // write to file
      if( extraClauseLit != lit_Undef ) fprintf(drupProofFile, "%i ", (var(extraClauseLit) + 1) * (-2 * sign(extraClauseLit) + 1)); // print this literal first (e.g. for DRAT clauses)
      for (int i = 0; i < clause.size(); i++) {
	if( clause[i] == lit_Undef || clause[i] == extraClauseLit ) continue;	// print the remaining literal, if they have not been printed yet
	fprintf(drupProofFile, "%i ", (var(clause[i]) + 1) * (-2 * sign(clause[i]) + 1));
      }
      fprintf(drupProofFile, "0\n");
      
      // add to clause storage
      CRef ref = ca.alloc( clause , false );
      ca[ref].setLBD(1); // the clause is currently present once in the proof
      // add to hash table
      hashTable[thisHash].push_back( ref );
    } else { // else increase the counter of that clause by 1
      ca[ hashClause ].setLBD(ca[ hashClause ].lbd() + 1); // re-use LBD
    }
    // unlock
    ownLock.unlock();
  }
}


template <class T>
inline void ProofMaster::addInputToProof(const T& clause, int numberOfOccurrence)
{
  unsigned long long thisHash = getHash( clause );
  // add to clause storage
  CRef ref = ca.alloc( clause , false );
  ca[ref].setLBD(numberOfOccurrence); // the clause is currently present once in the proof
  // add to hash table
  hashTable[thisHash].push_back( ref );
}


inline void ProofMaster::delFromProof(const Lit& unit, int ownerID, bool local)
{
  if( local ) { // simply extend the local pool
    localClauses[ownerID].push_back( lit_Error ); // define that this clause is to be deleted!
    localClauses[ownerID].push_back( unit );	    // add unit literal
    localClauses[ownerID].push_back( lit_Undef ); // mark the end of the clause with "lit_Undef"
  } else { // interesting part, write to global proof
    // lock
    ownLock.lock();
    
    unsigned long long thisHash = getHash( unit );
    vector<CRef>& list = hashTable[ thisHash ];
    CRef hashClause = CRef_Undef;
    int hashListEntry = -1;
    if( list.size() > 0 ) { // there are clauses that have to be matched
      for( int i = 0 ; i < list.size(); ++ i ) {
	const Clause& c = ca[ list[i] ];
	if( c.size() != 1 || c[0] != unit ) continue; // not the same clause, if the size check fails
	hashListEntry = i; hashClause = list[i]; 	// store current reference
	break;	// stop loop
      }
    }
    // if not present, write clause to proof
    assert( hashClause != CRef_Undef && "a clause that is deleted should be present in the proof" );
    if( hashClause != CRef_Undef ) { // for the safety of the proof do not remove clauses that are not present!
      assert( ca[ hashClause ].lbd() > 0 && "all clauses in the proof should be present at least once" );
      ca[ hashClause ].setLBD(ca[ hashClause ].lbd() - 1); // re-use LBD, decrease presence of clause
      if( ca[ hashClause ].lbd() == 0 ) {
	// write to file
	fprintf(drupProofFile, "d %i 0\n", (var(unit) + 1) * (-2 * sign(unit) + 1));
	
	// remove entry from hashTable (fast, unsorted)
	list[ hashListEntry ] = list[ list.size() - 1 ];
	list.pop_back();
	
	// remove clause from storage TODO: garbage collect? (remove from hash table before garbage collect!)
	ca[ hashClause ].mark(1);
	ca.free(hashClause);
      }
    } else {
      static bool didit = false; // will be printed at most once
      if( !didit ) {
	cerr << "c owner[" << ownerID << "] should delete a unit clause that is not present: " << unit << endl;
	didit = true;
      }
    }

    // unlock
    ownLock.unlock();
  }
}

template <class T>
inline void ProofMaster::delFromProof(const T& clause, const Lit& extraClauseLit, int ownerID, bool local)
{
  if( local ) { // simply extend the local pool
    localClauses[ownerID].push_back( lit_Error ); // define that this clause is to be deleted!
    if( extraClauseLit != lit_Undef ) localClauses[ownerID].push_back( extraClauseLit ); // have the extra literal first!
    for( int i = 0 ; i < clause.size(); ++ i ) 
      if( clause[i] != extraClauseLit ) localClauses[ownerID].push_back( clause[i] ); // do not add the extra lit twice!
    localClauses[ownerID].push_back( lit_Undef ); // mark the end of the clause with "lit_Undef"
  } else { // interesting part, write to global proof
    // lock
    ownLock.lock();
    unsigned long long thisHash = getHash( clause, extraClauseLit );
    vector<CRef>& list = hashTable[ thisHash ];
    CRef hashClause = CRef_Undef;
    int hashListEntry = -1;
    if( list.size() > 0 ) { // there are clauses that have to be matched
      matchArray.nextStep();
      int clauseSize = 0 ;
      if( extraClauseLit != lit_Undef ) { clauseSize ++; matchArray.setCurrentStep( toInt ( extraClauseLit ) ); } // mark all literals of the current clause
      for( int i = 0 ; i < clause.size(); ++ i ) {
	if( extraClauseLit != clause[i] ) {
	  clauseSize ++;
	  matchArray.setCurrentStep( toInt ( clause[i] ) ); // mark all literals of the current clause
	}
      }
      for( int i = 0 ; i < list.size(); ++ i ) {
	const Clause& c = ca[ list[i] ];
	if( c.size() != clauseSize ) continue; // not the same clause, if the size check fails
	int j = 0;
	for(  ; j < c.size(); ++ j ) { // try to find all literals of the current clause in the matchArray
	  if( ! matchArray.isCurrentStep( toInt(c[j] ) ) ) {
	    break;
	  }
	}
	if( j == c.size() ) { // found all literals -> same clause! (because also same size)
	  hashListEntry = i; hashClause = list[i]; break;	// store current reference, stop loop
	}
      }
    }
    // if not present, write clause to proof
    assert( hashClause != CRef_Undef && "a clause that is deleted should be present in the proof" );
    if( hashClause != CRef_Undef ) { // for the safety of the proof do not remove clauses that are not present!
      assert( ca[ hashClause ].lbd() > 0 && "all clauses in the proof should be present at least once" );
      ca[ hashClause ].setLBD(ca[ hashClause ].lbd() - 1); // re-use LBD, decrease presence of clause
      if( ca[ hashClause ].lbd() == 0 ) {
	// write to file
	fprintf(drupProofFile, "d "); // clause should be deleted
	if( extraClauseLit != lit_Undef ) fprintf(drupProofFile, "%i ", (var(extraClauseLit) + 1) * (-2 * sign(extraClauseLit) + 1)); // print this literal first (e.g. for DRAT clauses)
	for (int i = 0; i < clause.size(); i++) {
	  if( clause[i] == lit_Undef || clause[i] == extraClauseLit ) continue;	// print the remaining literal, if they have not been printed yet
	  fprintf(drupProofFile, "%i ", (var(clause[i]) + 1) * (-2 * sign(clause[i]) + 1));
	}
	fprintf(drupProofFile, "0\n");
	
	// remove entry from hashTable (fast, unsorted)
	list[ hashListEntry ] = list[ list.size() - 1 ];
	list.pop_back();
	
	// remove clause from storage TODO: garbage collect? (remove from hash table before garbage collect!)
	ca[ hashClause ].mark(1);
	ca.free(hashClause);	
      }

    } else {
      static bool didit = false; // will be printed at most once
      if( !didit ) {
	cerr << "c owner[" << ownerID << "] should delete a clause that is not present: " << clause << endl;
	didit = true;
      }
    }

    // unlock
    ownLock.unlock();

  }
}


template <class T>
inline unsigned long long ProofMaster::getHash(const T& clause, const Lit& extraClauseLit)
{
    unsigned long long sum = 0, prod = 1, X = 0;
    // process extra lit
    if( extraClauseLit != lit_Undef ) {
      const int l = toInt( extraClauseLit );
      prod *= l; sum += l; X ^= l;
    }
    
    for( int i = 0 ; i < clause.size(); ++ i ) 
    { 
      if( clause[i] != extraClauseLit ) { // but do not process the extraLit twice!
	const int l = toInt( clause[i] );
	prod *= l; sum += l; X ^= l;
      }
    }
    return (1023 * sum + prod ^ (31 * X)) % HASHMAX; 
}

inline long long unsigned int ProofMaster::getHash(const Lit& unit)
{
    unsigned long long sum = 0, prod = 1, X = 0;
    for( int i = 0 ; i < 1; ++ i ) 
    { 
      const int l = toInt( unit );
      prod *= l; sum += l; X ^= l;
    }
    return (1023 * sum + prod ^ (31 * X)) % HASHMAX; 
}


#endif