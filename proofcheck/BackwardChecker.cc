/******************************************************************************[BackwardChecker.cc]
Copyright (c) 2015, All rights reserved, Norbert Manthey
**************************************************************************************************/

#include "proofcheck/BackwardChecker.h"

#ifdef ONEWATCHLISTCHECKER
  #include "proofcheck/BackwardVerificationOneWatch.h"
#else
  #include "proofcheck/BackwardVerificationWorker.h"
#endif

#include "riss/mtl/Sort.h"
#include "riss/utils/ThreadController.h"
#include "riss/utils/Options.h"

#include <fstream>
#include <sstream>

using namespace Riss;
using namespace std;

static IntOption  opt_splitLoad      ("BACKWARD-CHECK", "splitLoad",          "number of clauses in queue before splitting", 8, IntRange(2, INT32_MAX));
static IntOption  opt_verbose        ("BACKWARD-CHECK", "bwc-verbose",        "verbosity level of the checker", 0, IntRange(0, 8));
static BoolOption opt_statistics     ("BACKWARD-CHECK", "bwc-stats",          "print statistics about backward verification", true);
static BoolOption opt_minimalCore    ("BACKWARD-CHECK", "bwc-min-core",       "try to use as few formula clauses as possible", true);
static BoolOption opt_space_saving   ("BACKWARD-CHECK", "bwc-space-saving",   "read only access on shared data strucutres (slower)", false);
static IntOption  opt_duplicate_cls  ("BACKWARD-CHECK", "bwc-dup-cls",        "handle duplicate clauses  (0=off,1=check,2=merge)",  0, IntRange(0, 2));
static IntOption  opt_duplicate_lits ("BACKWARD-CHECK", "bwc-dup-lits",       "handle duplicate literals (0=off,1=check,2=delete)", 0, IntRange(0, 2));
static BoolOption opt_pseudoParallel ("BACKWARD-CHECK", "bwc-pseudoParallel", "reduce the number of actual workers to 1", false);
static BoolOption opt_strictProof    ("BACKWARD-CHECK", "bwc-strict",         "be strict with the proof (false=ignore wrong deletes)", true);
static BoolOption opt_dependencyAna  ("BACKWARD-CHECK", "bwc-depenencyAna",   "analyze clause dependencies (e.g. for trace-check)", false);
static BoolOption opt_loadbalancing  ("BACKWARD-CHECK", "bwc-balance",        "re-assign idle threads (split workload recursively)", false);

static StringOption opt_coreFile   ("BACKWARD-CHECK", "cores",  "Write unsatisfiable sub formula into this file",0);
static StringOption opt_proofFile  ("BACKWARD-CHECK", "lemmas", "Write relevant part of proof in this file",0);


BackwardChecker::BackwardChecker(bool opt_drat, int opt_threads, bool opt_fullRAT)
:
proofWidth(-1), 
proofLength(-1), 
unsatisfiableCore(-1),
formulaClauses( 0 ),
// oneWatch ( ClauseHashDeleted() ), // might fail in compilation depending on which features of the object are used
drat( opt_drat ),
fullRAT( opt_fullRAT ),
threads( opt_threads ),
checkDuplicateLits (opt_duplicate_lits),
verbose(opt_verbose), // highest possible for development
removedInvalidElements (0),
checkDuplicateClauses (opt_duplicate_cls),
variables(0),
hasBeenInterupted( false ),
readsFormula( true ),
currentID(0),
sawEmptyClause( false ),
formulaContainsEmptyClause( false ),
inputMode( true ),
invalidProof( false ),
duplicateClauses(0), 
clausesWithDuplicateLiterals(0),
mergedElement(0),
loadUnbalanced(0)
{
  ca.extra_clause_field = drat; // if we do drat verification, we want to remember the first literal in the extra field
  if( verbose > 0 ) cerr << "c [BW-CHK] create backward checker with " << threads << " threads, and drat: " << drat << endl;
}

void BackwardChecker::interupt()
{
  hasBeenInterupted = true;
#warning FORWARD INTERUPT TO VERIFICATION WORKER OBJECTS
}

void BackwardChecker::setDRUPproof()
{
  assert( sequentialChecker == 0 && "change format only if we currently have no checker present" );
  if( drat && verbose > 0 ) cerr << "c [BW-CHK] change format from DRAT to DRUP based on proof" << endl;
  drat = false;
}


int BackwardChecker::newVar()
{
  const int v = variables;  
//   oneWatch.init(mkLit(variables, false)); // get space in onewatch structure, if we are still parsing
//   oneWatch.init(mkLit(variables, true ));
  variables ++;
  return v;
}

void BackwardChecker::reserveVars(int newVariables)
{
  variables = newVariables;
//   oneWatch.init(mkLit(newVariables, false));
//   oneWatch.init(mkLit(newVariables, true ));
}

bool BackwardChecker::addProofClause(vec< Lit >& ps, bool proofClause, bool isDelete)
{
  if( sawEmptyClause ) return true;
  
  // check the state
  if( !inputMode ) {
    cerr << "c WARNING: tried to add clauses after input mode has been deactivated already" << endl;
    invalidProof = true;
    return false;
  }
  
  if( !readsFormula && !proofClause ) {
    invalidProof = true;
    return false; // if we added proof clauses already, no more formula clauses can be added
  }
  if( readsFormula && proofClause ) formulaClauses = fullProof.size(); // memorize the number of clauses in the formula
  readsFormula = readsFormula && !proofClause;     // memorize that we saw a proof clause
  
  if( verbose > 2 ) cerr << "c [BW-CHK] add clause " << ps << " toProof?" << proofClause << " as delete?" << isDelete << endl;
  
  const int64_t id = currentID; // id of the current clause

  if( ps.size() == 0 ) {
    fullProof.push( ProofClauseData( CRef_Undef, id ) );
    currentID ++;
    sawEmptyClause = true;
    if( verbose > 4 ) cerr << "c [BW-CHK] parsed the empty clause" << endl;
    if( readsFormula ) formulaContainsEmptyClause = true;
    return true;
  }
  
  Lit minLit = ps[0];
  bool hasDuplicateLiterals = false; // stats to be counted in the object
  bool foundDuplicateClause = false; // stats to be counted in the object
  bool isTautology = false;          // stats to be counted in the object

  // memorize the first literal
  const Lit firstLiteral = ps.size() > 0 ? ps[0] : lit_Error;
  // sort literals to perform merge sort
  sort( ps );
  uint64_t hash = 0;
  // find minimal literal
  for( int i = 0 ; i < ps.size(); ++ i ){
    assert( var( ps[i] ) < variables && "variables must be in the range" );
    minLit = ps[i] < minLit ? ps[i] : minLit;
    hash |= (1 << (toInt(minLit) & 63)); // update hash
  }

  // duplicate analysis
  if( checkDuplicateLits > 0  ) {
    int litsToKeep = 1;
    // make a list of how many literals are present of which type
    for( int i = 1 ; i < ps.size(); ++ i ) {
      if( ps[i] == ~ps[litsToKeep] ) isTautology = true;
      if( ps[i] != ps[litsToKeep] ) {
	ps[++litsToKeep] = ps[i];
      } else {
	if( checkDuplicateLits == 1 ) {
	  ps[litsToKeep++] = ps[i];
	} else {
	  // implicitely remove this literal 
	  hasDuplicateLiterals = true;
	}
      }
    }
    if( verbose > 5 && litsToKeep != ps.size() ) cerr << "c [BW-CHK] remove " << ps.size() - litsToKeep << " literals from the clause" << endl;
    ps.shrink_( ps.size() - litsToKeep );   // remove the literals that occured multiple times
    
    if( hasDuplicateLiterals ) {
      if( verbose > 0 ) cerr << "c WARNING: clause " << ps << " has/had duplicate literals" << endl;
    }
  }
  
  // scan for duplicate clauses, or the first clause that could be deleted
  int clausePosition = -1;  // position of the clause in the occurrence list of minLit
  int bucketHash = -1;      // bucket of the hash map
  if( (checkDuplicateClauses != 0 || isDelete) && oneWatchMap.elements() > 0 ) { // enter only, if there are already elements
    ClauseHashHashFunction hasher; 
    bucketHash = hasher.operator()( ClauseHash ( ProofClauseData( CRef_Undef, id ), hash, ps.size(), minLit ) );
    
    vec< Map< ClauseHash, EmptyData, ClauseHashHashFunction >::Pair >& hasBucket = oneWatchMap.getBucket( bucketHash ); // in the dummy, the ref is not important
    if( verbose > 4 ) cerr << "c [BW-CHK] compare with minLit " << minLit << " against " << hasBucket.size() << " clauses" << endl;
    
    for( int i = 0 ; i < hasBucket.size(); ++ i ) {
      if( verbose > 5 ) cerr << "c [BW-CHK] compare to clause-ref " << hasBucket[i].key.cd.getRef() << " with id " << hasBucket[i].key.cd.getID() << " clause: " << ca[ hasBucket[i].key.cd.getRef() ] << endl;
      
      // continue, if hash and size do not match (no need to get the clause into the cache again ...
      if( ps.size() != hasBucket[i].key.size || hash != hasBucket[i].key.hash) continue;
      
      const Clause& clause = ca[ hasBucket[i].key.cd.getRef() ];

      bool matches = true;

      // merge compare routine
      for( int j = 0 ; j < ps.size(); ++j ) {
	assert( (j == 0 || ps[j-1] <= ps[j]) && "added clause has to be sorted" );
	assert( (j == 0 || clause[j-1] <= clause[j]) && "database clause has to be sorted" );
	if( ps[j] != clause[j] ) { 
	  if( verbose > 6 ) cerr << "c [BW-CHK] did not find match for literal " << ps[j] << endl;
	  matches = false; 
	  break;
	}
      }
      // found a clause we are looking for
      if( matches ) {
	clausePosition = i;
	foundDuplicateClause = true;
	if( verbose > 6 ) cerr << "c [BW-CHK] found match, store position " << clausePosition << " position: " << i << endl;
	// do not look for another match
	break;
      }
    }
    
    if( checkDuplicateClauses != 0 ) { // optimize most relevant execution path
      if( foundDuplicateClause && checkDuplicateClauses == 1 ) {
	cerr << "c WARNING: clause " << ps << " occurs multiple times in the proof, e.g. at position " << hasBucket[ clausePosition ].key.cd.getID() << " of the full proof" << endl;
	#warning add a verbosity option to the object, so that this information is only shown in higher verbosities
      }
    }
  }
  
  // tautologies are ignored, if duplicate literals should be handled properly
  if( isTautology && checkDuplicateLits == 2 ) {
    return true; // tautology is always fine
  }
  
  // handle the data and add current clause to the proof
  if ( isDelete ) {
    // did we find the corresponding clause?
    if( clausePosition == -1 ) {
      cerr << "c WARNING: clause " << ps << " that should be deleted at position " << id << " did not find a maching partner" << endl;
      invalidProof = true;
      return false;
    }
    
    assert( clausePosition >= 0 && "the clause that should be deleted has to exist in the proof" );
    
    bool hasToDelete = true;  // indicate that the current clause has to be delete (might not be the case due to merging clauses)
    // find the corresponding clause, add the deletion information, delete this clause from the current occurrence list (as its not valid any more)
    if( checkDuplicateClauses == 2 ) {
      const int otherID = oneWatchMap.getBucket( bucketHash )[ clausePosition ].key.cd.getID();
      clauseCount.growTo( otherID + 1 );
      if( clauseCount[ otherID ] > 0 ) {
	clauseCount[ otherID ] --;
	hasToDelete = false;
      }
    }
    
    if( hasToDelete ) { // either duplicates are not merged, or the last occuring clause has just been removed
      const int otherID = oneWatchMap.getBucket( bucketHash )[ clausePosition ].key.cd.getID();
      ProofClauseData& cdata = fullProof[ otherID ];
      assert( cdata.getID() == otherID && "make sure we are working with the correct object (clause)" );
      assert( cdata.isValidAt( id )  && "until here this clause should be valid" );
      cdata.setInvalidation( id );      // tell the clause that it is no longer valid
      assert( !cdata.isValidAt( id ) && "now the clause should be invalid" );
      
      oneWatchMap.getBucket( bucketHash )[ clausePosition ] = oneWatchMap.getBucket( bucketHash )[ oneWatchMap.getBucket( bucketHash ).size() - 1 ]; // remove element from oneWatch
      oneWatchMap.getBucket( bucketHash ).shrink_(1);  // fast version of shrink (do not call descructor)
      oneWatchMap.removedElementsExternally(1);     // tell map that we removed one object
      
      // add to proof an element that represents this deletion
      fullProof.push( ProofClauseData( cdata.getRef(), otherID ) ); // store the ID of the clause that is enabled when running backward over this item
      fullProof[ fullProof.size() - 1 ].setIsDelete(); // tell the item that its an deletion item
    }
    
  } else {
    if( foundDuplicateClause ) duplicateClauses ++;             // count statistics
    if( hasDuplicateLiterals ) clausesWithDuplicateLiterals ++; // count statistics
    if( ps.size() == 0 ) {
      sawEmptyClause = true;
      if( readsFormula ) formulaContainsEmptyClause = true;
      fullProof.push( ProofClauseData() );
      fullProof[ fullProof.size() -1 ].setIsEmpty();
    } else {
      if( !foundDuplicateClause || checkDuplicateClauses != 2 ) { // only add the clause, if we do not merge duplicate clauses
	assert( ps.size() > 0 && "should not add the empty clause again to the proof" ); // once seeing the empty clause should actually stop adding further clauses to the proof
	const CRef cref = ca.alloc( ps );                         // allocate clause
	if( drat ) ca[ cref ].setExtraLiteral( firstLiteral );    // memorize first literal of the clause (if DRAT should be checked on the first literal only)
// 	oneWatch[minLit].push( ClauseHash ( ProofClauseData( cref, id ), hash, ps.size() ) );          // add clause to structure so that it can be checked

	oneWatchMap.insert( ClauseHash ( ProofClauseData( cref, id ), hash, ps.size(), minLit ), EmptyData() ); // add element to hash map
	assert( oneWatchMap.elements() > 0 && "there has to be something in the map" );
	
	if( verbose > 2 ) cerr << "c [BW-CHK] add clause " << ca[cref] << " with id " << id << " ref " << cref << " and minLit " << minLit << endl;
	
	fullProof.push( ProofClauseData( cref, id ) );                 // add clause data to the proof (formula)
      } else {
	clauseCount.growTo( oneWatchMap.getBucket( bucketHash )[ clausePosition ].key.cd.getID() + 1 );  // make sure the storage for the other ID exists
	clauseCount[ oneWatchMap.getBucket( bucketHash )[ clausePosition ].key.cd.getID() + 1 ] ++;      // memorize that this clause has been seen one time more now 
      }
    }
  }
  
  // update ID, if something has been added to the proof
  assert( (currentID == fullProof.size() || currentID + 1 == fullProof.size() ) && "the change can be at most one element" );
  if( fullProof.size() > currentID ) {
    currentID ++;
    if( currentID == ((1ull << 32) -1 ) ) {
      cerr << "c WARNING: handling very large proof (" << currentID << ")" << endl;
    }
    if( currentID == ((1ull << 48) -2 ) ) {
      cerr << "c WARNING: handling very very large proof (" << currentID << ")" << endl;
    }
  } else {
    mergedElement ++; // count statistics
  }
  

  return true;
}


void BackwardChecker::printStatistics(std::ostream& s) {
  if( !opt_statistics ) return; // do not print, if disabled

  // make sure we do not divide by 0
  const double cpuT = verificationClock.getCpuTime() == 0 ? 0.000001 : verificationClock.getCpuTime();
  const double wallT = verificationClock.getWallClockTime() == 0 ? 0.000001 : verificationClock.getWallClockTime();
  
  s << "c ======== VERIFICATION STATISTICS - OVERVIEW ========" << endl;
  
  s << "c verification time: cpu: " << cpuT << " wall: " << wallT << endl;
  s << "c proof: length: " << proofLength << " width: " << proofWidth << " core: " << unsatisfiableCore << endl;
  
  if( threads > 1 ) {
    s << "c ======== PARALLEL VERIFICATION STATISTICS ========" << endl;
    s << "c threads: " << threads << " speedup: " << cpuT / wallT << " efficiency: " << cpuT / ( wallT * threads ) << endl;
    s << "c ID checks RATchecks propagatedLits maxTodo usedOtherMarked reuseVerify resolutions redLits redProps resSpace" << endl;
    for( int i = 0; i < threads; ++ i ) {
      s << "c " << i << " " 
        << statistics[i].checks << " "
	<< statistics[i].RATchecks << " "
	<< statistics[i].prop_lits << " "
	<< statistics[i].max_marked << " "
	<< statistics[i].reusedOthersMark  << " "
	<< statistics[i].checkedByOtherWorker << " "
	<< statistics[i].resolutions << " "
	<< statistics[i].removedLiterals << " "
	<< statistics[i].redundantPropagations << " "
	<< statistics[i].maxSpace
	<< endl;
      if( i > 0 ) {
	statistics[0].checks += statistics[i].checks;
	statistics[0].RATchecks += statistics[i].RATchecks;
	statistics[0].prop_lits += statistics[i].prop_lits;
	statistics[0].max_marked += statistics[i].max_marked;
	statistics[0].reusedOthersMark += statistics[i].reusedOthersMark;
	statistics[0].checkedByOtherWorker += statistics[i].checkedByOtherWorker;
	statistics[0].resolutions += statistics[i].resolutions ;
	statistics[0].removedLiterals += statistics[i].removedLiterals ;
	statistics[0].redundantPropagations += statistics[i].redundantPropagations ;
	statistics[0].maxSpace = statistics[0].maxSpace > statistics[i].maxSpace ? statistics[0].maxSpace : statistics[i].maxSpace;
      }
    }
  }
  s << "c ========= VERIFICATION STATISTICS =========" << endl;
  s << "c    checks RATchecks propagatedLits maxTodo usedOtherMarked" << endl;
  s << "c ALL checks RATchecks propagatedLits maxTodo usedOtherMarked reuseVerify resolutions redLits redProps resSpace" << endl;
      s << "c TOTAL " 
        << statistics[0].checks << " "
	<< statistics[0].RATchecks << " "
	<< statistics[0].prop_lits << " "
	<< statistics[0].max_marked << " "
	<< statistics[0].reusedOthersMark  << " "
	<< statistics[0].checkedByOtherWorker << " "
	<< statistics[0].resolutions << " "
	<< statistics[0].removedLiterals << " "
	<< statistics[0].redundantPropagations << " "
	<< statistics[0].maxSpace
	<< endl;
  s << "c ===========================================" << endl;
  s << "c check/sec: cpu: " << statistics[0].checks / cpuT
    << " wall: "  << statistics[0].checks / wallT  << endl;
  s << "c ====================================================" << endl;
}

void BackwardChecker::clearLabels(bool freeResources) {
  label.clear( freeResources );
  if( ! freeResources ) {
    label.growTo( fullProof.size() );
  }
}

#ifdef ONEWATCHLISTCHECKER
void BackwardChecker::addStatistics( Statistics& stat, BackwardVerificationOneWatch* worker )
#else
void BackwardChecker::addStatistics( Statistics& stat, BackwardVerificationWorker* worker )
#endif
{
  stat.max_marked += worker->maxClausesToBeChecked;
  stat.prop_lits += worker->num_props;
  stat.checks += worker->verifiedClauses;
  stat.RATchecks += worker->ratChecks;
  stat.reusedOthersMark += worker->usedOthersMark;
  stat.checkedByOtherWorker += worker->checkedByOtherWorker;
  stat.resolutions += worker->resolutions;
  stat.removedLiterals += worker->removedLiterals;
  stat.redundantPropagations += worker->redundantPropagations;
  stat.maxSpace = stat.maxSpace >= worker->maxSpace ? stat.maxSpace : worker->maxSpace;  // do not sum maxSpace but compare!
}

bool BackwardChecker::checkClause(vec< Lit >& clause, bool drupOnly, bool workOnCopy)
{
  // measure the time for this method call with the clock
  verificationClock.reset();
  MethodClock mc( verificationClock );
  
  label.growTo( fullProof.size() );
  if( opt_dependencyAna ) {
    dependencies.growTo( fullProof.size() );
  }
  
  resetStatistics();
  if( opt_statistics ) statistics.growTo( threads );
  
  // if the empty clause is part of the formula we are done already
  if( formulaContainsEmptyClause ) {
    if( verbose > 0 ) cerr << "c [BW-CHK] formula contains empty clause" << endl;
    return true;
  }

  // mark all clauses of the formula, if enabled by the routine (clauses are not checked, but help to propagate more eagerly)
  if( !opt_minimalCore ) {
    for( int id = 0; id < formulaClauses; ++ id ) 
      if ( !fullProof[id].isDelete() && !fullProof[id].isEmptyClause() ) label[id].setMarked();
  }
  
  // setup checker, verify, destroy object
#warning have an option that allows to keep the original clauses

  if( verbose > 0 ) cerr << "c [BW-CHK] verifiy clause with " << threads << " threads" << endl;

  assert( (!opt_statistics || threads == statistics.size()) && "enough space for all the statistics" );
  
  bool ret = false;
  if (threads == 1 ) {
#ifdef ONEWATCHLISTCHECKER
    sequentialChecker = new BackwardVerificationOneWatch( drupOnly ? false : drat, ca, fullProof, label, dependencies, formulaClauses, variables, workOnCopy );
#else
    sequentialChecker = new BackwardVerificationWorker  ( drupOnly ? false : drat, ca, fullProof, label, dependencies, formulaClauses, variables, workOnCopy );
#endif
    
    sequentialChecker->initialize( fullProof.size(), false );
    ret = ( l_True == sequentialChecker->checkClause( clause, 0, readsFormula ) ); // check only single clause, if we did not see the proof yet
    
    if( opt_statistics ) addStatistics( statistics[0], sequentialChecker ); // take care of worker-statistics
    
    sequentialChecker->release(); // write back the clause storage
    delete sequentialChecker;
    sequentialChecker = 0;
  } else {
    assert ( threads > 1 && "there have to be multiple workers"  );
    
    // first worker does not work on a copy
#ifdef ONEWATCHLISTCHECKER
    sequentialChecker = new BackwardVerificationOneWatch( drupOnly ? false : drat, ca, fullProof, label, dependencies, formulaClauses, variables, workOnCopy );
    sequentialChecker->setParallelMode( BackwardVerificationOneWatch::copy ); // so far, we copy the clause data base, so that each worker has full read and write access
#else
    sequentialChecker = new BackwardVerificationWorker( drupOnly ? false : drat, ca, fullProof, label, dependencies, formulaClauses, variables, workOnCopy );
    sequentialChecker->setParallelMode( BackwardVerificationWorker::copy ); // so far, we copy the clause data base, so that each worker has full read and write access
#endif

    sequentialChecker->initialize( fullProof.size(), false );
    // set sequential limit, so that work can be split afterwards
    sequentialChecker->setSequentialLimit( opt_splitLoad ); 
    
    // let sequential checker make a start and verify clauses until the set sequential limit of clauses has to be verified
    lbool intermediateResult = sequentialChecker->checkClause( clause, 0, readsFormula ); // here, still act as we own the shared data (there are no other workers around)
    sequentialChecker->setSequentialLimit( 0 ); // disable limit again
    
    ret = intermediateResult == l_True; // intermediate results
    if( verbose > 0 ) cerr << "c [BW-CHK] sequential check sufficient: " << ret << endl;
    bool parallelJobFailed = false;     // indicate that one job failed
    if( intermediateResult == l_Undef )  { // do the parallel work // TODO have separate method
      
      // create workers
      int assignedWorkers = 1; // initially we have one worker
      
#ifdef ONEWATCHLISTCHECKER
    BackwardVerificationOneWatch** workers = new BackwardVerificationOneWatch*[ threads ];
#else
    BackwardVerificationWorker** workers = new BackwardVerificationWorker*[ threads ];
#endif
      
      lbool* results = new lbool[ threads ];
      for( int i = 0 ; i < threads; ++ i ) {
	workers[i] = 0; results[i] = l_Undef;
      }
      workers[0] = sequentialChecker;

#ifndef ONEWATCHLISTCHECKER      
      if( opt_space_saving ) sequentialChecker->setParallelMode( BackwardVerificationWorker::shared );
#endif
      
      if( verbose > 3 ) { 
	cerr << "c [BW-CHK] proof marks after sequential work initialization (labels: " << label.size() << ")" << endl;
	for( int i = formulaClauses ; i < fullProof.size(); ++ i ) { // for all clauses in the proof
	  cerr << "c item [" << i << "] global: " << label[i].isMarked() << " (v=" << label[i].isVerified() << ")"  << " marks:";
	  for( int j = 0 ; j < 1; ++ j ) cerr << " " << workers[j]->markedItem( i );
	  cerr << endl;
	}
      }
      
      // split work for all threads
      int splitIteration = 0;
      while( assignedWorkers < threads ) {
	
	// split work 
	int previouslyAssignedWorkers = assignedWorkers;
	for( int i = 0 ; i < previouslyAssignedWorkers && assignedWorkers < threads; ++ i ) {
	  cerr << "c split from " << i << " to " << assignedWorkers << endl;
	  workers[assignedWorkers] = workers[i]->splitWork();
	  assignedWorkers ++;
	}
	splitIteration ++;
	cerr << "c split iteration: " << splitIteration << " assignedWorkers: " << assignedWorkers << endl;
	
	if( assignedWorkers == threads ) break;
	
	// create jobs that can be executed in parallel
	for( int i = 0 ; i < assignedWorkers; ++ i ) {
	  // execute in parallel!
	  workers[i]->setSequentialLimit( opt_splitLoad ); // set limit
	  results[i] = workers[i]->continueCheck();        // create more clauses
	}
	
	// wait for all jobs!
	
	// process results
	for( int i = 0 ; i < assignedWorkers; ++ i ) {
	  if( results[i] == l_False ) parallelJobFailed = true; // job failed
	  if( results[i] == l_True ) loadUnbalanced ++;    // indicate that one sequential jobs already finished its part // TODO add load balancing
	}
      }
      
      // solve all parts only, if jobs did not fail already
      if( ! parallelJobFailed ) {
      
	assert( assignedWorkers == threads && "all workers should be used now" );
	
	threadContoller = new ThreadController(threads);
	threadContoller->init();
	vector<Job> jobs( threads );       // have enough jobs
	WorkerData workerData [ threads ]; // data for each job
	
	// run all jobs in parallel
	// create jobs that can be executed in parallel
	for( int i = 0 ; i < assignedWorkers; ++ i ) {
	  // execute in parallel!
	  workerData[i].worker = workers[i]; // assign pointer
	  workerData[i].result = l_Undef; // set to be not finished, but not false either
	  workers[i]->setSequentialLimit( 0 ); // set limit
	  
	  // assign jobs
	  jobs[i].function  = BackwardChecker::runParallelVerfication; // add pointer to the method
	  jobs[i].argument  = &(workerData[i]);                        // add pointer to the argument of the method
	  
	}

	if( opt_pseudoParallel ) {
	  if( verbose > 1 ) cerr << "c [BW-CHK] pseudo parallel verification of remaining proof with " << threads << " threads" << endl; 
	  for( int i = 0 ; i < assignedWorkers; ++ i ) {
	    jobs[i].function ( jobs[i].argument );
	  }
	} else {
	  if( verbose > 1 ) cerr << "c [BW-CHK] parallel verification of remaining proof with " << threads << " threads" << endl; 
	  if( opt_loadbalancing ) {
	    // submit all jobs
	    for( int i = 0 ; i < jobs.size(); ++i ) threadContoller->submitJob(jobs[i]);
	    // wait until there is another idle client
	    threadContoller->waitForAllFinished();
	    assert( false && "currently there is no load balancing" );
	  } else {
	    // execute all jobs, and wait for all jobs!
	    threadContoller->runJobs( jobs );
	  }
	}
	
	// process results
	for( int i = 0 ; i < assignedWorkers; ++ i ) {
	  results[i] = workerData[i].result;                    // copy result of jobs that has been executed in parallel
	  if( results[i] == l_False ) parallelJobFailed = true; // job failed
	}
	
	// free resources
	delete threadContoller ; threadContoller = 0;
	
      }

      if( verbose > 3 ) { 
	cerr << "c [BW-CHK] proof marks after sequential work initialization" << endl;
	for( int i = formulaClauses ; i < fullProof.size(); ++ i ) { // for all clauses in the proof
	  cerr << "c item [" << i << "] global: " << label[i].isMarked() << " (v=" << label[i].isVerified() << ")"  << " marks:";
	  for( int j = 0 ; j < threads; ++ j ) cerr << " " << workers[j]->markedItem( i );
	  cerr << endl;
	}
      }
      
      // collect all results
      ret = true;
      for( int i = 0 ; i < threads; ++ i ) {
	ret = ret && (results[i] == l_True);
	cerr << "c verification success after worker " << i << " : " << ret << endl;
      }
      
      if( verbose > 3 ) { 
	cerr << "c [BW-CHK] proof marks after parallel verification" << endl;
	for( int i = formulaClauses ; i < fullProof.size(); ++ i ) { // for all clauses in the proof
	  cerr << "c item [" << i << "] global: " << label[i].isMarked() << " (v=" << label[i].isVerified() << ")"  << " marks:";
	  for( int j = 0 ; j < threads; ++ j ) cerr << " " << workers[j]->markedItem( i );
	  cerr << endl;
	}
      }
      
      if( opt_statistics ) { // collect statistics per worker
	for( int i = 1 ; i < threads; ++ i ) addStatistics( statistics[i], workers[i] ); // take care of worker-statistics
      }
      
      // free resources
      for( int i = 1 ; i < threads; ++ i ) {
	delete workers[i]; // delete all workers (do not delete sequentialChecker
      }
      delete [] results;
      delete [] workers;
    }
    // statistics
    if( opt_statistics ) addStatistics( statistics[0], sequentialChecker ); // take care of worker-statistics
    // clean up all data structures
    sequentialChecker->release(); // write back the clause storage
    delete sequentialChecker;     // free resources
    sequentialChecker = 0;        // do not delete sequential worker twice
    // TODO: clean up other workers nicely
#warning free resources
  }
  
  if( ret && !readsFormula ) {
    for( int i = formulaClauses ; i < fullProof.size(); ++ i ) { // for all clauses in the proof
      assert( ( !label[i].isMarked() || label[i].isVerified() ) && "each labeled clause has to be verified" );
    }
  }
  
  if( verbose > 5 ) { 
    cerr << "c [BW-CHK] all proof elements:" << endl;
    for( int i = 0; i < fullProof.size(); ++ i ) { // for all clauses in the proof
      cerr << "c item [" << i << "] ";
      if( fullProof[i].isDelete() ) cerr << "d " << ca[ fullProof[ fullProof[i].getID() ].getRef() ] << endl;
      else if ( fullProof[i].isEmptyClause() ) cerr << " EMPTY CLAUSE" << endl;
      else cerr << ca[ fullProof[i].getRef() ] << endl;
    }
  }
  
  assert( sequentialChecker == 0 && "freed all resources, hence the pointer has to be cleared as well" );
  return ret;
}


bool BackwardChecker::verifyProof () {

  if( !sawEmptyClause ) {
    cerr << "c WARNING: did not parse an empty clause, proof verification will fail" << endl;
    return false;
  }
  
  if( invalidProof && opt_strictProof ) {
    cerr << "c WARNING: proof is not correct based on invalid clause additions during parsing (see warnings)" << endl;
    return false;    
  }
  
  inputMode = false;          // we do not expect any more clauses
//   oneWatch.clear( true );     // free used resources
  oneWatchMap.clear();        // free used resources
  clauseCount.clear( true );  // free used resources
  
  // minimize memory consumption for shared and copied structures
  ca.fitSize();
  label.fitSize();
  fullProof.fitSize();
  
  // renew labels
  label.clear(); label.growTo( fullProof.size() );
  // renew dependencies
  if( opt_dependencyAna ) {
    dependencies.clear();
    dependencies.growTo( fullProof.size() );
  }
  
  assert( fullProof.last().isEmptyClause() && "last element of the proof should be the empty clause" );
  
  // if the last clause is the empty clause, set it marked and verified
  if( fullProof.last().isEmptyClause() ) {
    label.last().setMarked();
    label.last().setVerified();
  }
  
  // do the actual verification of the empty clause  
  vec<Lit> dummy; // will not allocate memory, so it's ok to be used as a temporal object
  bool ret = checkClause( dummy );
  
  // collect statistics
  if( statistics ) {
    unsatisfiableCore = 0; proofWidth = 0; proofLength = 0;
    assert( label.size() == fullProof.size() && "data should be present" );
    for( int i = 0; i < fullProof.size(); ++ i ) { // for all clauses in the proof
//       assert( (i < formulaClauses || !label[i].isMarked() || label[i].isVerified() ) && "each labeled clause has to be verified" );
      if( label[i].isMarked() && ( !fullProof[i].isEmptyClause() || i + 1 < fullProof.size()) ) {
	assert( !fullProof[i].isDelete() && "has to be a usual clause" );
	if( i < formulaClauses ) unsatisfiableCore ++;
	else {
	  if( ! fullProof[i].isEmptyClause() ) {
	    const Clause& c = ca[ fullProof[i].getRef() ];
	    proofWidth = proofWidth >= c.size() ? proofWidth : c.size();
	  }
	  proofLength ++;
	}
      }
    }
  }
  
  if( opt_coreFile != 0 ) {
    std::ofstream file;
    file.open( (const char*) opt_coreFile );
    if( file ) {
      std::stringstream formula;
      int cls = 0, vars = 0;
      for( int i = 0; i < formulaClauses; ++ i ) { // for all clauses in the proof
	if( label[i].isMarked() ) { // add all labeled clauses to the formula
	  assert( !fullProof[i].isDelete() && !fullProof[i].isEmptyClause() && "has to be a usual clause" );
	  const Clause& c = ca[ fullProof[i].getRef() ];
	  for( int i = 0 ; i < c.size(); ++ i ) { 
	    formula << c[i] << " ";
	    vars = vars >= var(c[i]) ? vars: var(c[i]);
	  }
	  formula << "0" << endl;
	  cls ++;
	}
      }
      // write file, including header information
      file << "p cnf " << vars + 1 << " " << cls << endl;
      file << formula.str();
      file.close();
    } else {
      cerr << "c WARNING: could not open file " << (const char*) opt_coreFile << " for writing unsatisfiable core" << endl;
    }
  }
  if( opt_proofFile != 0 ) {
    std::ofstream file;
    file.open( (const char*) opt_proofFile );
    if( file ) {
      file << "o proof " << (drat ? "DRAT" : "DRUP") << endl; // print proof header
      for( int i = formulaClauses; i < fullProof.size(); ++ i ) { // for all clauses in the proof
	if( label[i].isMarked() ) { // add all labeled clauses to the formula
	  std::stringstream clause;
	  assert( !fullProof[i].isDelete() && "has to be a usual clause" );
	  if( !fullProof[i].isEmptyClause() ) { // print the clause, if its not the empty clause
	    const Clause& c = ca[ fullProof[i].getRef() ];
	    if( !drat ) for( int i = 0 ; i < c.size(); ++ i ) clause << c[i] << " ";
	    else { // f we have drat proofs, print the first literal first, and not twice!
	      assert( c.size() > 0 && "cannot be the empty clause" );
	      clause << c.getExtraLiteral() << " "; // print extra literal
	      for( int i = 0 ; i < c.size(); ++ i ) { // print all other literals
		if( c[i] != c.getExtraLiteral() ) clause << c[i] << " ";
	      }
	    }
	  }
	  clause << "0" << endl;
	  file << clause.str();
	} else if ( fullProof[i].isDelete() && label[ fullProof[i].getID() ].isMarked() ) {
	  std::stringstream clause;
	  assert( !fullProof[ fullProof[i].getID() ].isDelete() && !fullProof[ fullProof[i].getID() ].isEmptyClause() && "has to be a usual clause" );
	  const Clause& c = ca[ fullProof[i].getRef() ];
	  clause << "d "; // add this deletion information, as the corresponding clause has also been used
	  for( int i = 0 ; i < c.size(); ++ i ) clause << c[i] << " ";
	  clause << "0" << endl;
	  file << clause.str();
	}
      }
      file.close();
    } else {
      cerr << "c WARNING: could not open file " << (const char*) opt_proofFile << " for writing minimized proof" << endl;
    }
  }
  
  // print statistics
  printStatistics(cerr);
  
  return ret;
}

void* BackwardChecker::runParallelVerfication(void* verificationData)
{
  WorkerData* workerData = (WorkerData*) verificationData;
  workerData->result = workerData->worker->continueCheck();
  return 0;
}
