/***************************************************************************************[Solver.cc]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
				CRIL - Univ. Artois, France
				LRI  - Univ. Paris Sud, France
 
Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------

Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2012-2014, Norbert Manthey

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <math.h>

#include "riss/mtl/Sort.h"
#include "riss/core/Solver.h"
#include "riss/core/Constants.h"

// to be able to use the preprocessor
#include "coprocessor/Coprocessor.h"
#include "coprocessor/CoprocessorTypes.h"

// to be able to read var files
#include "coprocessor/VarFileParser.h"





using namespace Riss;

//=================================================================================================
// Options:




// useful methods


//=================================================================================================
// Constructor/Destructor:


Solver::Solver(CoreConfig& _config) :
    config(_config)
    // DRUP output file
    , drupProofFile (0)
    // Parameters (user settable):
    //
    , verbosity      (config.opt_verb)
    , verbEveryConflicts (100000)
    , K              (config.opt_K)
    , R              (config.opt_R)
    , sizeLBDQueue   (config.opt_size_lbd_queue)
    , sizeTrailQueue   (config.opt_size_trail_queue)
    , firstReduceDB  (config.opt_first_reduce_db)
    , incReduceDB    (config.opt_inc_reduce_db)
    , specialIncReduceDB    (config.opt_spec_inc_reduce_db)
    , lbLBDFrozenClause (config.opt_lb_lbd_frozen_clause)
    , lbSizeMinimizingClause (config.opt_lb_size_minimzing_clause)
    , lbLBDMinimizingClause (config.opt_lb_lbd_minimzing_clause)
  , var_decay        (config.opt_var_decay_start)
  , clause_decay     (config.opt_clause_decay)
  , random_var_freq  (config.opt_random_var_freq)
  , random_seed      (config.opt_random_seed)
  , ccmin_mode       (config.opt_ccmin_mode)
  , phase_saving     (config.opt_phase_saving)
  , rnd_pol          (random_var_freq > 0)     // if there is a random variable frequency, allow random decisions
  , rnd_init_act     (config.opt_rnd_init_act)
  , garbage_frac     (config.opt_garbage_frac)


    // Statistics: (formerly in 'SolverStats')
    //
  , nbRemovedClauses(0),nbReducedClauses(0), nbDL2(0),nbBin(0),nbUn(0) , nbReduceDB(0)
  , solves(0), starts(0), decisions(0), rnd_decisions(0), propagations(0), conflicts(0),nbstopsrestarts(0),nbstopsrestartssame(0),lastblockatrestart(0)
  , dec_vars(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0)
  , curRestart(1)

  , ok                 (true)
  , cla_inc            (1)
  , var_inc            (1)
  , watches            (WatcherDeleted(ca))
//  , watchesBin            (WatcherDeleted(ca))
  , qhead              (0)
  , realHead           (0)
  , simpDB_assigns     (-1)
  , simpDB_props       (0)
  , order_heap         (VarOrderLt(activity))
  , progress_estimate  (0)
  , remove_satisfied   (true)
  
  , preprocessCalls (0)
  , inprocessCalls (0)
  
    // Resource constraints:
    //
  , conflict_budget    (-1)
  , propagation_budget (-1)
  , asynch_interrupt   (false)
  
  , terminationCallbackState(0)
  , terminationCallbackMethod(0)
  
  // Online proof checking class
  , onlineDratChecker( config.opt_checkProofOnline != 0 ? new OnlineProofChecker(dratProof) : 0)
  
  // UIP hack
  , l1conflicts(0)
  , multiLearnt(0)
  , learntUnit(0)

  // restart interval hack
  , conflictsSinceLastRestart(0)
  , currentRestartIntervalBound(config.opt_rMax)
  , intervalRestart(0)
  
  // LA hack
  ,laAssignments(0)
  ,tabus(0)
  ,las(0)
  ,failedLAs(0)
  ,maxBound(0)
  ,laTime(0)
  ,maxLaNumber(config.opt_laBound)
  ,topLevelsSinceLastLa(0)
  ,laEEvars(0)
  ,laEElits(0)
  ,untilLa(config.opt_laEvery)
  ,laBound(config.opt_laEvery)
  ,laStart(false)
  
  ,startedSolving(false)
  
  ,useVSIDS(config.opt_vsids_start)
  
  ,simplifyIterations(0)
  ,learnedDecisionClauses(0)
  
  ,totalLearnedClauses(0)
  ,sumLearnedClauseSize(0)
  ,sumLearnedClauseLBD(0)
  ,maxLearnedClauseSize(0)
  
  // for partial restarts
  , rs_partialRestarts(0)
  , rs_savedDecisions(0)
  , rs_savedPropagations(0)
  , rs_recursiveRefinements(0)
  
  // probing during learning
  , big(0)
  , lastReshuffleRestart(0)
  , L2units(0)
  , L3units(0)
  , L4units(0)
  
  // UHLE for learnt clauses
  , searchUHLEs(0)
  , searchUHLElits(0)
  
  // preprocessor
  , coprocessor(0)
  , useCoprocessorPP(config.opt_usePPpp)
  , useCoprocessorIP(config.opt_usePPip)
  
  // communication to other solvers that might be run in parallel
  , communication (0)
  , currentTries(0)
  , receiveEvery(0)
  , currentSendSizeLimit(0)
  , currentSendLbdLimit(0)
  , succesfullySend(0)
  , succesfullyReceived(0)
  , sendSize(0)
  , sendLbd(0)
  , sendMaxSize(0)
  , sendMaxLbd(0)
  , sizeChange(0)
  , lbdChange(0)
  , sendRatio(0)
{
  MYFLAG=0;
  hstry[0]=lit_Undef;hstry[1]=lit_Undef;hstry[2]=lit_Undef;hstry[3]=lit_Undef;hstry[4]=lit_Undef;hstry[5]=lit_Undef;
  
  if( onlineDratChecker != 0 ) onlineDratChecker->setVerbosity( config.opt_checkProofOnline );
  
}



Solver::~Solver()
{
  if( big != 0 )         { big->BIG::~BIG(); delete big; big = 0; } // clean up! 
  if( coprocessor != 0 ) { delete coprocessor; coprocessor = 0; }
}


//=================================================================================================
// Minor methods:


// Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(bool sign, bool dvar, char type)
{
    int v = nVars();
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true ));

    varFlags. push( VarFlags( sign ) );
    
//     assigns  .push(l_Undef);
    vardata  .push(mkVarData(CRef_Undef, 0));
    //activity .push(0);
    activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
//     seen     .push(0);
    permDiff  .resize(2*v+2); // add space for the next variable

    trail    .capacity(v+1);
    setDecisionVar(v, dvar);
    
    return v;
}

void Solver::reserveVars(Var v)
{
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true ));
//    watchesBin  .init(mkLit(v, false));
//    watchesBin  .init(mkLit(v, true ));
    
//     assigns  .capacity(v+1);
    vardata  .capacity(v+1);
    //activity .push(0);
    activity .capacity(v+1);
//     seen     .capacity(v+1);
    permDiff  .capacity(2*v+2);
    varFlags. capacity(v+1);
    trail    .capacity(v+1);
}



bool Solver::addClause_(vec<Lit>& ps)
{
    assert(decisionLevel() == 0);
    if (!ok) return false;

    // Check if clause is satisfied and remove false/duplicate literals:
    sort(ps);

    // analyze for DRUP - add if necessary!
    Lit p; int i, j, flag = 0;
    if( outputsProof() ) {
      oc.clear();
      for (i = j = 0, p = lit_Undef; i < ps.size(); i++) {
	  oc.push(ps[i]);
	  if (value(ps[i]) == l_True || ps[i] == ~p || value(ps[i]) == l_False)
	    flag = 1;
      }
    }

    if( !config.opt_hpushUnit ) { // do not analyzes clauses for being satisfied or simplified
      for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
	  if (value(ps[i]) == l_True || ps[i] == ~p)
	      return true;
	  else if (value(ps[i]) != l_False && ps[i] != p)
	      ps[j++] = p = ps[i];
      ps.shrink_(i - j);
    }

    // add to Proof that the clause has been changed
    if ( flag &&  outputsProof() ) {
      addCommentToProof("reduce due to assigned literals, or duplicates");
      addToProof(ps);
      addToProof(oc,true);
    } else if( outputsProof() && config.opt_verboseProof == 2 ) {
      cerr << "c added usual clause " << ps << " to solver" << endl; 
    }

    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1){
	if( config.opt_hpushUnit ) {
	  if( value( ps[0] ) == l_False ) return ok = false;
	  if( value( ps[0] ) == l_True ) return true;
	}
	uncheckedEnqueue(ps[0]);
	if( !config.opt_hpushUnit ) return ok = (propagate(true) == CRef_Undef);
	else return ok;
    }else{
        CRef cr = ca.alloc(ps, false);
        clauses.push(cr);
        attachClause(cr);
    }

    return true;
}

bool Solver::addClause(const Clause& ps)
{
    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1){
	if( config.opt_hpushUnit ) {
	  if( value( ps[0] ) == l_False ) return ok = false;
	  if( value( ps[0] ) == l_True ) return true;
	}
	uncheckedEnqueue(ps[0]);
	if( !config.opt_hpushUnit ) return ok = (propagate() == CRef_Undef);
	else return ok;
    }else{
        CRef cr = ca.alloc(ps, ps.learnt() );
	if( !ps.learnt() ) clauses.push(cr);
        else learnts.push( cr );
        attachClause(cr);
    }

    return true;
}


void Solver::attachClause(CRef cr) {
    const Clause& c = ca[cr];
    assert(c.size() > 1 && "cannot watch unit clauses!");
    assert( c.mark() == 0 && "satisfied clauses should not be attached!" );
    
//     cerr << "c attach clause " << cr << " which is " << ca[cr] << endl;
    
    // check for duplicates here!
//     for (int i = 0; i < c.size(); i++)
//       for (int j = i+1; j < c.size(); j++)
// 	assert( c[i] != c[j] && "have no duplicate literals in clauses!" );
    
    if(c.size()==2) {
      watches[~c[0]].push(Watcher(cr, c[1], 0)); // add watch element for binary clause
      watches[~c[1]].push(Watcher(cr, c[0], 0)); // add watch element for binary clause
    } else {
//      cerr << "c DEBUG-REMOVE watch clause " << c << " in lists for literals " << ~c[0] << " and " << ~c[1] << endl;
      watches[~c[0]].push(Watcher(cr, c[1], 1));
      watches[~c[1]].push(Watcher(cr, c[0], 1));
    }
    if (c.learnt()) learnts_literals += c.size();
    else            clauses_literals += c.size(); }




void Solver::detachClause(CRef cr, bool strict) {
    const Clause& c = ca[cr];
    
//     cerr << "c detach clause " << cr << " which is " << ca[cr] << endl;
    
    // assert(c.size() > 1 && "there should not be unit clauses - on the other hand, LHBR and OTFSS could create unit clauses");
//     if( c.size() == 1 ) {
//       cerr << "c extra bug - unit clause is removed" << endl;
//       exit( 36 );
//     }

    const int watchType = c.size()==2 ? 0 : 1; // have the same code only for different watch types!
    if (strict){
	if( config.opt_fast_rem ) {
	  removeUnSort(watches[~c[0]], Watcher(cr, c[1],watchType)); 
	  removeUnSort(watches[~c[1]], Watcher(cr, c[0],watchType)); 
	} else {
	  remove(watches[~c[0]], Watcher(cr, c[1],watchType)); // linear (touchs all elements)!
	  remove(watches[~c[1]], Watcher(cr, c[0],watchType)); // linear (touchs all elements)!
	}
    }else{
      // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
      watches.smudge(~c[0]);
      watches.smudge(~c[1]);
    }

    if (c.learnt()) learnts_literals -= c.size();
    else            clauses_literals -= c.size(); }


void Solver::removeClause(Riss::CRef cr, bool strict) {
  
  Clause& c = ca[cr];

  // tell DRUP that clause has been deleted, if this was not done before already!
  if( c.mark() == 0 ) {
    addCommentToProof("delete via clause removal",true);
    addToProof(c,true);	// clause has not been removed yet
  }
  DOUT( if(config.opt_learn_debug) cerr << "c remove clause [" << cr << "]: " << c << endl; );

  detachClause(cr, strict); 
  // Don't leave pointers to free'd memory!
  if (locked(c)) {
    vardata[var(c[0])].reason = CRef_Undef;
  }
  c.mark(1); 
   ca.free(cr);
}


bool Solver::satisfied(const Clause& c) const {

    // quick-reduce option
    if(config.opt_quick_reduce)  // Check clauses with many literals is time consuming
        return (value(c[0]) == l_True) || (value(c[1]) == l_True);

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True)
            return true;
    return false; }

/************************************************************
 * Compute LBD functions
 *************************************************************/

inline unsigned int Solver::computeLBD(const vec<Lit> & lits) {
  int nblevels = 0;
  permDiff.nextStep();
  bool withLevelZero = false;
    for(int i=0;i<lits.size();i++) {
      int l = level(var(lits[i]));
      withLevelZero = (l==0);
      if ( ! permDiff.isCurrentStep(l) ) {
	permDiff.setCurrentStep(l);
	nblevels++;
      }
    }
  if( config.opt_lbd_ignore_l0 && withLevelZero ) return nblevels - 1;
  else return nblevels;
}

inline unsigned int Solver::computeLBD(const Clause &c) {
  int nblevels = 0;
  permDiff.nextStep();
  bool withLevelZero = false;
    for(int i=0;i<c.size();i++) {
      int l = level(var(c[i]));
      withLevelZero = (l==0);
      if ( ! permDiff.isCurrentStep(l) ) {
	permDiff.setCurrentStep(l);
	nblevels++;
      }
    }
  if( config.opt_lbd_ignore_l0 && withLevelZero ) return nblevels - 1;
  else return nblevels;
}


/******************************************************************
 * Minimisation with binary reolution
 ******************************************************************/
bool Solver::minimisationWithBinaryResolution(vec< Lit >& out_learnt, unsigned int& lbd) {
  
  // Find the LBD measure                                                                                                         
  // const unsigned int lbd = computeLBD(out_learnt);
  const Lit p = ~out_learnt[0];

      if(lbd<=lbLBDMinimizingClause){
      permDiff.nextStep();
      for(int i = 1;i<out_learnt.size();i++) permDiff.setCurrentStep( var(out_learnt[i]) );
      const vec<Watcher>&  wbin  = watches[p]; // const!
      int nb = 0;
      for(int k = 0;k<wbin.size();k++) {
	if( !wbin[k].isBinary() ) continue; // has been looping on binary clauses only before!
	const Lit imp = wbin[k].blocker();
	if(  permDiff.isCurrentStep(var(imp)) && value(imp)==l_True) {
	  nb++;
	  permDiff.reset( var(imp) );
#ifdef CLS_EXTRA_INFO
	  extraInfo = extraInfo >= ca[wbin[k].cref()].extraInformation() ? extraInfo : ca[wbin[k].cref()].extraInformation();
#endif
	}
      }
      int l = out_learnt.size()-1;
      if(nb>0) {
	nbReducedClauses++;
	for(int i = 1;i<out_learnt.size()-nb;i++) {
	  if( ! permDiff.isCurrentStep( var(out_learnt[i])) ) {
	    const Lit p = out_learnt[l];
	    out_learnt[l] = out_learnt[i];
	    out_learnt[i] = p;
	    l--;i--;
	  }
	}
	out_learnt.shrink_(nb);
	return true; // literals have been removed from the clause
      } else return false; // no literals have been removed
    } else return false; // no literals have been removed
}


/******************************************************************
 * Minimisation with binary implication graph
 ******************************************************************/
bool Solver::searchUHLE(vec<Lit>& learned_clause, unsigned int& lbd ) {
  if(lbd<=config.uhle_minimizing_lbd){ // should not touch the very first literal!
      const Lit p = learned_clause[0]; // this literal cannot be removed!
      const uint32_t cs = learned_clause.size(); // store the size of the initial clause
      Lit Splus  [cs];		// store sorted literals of the clause
      for( uint32_t ci = 0 ; ci  < cs; ++ ci ) Splus [ci] = learned_clause[ci];
      
      { // sort the literals according to the time stamp they have been found
	const uint32_t s = cs;
	for (uint32_t j = 1; j < s; ++j)
	{
	      const Lit key = Splus[j];
	      const uint32_t keyDsc = big->getStart(key);
	      int32_t i = j - 1;
	      while ( i >= 0 && big->getStart( Splus[i]) > keyDsc )
	      {
		  Splus[i+1] = Splus[i]; i--;
	      }
	      Splus[i+1] = key;
	}
      }

      // apply UHLE for the literals of the clause
	uint32_t pp = cs;
	uint32_t finished = big->getStop(Splus[cs-1]);
	Lit finLit = Splus[cs-1];
	for(pp = cs-1 ; pp > 0; -- pp ) {
	  const Lit l = Splus[ pp - 1];
	  const uint32_t fin = big->getStop(l);
	  if( fin > finished ) {
	    for( int i = 0 ; i < learned_clause.size(); ++ i ) { // remove the literal l from the current learned clause
	      if( learned_clause[i] == l ) {
		learned_clause[i] = learned_clause[ learned_clause.size() - 1]; learned_clause.pop();
	      }
	    }
	  } else {
	    finished = fin;
	    finLit = l;
	  }
	}

      
      // do UHLE for the complementary literals in the clause
	const uint32_t csn = learned_clause.size();
	Lit Sminus [csn];	// store the complementary literals sorted to their discovery in the BIG
	for( uint32_t ci = 0 ; ci < csn; ++ ci ) {
	  Sminus[ci] = ~learned_clause[ci];
	}
	{ // insertion sort for discovery of complementary literals
	  const uint32_t s = csn;
	  for (uint32_t j = 1; j < s; ++j)
	  {
		const Lit key = Sminus[j];
		const uint32_t keyDsc = big->getStart(key);
		int32_t i = j - 1;
		while ( i >= 0 && big->getStart( Sminus[i]) > keyDsc )
		{
		    Sminus[i+1] = Sminus[i]; i--;
		}
		Sminus[i+1] = key;
	  }
	}
	
	// run UHLE for the complementary literals
	finished = big->getStop( Sminus[0] );
	finLit = Sminus[ 0 ];
	for(uint32_t pn = 1; pn < csn; ++ pn) {
	  const Lit l = Sminus[ pn ];
	  const uint32_t fin = big->getStop(l);
	  if( fin < finished ) { // remove the complementary literal from the clause!
	    for( int i = 0 ; i < learned_clause.size(); ++ i ) { // remove the literal l from the current learned clause
	      if( learned_clause[i] == ~l ) {
		learned_clause[i] = learned_clause[ learned_clause.size() - 1]; learned_clause.pop();
	      }
	    }
	  } else {
	    finished = fin;
	    finLit = l;
	  }
	}
      // do some stats!
      searchUHLEs ++;
      searchUHLElits += ( cs - learned_clause.size() );
      if( cs != learned_clause.size() ) return true; // some literals have been removed
      else return false; // no literals have been removed
  } else return false;// no literals have been removed
}

/** check whether there is an AND-gate that can be used to simplify the given clause
 */
bool Solver::erRewrite(vec<Lit>& learned_clause, unsigned int& lbd ){
  // TODO: put into extra method
  return false;
}

// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
    if (decisionLevel() > level){
      DOUT( if( config.opt_learn_debug) cerr << "c call cancel until " << level << " move propagation head from " << qhead << " to " << trail_lim[level] << endl; );
        for (int c = trail.size()-1; c >= trail_lim[level]; c--){
            Var      x  = var(trail[c]);
            varFlags [x].assigns = l_Undef;
	    vardata [x].dom = lit_Undef; // reset dominator
	    vardata [x].reason = CRef_Undef; // TODO for performance this is not necessary, but for assertions and all that!
            if (phase_saving > 1  ||   ((phase_saving == 1) && c > trail_lim.last())  ) // TODO: check whether publication said above or below: workaround: have another parameter value for the other case!
                varFlags[x].polarity = sign(trail[c]);
            insertVarOrder(x); }
        qhead = trail_lim[level];
	realHead = trail_lim[level];
        trail.shrink_(trail.size() - trail_lim[level]);
        trail_lim.shrink_(trail_lim.size() - level);
    } }


//=================================================================================================
// Major methods:


Lit Solver::pickBranchLit()
{
    Var next = var_Undef;

    // NuSMV: PREF MOD
    // Selection from preferred list
    for (int i = 0; i < preferredDecisionVariables.size(); i++) {
      if ( value(preferredDecisionVariables[i]) == l_Undef) {
	next = preferredDecisionVariables[i];
      }
    }
    // NuSMV: PREF MOD END
    
    // Random decision:
    if (
	    // NuSMV: PREF MOD
   		next == var_Undef && 
 		  // NuSMV: PREF MOD END
      drand(random_seed) < random_var_freq && !order_heap.empty()){
        next = order_heap[irand(random_seed,order_heap.size())];
        if (value(next) == l_Undef && varFlags[next].decision)
            rnd_decisions++; }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || ! varFlags[next].decision)
        if (order_heap.empty()){
            next = var_Undef;
            break;
        }else
            next = order_heap.removeMin();

    const Lit returnLit =  next == var_Undef ? lit_Undef : mkLit(next, rnd_pol ? drand(random_seed) < random_var_freq : varFlags[next].polarity );
    return returnLit; 
}


/*_________________________________________________________________________________________________
|
|  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
|  
|  Description:
|    Analyze conflict and produce a reason clause.
|  
|    Pre-conditions:
|      * 'out_learnt' is assumed to be cleared.
|      * Current decision level must be greater than root level.
|  
|    Post-conditions:
|      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
|      * If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the 
|        rest of literals. There may be others from the same level though.
|  
|________________________________________________________________________________________________@*/

int Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel,unsigned int &lbd, uint64_t& extraInfo)
{
    int pathC = 0;
    Lit p     = lit_Undef;
    int pathLimit = 0; // for bi-asserting clauses

    bool foundFirstLearnedClause = false;
    int units = 0, resolvedWithLarger = 0; // stats
    bool isOnlyUnit = true;
    lastDecisionLevel.clear();  // must clear before loop, because alluip can abort loop and would leave filled vector
    int currentSize = 0;        // count how many literals are inside the resolvent at the moment! (for otfss)
    CRef lastConfl = CRef_Undef;
    
    varsToBump.clear();clssToBump.clear(); // store info for bumping
    
    // Generate conflict clause:
    //
    out_learnt.push();      // (leave room for the asserting literal)
    int index   = trail.size() - 1;

    do{
	DOUT( if( config.opt_learn_debug ) cerr << "c enter loop with lit " << p << endl; );
        Clause& c = ca[confl];

	int clauseReductSize = c.size();
	// Special case for binary clauses
	// The first one has to be SAT
	if( p != lit_Undef && c.size()==2 && value(c[0])==l_False) {
	  assert(value(c[1])==l_True);
	  Lit tmp = c[0];
	  c[0] =  c[1], c[1] = tmp;
	}
	
	resolvedWithLarger = (c.size() == 2) ? resolvedWithLarger : resolvedWithLarger + 1; // how often do we resolve with a clause whose size is larger than 2?
	
	if(!foundFirstLearnedClause ) { // dynamic adoption only until first learned clause!
	  if (c.learnt() ){
	      if( config.opt_cls_act_bump_mode == 0 ) claBumpActivity(c);
	      else clssToBump.push( confl );
	  }

	  if( config.opt_update_lbd == 1  ) { // update lbd during analysis, if allowed
	      if(c.learnt()  && c.lbd()>2 ) { 
		unsigned int nblevels = computeLBD(c);
		if(nblevels+1<c.lbd() || config.opt_lbd_inc ) { // improve the LBD (either LBD decreased,or option is set)
		  if(c.lbd()<=lbLBDFrozenClause) {
		    c.setCanBeDel(false); 
		  }
		  // seems to be interesting : keep it for the next round
		  c.setLBD(nblevels); // Update it
		}
	      }
	  }
	}

#ifdef CLS_EXTRA_INFO // if resolution is done, then take also care of the participating clause!
	extraInfo = extraInfo >= c.extraInformation() ? extraInfo : c.extraInformation();
#endif
       
        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            const Lit& q = c[j];
	    DOUT( if( config.opt_learn_debug ) cerr << "c level for " << q << " is " << level(var(q)) << endl; );
            if (!varFlags[var(q)].seen && level(var(q)) > 0){ // variable is not in the clause, and not on top level
                currentSize ++;
                if( !foundFirstLearnedClause  ) varsToBump.push( var(q) );
		DOUT( if( config.opt_learn_debug ) cerr << "c set seen for " << q << endl; );
                varFlags[var(q)].seen = 1;
                if (level(var(q)) >= decisionLevel()) {
                    pathC++;
#ifdef UPDATEVARACTIVITY
		if( !foundFirstLearnedClause && config.opt_updateLearnAct ) {
		    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
		    if((reason(var(q))!= CRef_Undef)  && ca[reason(var(q))].learnt())  {
		      DOUT( if (config.opt_learn_debug) cerr << "c add " << q << " to last decision level" << endl; );
		      lastDecisionLevel.push(q);
		    }
		}
#endif

		} else {
                    out_learnt.push(q);
		    isOnlyUnit = false; // we found a literal from another level, thus the multi-unit clause cannot be learned
		}
	    } 
        }
        
        if( !isOnlyUnit && units > 0 ) break; // do not consider the next clause, because we cannot continue with units
        
        // Select next clause to look at:
        while (! varFlags[ var( trail[index--] ) ].seen ) {} // cerr << "c check seen for literal " << (sign(trail[index]) ? "-" : " ") << var(trail[index]) + 1 << " at index " << index << " and level " << level( var( trail[index] ) )<< endl;
        p     = trail[index+1];
	lastConfl = confl;
        confl = reason(var(p));
	DOUT( if( config.opt_learn_debug ) cerr << "c reset seen for " << p << endl; );
        varFlags[var(p)].seen = 0;
        pathC--;
	currentSize --;

	// do unit analysis only, if the clause did not become larger already!
	if( config.opt_allUipHack > 0  && pathC <= 0 && isOnlyUnit && out_learnt.size() == 1+units ) { 
	  learntUnit ++; 
	  units ++; // count current units
	  out_learnt.push( ~p ); // store unit
	  DOUT( if( config.opt_learn_debug ) cerr << "c learn unit clause " << ~p << " with pathLimit=" << pathLimit << endl; );
	  if( config.opt_allUipHack == 1 ) break; // for now, stop at the first unit! // TODO collect all units
	  pathLimit = 0;	// do not use bi-asserting learning once we found one unit clause
	  foundFirstLearnedClause = true; // we found a first clause, hence, stop all heuristical updates for the following steps
	}
	
	// do stop here
    } while (
       //if no unit clause is learned, and the first UIP is hit, or a bi-asserting clause is hit
         (units == 0 && pathC > pathLimit)
      // or 1stUIP is unit, but the current learned clause would be bigger, and there are still literals on the current level
      || (isOnlyUnit && units > 0 && index >= trail_lim[ decisionLevel() - 1] ) 
    );
    assert( (units == 0 || pathLimit == 0) && "there cannot be a bi-asserting clause that is a unit clause!" );
    assert( out_learnt.size() > 0 && "there has to be some learnt clause" );
    
    // if we do not use units, add asserting literal to learned clause!
    if( units == 0 ) {
      out_learnt[0] = ~p; // add the last literal to the clause
      if( pathC > 0 ) { // in case of bi-asserting clauses, the remaining literals have to be collected
	// look for second literal of this level
	while (! varFlags[var(trail[index--])].seen);
	p = trail[index+1];
	out_learnt.push( ~p );
      }
    } else { 
      // process learnt units!
      // clear seen
      for( int i = units+1; i < out_learnt.size() ; ++ i ) varFlags[ var(out_learnt[i]) ].seen = 0;
      out_learnt.shrink_( out_learnt.size() - 1 - units );  // keep units+1 elements!
      
      assert( out_learnt.size() > 1 && "there should have been a unit" );
      out_learnt[0] = out_learnt.last(); out_learnt.pop(); // close gap in vector
      // printf("c first unit is %d\n", var( out_learnt[0] ) + 1 );
      
      out_btlevel = 0; // jump back to level 0!
      
      // clean seen, if more literals have been added
      if( !isOnlyUnit ) while (index >= trail_lim[ decisionLevel() - 1 ] ) varFlags[ var(trail[index--]) ].seen = 0;
      
      lbd = 1; // for glucoses LBD score
      return units; // for unit clauses no minimization is necessary
    }

    currentSize ++; // the literal "~p" has been added additionally
    if( currentSize != out_learnt.size() ) cerr << "c different sizes: clause=" << out_learnt.size() << ", counted=" << currentSize << " and collected vector: " << out_learnt << endl;
    assert( currentSize == out_learnt.size() && "counted literals has to be equal to actual clause!" );
    
   
    bool doMinimizeClause = true; // created extra learnt clause? yes -> do not minimize
    lbd = computeLBD(out_learnt);
    bool recomputeLBD = false; // current lbd is valid
    if( decisionLevel() > 0 && out_learnt.size() > decisionLevel() && out_learnt.size() > config.opt_learnDecMinSize && config.opt_learnDecPrecent != -1) { // is it worth to check for decisionClause?
      if( lbd > (config.opt_learnDecPrecent * decisionLevel() + 99 ) / 100 ) {
	// instead of learning a very long clause, which migh be deleted very soon (idea by Knuth, already implemented in lingeling(2013)
	for (int j = 0; j < out_learnt.size(); j++) varFlags[var(out_learnt[j])].seen = 0;    // ('seen[]' is now cleared)
	out_learnt.clear();
	
	for(int i=0; i<decisionLevel(); ++i) {
	  if( (i == 0 || trail_lim[i] != trail_lim[i-1]) && trail_lim[i] < trail.size() ) // no dummy level caused by assumptions ...
	    out_learnt.push( ~trail[ trail_lim[i] ] ); // get the complements of all decisions into dec array
	}
	DOUT( if( config.opt_printDecisions > 2 || config.opt_learn_debug) cerr << endl << "c current decision stack: " << out_learnt << endl ; );
	const Lit tmpLit = out_learnt[ out_learnt.size() -1 ]; // 
	out_learnt[ out_learnt.size() -1 ] = out_learnt[0]; // have first decision as last literal
	out_learnt[0] = tmpLit; // ~p; // new implied literal is the negation of the asserting literal ( could also be the last decision literal, then the learned clause is a decision clause) somehow buggy ...
	learnedDecisionClauses ++;
	DOUT( if( config.opt_printDecisions > 2 || config.opt_learn_debug ) cerr << endl << "c learn decisionClause " << out_learnt << endl << endl; );
	doMinimizeClause = false;
      }
    }
    
   
    if( doMinimizeClause ) {
    // Simplify conflict clause:
    //
    int i, j;
    uint64_t minimize_extra_info = extraInfo;
    out_learnt.copyTo(analyze_toclear);
    if (ccmin_mode == 2){
        uint32_t abstract_level = 0;
        for (i = 1; i < out_learnt.size(); i++)
            abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)

        
        for (i = j = 1; i < out_learnt.size(); i++) {
	    minimize_extra_info = extraInfo;
            if (reason(var(out_learnt[i])) == CRef_Undef ) {
	      out_learnt[j++] = out_learnt[i]; // keep, since we cannot resolve on decisino literals
	    } else if ( !litRedundant(out_learnt[i], abstract_level, extraInfo)) {
	      extraInfo=minimize_extra_info; // not minimized, thus, keep the old value
              out_learnt[j++] = out_learnt[i]; // keep, since removing the literal would probably introduce new levels
	    }
	}
        
    }else if (ccmin_mode == 1){
        for (i = j = 1; i < out_learnt.size(); i++){
            Var x = var(out_learnt[i]);

            if (reason(x) == CRef_Undef)
                out_learnt[j++] = out_learnt[i];
            else{
                Clause& c = ca[reason(var(out_learnt[i]))];
		int k = ((c.size()==2) ? 0:1); // bugfix by Siert Wieringa
                for (; k < c.size(); k++)
		{
                    if (! varFlags[var(c[k])].seen && level(var(c[k])) > 0){
                        out_learnt[j++] = out_learnt[i];
                        break;
		    }
		}
#ifdef CLS_EXTRA_INFO
		if( k == c.size() ) {
		  extraInfo = extraInfo >= c.extraInformation() ? extraInfo : c.extraInformation();
		}
#endif
            }
        }
    }else
        i = j = out_learnt.size();

    max_literals += out_learnt.size();
    out_learnt.shrink_(i - j);
    tot_literals += out_learnt.size();
    if( i != j ) recomputeLBD = true; // necessary to recompute LBD here!


      /* ***************************************
	Minimisation with binary clauses of the asserting clause
	First of all : we look for small clauses
	Then, we reduce clauses with small LBD.
	Otherwise, this can be useless
      */

      if( out_learnt.size()<=lbSizeMinimizingClause ) {
	if( recomputeLBD ) lbd = computeLBD(out_learnt); // update current lbd
	recomputeLBD = minimisationWithBinaryResolution(out_learnt, lbd); // code in this method should execute below code until determining correct backtrack level
      }
      
      if( out_learnt.size() <= config.uhle_minimizing_size ) {
	if( recomputeLBD ) lbd = computeLBD(out_learnt); // update current lbd
	recomputeLBD = searchUHLE( out_learnt, lbd );
      }
      
      // rewrite clause only, if one of the two systems added information
      if( out_learnt.size() <= 0 ) { // FIXME not used yet
	if( recomputeLBD ) lbd = computeLBD(out_learnt); // update current lbd
	recomputeLBD = erRewrite(out_learnt, lbd);
      }
    } // end working on usual learnt clause (minimize etc.)
    
    
    // Find correct backtrack level:
    //
    // yet, the currently learned clause is not bi-asserting (bi-asserting ones could be turned into asserting ones by minimization
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else{
        int max_i = 1;
	int decLevelLits = 1;
        // Find the first literal assigned at the next-highest level:
	{
	  for (int i = 2; i < out_learnt.size(); i++){
	    if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
	      max_i = i;
	  }
	}
        // Swap-in this literal at index 1:
        const Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
        out_btlevel       = level(var(p));	// otherwise, use the level of the variable that is moved to the front!
    }

    assert( out_btlevel < decisionLevel() && "there should be some backjumping" );
    
    // Compute LBD, if the current value is not the right value
    if( recomputeLBD ) lbd = computeLBD(out_learnt);

  
#ifdef UPDATEVARACTIVITY
  // UPDATEVARACTIVITY trick (see competition'09 companion paper)
  if(lastDecisionLevel.size()>0 ) {
    for(int i = 0;i<lastDecisionLevel.size();i++) {
      if(ca[reason(var(lastDecisionLevel[i]))].lbd()<lbd) {
	DOUT( if ( config.opt_learn_debug ) cerr << "c add " << lastDecisionLevel[i] << " to bump, with " << ca[reason(var(lastDecisionLevel[i]))].lbd() << " vs " << lbd << endl; );
	varsToBump.push( var(lastDecisionLevel[i]) ) ;
      }
    }
    lastDecisionLevel.clear();
  } 
#endif	    

    for (int j = 0; j < analyze_toclear.size(); j++) {
      DOUT( if( config.opt_learn_debug ) cerr << "c reset seen for " << analyze_toclear[j] << endl; );
      varFlags[var(analyze_toclear[j])].seen = 0;    // ('seen[]' is now cleared)
    }

  // bump the used clauses!
  for( int i = 0 ; i < clssToBump.size(); ++ i ) {
    claBumpActivity( ca[ clssToBump[i] ], config.opt_cls_act_bump_mode == 1 ? out_learnt.size() : lbd );
  }
  for( int i = 0 ; i < varsToBump.size(); ++ i ) {
    varBumpActivity( varsToBump[i], config.opt_var_act_bump_mode == 0 ? 1 : (config.opt_var_act_bump_mode == 1 ? out_learnt.size() : lbd) );
  }
    
#ifdef CLS_EXTRA_INFO // current version of extra info measures the height of the proof. height of new clause is max(resolvents)+1
    extraInfo ++;
#endif
    return 0;

}


// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool Solver::litRedundant(Lit p, uint32_t abstract_levels,uint64_t& extraInfo)
{
    analyze_stack.clear(); analyze_stack.push(p);
    int top = analyze_toclear.size();
    while (analyze_stack.size() > 0){
        assert(reason(var(analyze_stack.last())) != CRef_Undef);
        Clause& c = ca[reason(var(analyze_stack.last()))]; analyze_stack.pop();
	if(c.size()==2 && value(c[0])==l_False) {
	  assert(value(c[1])==l_True);
	  Lit tmp = c[0];
	  c[0] =  c[1], c[1] = tmp;
	}
#ifdef CLS_EXTRA_INFO // if minimization is done, then take also care of the participating clause!
	extraInfo = extraInfo >= c.extraInformation() ? extraInfo : c.extraInformation();
#endif
        for (int i = 1; i < c.size(); i++){
            Lit p  = c[i];
            if (!varFlags[var(p)].seen && level(var(p)) > 0){
                if (reason(var(p)) != CRef_Undef && (abstractLevel(var(p)) & abstract_levels) != 0){ // can be used for minimization
                    varFlags[var(p)].seen = 1;
                    analyze_stack.push(p);
                    analyze_toclear.push(p);
                }else{
                    for (int j = top; j < analyze_toclear.size(); j++)
                        varFlags[var(analyze_toclear[j])].seen = 0;
                    analyze_toclear.shrink_(analyze_toclear.size() - top);
                    return false;
                }
            }
        }
    }

    return true;
}


/*_________________________________________________________________________________________________
|
|  analyzeFinal : (p : Lit)  ->  [void]
|  
|  Description:
|    Specialized analysis procedure to express the final conflict in terms of assumptions.
|    Calculates the (possibly empty) set of assumptions that led to the assignment of 'p', and
|    stores the result in 'out_conflict'.
|________________________________________________________________________________________________@*/
void Solver::analyzeFinal(Lit p, vec<Lit>& out_conflict)
{
    out_conflict.clear();
    out_conflict.push(p);

    if (decisionLevel() == 0)
        return;

    varFlags[var(p)].seen = 1;

    for (int i = trail.size()-1; i >= trail_lim[0]; i--){
        Var x = var(trail[i]);
        if (varFlags[x].seen){
            if (reason(x) == CRef_Undef){
                assert(level(x) > 0);
                out_conflict.push(~trail[i]);
            }else{
                Clause& c = ca[reason(x)];
		//                for (int j = 1; j < c.size(); j++) Minisat (glucose 2.0) loop 
		// Bug in case of assumptions due to special data structures for Binary.
		// Many thanks to Sam Bayless (sbayless@cs.ubc.ca) for discover this bug.
		for (int j = ((c.size()==2) ? 0:1); j < c.size(); j++)
                    if (level(var(c[j])) > 0)
                        varFlags[var(c[j])].seen = 1;
            }  

            varFlags[x].seen = 0;
        }
    }

    varFlags[var(p)].seen = 0;
}


void Solver::uncheckedEnqueue(Lit p, Riss::CRef from, bool addToProof, const uint64_t extraInfo)
{
  /*
   *  Note: this code is also executed during extended resolution, so take care of modifications performed there!
   */
    if( addToProof  ) { // whenever we are at level 0, add the unit to the proof (might introduce duplicates, but this way all units are covered
      assert( decisionLevel() == 0 && "proof can contain only unit clauses, which have to be created on level 0" );
      addCommentToProof("add unit clause that is created during adding the formula");
      addUnitToProof( p );
    }
  
    assert(value(p) == l_Undef && "cannot enqueue a wrong value");
    varFlags[var(p)].assigns = lbool(!sign(p));
    /** include variableExtraInfo here, if required! */
    vardata[var(p)] = mkVarData(from, decisionLevel());
    
//       trailPos[ var(p) ] = (int)trail.size(); /// modified learning, important: before trail.push()!

    // prefetch watch lists
    // __builtin_prefetch( & watchesBin[p], 1, 0 ); // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    __builtin_prefetch( & watches[p], 1, 0 ); // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    DOUT( if(config.opt_printDecisions > 1 ) {cerr << "c uncheched enqueue " << p; if( from != CRef_Undef ) cerr << " because of [" << from << "] " <<  ca[from]; cerr << endl;} );
      
    trail.push_(p);
}


/*_________________________________________________________________________________________________
|
|  propagate : [void]  ->  [Clause*]
|  
|  Description:
|    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
|    otherwise CRef_Undef.
|  
|    Post-conditions:
|      * the propagation queue is empty, even if there was a conflict.
|________________________________________________________________________________________________@*/
CRef Solver::propagate(bool duringAddingClauses)
{
    assert( ( decisionLevel() == 0 || !duringAddingClauses ) && "clauses can only be added at level 0!" );
    // if( config.opt_printLhbr ) cerr << endl << "c called propagate" << endl;
    DOUT( if( config.opt_learn_debug ) cerr << "c call propagate with " << qhead << " for " <<  trail.size() << " lits" << endl; );
  
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll(); clssToBump.clear();
    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        DOUT( if( config.opt_learn_debug ) cerr << "c propagate literal " << p << endl; );
        realHead = qhead;
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;
	
	    // First, Propagate binary clauses 
	const vec<Watcher>&  wbin  = watches[p];
	
	for(int k = 0;k<wbin.size();k++) {
	  if( !wbin[k].isBinary() ) continue;
	  const Lit& imp = wbin[k].blocker();
	  assert( ca[ wbin[k].cref() ].size() == 2 && "in this list there can only be binary clauses" );
	  DOUT( if( config.opt_learn_debug ) cerr << "c checked binary clause " << ca[wbin[k].cref() ] << " with implied literal having value " << toInt(value(imp)) << endl; );
	  if(value(imp) == l_False) {
	    if( !config.opt_long_conflict ) return wbin[k].cref();
	    confl = wbin[k].cref();
	    break;
	  }
	  
	  if(value(imp) == l_Undef) {
	    uncheckedEnqueue(imp,wbin[k].cref(), duringAddingClauses);
	  } 
	}

        // propagate longer clauses here!
        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;)
	{
	    if( i->isBinary() ) { *j++ = *i++; continue; } // skip binary clauses (have been propagated before already!}
	    
	    DOUT( if( config.opt_learn_debug ) cerr << "c check clause [" << i->cref() << "]" << ca[i->cref()] << endl; );
#ifndef PCASSO // PCASS reduces clauses during search without updating the watch lists ...
	    assert( ca[ i->cref() ].size() > 2 && "in this list there can only be clauses with more than 2 literals" );
#endif
	    
            // Try to avoid inspecting the clause:
            const Lit blocker = i->blocker();
            if (value(blocker) == l_True){ // keep binary clauses, and clauses where the blocking literal is satisfied
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            const CRef cr = i->cref();
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit && "wrong literal order in the clause!");
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
#ifndef PCASSO // Pcasso reduces clauses without updating the watch lists
	    assert( c.size() > 2 && "at this point, only larger clauses should be handled!" );
#endif
            const Watcher& w     = Watcher(cr, first, 1); // updates the blocking literal
            if (first != blocker && value(first) == l_True) // satisfied clause
	    {
	      
	      *j++ = w; continue; } // same as goto NextClause;
	    
            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
		    DOUT( if( config.opt_learn_debug ) cerr << "c new watched literal for clause " << ca[cr] << " is " << c[1] <<endl; );
                    watches[~c[1]].push(w);
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!

		
            // Did not find watch -- clause is unit under assignment:
            *j++ = w; 
            // if( config.opt_printLhbr ) cerr << "c keep clause (" << cr << ")" << c << " in watch list while propagating " << p << endl;
            if ( value(first) == l_False ) {
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            } else {
		DOUT( if( config.opt_learn_debug ) cerr << "c current clause is unit clause: " << ca[cr] << endl; );
                uncheckedEnqueue(first, cr, duringAddingClauses);
		
		// if( config.opt_printLhbr ) cerr << "c final common dominator: " << commonDominator << endl;
		
		if( c.mark() == 0  && config.opt_update_lbd == 0) { // if LHBR did not remove this clause
		  int newLbd = computeLBD( c );
		  if( newLbd < c.lbd() || config.opt_lbd_inc ) { // improve the LBD (either LBD decreased,or option is set)
		    if( c.lbd() <= lbLBDFrozenClause ) {
		      c.setCanBeDel( false ); // LBD of clause improved, so that its not considered for deletion
		    }
		    c.setLBD(newLbd);
		  }
		}
	    }
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!

    }
    propagations += num_props;
    simpDB_props -= num_props;
    
    return confl;
}


/*_________________________________________________________________________________________________
|
|  reduceDB : ()  ->  [void]
|  
|  Description:
|    Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
|    clauses are clauses that are reason to some assignment. Binary clauses are never removed.
|________________________________________________________________________________________________@*/
struct reduceDB_lt { 
    ClauseAllocator& ca;
    reduceDB_lt(ClauseAllocator& ca_) : ca(ca_) {}
    bool operator () (CRef x, CRef y) { 
 
    // Main criteria... Like in MiniSat we keep all binary clauses
    if(ca[x].size()> 2 && ca[y].size()==2) return 1;
    
    if(ca[y].size()>2 && ca[x].size()==2) return 0;
    if(ca[x].size()==2 && ca[y].size()==2) return 0;
    
    // Second one  based on literal block distance
    if(ca[x].lbd()> ca[y].lbd()) return 1;
    if(ca[x].lbd()< ca[y].lbd()) return 0;    
    
    
    // Finally we can use old activity or size, we choose the last one
        return ca[x].activity() < ca[y].activity();
	//return x->size() < y->size();

        //return ca[x].size() > 2 && (ca[y].size() == 2 || ca[x].activity() < ca[y].activity()); } 
    }    
};

void Solver::reduceDB()
{
  DOUT( if( config.opt_removal_debug > 0)  cerr << "c reduceDB ..." << endl; );
  reduceDBTime.start();
  int     i, j;
  nbReduceDB++;
  
  if( big != 0 ) {  // build a new BIG that is valid on the current
    big->recreate( ca, nVars(), clauses, learnts );
    big->removeDuplicateEdges( nVars() );
    big->generateImplied( nVars(), add_tmp );
    if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
  }
  
  sort(learnts, reduceDB_lt(ca) ); // sort size 2 and lbd 2 to the back!

  // We have a lot of "good" clauses, it is difficult to compare them. Keep more !
  if(ca[learnts[learnts.size() / RATIOREMOVECLAUSES]].lbd()<=3) nbclausesbeforereduce +=specialIncReduceDB; 
  // Useless :-)
  if(ca[learnts.last()].lbd()<=5)  nbclausesbeforereduce +=specialIncReduceDB; 
  
  
  // Don't delete binary or locked clauses. From the rest, delete clauses from the first half
  // Keep clauses which seem to be usefull (their lbd was reduce during this sequence)

  int limit = learnts.size() / 2;
  DOUT( if( config.opt_removal_debug > 1) cerr << "c reduce limit: " << limit << endl; ) ;

  const int delStart = (int) (config.opt_keep_worst_ratio * (double)learnts.size()); // keep some of the bad clauses!
  for (i = j = 0; i < learnts.size(); i++){
    Clause& c = ca[learnts[i]];
    if (i >= delStart && c.lbd()>2 && c.size() > 2 && c.canBeDel() &&  !locked(c) && (i < limit)) {
      removeClause(learnts[i]);
      nbRemovedClauses++;
      DOUT( if( config.opt_removal_debug > 2) cerr << "c remove clause " << c << endl; );
    }
    else {
      if(!c.canBeDel()) limit++; //we keep c, so we can delete an other clause
      c.setCanBeDel(true);       // At the next step, c can be delete
      DOUT( if( config.opt_removal_debug > 2) cerr << "c keep clause " << c << " due to set 'canBeDel' flag" << endl; ); 
      learnts[j++] = learnts[i];
    }
  }
  learnts.shrink_(i - j);
  DOUT( if( config.opt_removal_debug > 0) cerr << "c resulting learnt clauses: " << learnts.size() << endl; );
  checkGarbage();
  reduceDBTime.stop();
}


void Solver::removeSatisfied(vec<CRef>& cs)
{
  
    int i, j;
    for (i = j = 0; i < cs.size(); i++){
        Clause& c = ca[cs[i]];
        if (c.size() > 1 && satisfied(c)) // do not remove unit clauses!
            removeClause(cs[i]);
        else
            cs[j++] = cs[i];
    }
    cs.shrink_(i - j);
}


void Solver::rebuildOrderHeap()
{
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if ( varFlags[v].decision && value(v) == l_Undef)
            vs.push(v);
    order_heap.build(vs);
}

// NuSMV: PREF MOD
void Solver::addPreferred(Var v)
{
  preferredDecisionVariables.push(v);
}


void Solver::clearPreferred()
{
  preferredDecisionVariables.clear(0);
}
// NuSMV: PREF MOD END
               


/*_________________________________________________________________________________________________
|
|  simplify : [void]  ->  [bool]
|  
|  Description:
|    Simplify the clause database according to the current top-level assigment. Currently, the only
|    thing done here is the removal of satisfied clauses, but more things can be put here.
|________________________________________________________________________________________________@*/
bool Solver::simplify()
{
    // clean watches
    watches.cleanAll();
    
    assert(decisionLevel() == 0);

    if( !config.opt_hpushUnit || startedSolving ) { // push the first propagate until solving started
      if (!ok || propagate() != CRef_Undef)
	  return ok = false;
    }

    if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
        return true;

    // Remove satisfied clauses:
    removeSatisfied(learnts);
    if (remove_satisfied)        // Can be turned off.
        removeSatisfied(clauses);
    checkGarbage();
    rebuildOrderHeap();

    simpDB_assigns = nAssigns();
    simpDB_props   = clauses_literals + learnts_literals;   // (shouldn't depend on stats really, but it will do for now)

    return true;
}

/*_________________________________________________________________________________________________
|
|  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
|  
|  Description:
|    Search for a model the specified number of conflicts. 
|    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
|  
|  Output:
|    'l_True' if a partial assigment that is consistent with respect to the clauseset is found. If
|    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
|    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
|________________________________________________________________________________________________@*/
lbool Solver::search(int nof_conflicts)
{
    assert(ok);
    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    unsigned int nblevels;
    bool blocked=false;
    starts++;
    int proofTopLevels = 0;
    if( trail_lim.size() == 0 ) proofTopLevels = trail.size(); else proofTopLevels  = trail_lim[0]; 
    for (;;){
	propagationTime.start();
        CRef confl = propagate();
	propagationTime.stop();
	
	if( decisionLevel() == 0 && outputsProof() ) { // add the units to the proof that have been added by being propagated on level 0
	  for( ; proofTopLevels < trail.size(); ++ proofTopLevels ) addUnitToProof( trail[ proofTopLevels ] );
	}
	
        if (confl != CRef_Undef){
            // CONFLICT
	  conflicts++; conflictC++;
	  printConflictTrail(confl);

	  updateDecayAndVMTF(); // update dynamic parameters
	  printSearchProgress(); // print current progress
	  
	  if (decisionLevel() == 0) { // top level conflict - stop!
	    return l_False;
	  }

	  trailQueue.push(trail.size());
	  if( conflicts>LOWER_BOUND_FOR_BLOCKING_RESTART && lbdQueue.isvalid()  && trail.size()>R*trailQueue.getavg()) {
	    lbdQueue.fastclear();
	    nbstopsrestarts++;
	    if(!blocked) {lastblockatrestart=starts;nbstopsrestartssame++;blocked=true;}
	  }

	    l1conflicts = ( decisionLevel() != 1 ? l1conflicts : l1conflicts + 1 );
	    
	      learnt_clause.clear();
	      
	      uint64_t extraInfo = 0;
	      analysisTime.start();
	      // perform learnt clause derivation
	      int ret = analyze(confl, learnt_clause, backtrack_level,nblevels,extraInfo);
	      analysisTime.stop();
#ifdef CLS_EXTRA_INFO
	      maxResHeight = extraInfo;
#endif
	      
     
	      // OTFSS TODO put into extra method!
	      bool backTrackedBeyondAsserting = false; // indicate whether learnt clauses can become unit (if no extra backtracking is performed, this stament is true)
	      cancelUntil( backtrack_level );          // cancel trail so that learned clause becomes a unit clause
	      
	      // add the new clause(s) to the solver, perform more analysis on them
	      if( ret > 0 ) { // multiple learned clauses
		if( l_False == handleMultipleUnits( learnt_clause ) ) return l_False;
		updateSleep( &learnt_clause, true ); // share multiple unit clauses!
	      } else { // treat usual learned clause!
		if( l_False == handleLearntClause( learnt_clause, backTrackedBeyondAsserting, nblevels, extraInfo ) ) return l_False;
	      }
	    
	      varDecayActivity();
	      claDecayActivity();

	    conflictsSinceLastRestart ++;
           
        }else{ // there has not been a conflict
	  if( restartSearch(nof_conflicts, conflictC) ) return l_Undef; // perform a restart

	  // Handle Simplification Here!
	  //
          // Simplify the set of problem clauses - but do not do it each iteration!
	  if (simplifyIterations > config.opt_simplifyInterval && decisionLevel() == 0 && !simplify()) {
	    DOUT( if(config.opt_printDecisions > 0) cerr << "c ran simplify" << endl; );
	    simplifyIterations = 0;
	    if( verbosity > 1 ) fprintf(stderr,"c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
	    return l_False;
	  }
	  if( decisionLevel() == 0  ) simplifyIterations ++;
	  
	  if( !withinBudget() ) return l_Undef; // check whether we can still do conflicts
	  
	  // check for communication to the outside (for example in the portfolio solver)
	  int result = updateSleep(0);
	  if( -1 == result ) {
	    // interrupt via communication
	    return l_Undef;
	  } else if( result == 1 ) {
	    // interrupt received a clause that leads to a conflict on level 0
	    conflict.clear();
	    return l_False;
	  }
            
	  // Perform clause database reduction !
	  //
	  clauseRemoval(); // check whether learned clauses should be removed
	    
            Lit next = lit_Undef;
	    bool checkedLookaheadAlready = false;
	    while( next == lit_Undef )
	    {
	      while (decisionLevel() < assumptions.size()){
		  // Perform user provided assumption:
		  Lit p = assumptions[decisionLevel()];

		  if (value(p) == l_True){
		      // Dummy decision level: // do not have a dummy level here!!
		      DOUT( if(config.opt_printDecisions > 0) cerr << "c have dummy decision level for assumptions" << endl; );
		      newDecisionLevel();
		  }else if (value(p) == l_False){
		      analyzeFinal(~p, conflict);
		      return l_False;
		  }else{
		      DOUT( if(config.opt_printDecisions > 0) cerr << "c use assumptoin as next decision : " << p << endl; );
		      next = p;
		      break;
		  }
	      }

	      if (next == lit_Undef){
		  // New variable decision:
		  decisions++;
		  next = pickBranchLit();

		  if (next == lit_Undef){
		    if(verbosity > 1 ) fprintf(stderr,"c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
		    // Model found:
		    return l_True;
		  }
	      }
	      
	      // if sufficiently many new top level units have been learned, trigger another LA!
	      if( !checkedLookaheadAlready ) {
		checkedLookaheadAlready = true; // memorize that we did the check in the first iteration
		if( config.opt_laTopUnit != -1 && topLevelsSinceLastLa >= config.opt_laTopUnit && maxLaNumber != -1) { maxLaNumber ++; topLevelsSinceLastLa = 0 ; }
		if(config.localLookAhead && (maxLaNumber == -1 || (las < maxLaNumber)) ) { // perform LA hack -- only if max. nr is not reached?
		  // if(config.opt_printDecisions > 0) cerr << "c run LA (lev=" << decisionLevel() << ", untilLA=" << untilLa << endl;
		  int hl = decisionLevel();
		  if( hl == 0 ) if( --untilLa == 0 ) { laStart = true; DOUT(if(config.localLookaheadDebug)cerr << "c startLA" << endl;); }
		  if( laStart && hl == config.opt_laLevel ) {
		    if( !laHack(learnt_clause) ) return l_False;
		    topLevelsSinceLastLa = 0;
// 		    cerr << "c drop decision literal " << next << endl;
		    if( !order_heap.inHeap(var(next)) ) order_heap.insert( var(next) ); // add the literal back to the heap!
		    next = lit_Undef;
		    continue; // after local look-ahead re-check the assumptions
		  }
		}
	      }
	      assert( next != lit_Undef && value( next ) == l_Undef && "the literal that is picked exists, and is unassigned" );
	    }
            
            // Increase decision level and enqueue 'next'
            newDecisionLevel();
	    DOUT( if(config.opt_printDecisions > 0) printf("c decide %s%d at level %d\n", sign(next) ? "-" : "", var(next) + 1, decisionLevel() ); );
            uncheckedEnqueue(next);
        }
    }
    return l_Undef;
}

void Solver::clauseRemoval()
{
  if(conflicts>=curRestart* nbclausesbeforereduce && learnts.size() > 0) // perform only if learnt clauses are present
  {
    curRestart = (conflicts/ nbclausesbeforereduce)+1;
    reduceDB();
    nbclausesbeforereduce += incReduceDB;
  }
}


bool Solver::restartSearch(int& nof_conflicts, const int conflictC)
{
  { 
    // dynamic glucose restarts
    if( config.opt_restarts_type == 0 ) {
      // Our dynamic restart, see the SAT09 competition compagnion paper 
      if (
	  ( lbdQueue.isvalid() && ((lbdQueue.getavg()*K) > (sumLBD / (conflicts > 0 ? conflicts : 1 ) )))
	  || (config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound )// if thre have been too many conflicts
	) {
	
	{
	  // increase current limit, if this has been the reason for the restart!!
	  if( (config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound ) ) { 
	    intervalRestart++;conflictsSinceLastRestart = (double)conflictsSinceLastRestart * (double)config.opt_rMaxInc; 
	  }
	  conflictsSinceLastRestart = 0;
	  lbdQueue.fastclear();
	  progress_estimate = progressEstimate();
	  int partialLevel = 0;
	  if( config.opt_restart_level != 0 ) {
	    partialLevel = getRestartLevel();
	    if( partialLevel == -1 ) {
	      if( verbosity > 0 ) {
		static bool didIt = false;
		if( !didIt ) { cerr << "c prevent search from restarting while we have SAT" << endl; didIt = false;}
	      }
	      return false; // we found that we should not restart, because we have a (partial) model
	    }
	  }
	  cancelUntil(partialLevel);
	  return true;
	}
      }
    } else { // usual static luby or geometric restarts
      if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()) {
	
	  {
	    progress_estimate = progressEstimate();
	    int partialLevel = 0;
	    if( config.opt_restart_level != 0 ) {
	      partialLevel = getRestartLevel();
	      if( partialLevel == -1 ) {
		if( verbosity > 0 ) {
		  static bool didIt = false;
		  if( !didIt ) { cerr << "c prevent search from restarting while we have SAT" << endl; didIt = false;}
		}
		return false; // we found that we should not restart, because we have a (partial) model
	      }
	    }
	    cancelUntil(partialLevel);
	    return true;
	  }
      }
    }
  }
  return false;
}


bool Solver::analyzeNewLearnedClause(const CRef newLearnedClause)
{
  if( config.opt_uhdProbe == 0 ) return false;
  
  if( decisionLevel() == 0 ) return false; // no need to analyze the clause, if its satisfied on the top level already!

  const Clause& clause = ca[ newLearnedClause ];
  
  MethodClock mc( bigBackboneTime );
  
  if( clause.size() == 2 ){ // perform the probing algorithm based on a binary clause
    const Lit* aList = big->getArray( clause[0] );
    const Lit* bList = big->getArray( clause[1] );
    const int aSize = big->getSize( clause[0] ) + 1;
    const int bSize = big->getSize( clause[1] ) + 1;
    
    for( int j = 0 ; j < aSize; ++ j ) 
    {
      const Lit aLit = j == 0 ? clause[0] : aList[ j - 1];
      for( int k = 0; k < (( config.opt_uhdProbe > 1 || j == 0 ) ? bSize : 1); ++ k ) // even more expensive method
      {
	const Lit bLit = k == 0 ? clause[1] : bList[ k - 1];
	// a -> aLit -> bLit, and b -> bLit ; thus F \land (a \lor b) -> bLit, and bLit is a backbone!
	
	if( ( big->getStart(aLit) < big->getStart(bLit) && big->getStop(bLit) < big->getStop(aLit) ) 
	// a -> aLit, b -> bLit, -bLit -> -aLit = aLit -> bLit -> F -> bLit
	||  ( big->getStart(~bLit) < big->getStart(~aLit) && big->getStop(~aLit) < big->getStop(~bLit) ) ){
	  if( decisionLevel() != 0 ) cancelUntil(0); // go to level 0, because a unit is added next
	  if( value( bLit ) == l_Undef ) { // only if not set already
	    if ( j == 0 || k == 0) L2units ++; else L3units ++; // stats
	    DOUT( if( config.opt_learn_debug ) cerr << "c uhdPR bin(b) enqueue " << bLit << "@" << decisionLevel() << endl; );
	    uncheckedEnqueue( bLit );
	    addCommentToProof("added by uhd probing:"); addUnitToProof(bLit); // not sure whether DRUP can always find this
	  } else if (value( bLit ) == l_False ) {
	    DOUT( if( config.opt_learn_debug ) cerr << "c contradiction on literal bin(b) " << bLit << "@" << decisionLevel() << " when checking clause " << clause << endl; );
	    return true; // found a contradiction on level 0! on higher decision levels this is not known!
	  }
	} else {
	  if( ( big->getStart(bLit) < big->getStart(aLit) && big->getStop(aLit) < big->getStop(bLit) ) 
	  ||  ( big->getStart(~aLit) < big->getStart(~bLit) && big->getStop(~bLit) < big->getStop(~aLit) ) ){
	    if( decisionLevel() != 0 ) cancelUntil(0); // go to level 0, because a unit is added next
	    if( value( aLit ) == l_Undef ) { // only if not set already
	      if ( j == 0 || k == 0) L2units ++; else L3units ++; // stats
	      DOUT( if( config.opt_learn_debug ) cerr << "c uhdPR bin(a) enqueue " << aLit << "@" << decisionLevel() << endl; );
	      uncheckedEnqueue( aLit);
	      addCommentToProof("added by uhd probing:"); addUnitToProof(aLit);
	    } else if (value( aLit ) == l_False ) {
	      DOUT( if( config.opt_learn_debug ) cerr << "c contradiction on literal bin(a) " << aLit << "@" << decisionLevel() << " when checking clause " << clause << endl; );
	      return true; // found a contradiction
	    }
	  }
	}
	
      }
    }
  } else if (clause.size() > 2 && clause.size() <= config.opt_uhdProbe ) {
      bool oneDoesNotImply = false;
      for( int j = 0 ; j < clause.size(); ++ j ) {
	if( big->getSize( clause[j] ) == 0 ) { oneDoesNotImply = true; break; }
      }
      if( !oneDoesNotImply ) 
      {
	analyzePosition.assign( clause.size(), 0 ); // initialize position of all big lists for the literals in the clause 
	analyzeLimits.assign( clause.size(), 0 );
	bool oneDoesNotImply = false;
	for( int j = 0 ; j < clause.size(); ++ j ) {
	  analyzeLimits[j] = big->getSize( clause[j] ); // initialize current imply test lits
	  sort( big->getArray(clause[j]), big->getSize( clause[j] ) ); // sort all arrays (might be expensive)
	}
	
	bool allInLimit = true;
	
	// this implementation does not cover the case that all literals of a clause except one imply this one literal!
	int whileIteration = 0;
	while( allInLimit ) {
	  whileIteration ++;
	  // find minimum literal
	  bool allEqual = true;
	  Lit minLit = big->getArray( clause[0] )[ analyzePosition[0] ];
	  int minPosition = 0;
	  
	  for( int j = 1 ; j < clause.size(); ++ j ) {
	    if( big->getArray( clause[j] )[ analyzePosition[j] ] < minLit ) {
	      minLit = big->getArray( clause[j] )[ analyzePosition[j] ];
	      minPosition = j;
	    }
	    if( big->getArray( clause[j] )[ analyzePosition[j] ] != big->getArray( clause[j-1] )[ analyzePosition[j-1] ] ) allEqual = false;
	  }
	  
	  if( allEqual ) { // there is a commonly implied literal
	    if( decisionLevel() != 0 ) cancelUntil(0); // go to level 0, because a unit clause in added next
	    if( value( minLit ) == l_Undef ) {
	      L4units ++;
	      DOUT( if( config.opt_learn_debug ) cerr << "c uhdPR long enqueue " << minLit << "@" << decisionLevel() << endl; );
	      uncheckedEnqueue( minLit );
	    } else if (value(minLit) == l_False ){
	      DOUT( if( config.opt_learn_debug ) cerr << "c contradiction on commonly implied liteal " << minLit << "@" << decisionLevel() << " when checking clause " << clause << endl; );
	      return true;
	    }
	    for( int j = 0 ; j < clause.size(); ++ j ) {
	      analyzePosition[j] ++;
	      if( analyzePosition[j] >= analyzeLimits[j] ) allInLimit = false; // stop if we dropped out of the list of implied literals! 
	    }
	  } else { // only move the literal of the minimum!
	    analyzePosition[minPosition] ++;
	    if( analyzePosition[minPosition] >= analyzeLimits[minPosition] ) allInLimit = false; // stop if we dropped out of the list of implied literals!
	  }
	}
	
      }
  }
  return false;
}



void Solver::fillLAmodel( vec<LONG_INT>& pattern, const int steps, vec<Var>& relevantVariables, const bool moveOnly){ // for negative, add bit patter 10, for positive, add bit pattern 01!
  if( !moveOnly ) { // move and add pattern
    int keepVariables = 0;  // number of variables that are kept
    for( int i = 0 ; i < relevantVariables.size(); ++ i ) {
      if( value( relevantVariables[i] ) != l_Undef ) { // only if the variable is kept, move and add!
	relevantVariables[ keepVariables++ ] = relevantVariables[i];
	const Var& v = relevantVariables[i];	// the current kept variable
	const Lit l = mkLit( v, value(v) == l_False ); // the actual literal on the trail
	pattern[v] = (pattern[v]<< (2 * steps ) ); // move the variables according to the number of failed propagations
	pattern[v] +=(sign(l)?2:1);	// add the pattern for the kept variable
      }
    }
    relevantVariables.shrink_( relevantVariables.size() - keepVariables ); // remove the variables that are not needed any more
  } else { // only move all the relevant variables
    for( int i = 0 ; i < relevantVariables.size(); ++ i ) {
	const Var& v = relevantVariables[i];	// the current kept variable
	pattern[v] = (pattern[v]<< (2 * steps ) ); // move the variables according to the number of failed propagations
    }
  }
}

bool Solver::laHack(vec<Lit>& toEnqueue ) {
  assert(decisionLevel() == config.opt_laLevel && "can perform LA only, if level is correct" );
  laTime = cpuTime() - laTime;

  const LONG_INT hit[]   ={5,10,  85,170, 21845,43690, 1431655765,2863311530,  6148914691236517205, 12297829382473034410ull}; // compare numbers for lifted literals
  const LONG_INT hitEE0[]={9, 6, 153,102, 39321,26214, 2576980377,1717986918, 11068046444225730969ull, 7378697629483820646}; // compare numbers for l == dec[0] or l != dec[0]
  const LONG_INT hitEE1[]={0, 0, 165, 90, 42405,23130, 2779096485,1515870810, 11936128518282651045ull, 6510615555426900570}; // compare numbers for l == dec[1]
  const LONG_INT hitEE2[]={0, 0,   0,  0, 43605,21930, 2857740885,1437226410, 12273903644374837845ull, 6172840429334713770}; // compare numbers for l == dec[2]
  const LONG_INT hitEE3[]={0, 0,   0,  0,     0,    0, 2863289685,1431677610, 12297735558912431445ull, 6149008514797120170}; // compare numbers for l == dec[3]
  const LONG_INT hitEE4[]={0, 0,   0,  0,     0,    0,          0,         0, 12297829381041378645ull, 6148914692668172970}; // compare numbers for l == dec[4] 
  // FIXME have another line for level 6 here!
 
//  if(config.localLookaheadDebug) cerr << "c initial pattern: " << pt << endl;
  Lit d[6];
  int j = 0;
  for(int i=0;i<config.opt_laLevel;++i) {
    if( (i == 0 || trail_lim[i] != trail_lim[i-1]) && trail_lim[i] < trail.size() ) // no dummy level caused by assumptions ...
      d[j++]=trail[ trail_lim[i] ]; // get all decisions into dec array
  }
  const int actualLAlevel = j;

  DOUT( if( config.localLookaheadDebug ) {
    cerr << "c LA decisions: "; 
    for( int i = 0 ; i< actualLAlevel; ++ i ) cerr << " " << d[i] << " (vs. [" << trail_lim[i] << "] " << trail[ trail_lim[i] ] << " ) ";
    cerr << endl;
  });

  if(config.tb){ // use tabu
    bool same = true;
    for( int i = 0 ; i < actualLAlevel; ++i ){
      for( int j = 0 ; j < actualLAlevel; ++j )
	if(var(d[i])!=var(hstry[j]) ) same = false; 
    }
    if( same ) { laTime = cpuTime() - laTime; return true; }
    for( int i = 0 ; i < actualLAlevel; ++i ) hstry[i]=d[i];
    for( int i = actualLAlevel; i < config.opt_laLevel; ++i ) hstry[i] = lit_Undef;
  }
  las++;
  
  int bound=1<<actualLAlevel, failedTries = 0;
  
  variablesPattern.growTo(nVars());	// have a pattern for all variables
  memset(&(variablesPattern[0]),0,nVars()*sizeof(LONG_INT)); // initialized to 0
  varFlags.copyTo(backupSolverState);	// backup the current solver state
  LONG_INT patternMask=~0; // current pattern mask, everything set -> == 2^64-1
  
  relevantLAvariables.clear();	// the current set of relevant variables is empty
  int start = 0; // first literal that has been used during propagation
  if( trail_lim.size() > 0 ) start = trail_lim[0];
  // collect all the variables that are put on the trail here
  for( ; start < trail.size(); ++start ) relevantLAvariables.push( var(trail[start]) ); // only these variables are relevant for LA, because more cannot be in the intersection
  start = 0;


  DOUT( if(config.localLookaheadDebug) cerr << "c do LA until " << bound << " starting at level " << decisionLevel() << endl; );
  fillLAmodel(variablesPattern,0,relevantLAvariables); // fill current model
  int failedProbesInARow = 0;
  for(LONG_INT i=1;i<bound;++i){ // for each combination
    cancelUntil(0);
    newDecisionLevel();
    for(int j=0;j<actualLAlevel;++j) uncheckedEnqueue( (i&(1<<j))!=0 ? ~d[j] : d[j] ); // flip polarity of d[j], if corresponding bit in i is set -> enumerate all combinations!
    bool f = propagate() != CRef_Undef; // for tests!
//    if(config.localLookaheadDebug) { cerr << "c propagated iteration " << i << " : " ;  for(int j=0;j<actualLAlevel;++j) cerr << " " << ( (i&(1<<j))!=0 ? ~d[j] : d[j] ) ; cerr << endl; }
    DOUT( if(config.localLookaheadDebug) { cerr << "c corresponding trail: "; if(f) cerr << " FAILED! "; else  for( int j = trail_lim[0]; j < trail.size(); ++ j ) cerr << " "  << trail[j]; cerr << endl; });
    
    if(f) {
      LONG_INT m=3;
      patternMask=(patternMask&(~(m<<(2*i)))); 
      failedTries ++; // global counter
      failedProbesInARow ++; // local counter
    } else {
      fillLAmodel(variablesPattern,failedProbesInARow + 1, relevantLAvariables);
      failedProbesInARow = 0;
    }
//    if(config.localLookaheadDebug) cerr << "c this propafation [" << i << "] failed: " << f << " current match pattern: " << pt << "(inv: " << ~pt << ")" << endl;
    DOUT(if(config.localLookaheadDebug) { cerr << "c cut: "; for(int j=0;j<2<<actualLAlevel;++j) cerr << ((patternMask&(1<<j))  != (LONG_INT)0 ); cerr << endl; });
  }
  if( failedProbesInARow > 0 ) fillLAmodel(variablesPattern,failedProbesInARow,relevantLAvariables,true); // finally, moving all variables right
  cancelUntil(0);
  
  // for(int i=0;i<opt_laLevel;++i) cerr << "c value for literal[" << i << "] : " << d[i] << " : "<< p[ var(d[i]) ] << endl;
  
  int t=2*actualLAlevel-2;
  // evaluate result of LA!
  bool foundUnit=false;
//  if(config.localLookaheadDebug) cerr << "c pos hit: " << (pt & (hit[t])) << endl;
//  if(config.localLookaheadDebug) cerr << "c neg hit: " << (pt & (hit[t+1])) << endl;
  toEnqueue.clear();
  bool doEE = ( (failedTries * 100)/ bound ) < config.opt_laEEp; // enough EE candidates?!
  DOUT(if( config.localLookaheadDebug) cerr << "c tries: " << bound << " failed: " << failedTries << " percent: " <<  ( (failedTries * 100)/ bound ) << " doEE: " << doEE << " current laEEs: " << laEEvars << endl;);
  for(int variableIndex = 0 ; variableIndex < relevantLAvariables.size(); ++ variableIndex) // loop only over the relevant variables
  { 
    const Var& v = relevantLAvariables[variableIndex];
    if(value(v)==l_Undef){ // l_Undef == 2
      if( (patternMask & variablesPattern[v]) == (patternMask & (hit[t])) ){  foundUnit=true;toEnqueue.push( mkLit(v,false) );laAssignments++;
	// cerr << "c LA enqueue " << mkLit(v,false) << " (pat=" << hit[t] << ")" << endl;
      } // pos consequence
      else if( (patternMask & variablesPattern[v]) == (patternMask & (hit[t+1])) ){foundUnit=true;toEnqueue.push( mkLit(v,true)  );laAssignments++;
	// cerr << "c LA enqueue " << mkLit(v,true) << " (pat=" << hit[t] << ")" << endl;
      } // neg consequence
      else if ( doEE  ) { 
	analyze_stack.clear(); // get a new set of literals!
	if( var(d[0]) != v ) {
	  if( (patternMask & variablesPattern[v]) == (patternMask & (hitEE0[t])) ) analyze_stack.push( ~d[0] );
	  else if ( (patternMask & variablesPattern[v]) == (patternMask & (hitEE0[t+1])) ) analyze_stack.push( d[0] );
	}
	if( var(d[1]) != v && hitEE1[t] != 0 ) { // only if the field is valid!
	  if( (patternMask & variablesPattern[v]) == (patternMask & (hitEE1[t])) ) analyze_stack.push( ~d[1] );
	  else if ( (patternMask & variablesPattern[v]) == (patternMask & (hitEE1[t+1])) ) analyze_stack.push( d[1] );
	}
	if( var(d[2]) != v && hitEE2[t] != 0 ) { // only if the field is valid!
	  if( (patternMask & variablesPattern[v]) == (patternMask & (hitEE2[t])) ) analyze_stack.push( ~d[2] );
	  else if ( (patternMask & variablesPattern[v]) == (patternMask & (hitEE2[t+1])) ) analyze_stack.push( d[2] );
	}
	if( var(d[3]) != v && hitEE3[t] != 0 ) { // only if the field is valid!
	  if( (patternMask & variablesPattern[v]) == (patternMask & (hitEE3[t])) ) analyze_stack.push( ~d[3] );
	  else if ( (patternMask & variablesPattern[v]) == (patternMask & (hitEE3[t+1])) ) analyze_stack.push( d[3] );
	}
	if( var(d[4]) != v && hitEE4[t] != 0 ) { // only if the field is valid!
	  if( (patternMask & variablesPattern[v]) == (patternMask & (hitEE4[t])) ) analyze_stack.push( ~d[4] );
	  else if ( (patternMask & variablesPattern[v]) == (patternMask & (hitEE4[t+1])) ) analyze_stack.push( d[4] );
	}
	if( analyze_stack.size() > 0 ) {
	  analyze_toclear.clear();
	  analyze_toclear.push(lit_Undef);analyze_toclear.push(lit_Undef);
	  laEEvars++;
	  laEElits += analyze_stack.size();
	  for( int i = 0; i < analyze_stack.size(); ++ i ) {
	    DOUT(
	      if( config.localLookaheadDebug) {
	      cerr << "c EE [" << i << "]: " << mkLit(v,false) << " <= " << analyze_stack[i] << ", " << mkLit(v,true) << " <= " << ~analyze_stack[i] << endl;
  /*
	      cerr << "c match: " << var(mkLit(v,false)  )+1 << " : " << p[var(mkLit(v,false)  )] << " wrt. cut: " << (pt & p[var(mkLit(v,false)  )]) << endl;
	      cerr << "c match: " << var(analyze_stack[i])+1 << " : " << p[var(analyze_stack[i])] << " wrt. cut: " << (pt & p[var(analyze_stack[i])]) << endl;
	      
	      cerr << "c == " <<  d[0] << " ^= HIT0 - pos: " <<   hitEE0[t] << " wrt. cut: " << (pt & (  hitEE0[t])) << endl;
	      cerr << "c == " << ~d[0] << " ^= HIT0 - neg: " << hitEE0[t+1] << " wrt. cut: " << (pt & (hitEE0[t+1])) << endl;
	      cerr << "c == " <<  d[1] << " ^= HIT1 - pos: " <<   hitEE1[t] << " wrt. cut: " << (pt & (  hitEE1[t])) << endl;
	      cerr << "c == " << ~d[1] << " ^= HIT1 - neg: " << hitEE1[t+1] << " wrt. cut: " << (pt & (hitEE1[t+1])) << endl;
	      cerr << "c == " <<  d[2] << " ^= HIT2 - pos: " <<   hitEE2[t] << " wrt. cut: " << (pt & (  hitEE2[t])) << endl;
	      cerr << "c == " << ~d[2] << " ^= HIT2 - neg: " << hitEE2[t+1] << " wrt. cut: " << (pt & (hitEE2[t+1])) << endl;
	      cerr << "c == " <<  d[3] << " ^= HIT3 - pos: " <<   hitEE3[t] << " wrt. cut: " << (pt & (  hitEE3[t])) << endl;
	      cerr << "c == " << ~d[3] << " ^= HIT3 - neg: " << hitEE3[t+1] << " wrt. cut: " << (pt & (hitEE3[t+1])) << endl;
	      cerr << "c == " <<  d[4] << " ^= HIT4 - pos: " <<   hitEE4[t] << " wrt. cut: " << (pt & (  hitEE4[t])) << endl;
	      cerr << "c == " << ~d[4] << " ^= HIT4 - neg: " << hitEE4[t+1] << " wrt. cut: " << (pt & (hitEE4[t+1])) << endl;
  */
	      }
	    );
	    
	    for( int pol = 0; pol < 2; ++ pol ) { // encode a->b, and b->a
	      analyze_toclear[0] = pol == 0 ? ~analyze_stack[i]  : analyze_stack[i];
	      analyze_toclear[1] = pol == 0 ?     mkLit(v,false) :    mkLit(v,true);
	      CRef cr = ca.alloc(analyze_toclear, config.opt_laEEl ); // create a learned clause?
	      if( config.opt_laEEl ) { 
		ca[cr].setLBD(2);
		learnts.push(cr);
		claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 0 ? 1 : (config.opt_cls_act_bump_mode == 1) ? analyze_toclear.size() : 2 )  ); // bump activity based on its size); }
	      }
	      else clauses.push(cr);
	      attachClause(cr);
	      DOUT( if( config.localLookaheadDebug) cerr << "c add as clause: " << ca[cr] << endl;);
	    }
	  }
	}
	analyze_stack.clear();
  //opt_laEEl
      }
    } 
  }

  analyze_toclear.clear();
  
  // enqueue new units
  for( int i = 0 ; i < toEnqueue.size(); ++ i ) uncheckedEnqueue( toEnqueue[i] );

  // TODO: apply schema to have all learned unit clauses in DRUP! -> have an extra vector!
  if (outputsProof()) {
    
    if( actualLAlevel != 5 ) {
      static bool didIT = false;
      if( ! didIT ) {
	cerr << "c WARNING: DRUP proof can produced only for la level 5!" << endl;
	didIT = true;
      }
    }
    
     // construct look-ahead clauses
    for( int i = 0 ; i < toEnqueue.size(); ++ i ){
      // cerr << "c write literal " << i << " from LA " << las << endl;
      const Lit l = toEnqueue[i];
      localLookAheadProofClauses.clear();
      localLookAheadTmpClause.clear();

      const int litList[] = {0,1,2,3,4,5,-1,0,1,2,3,5,-1,0,1,2,4,5,-1,0,1,2,5,-1,0,1,3,4,5,-1,0,1,3,5,-1,0,1,4,5,-1,0,1,5,-1,0,2,3,4,5,-1,0,2,3,5,-1,0,2,4,5,-1,0,2,5,-1,0,3,4,5,-1,0,3,5,-1,0,4,5,-1,0,5,-1,1,2,3,4,5,-1,1,2,3,5,-1,1,2,4,5,-1,1,2,5,-1,1,3,4,5,-1,1,3,5,-1,1,4,5,-1,1,5,-1,2,3,4,5,-1,2,3,5,-1,2,4,5,-1,2,5,-1,3,4,5,-1,3,5,-1,4,5,-1,5,-1};
      int cCount = 0 ;
      localLookAheadTmpClause.clear();
      assert(actualLAlevel == 5 && "current proof generation only works for level 5!" );
      for ( int j = 0; true; ++ j ) { // TODO: count literals!
	int k = litList[j];
	if( k == -1 ) { localLookAheadProofClauses.push_back(localLookAheadTmpClause);
	  // cerr << "c write " << tmp << endl;
	  localLookAheadTmpClause.clear(); 
	  cCount ++;
	  if( cCount == 32 ) break;
	  continue; 
	}
	if( k == 5 ) localLookAheadTmpClause.push_back( l );
	else localLookAheadTmpClause.push_back( d[k] );
      }

      // write all clauses to proof -- including the learned unit
      addCommentToProof("added by lookahead");
      for( int j = 0; j < localLookAheadProofClauses.size() ; ++ j ){
	if(  0  ) cerr << "c write clause [" << j << "] " << localLookAheadProofClauses[ j ] << endl;
	addToProof(localLookAheadProofClauses[j]);
      }
      // delete all redundant clauses
      addCommentToProof("removed redundant lookahead clauses",true);
      for( int j = 0; j+1 < localLookAheadProofClauses.size() ; ++ j ){
	assert( localLookAheadProofClauses[j].size() > 1 && "the only unit clause in the list should not be removed!" );
	addToProof(localLookAheadProofClauses[j],true);
      }
    }
    
  }

  toEnqueue.clear();
  
  if( propagate() != CRef_Undef ){laTime = cpuTime() - laTime; return false;}
  
  // done with la, continue usual search, until next time something is done
  for( int i = 0 ; i < backupSolverState.size(); ++ i ) varFlags[i].polarity = backupSolverState[i].polarity;
  
  if(config.opt_laDyn){
    if(foundUnit)laBound=config.opt_laEvery; 		// reset down to specified minimum
    else {if(laBound<config.opt_laMaxEvery)laBound++;}	// increase by one!
  }
  laStart=false;untilLa=laBound; // reset counter
  laTime = cpuTime() - laTime;
  
  if(!foundUnit)failedLAs++;
  maxBound=maxBound>laBound ? maxBound : laBound;
  return true;
}


double Solver::progressEstimate() const
{
    double  progress = 0;
    double  F = 1.0 / nVars();

    for (int i = 0; i <= decisionLevel(); i++){
        int beg = i == 0 ? 0 : trail_lim[i - 1];
        int end = i == decisionLevel() ? trail.size() : trail_lim[i];
        progress += pow(F, i) * (end - beg);
    }

    return progress / nVars();
}

/// to create the luby series
static double luby(double y, int x){

    // Find the finite subsequence that contains index 'x', and the
    // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size+1);

    while (size-1 != x){
        size = (size-1)>>1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

lbool Solver::initSolve(int solves)
{
    bool changedActivities = false; // indicate whether the decision heap has to be rebuild
    // reset the counters that guide the search (and stats)
    if( config.opt_reset_counters != 0 && solves % config.opt_reset_counters == 0 ) {
      nbRemovedClauses =0; nbReducedClauses = 0; 
      nbDL2 = 0; nbBin = 0; nbUn = 0; nbReduceDB = 0;
      starts = 0; decisions = 0; rnd_decisions = 0;
      propagations = 0; conflicts = 0; nbstopsrestarts = 0;
      nbstopsrestartssame = 0; lastblockatrestart = 0;
      las=0;failedLAs=0;maxBound=0; maxLaNumber=config.opt_laBound;
      topLevelsSinceLastLa=0; untilLa=config.opt_laEvery;
    }
    
    // initialize activities and polarities
    if( config.opt_init_act != 0 || config.opt_init_pol != 0 ) {
      if( solves == 1
	|| ( config.resetActEvery != 0 && solves % config.resetActEvery == 0 ) 
	|| ( config.resetPolEvery != 0 && solves % config.resetPolEvery == 0 )
      ) {
	double* jw = new double [nVars()]; // todo: could be done in one array with a struct!	
	int32_t* moms = new int32_t [nVars()];
	memset( jw, 0, sizeof( double ) * nVars() );
	memset( moms, 0, sizeof( int32_t ) * nVars() );
	
	for( int i = 0 ; i < clauses.size(); ++ i ) {
	  const Clause& c = ca[clauses[i]];
	  const double cs = 1 / ( pow(2.0, c.size()) );
	  for( int j = 0 ; j < c.size(); ++ j ) {
	    jw[ var(c[j]) ] = (sign(c[j]) ? jw[ var(c[j]) ]  - cs : jw[ var(c[j]) ] + cs );
	    moms[ var(c[j]) ] = (sign(c[j]) ? moms[ var(c[j]) ]  - 1 : moms[ var(c[j]) ] + 1 );
	  }
	}
	// set initialization based on calculated values
	for( Var v = 0 ; v < nVars(); ++ v ) {
	  if( solves == 1 || ( config.resetActEvery != 0 && solves % config.resetActEvery == 0 )) {
	    if( config.opt_init_act == 1 ) activity[v] = v;
	    else if( config.opt_init_act == 2 ) activity[v] = pow(1.0 / config.opt_var_decay_start, 2*v / nVars() );
	    else if( config.opt_init_act == 3 ) activity[nVars() - v - 1] = v;
	    else if( config.opt_init_act == 4 ) activity[nVars() - v - 1] = pow(1.0 / config.opt_var_decay_start, 2*v / nVars() );
	    else if( config.opt_init_act == 5 ) activity[v] = drand(random_seed);
	    else if( config.opt_init_act == 6 ) activity[v] = jw[v] > 0 ? jw[v] : -jw[v];
	    changedActivities = true;
	  }
	  
	  if( solves == 1 || ( config.resetPolEvery != 0 && solves % config.resetPolEvery == 0 ) ) {
	    if( config.opt_init_pol == 1 ) varFlags[v].polarity = jw[v] > 0 ? 0 : 1;
	    else if( config.opt_init_pol == 2 ) varFlags[v].polarity = jw[v] > 0 ? 1 : 0;
	    else if( config.opt_init_pol == 3 ) varFlags[v].polarity = moms[v] > 0 ? 1 : 0;
	    else if( config.opt_init_pol == 4 ) varFlags[v].polarity = moms[v] > 0 ? 0 : 1;
	    else if( config.opt_init_pol == 5 ) varFlags[v].polarity = irand(random_seed,100) > 50 ? 1 : 0;
	    else if( config.opt_init_pol == 6 ) varFlags[v].polarity = ~ varFlags[v].polarity;
	  }
	}
	delete [] moms;
	delete [] jw;
      }
    }
    
    
    // parse for variable polarities from file!
    if( solves == 1 && config.polFile ) { // read polarities from file, initialize phase polarities with this value!
      string filename = string(config.polFile);
      Coprocessor::VarFileParser vfp( filename );
      vector<int> polLits;
      vfp.extract( polLits );
      for( int i = 0 ; i < polLits.size(); ++ i ) {
	const Var v = polLits[i] > 0 ? polLits[i] : - polLits[i];
	if( v - 1 >= nVars() ) continue; // other file might contain more variables
	Lit thisL = mkLit(v-1, polLits[i] < 0 );
	if( config.opt_pol ) thisL = ~thisL;
	varFlags[v-1].polarity = sign(thisL);
      }
      cerr << "c adopted poarity of " << polLits.size() << " variables" << endl;
    }
    
    
    // parse for activities from file!
    if( solves == 1 && config.actFile ) { // set initial activities
      string filename = string(config.actFile);
      Coprocessor::VarFileParser vfp( filename );
      vector<int> actVars;
      vfp.extract(actVars);
      
      double thisValue = config.opt_actStart;
      // reverse the order
      if( config.opt_act == 2 || config.opt_act == 3 ) for( int i = 0 ; i < actVars.size()/2; ++ i ) { int tmp = actVars[i]; actVars[i] = actVars[ actVars.size() - i - 1 ]; actVars[ actVars.size() - i - 1 ] = tmp; }
      for( int i = 0 ; i < actVars.size(); ++ i ) {
	const Var v = actVars[i]-1;
	if( v >= nVars() ) continue; // other file might contain more variables
	activity[ v] = thisValue;
	thisValue = ( (config.opt_act == 0 || config.opt_act == 2 )? thisValue - config.pot_actDec : thisValue / config.pot_actDec );
      }
      cerr << "c adopted activity of " << actVars.size() << " variables" << endl;
      changedActivities = true;
    }
    
    if( changedActivities ) rebuildOrderHeap();
    

    
    //
    // incremental sat solver calls
    //
    if( config.intenseCleaningEvery != 0 && solves % config.intenseCleaningEvery == 0 ) { // clean the learnt clause data base intensively
      int i = 0,j = 0;
      for( ; i < learnts.size(); ++ i ) {
	Clause& c = ca[ learnts[i] ];
	if ( c.size() > config.incKeepSize || c.lbd() > config.incKeepLBD && !locked(c) ) { // remove clauses with too large size or lbd
	  removeClause(learnts[i]);
	} else {
	  learnts[j++] = learnts[i]; // move clause forward! 
	}
      }
      learnts.shrink_(i - j);
    }
    
    return l_Undef;
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_()
{
    totalTime.start();
    startedSolving = true;
    model.clear();
    conflict.clear();
    if (!ok) return l_False;
    lbdQueue.initSize(sizeLBDQueue);
    trailQueue.initSize(sizeTrailQueue);
    sumLBD = 0;
    solves++;
    
    lbool initValue = initSolve(solves);
    if( initValue != l_Undef )  return initValue;

    lbool   status        = l_Undef;
    nbclausesbeforereduce = firstReduceDB;

    printHeader();
    
    // preprocess
    if( status == l_Undef ) { // TODO: freeze variables of assumptions!
	status = preprocess();
	if( config.ppOnly ) return l_Undef; 
    }
    
    if(verbosity>=1) {
      printf("c | solve clauses: %12d  solve variables: %12d                                            |\n", nClauses(), nVars());
      printf("c =========================================================================================================\n");
    }
    
    // probing during search, or UHLE for learnt clauses
    if( config.opt_uhdProbe > 0 || (config.uhle_minimizing_size > 0 && config.uhle_minimizing_lbd > 0) ) {
      if( big == 0 ) big = new Coprocessor::BIG(); // if there is no big yet, create it!
      big->recreate( ca, nVars(), clauses, learnts );
      big->removeDuplicateEdges( nVars() );
      big->generateImplied( nVars(), add_tmp );
      if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
    }
    
    if( false ) {
      cerr << "c solver state after preprocessing" << endl;
      cerr << "c start solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts decision vars: " << order_heap.size() << endl;
      cerr << "c units: " ; for( int i = 0 ; i < trail.size(); ++ i ) cerr << " " << trail[i]; cerr << endl;
      cerr << "c clauses: " << endl; for( int i = 0 ; i < clauses.size(); ++ i ) cerr << "c [" << clauses[i] << "]m: " << ca[clauses[i]].mark() << " == " << ca[clauses[i]] << endl;
      cerr << "c assumptions: "; for ( int i = 0 ; i < assumptions.size(); ++ i ) cerr << " " << assumptions[i]; cerr << endl;
      cerr << "c solve with " << config.presetConfig() << endl;
    }
    
    //
    // Search:
    //
    int curr_restarts = 0; 
    lastReshuffleRestart = 0;
    
    // substitueDisjunctions
    // for now, works only if there are no assumptions!
    int solveVariables = nVars();
    int currentSDassumptions = 0;
    
    
    printSearchHeader();
    
    
    do {
      //if (verbosity >= 1) printf("c start solving with %d assumptions\n", assumptions.size() );
      while (status == l_Undef){
	
	double rest_base = 0;
	if( config.opt_restarts_type != 0 ) // set current restart limit
	  rest_base = config.opt_restarts_type == 1 ? luby(config.opt_restart_inc, curr_restarts) : pow(config.opt_restart_inc, curr_restarts);
	
	// re-shuffle BIG, if a sufficient number of restarts is reached
	if( big != 0 && config.opt_uhdRestartReshuffle > 0 && curr_restarts - lastReshuffleRestart >= config.opt_uhdRestartReshuffle ) {
	  if( nVars() > big->getVars() ) { // rebuild big, if new variables are present
	    big->recreate( ca, nVars(), clauses, learnts ); // build a new BIG that is valid on the "new" formula!
	    big->removeDuplicateEdges( nVars() );
	  }
	  big->generateImplied( nVars(), add_tmp );
	  if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
	  lastReshuffleRestart = curr_restarts; // update number of last restart
	}
	
	status = search(rest_base * config.opt_restart_first); // the parameter is useless in glucose - but interesting for the other restart policies
	if (!withinBudget()) break;
	  curr_restarts++;
	  
	  status = inprocess(status);
      }
      
      // another cegr iteration
      break; // if non of the cegar methods found something, stop here!
    } while ( true ); // if nothing special happens, a single search call is sufficient
    totalTime.stop();
    
    // cerr << "c finish solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts and status " << (status == l_Undef ? "UNKNOWN" : ( status == l_True ? "SAT" : "UNSAT" ) ) << endl;
    
    //
    // print statistic output
    //
    if (verbosity >= 1)
      printf("c =========================================================================================================\n");
    
    if (verbosity >= 1 || config.opt_solve_stats)
    {
#if defined TOOLVERSION && TOOLVERSION < 400

#else
	    const double overheadC = totalTime.getCpuTime() - ( propagationTime.getCpuTime() + analysisTime.getCpuTime() + extResTime.getCpuTime() + preprocessTime.getCpuTime() + inprocessTime.getCpuTime() ); 
	    const double overheadW = totalTime.getWallClockTime() - ( propagationTime.getWallClockTime() + analysisTime.getWallClockTime() + extResTime.getWallClockTime() + preprocessTime.getWallClockTime() + inprocessTime.getWallClockTime() );
	    printf("c Tinimt-Ratios: ratioCpW: %.2lf ,overhead/Total %.2lf %.2lf \n", 
		   totalTime.getCpuTime()/totalTime.getWallClockTime(), overheadC / totalTime.getCpuTime(), overheadW / totalTime.getWallClockTime() );
	    printf("c Timing(cpu,wall, in s): total: %.2lf %.2lf ,prop: %.2lf %.2lf ,analysis: %.2lf %.2lf ,ext.Res.: %.2lf %.2lf ,reduce: %.2lf %.2lf ,overhead %.2lf %.2lf\n",
		   totalTime.getCpuTime(), totalTime.getWallClockTime(), propagationTime.getCpuTime(), propagationTime.getWallClockTime(), analysisTime.getCpuTime(), analysisTime.getWallClockTime(), extResTime.getCpuTime(), extResTime.getWallClockTime(), reduceDBTime.getCpuTime(), reduceDBTime.getWallClockTime(),
		   overheadC, overheadW );
	    printf("c PP-Timing(cpu,wall, in s): preprocessing( %d ): %.2lf %.2lf ,inprocessing (%d ): %.2lf %.2lf\n",
		   preprocessCalls, preprocessTime.getCpuTime(), preprocessTime.getWallClockTime(), inprocessCalls, inprocessTime.getCpuTime(), inprocessTime.getWallClockTime() );
            printf("c Learnt At Level 1: %d  Multiple: %d Units: %d\n", l1conflicts, multiLearnt,learntUnit);
	    printf("c LAs: %lf laSeconds %d LA assigned: %d tabus: %d, failedLas: %d, maxEvery %d, eeV: %d eeL: %d \n", laTime, las, laAssignments, tabus, failedLAs, maxBound, laEEvars, laEElits );
	    printf("c learning: %ld cls, %lf avg. size, %lf avg. LBD, %ld maxSize\n", 
		   (int64_t)totalLearnedClauses, 
		   sumLearnedClauseSize/totalLearnedClauses, 
		   sumLearnedClauseLBD/totalLearnedClauses,
		   (int64_t)maxLearnedClauseSize
		  );
	    printf("c search-UHLE: %d attempts, %d rem-lits\n", searchUHLEs, searchUHLElits );
	    printf("c decisionClauses: %d\n", learnedDecisionClauses );
	    printf("c IntervalRestarts: %d\n", intervalRestart);
	    printf("c partial restarts: %d saved decisions: %d saved propagations: %d recursives: %d\n", rs_partialRestarts, rs_savedDecisions, rs_savedPropagations, rs_recursiveRefinements );
	    printf("c uhd probe: %lf s, %d L2units, %d L3units, %d L4units\n", bigBackboneTime.getCpuTime(), L2units, L3units, L4units );
#endif
    }

    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
	if( model.size() > solveVariables ) model.shrink_( model.size() - solveVariables ); // if SD has been used, nobody knows about these variables, so remove them before doing anything else next
	
    if( false ) {
      cerr << "c solver state after solving with solution" << endl;
      cerr << "c check clauses: " << endl; 
      for( int i = 0 ; i < clauses.size(); ++ i ) {
	int j = 0;
	const Clause& c = ca[clauses[i]];
	for( ; j < c.size(); j++ ) {
	  if( model[ var(c[j]) ] == l_True && !sign(c[j] ) ) break;
	  else if (  model[ var(c[j]) ] == l_False && sign(c[j] ) ) break;
	}
	if( j == c.size() ) cerr << "c unsatisfied clause [" << clauses[i] << "] m: " << ca[clauses[i]].mark() << " == " << ca[clauses[i]] << endl;
      }
    }

	if( coprocessor != 0 && (useCoprocessorPP || useCoprocessorIP) ) coprocessor->extendModel(model);
	
    }else if (status == l_False && conflict.size() == 0)
        ok = false;
    cancelUntil(0);
    
    // cerr << "c finish solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts and status " << (status == l_Undef ? "UNKNOWN" : ( status == l_True ? "SAT" : "UNSAT" ) ) << endl;
    
    return status;
}

//=================================================================================================
// Writing CNF to DIMACS:
// 
// FIXME: this needs to be rewritten completely.

static Var mapVar(Var x, vec<Var>& map, Var& max)
{
    if (map.size() <= x || map[x] == -1){
        map.growTo(x+1, -1);
        map[x] = max++;
    }
    return map[x];
}


void Solver::toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max)
{
    if (satisfied(c)) return;

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False)
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max)+1);
    fprintf(f, "0\n");
}


void Solver::toDimacs(const char *file, const vec<Lit>& assumps)
{
    FILE* f = fopen(file, "wr");
    if (f == NULL)
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    toDimacs(f, assumps);
    fclose(f);
}


void Solver::toDimacs(FILE* f, const vec<Lit>& assumps)
{
    // Handle case when solver is in contradictory state:
    if (!ok){
        fprintf(f, "p cnf 1 2\n1 0\n-1 0\n");
        return; }

    vec<Var> map; Var max = 0;

    // Cannot use removeClauses here because it is not safe
    // to deallocate them at this point. Could be improved.
    int cnt = 0;
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]]))
            cnt++;
        
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]])){
            Clause& c = ca[clauses[i]];
            for (int j = 0; j < c.size(); j++)
                if (value(c[j]) != l_False)
                    mapVar(var(c[j]), map, max);
        }

    // Assumptions are added as unit clauses:
    cnt += assumptions.size();

    fprintf(f, "p cnf %d %d\n", max, cnt);

    for (int i = 0; i < assumptions.size(); i++){
        assert(value(assumptions[i]) != l_False);
        fprintf(f, "%s%d 0\n", sign(assumptions[i]) ? "-" : "", mapVar(var(assumptions[i]), map, max)+1);
    }

    for (int i = 0; i < clauses.size(); i++)
        toDimacs(f, ca[clauses[i]], map, max);

    if (verbosity > 0)
        printf("Wrote %d clauses with %d variables.\n", cnt, max);
}


//=================================================================================================
// Garbage Collection methods:

void Solver::relocAll(ClauseAllocator& to)
{
    // All watchers:
    //
    // for (int i = 0; i < watches.size(); i++)
    watches.cleanAll();
    for (int v = 0; v < nVars(); v++)
        for (int s = 0; s < 2; s++){
            Lit p = mkLit(v, s);
            // printf(" >>> RELOCING: %s%d\n", sign(p)?"-":"", var(p)+1);
            vec<Watcher>& ws = watches[p];
            for (int j = 0; j < ws.size(); j++)
                ca.reloc(ws[j].cref(), to);
        }

    // All reasons:
    //
    for (int i = 0; i < trail.size(); i++){
        Var v = var(trail[i]);
	if ( level(v) == 0 ) vardata[v].reason = CRef_Undef; // take care of reason clauses for literals at top-level
        else
        if (reason(v) != CRef_Undef && (ca[reason(v)].reloced() || locked(ca[reason(v)])))
            ca.reloc(vardata[v].reason, to);
	
    }

    // All learnt:
    //
    int keptClauses = 0;
    for (int i = 0; i < learnts.size(); i++)
    {
        ca.reloc(learnts[i], to);
	if( !to[ learnts[i]].mark() ){
	  learnts[keptClauses++] = learnts[i]; // keep the clause only if its not marked!
	}
    }
    learnts.shrink_( learnts.size() - keptClauses );

    // All original:
    //
    keptClauses = 0;
    for (int i = 0; i < clauses.size(); i++) 
    {
        ca.reloc(clauses[i], to);
	if( !to[ clauses[i]].mark() ) {
	  clauses[keptClauses++] = clauses[i]; // keep the clause only if its not marked!
	}
    }
    clauses.shrink_( clauses.size() - keptClauses );
}


void Solver::garbageCollect()
{
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() >= ca.wasted() ? ca.size() - ca.wasted() : 0); //FIXME security-workaround, for CP3 (due to inconsistend wasted-bug)

    relocAll(to);
    if (verbosity >= 2)
        printf("c |  Garbage collection:   %12d bytes => %12d bytes                                        |\n", 
               ca.size()*ClauseAllocator::Unit_Size, to.size()*ClauseAllocator::Unit_Size);
    to.moveTo(ca);
}

void Solver::buildReduct() {
    cancelUntil( 0 );
    int keptClauses = 0;
    uint64_t remLits = 0;
    for ( int j = 0; j < clauses.size(); ++ j ) {
      int keptLits = 0;
      bool isSat = false;
      Clause& c = ca[ clauses[j] ];
      for( int k = 0 ; k < c.size(); ++ k ) {
	if( value( c[k] ) == l_True ) { isSat = true; break; }
	else if (value( c[k] ) != l_False ) {
	  c[ keptLits ++ ] = c[k];
	} else remLits ++; // literal is falsified
      }
      if( !isSat ) {
	c.shrink(c.size() - keptLits);
	assert( c.size() != 1 && "propagation should have found this unit already" );
	clauses[ keptClauses++ ] = clauses [j];
      }
    }
    cerr << "c removed lits during reduct: " << remLits << " removed cls: " << clauses.size() - keptClauses << endl;
    clauses.shrink_( clauses.size() - keptClauses );
    
}

int Solver::getRestartLevel()
{
  if( config.opt_restart_level == 0 ) return 0;
  else {
    // get decision literal that would be decided next:
    
    if( config.opt_restart_level >= 1 ) {
      
      bool repeatReusedTrail = false;
      Var next = var_Undef;
      int restartLevel = 0;
      do {
	repeatReusedTrail = false; // get it right this time?
	
	// Activity based selection
	while (next == var_Undef || value(next) != l_Undef || ! varFlags[next].decision) // found a yet unassigned variable with the highest activity among the unassigned variables
	    if (order_heap.empty()){
		next = var_Undef;
		break;
	    } else
		next = order_heap.removeMin(); // get next element
	
	if( next == var_Undef ) return -1; // we cannot compare to any other variable, hence, we have SAT already
	// based on variable next, either check for reusedTrail, or matching Trail!
	// activity of the next decision literal
	const double cmpActivity = activity[ next ];
	restartLevel = 0;
	for( int i = 0 ; i < decisionLevel() ; ++i )
	{
	  if( activity[ var( trail[ trail_lim[i] ] ) ] < cmpActivity ) {
	    restartLevel = i;
	    break;
	  }
	}
	order_heap.insert( next ); // put the decision literal back, so that it can be used for the next decision
	
	if( config.opt_restart_level > 1 && restartLevel > 0 ) { // check whether jumping higher would be "more correct"
	  cancelUntil( restartLevel );
	  Var more = var_Undef;
	  while (more == var_Undef || value(more) != l_Undef || ! varFlags[more].decision)
	      if (order_heap.empty()){
		  more = var_Undef;
		  break;
	      }else
		  more = order_heap.removeMin();

	  // actually, would have to jump higher than the current level!
	  if( more != var_Undef && activity[more] > var( trail[ trail_lim[ restartLevel - 1 ] ] ) ) 
	  {
	    repeatReusedTrail = true;
	    next = more; // no need to insert, and get back afterwards again!
	    rs_recursiveRefinements ++;
	  } else {
	    order_heap.insert( more ); 
	  }
	}
      } while ( repeatReusedTrail );
      // stats
      if( restartLevel > 0 ) { // if a partial restart is done 
	rs_savedDecisions += restartLevel;
	const int thisPropSize = restartLevel == decisionLevel() ? trail.size() : trail_lim[ restartLevel ];
	rs_savedPropagations += (thisPropSize - trail_lim[ 0 ]); // number of literals that do not need to be propagated
	rs_partialRestarts ++;
      } 
      // return restart level
      return restartLevel;
    } 
    return 0;
  }
}

uint64_t Solver::defaultExtraInfo() const 
{
  /** overwrite this method! */
  return 0;
}

uint64_t Solver::variableExtraInfo(const Var& v) const
{
  /** overwrite this method! */
#ifdef CLS_EXTRA_INFO
  return vardata[v].extraInfo;
#else
  return 0;
#endif
}

void Solver::setPreprocessor(Coprocessor::Preprocessor* cp)
{
  if ( coprocessor == 0 ) coprocessor = cp;
}

void Solver::setPreprocessor(Coprocessor::CP3Config* _config)
{
  assert( coprocessor == 0 && "there should not exist a preprocessor when this method is called" );
  coprocessor = new Coprocessor::Preprocessor( this, *_config ); 
}

void Solver::printHeader()
{
    if(verbosity>=1) {
      printf("c ========================================[ MAGIC CONSTANTS ]==============================================\n");
      printf("c | Constants are supposed to work well together :-)                                                      |\n");
      printf("c | however, if you find better choices, please let us known...                                           |\n");
      printf("c |-------------------------------------------------------------------------------------------------------|\n");
      printf("c |                                |                                |                                     |\n"); 
      printf("c | - Restarts:                    | - Reduce Clause DB:            | - Minimize Asserting:               |\n");
      printf("c |   * LBD Queue    : %6d      |   * First     : %6d         |    * size < %3d                     |\n",lbdQueue.maxSize(),firstReduceDB,lbSizeMinimizingClause);
      printf("c |   * Trail  Queue : %6d      |   * Inc       : %6d         |    * lbd  < %3d                     |\n",trailQueue.maxSize(),incReduceDB,lbLBDMinimizingClause);
      printf("c |   * K            : %6.2f      |   * Special   : %6d         |                                     |\n",K,specialIncReduceDB);
      printf("c |   * R            : %6.2f      |   * Protected :  (lbd)< %2d     |                                     |\n",R,lbLBDFrozenClause);
      printf("c |                                |                                |                                     |\n"); 
      printf("c =========================================================================================================\n");
    }
}

void Solver::printSearchHeader()
{
    if(verbosity>=1) {
      printf("c ==================================[ Search Statistics (every %6d conflicts) ]=========================\n",verbEveryConflicts);
      printf("c |                                                                                                       |\n"); 
      printf("c |          RESTARTS           |          ORIGINAL         |              LEARNT              | Progress |\n");
      printf("c |       NB   Blocked  Avg Cfc |    Vars  Clauses Literals |   Red   Learnts    LBD2  Removed |          |\n");
      printf("c =========================================================================================================\n");
    }
}

lbool Solver::preprocess()
{
  lbool status = l_Undef;
  // restart, triggered by the solver
  // if( coprocessor == 0 && useCoprocessor) coprocessor = new Coprocessor::Preprocessor(this); // use number of threads from coprocessor
  if( coprocessor != 0 && useCoprocessorPP){
    preprocessCalls++;
    preprocessTime.start();
    status = coprocessor->preprocess();
    preprocessTime.stop();
  }
  if (verbosity >= 1) printf("c =========================================================================================================\n");
  return status;
}


lbool Solver::inprocess(lbool status)
{
  if( status == l_Undef ) {
    // restart, triggered by the solver
    // if( coprocessor == 0 && useCoprocessor)  coprocessor = new Coprocessor::Preprocessor(this); // use number of threads from coprocessor
    if( coprocessor != 0 && useCoprocessorIP) {
      if( coprocessor->wantsToInprocess() ) {
	inprocessCalls ++;
	inprocessTime.start();
	status = coprocessor->inprocess();
	inprocessTime.stop();
	if( big != 0 ) {
	  big->recreate( ca, nVars(), clauses, learnts ); // build a new BIG that is valid on the "new" formula!
	  big->removeDuplicateEdges( nVars() );
	  big->generateImplied( nVars(), add_tmp );
	  if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
	}

      }
    }
  }
  return status;
}


void Solver::printConflictTrail(CRef confl)
{
  DOUT( if( config.opt_printDecisions > 2 ) {
    printf("c conflict at level %d\n", decisionLevel() );
    cerr << "c conflict clause: " << ca[confl] << endl;
    cerr << "c trail: " ;
    for( int i = 0 ; i < trail.size(); ++ i ) {
      cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?"; if( reason(var(trail[i]) ) == CRef_Undef ) cerr << "U"; else cerr <<reason(var(trail[i]));
    } cerr << endl;
  } );
}

void Solver::updateDecayAndVMTF()
{
	  // as in glucose 2.3, increase decay after a certain amount of steps - but have parameters here!
	  if( var_decay < config.opt_var_decay_stop && conflicts % config.opt_var_decay_dist == 0 ) { // div is the more expensive operation!
            var_decay += config.opt_var_decay_inc;
	    var_decay = var_decay >= config.opt_var_decay_stop ? config.opt_var_decay_stop : var_decay; // set upper boung
	  }
	  
	  // update the mixture between VMTF and VSIDS dynamically, similarly to the decay
	  if( useVSIDS != config.opt_vsids_end && conflicts % config.opt_vsids_distance == 0 ) {
	    if( config.opt_vsids_end > config.opt_vsids_start ) {
	      useVSIDS += config.opt_vsids_inc;
	      if( useVSIDS >= config.opt_vsids_end ) useVSIDS = config.opt_vsids_end;
	    } else if (  config.opt_vsids_end < config.opt_vsids_start ) {
	      useVSIDS -= config.opt_vsids_inc;
	      if( useVSIDS <= config.opt_vsids_end ) useVSIDS = config.opt_vsids_end;
	    } else {
	      useVSIDS = config.opt_vsids_end;
	    }
	  }
}

lbool Solver::handleMultipleUnits(vec< Lit >& learnt_clause)
{
  assert( decisionLevel() == 0 && "found units, have to jump to level 0!" );
  lbdQueue.push(1);sumLBD += 1; // unit clause has one level
  for( int i = 0 ; i < learnt_clause.size(); ++ i ) // add all units to current state
  { 
    if( value(learnt_clause[i]) == l_Undef ) uncheckedEnqueue(learnt_clause[i]);
    else if (value(learnt_clause[i]) == l_False ) return l_False; // otherwise, we have a top level conflict here! 
    else 
      DOUT( if( config.opt_learn_debug) cerr << "c tried to enqueue a unit clause that was already a unit clause ... " << endl; );
    DOUT( if( config.opt_printDecisions > 1 ) cerr << "c enqueue multi-learned literal " << learnt_clause[i] << "(" << i << "/" << learnt_clause.size() << ") at level " <<  decisionLevel() << endl; );
  }
    
  // write learned unit clauses to DRUP!
  for (int i = 0; i < learnt_clause.size(); i++){
    addCommentToProof("learnt unit");
    addUnitToProof(learnt_clause[i]);
  }
  // store learning stats!
  totalLearnedClauses += learnt_clause.size(); sumLearnedClauseSize+=learnt_clause.size();sumLearnedClauseLBD+=learnt_clause.size();
  maxLearnedClauseSize = 1 > maxLearnedClauseSize ? 1 : maxLearnedClauseSize;

  multiLearnt = ( learnt_clause.size() > 1 ? multiLearnt + 1 : multiLearnt ); // stats
  topLevelsSinceLastLa ++;
  return l_Undef;
}

lbool Solver::handleLearntClause(vec< Lit >& learnt_clause, bool backtrackedBeyond, unsigned int nblevels, uint64_t extraInfo)
{
  // when this method is called, backjumping has been done already!

  lbdQueue.push(nblevels);
  sumLBD += nblevels;
  // write learned clause to DRUP!
  addCommentToProof("learnt clause");
  addToProof( learnt_clause );
  // store learning stats!
  totalLearnedClauses ++ ; sumLearnedClauseSize+=learnt_clause.size();sumLearnedClauseLBD+=nblevels;
  maxLearnedClauseSize = learnt_clause.size() > maxLearnedClauseSize ? learnt_clause.size() : maxLearnedClauseSize;

  // parallel portfolio: send the learned clause!
  updateSleep(&learnt_clause);
  
  if (learnt_clause.size() == 1){
      assert( decisionLevel() == 0 && "enequeue unit clause on decision level 0!" );
      topLevelsSinceLastLa ++;
  #ifdef CLS_EXTRA_INFO
      vardata[var(learnt_clause[0])].extraInfo = extraInfo;
  #endif
      if( value(learnt_clause[0]) == l_Undef ) {uncheckedEnqueue(learnt_clause[0]);nbUn++;}
      else if (value(learnt_clause[0]) == l_False ) return l_False; // otherwise, we have a top level conflict here!
      DOUT( if( config.opt_printDecisions > 1 ) cerr << "c enqueue learned unit literal " << learnt_clause[0]<< " at level " <<  decisionLevel() << " from clause " << learnt_clause << endl; );
  }else{
    CRef cr = ca.alloc(learnt_clause, true);
    ca[cr].setLBD(nblevels); 
    if(nblevels<=2) nbDL2++; // stats
    if(ca[cr].size()==2) nbBin++; // stats
  #ifdef CLS_EXTRA_INFO
    ca[cr].setExtraInformation(extraInfo);
  #endif
    learnts.push(cr);
    attachClause(cr);
    
    claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 0 ? 1 : (config.opt_cls_act_bump_mode == 1) ? learnt_clause.size() : nblevels )  ); // bump activity based on its size

    uncheckedEnqueue(learnt_clause[0], cr); // this clause is only unit, if OTFSS jumped to the same level!
    DOUT( if( config.opt_printDecisions > 1  ) cerr << "c enqueue literal " << learnt_clause[0] << " at level " <<  decisionLevel() << " from learned clause " << learnt_clause << endl; );

    
    if( analyzeNewLearnedClause( cr ) )   // check whether this clause can be used to imply new backbone literals!
    {
      ok = false; // found a contradtion
      return l_False;	// interupt search!
    }
    
  }
  return l_Undef;
}

void Solver::printSearchProgress()
{
	if (verbosity >= 1 && (verbEveryConflicts == 0 || conflicts % verbEveryConflicts==0) ){
	    printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% | \n", 
		   (int)starts,(int)nbstopsrestarts, (int)(conflicts/starts), 
		   (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
		   (int)nbReduceDB, nLearnts(), (int)nbDL2,(int)nbRemovedClauses, progressEstimate()*100);
	  }
}
