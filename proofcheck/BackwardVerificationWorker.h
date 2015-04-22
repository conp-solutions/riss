/********************************************************************[BackwardVerificationWorker.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#ifndef SEQUENTIALBACKWARDWORKER_H
#define SEQUENTIALBACKWARDWORKER_H

#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"

#include "proofcheck/BackwardChecker.h"

#include <deque> // to have a queue for LIFO and FIFO

namespace Riss {

/** given an internal representation of a DRAT/DRUP proof, this class performs backward checking */
class BackwardVerificationWorker
{
public:
  
  enum ParallelMode {
    none = 0,    // sequential execution
    shared = 1,  // the clause data base (and the full occurrence information) is shared for read only access
    copy = 2,    // each worker has his (private) copy of the data structures
  };
  
protected:
  
  bool drat, fullDrat;          // check drat, and check drat on all literals of the clause
  ParallelMode  parallelMode;   // how to operate on the data
  int sequentialLimit;          // stop, if there is enough sequential work that could be parallelized (inactive, if == 0)
  int verbose;                  // verbosity of the tool
  
  // data structures that are re-used from the outside
  int formulaClauses;                          /// number of clauses in the formula (these clause do not have to be verified)
  vec<BackwardChecker::ClauseLabel>& label;    /// stores the label per clause in the formula and proof
  vec<BackwardChecker::ClauseData>& fullProof; /// store the data of the proof (including all clauses of the formula)
  ClauseAllocator  ca;                         /// storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  ClauseAllocator& originalClauseData;         /// storage for the literals of the clauses, gives all clauses an additional field so that the first literal can be stored redundantly once more
  bool workOnCopy;                             /// indicate whether we are really working on the storage, or on a copy
  std::deque< int64_t > idsToVerify;           /// queue of elements that have to be verified by this worker
  
  // data structures for propagation
  
  /** necessary object for watch list */
  
  struct ClauseDataClauseDeleted
  {
    const ClauseAllocator& ca;
    ClauseDataClauseDeleted(const ClauseAllocator& _ca) : ca(_ca) {}
    bool operator()(const BackwardChecker::ClauseData& w) const { return ca[ w.getRef() ].mark() == 1; }
  };
  
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> nonMarkedWatches;  /// watch list for all non-marked clauses
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> markedWatches;     /// watch list for all marked clauses
  OccLists<Lit, vec<BackwardChecker::ClauseData>, ClauseDataClauseDeleted> fullWatch;         /// full occurrence list fullWatch[l] gives the vector with all (currently active) proofitems that contain literal l
  int variables;                                                                              /// number of variables in the proof (highest variable)

  int      lastPosition;                      // next position in the proof that has to be verified
  int      minimalMarkedProofItem;            // minimal element in the proof that has been marked by this worker
  int      markedQhead, markedUnitHead;       // Head of queue,as index into the trail, also for units
  int      nonMarkedQHead, nonMarkedUnithead; // one for marked propagation, one for non-marked propagation

  vec<Lit> trail; // Assignment stack; stores all assigments made in the order they were made.
  vec<BackwardChecker::ClauseData> nonMarkedUnitClauses;  // vector of top level units (not yet marked)
  vec<BackwardChecker::ClauseData> markedUnitClauses;     // vector of top level units (marked)
  
  vec<lbool> assigns;     // current assignment, one space for each variable
  vec<Lit> resolventLits; // literals that are added to the resolvent wrt. the old clause
  
  /** private properties per proof item */
  class ProofItemProperty { 
    unsigned movedToMark : 1; // indicate that this thread marked a certain clause during propagation on a watch alredy
    unsigned markedSelf  : 1; // indicate that this thread marked a certain clause
    unsigned dummy       : 6;
  public:
    ProofItemProperty () : movedToMark(0), markedSelf(0), dummy(0) {}
    void moved() { movedToMark = 1; }
    bool isMoved() const { return movedToMark; }
    void markedByMe()   { markedSelf = 1; }
    bool isMarkedByMe() const { return markedSelf; }
    void resetMarked() { markedSelf = 0; }
  };
  
  vec<ProofItemProperty> proofItemProperties;  // TODO: could be merged with the vector below
  
  /** external watched literals for propagating clauses 
   *  stores the XOR of the two literals, as one literal is always known 
   *  space saving variant
   * TODO: could be merged with the above class!
   */
  class WatchedLiterals {
    Lit data; // xor of two watched literals
  public:
    WatchedLiterals () : data( toLit(0) ) {}
    /** initialize the data with the XOR of the two literals */
    WatchedLiterals( const Lit& l1, const Lit& l2 ) : data( toLit( toInt(l1) ^ toInt(l2) )  ) {}
    /** get the other literal by using XOR with the given literal */
    Lit getOther( const Lit& l1 ) const { return toLit( toInt(l1) ^ toInt(data) ); }
    /** replace the literal from with the literal to, update by using XOR twice */
    void update( const Lit& from, const Lit& to ) { data = toLit( toInt(data) ^ toInt(from) ^ toInt(to) ); }
    /** initialize with the two given literals */
     void init( const Lit& l1, const Lit& l2 ) { data = toLit( toInt(l1) ^ toInt(l2) ); }
  };
  vec<WatchedLiterals> watchedXORLiterals; /// vector that stores the current two watched literals for all the clauses in the current watch lists
  
public:
  
  // data and statistics
  int clausesToBeChecked;    // number of marked clauses in the proof
  int maxClausesToBeChecked; // maximum of the above number during one verification
  int num_props;             // number of propagated literals
  int verifiedClauses;       // number of verified clauses
  int ratChecks;             // counts how often AT checkes failed
  int usedOthersMark;        // counts how often we used clauses that have been marked by other threads/workers
  
  
  /** setup checker, use data structures that have been set up before from the outside 
   *  @param keepOriginalClauses the internal representation removes duplicate literals, hence the checker would copy the whole clause data base
   */
  BackwardVerificationWorker( bool opt_drat, 
			    ClauseAllocator& outer_ca, 
			    vec<BackwardChecker::ClauseData>& outer_fullProof, 
			    vec<BackwardChecker::ClauseLabel>& outer_label, 
			    int outer_formulaClauses, 
			    int outer_variables, 
			    bool keepOriginalClauses );
    
  /** try each literal of a clause as DRAT literal (might mark clauses unncessarily) */
  void setFullDrat() { fullDrat = true; }
  
  /** tell worker how to hande shared data structures 
   *  if object has been initialized already in copy mode and is turned into shared mode, data structures are updated
   */
  void setParallelMode(ParallelMode mode);
  
  /** return another worker such that the two workers together would verifiy the whole proof 
   *  sets the state such that both workers can "simply" continue verification
   * @return pointer to the other worker object (has to be deleted by the caller)
   */
  BackwardVerificationWorker* splitWork();
  
  /** continue the verification process from the last interupted position (or after splitWork)
   * @param untilProofPosition perform partial check until the specified position (when not using chronological, this might leave unverified clauses behind)
   * @return l_True, if the private part of this workers proof can be verified, l_False if the verification failed, l_Undef, if the verification is interupted
   */
  lbool continueCheck(int64_t untilProofPosition = 0);
  
  /** setup the internal data structures before performing verification 
   *  Note: might work on the outer clause allocator (that has been passed to the constructor before)
   *  @param proofPosition position in the proof from which on the current object should make its checks
   *  @param duplicateFreeClauses indicate whether checking for these clauses is relevant 
   */
  void initialize(int64_t proofPosition, bool duplicateFreeClauses = false);
  
  /** set sequential work limit, so that verification work can be split */
  void setSequentialLimit( int limit ) { sequentialLimit = limit > 0 ? limit : 0; }
  
  /** return value of limit */
  int getSequentialLimit() const { return sequentialLimit; }
  
  /** indicate whether the given item has been marked by this thread 
   * @return if the item has been marked by this thread
   */
  bool markedItem( const int itemID );
  
  /** check whether the given clause is in the proof 
   * @param clause clause to be verified
   * @param untilProofPosition perform partial check until the specified position (when not using chronological, this might leave unverified clauses behind)
   * @param returnAfterInitialCheck check only the given clause - if its check succeeds, return true
   * @return l_True, if the proof is a valid unsatisfiability proof (or the given claus can be added to the current proof), l_Undef if interupted by sequentialLimit, l_False if failure
   */
  lbool checkClause( vec<Lit>& clause, int64_t untilProofPosition = 0, bool returnAfterInitialCheck = false);
  
  /** check whether the proof is valid for the empty clause 
   * @param untilProofPosition perform partial check until the specified position
   * @return true, if the proof is a valid unsatisfiability proof
   */
  lbool checkClause(int64_t untilProofPosition = -1){ vec<Lit> dummy; return checkClause( dummy, untilProofPosition ); }
  
  /** correct changes, outer usage of clause allocation object becomes safe again */
  void release();
  
protected: 

  /** add the clause to the watch lists of the checker
   * @param cData proof item data of the current clause (including time steps of being valid)
   */
  void attachClauseNonMarked( const BackwardChecker::ClauseData& cData);
  
    /** add the clause to the watch lists of with marked clauses
   * @param cData proof item data of the current clause (including time steps of being valid)
   */
  void attachClauseMarked( const BackwardChecker::ClauseData& cData);
  
  /** add clause related to given proofItem to the active data  structures
   */
  void enableProofItem( const BackwardChecker::ClauseData& proofItem );
  
  /** add a literal to the trail and the assignment structure 
   * Note: does not check for a conflict
   */
  void uncheckedEnqueue(Lit p);
  
  /** The current value of a variable. */
  lbool value (Var x) const;     
  
  /** The current value of a literal. */
  lbool value (Lit p) const;    
  
  /** clear the full assignment information */
  void restart();
  
  /** remove all literals behind a given position */
#warning USE FOR DRAT and checking multiple resolvents
  void removeBehind( const int& position );

  /** modify all data structures so that its known that this clause has to be marked 
   * @return true, if this worker marked the clause, false, if already marked before
   */
  bool markToBeVerified( const int64_t& proofItemID );

  /** propagate only on valid already marked unit clauses
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateMarkedUnits(const int64_t currentID);
  
  /** propagate only on valid already marked clauses and unit clauses
   * Note: uses @see propagateMarkedUnits to propagate top level units
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateMarked(const int64_t currentID);

  /** propagate only on valid non-marked unit clauses
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUnmarkedUnits(const int64_t currentID);
  
  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued -- first will be used
   * Note: uses @see propagateUnmarkedUnits to propagate top level units
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueueEager(const int64_t currentID);

  
  /** propagate only on valid already marked clauses and unit clauses
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateMarkedShared(const int64_t currentID);

  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued -- first will be used
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueueEagerShared(const int64_t currentID);
  
  
  
  /** propagate only on non-marked clauses and unit clauses until the next literal can be enqueued -- all are collected and a decision is made afterwards
   * @param currentID id of the element that is checked. any element higher in the data structures will be deleted. Note: works only for backward checking
   * @return CRef_Undef, if no conflict was found, the id of the conflict clause otherwise
   */
  CRef propagateUntilFirstUnmarkedEnqueueManaged(const int64_t currentID);
  
  /** check AT of the given clause with respect to the partial proof until element currentID
   * @param currentID the position of that clause in the proof
   * @param c the clause to be checked
   * @param extraLits literals that belong to the clause (e.g. after resolution)
   * @param reuseClause do not re-enqueue the literals of c again, as it is ensured that those literals have already been tested
   * @return true, if the verification of this clause is ok
   */
  template<class ClauseType>
  bool checkSingleClauseAT( const int64_t currentID, const ClauseType* c, vec<Lit>& extraLits, bool reuseClause = false );
  
  /** check RAT of the given clause with respect to the partial proof until element currentID
   * Note: call this method only after the AT check failed
   * @param currentID the position of that clause in the proof
   * @param c the clause to be checked
   * @param extraLits literals that belong to the clause (e.g. after resolution)
   * @param reuseClause do not re-enqueue the literals of c again, as it is ensured that those literals have already been tested
   * @return true, if the verification of this clause is ok
   */
  bool checkSingleClauseRAT( const int64_t currentID, vec< Lit >& lits, const Clause* c = 0);

};


// 
// ================================================================================================
// Source code of methods that use template arguments
// 

template<class ClauseType>
bool BackwardVerificationWorker::checkSingleClauseAT(const int64_t currentID, const ClauseType* c, vec< Lit >& extraLits, bool reuseClause )
{
  assert( (reuseClause || trail.size() == 0) && "there cannot be assigned literals, other than the reused literals" );
  nonMarkedQHead = 0; // there might have been more marked clauses in between, so start over
  markedUnitHead = 0;
  
  if( verbose > 2 && c != 0 ) cerr << endl << endl << "c [S-BW-CHK] check AT for clause " << *c << " " << extraLits << " at proof position " << currentID << " (reuse: " << reuseClause << ")" << endl;
  
  if( !reuseClause && c != 0 ) {
    for( int i = 0 ; i < c->size(); ++ i ) { // enqueue all complements
      const Clause& clause = *c;
      if( value( ~clause[i] ) == l_False ) return true;                 // there is a conflict
      else if( value( ~clause[i] ) == l_Undef ) uncheckedEnqueue( ~clause[i] );   
    }
  }
  
  for( int i = 0 ; i < extraLits.size() ; ++ i ) {
    if( value( ~extraLits[i] ) == l_False ) return true;                 // there is a conflict
    else if( value( ~extraLits[i] ) == l_Undef ) uncheckedEnqueue( ~extraLits[i] );   
  }
  
  CRef confl = CRef_Undef;
  do {
    
    if( verbose > 4 ) {
      cerr << "c trail: " << trail.size() << " markedUnits: " << markedUnitClauses.size() << " non-markedUnits: " << nonMarkedUnitClauses.size() << endl;
      cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
      cerr << "c trail literals: " << trail << endl;
    }
    
    // propagate on all marked clauses we already collected
    confl = propagateMarked( currentID );
    assert( (confl != CRef_Undef || markedQhead == trail.size()) && "visitted all literals during propagation" );
    assert( (confl != CRef_Undef || markedUnitHead >= markedUnitClauses.size()) && "visitted all marked unit clauses" );
    // if we found a conflict, stop
    if( confl != CRef_Undef ) break;
    // end propagate on marked clauses
    
    if( verbose > 4 ) {
      cerr << "c trail: " << trail.size() << " markedUnits: " << markedUnitClauses.size() << " non-markedUnits: " << nonMarkedUnitClauses.size() << endl;
      cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl;
      cerr << "c trail literals: " << trail << endl;
    }
    
    // if there is not yet a conflict, enqueue the first non-marked unit clause
    // will move clauses from non-marked to marked, but this is ok, as we will backtrack after this call has finished
    confl = propagateUntilFirstUnmarkedEnqueueEager( currentID );
    // if we found a conflict, stop
    if( confl != CRef_Undef ) break;
    
  } while ( nonMarkedQHead < trail.size() || nonMarkedUnithead < nonMarkedUnitClauses.size() || markedQhead < trail.size() ); // saw all clauses that might be interesting

  if( verbose > 2 ) cerr << "c [S-BW-CHK] returned conflict reference: " << confl << endl;
  
  if( confl == CRef_Undef && verbose > 5 ) {
    cerr << "c [S-BW-CHK] no conflict" << endl;
    cerr << "c trail: " << trail.size() << " markedUnits: " << markedUnitClauses.size() << " non-markedUnits: " << nonMarkedUnitClauses.size() << endl;
    cerr << "c HEAD: markedUnit: " << markedUnitHead << " marked: " << markedQhead << " non-markedUnits: " << nonMarkedUnithead << " non-marked: " << nonMarkedQHead << endl; 
  }
  
  // return true, if there was a conflict (then the clause has AT)
  return confl != CRef_Undef;
}

inline
bool BackwardVerificationWorker::checkSingleClauseRAT(const int64_t currentID, vec< Lit >& lits, const Clause* c)
{
  // we come here after executing the call 
  //    checkSingleClauseRAT( currentID, c, dummy ) without performing a restart afterwards, dummy is empty
  
  const int oldTrailSize = trail.size();
  
  // the empty clause cannot have RAT, if AT already failed, because we cannot resolve
  if( lits.size() == 0 && ( c==0 || c->size() == 0 ) ) return false;
  
  // get the first literal
  const Lit firstLiteralComplement = ( c == 0 ) ? ~lits[0] : ~c->getExtraLiteral();
  const bool isUnitClause = lits.size() == 1;
  
  if( verbose > 2 ) cerr << "c [S-BW-CHK] RAT check on firstLiteral: " << ~firstLiteralComplement << " with ID " << currentID << endl;
  
  assert( !fullDrat && "full DRAT checking is not supported yet" );
  
  bool succeeds = true;
  int keptFullWatchEntries = 0;
  int currentItem = 0;
  
  /* built trail for optimized RAT check */
  restart();
  assert( c != 0 && "this method sould only be executed on clauses" );
  assert( lits.size() == 0 && "this method should be called without extra literals" );
  if( c != 0 ) {
    const Clause& clause = *c;
    const Lit& firstLit = c->getExtraLiteral();
    for( int i = 0 ; i < clause.size(); ++ i ) {                                // enqueue all complements
      if( clause[i] == firstLit ) continue;                                     // jump over first literal of the clause
      if( value( ~clause[i] ) == l_False ) return true;                         // there is a conflict
      else if( value( ~clause[i] ) == l_Undef ) uncheckedEnqueue( ~clause[i] );   
    }
  }
  const int beforeTrailSize = trail.size();
  
  for( ; succeeds && currentItem < fullWatch[ firstLiteralComplement ].size(); ++ currentItem ) {
    BackwardChecker::ClauseData& item = fullWatch[ firstLiteralComplement ][currentItem];
    assert( !item.isEmptyClause() && "cannot resolve with the empty clause" );
    assert( !item.isDelete()      && "delete information should not be in the full watch list" );
    
    // work only with valid items
    if( item.isValidAt( currentID - 1 ) ) {
      assert( item.getID() < currentID && "can use only clauses with a smaller index for DRAT checks" );
      
      // mark used clause to be verified as well
      markToBeVerified( item.getID() );
      
      if( verbose > 4 ) cerr << "c [S-BW-CHK] resolve with clause with ID " << item.getID() << endl;
      
      fullWatch[ firstLiteralComplement ][ keptFullWatchEntries++ ] = fullWatch[ firstLiteralComplement ][currentItem];
      // build resolvent wrt the current assignment
      resolventLits.clear();
      const Clause& d = ca[ item.getRef() ];
      
      if( verbose > 6 ) cerr << "c [S-BW-CHK] consider clause with " << item.getID() << " for resolution: " << d << endl;
      
      // the current assignment is build as follows: J = ~c P, where P are the literals that follow from ~c wrt F
      // the resolvent from c and d is ~c,~d (without the common literal firstLiteralComplement)
      // the AT check is done for J' = ~c, ~d, P, Q, where Q is implied by ~c and ~d together
      // literals in d are used for the resolvent, if they are still unassigned
      // literals in d, that are false, are true in ~d, and hence can be ignored as well
      // literals in d, that are true, are false in ~d, and hence indicate a tautologic clause, or that AT of the resolvent would fail
      bool resolventIsAT = false;     // if the resolvent is a tautology, the check is ok and we continue with the next clause immediately
      for( int j = 0 ; j < d.size(); ++ j ) {
	if( d[j] == firstLiteralComplement ) continue;
	if( value( d[j] ) == l_True ) { resolventIsAT = true; break; }
	else if ( value( d[j] ) == l_Undef ) {
	  resolventLits.push( d[j] );
	} else {
	  // the literal is not added to the resolvent, and hence silently ignored 
	}
      }
      if( resolventIsAT ) {
	if( verbose > 5 ) cerr << "c [S-BW-CHK] resolvent is a tautology" << endl;
	continue; // the AT check of the resolvent would succeed
      }
      
      // check AT of the resolvent - by reusing the literals that have been used in the AT check for c
      if( c != 0 ) cerr << "c perform RAT check for clause " << *c << " with resolvent lits " << resolventLits << " and trail: " << trail << endl;
      if( ! checkSingleClauseAT(currentID, c, resolventLits, true ) ) {  
	succeeds = false;
	if( verbose > 5 ) cerr << "c [S-BW-CHK] AT of resolvent failed" << endl;
	break;
      }
      removeBehind( beforeTrailSize );
    } else {
      // implicitely remove the current element
      if( verbose > 6 ) cerr << "c [S-BW-CHK] remove element with ID " << item.getID() << " from list" << endl;
    }
  }
  
  // move the other valid items and remove the invalid items
  for( currentItem = currentItem  + 1; currentItem < fullWatch[ firstLiteralComplement ].size(); ++ currentItem ) { // will enter only if check failed
    BackwardChecker::ClauseData& item = fullWatch[ firstLiteralComplement ][currentItem];
    if( item.isValidAt( currentID - 1) ) fullWatch[ firstLiteralComplement ][ keptFullWatchEntries++ ] = fullWatch[ firstLiteralComplement ][currentItem];
  }
  fullWatch[ firstLiteralComplement ].shrink_( fullWatch[ firstLiteralComplement ].size() - keptFullWatchEntries ); // remove invalid elements
  
  if( verbose > 6 ) cerr << "c [S-BW-CHK] RAT check succeeds: " << succeeds << endl;
  
  return succeeds;
}


}

#endif // SEQUENTIALBACKWARDWORKER_H
