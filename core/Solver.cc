/***************************************************************************************[Solver.cc]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
				CRIL - Univ. Artois, France
				LRI  - Univ. Paris Sud, France
 
Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------

Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2012-2013, Norbert Manthey

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

#include "mtl/Sort.h"
#include "core/Solver.h"
#include "core/Constants.h"

// to be able to use the preprocessor
#include "coprocessor-src/Coprocessor.h"
#include "coprocessor-src/CoprocessorTypes.h"

// to be able to read var files
#include "coprocessor-src/VarFileParser.h"

using namespace Minisat;

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
  , rnd_pol          (false)
  , rnd_init_act     (config.opt_rnd_init_act)
  , garbage_frac     (config.opt_garbage_frac)


    // Statistics: (formerly in 'SolverStats')
    //
  ,  nbRemovedClauses(0),nbReducedClauses(0), nbDL2(0),nbBin(0),nbUn(0) , nbReduceDB(0)
    , solves(0), starts(0), decisions(0), rnd_decisions(0), propagations(0), conflicts(0),nbstopsrestarts(0),nbstopsrestartssame(0),lastblockatrestart(0)
  , dec_vars(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0)
    , curRestart(1)

  , ok                 (true)
  , cla_inc            (1)
  , var_inc            (1)
  , watches            (WatcherDeleted(ca))
  , watchesBin            (WatcherDeleted(ca))
  , qhead              (0)
  , realHead           (0)
  , simpDB_assigns     (-1)
  , simpDB_props       (0)
  , order_heap         (VarOrderLt(activity))
  , progress_estimate  (0)
  , remove_satisfied   (true)
  
    // Resource constraints:
    //
  , conflict_budget    (-1)
  , propagation_budget (-1)
  , asynch_interrupt   (false)
  
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
  
  ,lhbrs(0)
  ,l1lhbrs(0)
  ,lhbr_news(0)
  ,l1lhbr_news(0)
  ,lhbrtests(0)
  ,lhbr_sub(0)
  ,learnedLHBRs(0)
  
  ,simplifyIterations(0)
  ,learnedDecisionClauses(0)
  
  ,otfsss(0)
  ,otfsssL1(0)
  ,otfssClss(0)
  ,otfssUnits(0)
  ,otfssBinaries(0)
  ,otfssHigherJump(0)
  
  , agility( config.opt_agility_init )
  , agility_decay ( config.opt_agility_decay )
  , agility_rejects(0)
  
  , dontTrustPolarity(0)
  
  , doAddVariablesViaER(true)
  
  ,totalLearnedClauses(0)
  ,sumLearnedClauseSize(0)
  ,sumLearnedClauseLBD(0)
  ,maxLearnedClauseSize(0)
  ,extendedLearnedClauses(0)
  ,extendedLearnedClausesCandidates(0)
  ,maxECLclause(0)
  ,totalECLlits(0)
  ,maxResDepth(0)
  
  // restricted extended resolution
  ,rerCommonLitsSum(0)
  ,rerLearnedClause(0)
  ,rerLearnedSizeCandidates(0)
  ,rerSizeReject(0)
  ,rerPatternReject(0)
  ,rerPatternBloomReject(0)
  ,maxRERclause(0)
  ,rerOverheadTrailLits(0)
  ,totalRERlits(0)
  
  // interleaved clause strengthening
  ,dynamicDataUpdates(true)
  ,lastICSconflicts(-1)
  ,icsCalls(0)
  ,icsCandidates(0)
  ,icsDroppedCandidates(0)
  ,icsShrinks(0)
  ,icsShrinkedLits(0)
  
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
  
  , isBiAsserting      (false)
  , biAssertingPostCount (0)
  , biAssertingPreCount  (0)
  
  // cegar
  , cegarLiteralHeap(0)
  
  // replace disjunctions methods
  , sdSteps(0)  
  , sdAssumptions(0) 
  , sdFailedCalls(0) 
  , sdClauses(0)
  , sdLits(0)
  , sdFailedAssumptions(0)
  
  , cbSteps(0)
  , cbClauses(0)
  , cbFailedCalls(0)
  , cbLits(0)
  , cbReduction(0)
  , cbReintroducedClauses(0)
  
  // preprocessor
  , coprocessor(0)
  , useCoprocessorPP(config.opt_usePPpp)
  , useCoprocessorIP(config.opt_usePPip)
{
  MYFLAG=0;
  hstry[0]=lit_Undef;hstry[1]=lit_Undef;hstry[2]=lit_Undef;hstry[3]=lit_Undef;hstry[4]=lit_Undef;
}



Solver::~Solver()
{
  if( coprocessor != 0 ) delete coprocessor;
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
    watchesBin  .init(mkLit(v, false));
    watchesBin  .init(mkLit(v, true ));
    assigns  .push(l_Undef);
    vardata  .push(mkVarData(CRef_Undef, 0));
    //activity .push(0);
    activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
    seen     .push(0);
    permDiff  .push(0);
    polarity .push(sign);
    decision .push();
    trail    .capacity(v+1);
    setDecisionVar(v, dvar);
    
    if( config.opt_hack > 0 ) trailPos.push(-1);
    
    return v;
}

void Solver::reserveVars(Var v)
{
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true ));
    watchesBin  .init(mkLit(v, false));
    watchesBin  .init(mkLit(v, true ));
    
    assigns  .capacity(v+1);
    vardata  .capacity(v+1);
    //activity .push(0);
    activity .capacity(v+1);
    seen     .capacity(v+1);
    permDiff  .capacity(v+1);
    polarity .capacity(v+1);
    decision .capacity(v+1);
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
      ps.shrink(i - j);
    }

    // add to Proof that the clause has been changed
    if ( flag &&  outputsProof() ) {
      addCommentToProof("reduce due to assigned literals, or duplicates");
      addToProof(ps);
      addToProof(oc,true);
    } else if( outputsProof() && config.opt_verboseProof ) {
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
	if( !config.opt_hpushUnit ) return ok = (propagate() == CRef_Undef);
	else return ok;
    }else{
        CRef cr = ca.alloc(ps, false);
        clauses.push(cr);
        attachClause(cr);
    }

    return true;
}


void Solver::attachClause(CRef cr) {
    const Clause& c = ca[cr];
    assert(c.size() > 1);
    
    // check for duplicates here!
//     for (int i = 0; i < c.size(); i++)
//       for (int j = i+1; j < c.size(); j++)
// 	assert( c[i] != c[j] && "have no duplicate literals in clauses!" );
    
    if(c.size()==2) {
      watchesBin[~c[0]].push(Watcher(cr, c[1]));
      watchesBin[~c[1]].push(Watcher(cr, c[0]));
    } else {
//      cerr << "c DEBUG-REMOVE watch clause " << c << " in lists for literals " << ~c[0] << " and " << ~c[1] << endl;
      watches[~c[0]].push(Watcher(cr, c[1]));
      watches[~c[1]].push(Watcher(cr, c[0]));
    }
    if (c.learnt()) learnts_literals += c.size();
    else            clauses_literals += c.size(); }




void Solver::detachClause(CRef cr, bool strict) {
    const Clause& c = ca[cr];
    
    // assert(c.size() > 1 && "there should not be unit clauses - on the other hand, LHBR and OTFSS could create unit clauses");
//     if( c.size() == 1 ) {
//       cerr << "c extra bug - unit clause is removed" << endl;
//       exit( 36 );
//     }
    if(c.size()==2) {
      if (strict){
	if( config.opt_fast_rem ) {
	  removeUnSort(watchesBin[~c[0]], Watcher(cr, c[1])); 
	  removeUnSort(watchesBin[~c[1]], Watcher(cr, c[0])); 
	} else {
	  remove(watchesBin[~c[0]], Watcher(cr, c[1])); // linear (touchs all elements)!
	  remove(watchesBin[~c[1]], Watcher(cr, c[0])); // linear (touchs all elements)!
	}
      }else{
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watchesBin.smudge(~c[0]);
        watchesBin.smudge(~c[1]);
      }
    } else {
      if (strict){
	if( config.opt_fast_rem ) {
	  removeUnSort(watches[~c[0]], Watcher(cr, c[1])); 
	  removeUnSort(watches[~c[1]], Watcher(cr, c[0])); 
	} else {
	  remove(watches[~c[0]], Watcher(cr, c[1])); // linear (touchs all elements)!
	  remove(watches[~c[1]], Watcher(cr, c[0])); // linear (touchs all elements)!
	}
      }else{
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watches.smudge(~c[0]);
        watches.smudge(~c[1]);
      }
    }
    if (c.learnt()) learnts_literals -= c.size();
    else            clauses_literals -= c.size(); }


void Solver::removeClause(Minisat::CRef cr, bool strict) {
  
  Clause& c = ca[cr];

  // tell DRUP that clause has been deleted, if this was not done before already!
  if( c.mark() == 0 ) {
    addCommentToProof("delete via clause removal",true);
    addToProof(c,true);
  }

  detachClause(cr, strict); 
  // Don't leave pointers to free'd memory!
  if (locked(c)) {
    if( config.opt_rer_debug ) cerr << "c remove reason for variable " << var(c[0]) + 1 << ", namely: " << c << endl;
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
  MYFLAG++;
    for(int i=0;i<lits.size();i++) {
      int l = level(var(lits[i]));
      if (permDiff[l] != MYFLAG) {
	permDiff[l] = MYFLAG;
	nblevels++;
      }
    }
  return nblevels;
}

inline unsigned int Solver::computeLBD(const Clause &c) {
  int nblevels = 0;
  MYFLAG++;

    for(int i=0;i<c.size();i++) {
      int l = level(var(c[i]));
      if (permDiff[l] != MYFLAG) {
	permDiff[l] = MYFLAG;
	nblevels++;
      }
    }
  return nblevels;
}


/******************************************************************
 * Minimisation with binary reolution
 ******************************************************************/
void Solver::minimisationWithBinaryResolution(vec<Lit> &out_learnt) {
  
  // Find the LBD measure                                                                                                         
  const unsigned int lbd = computeLBD(out_learnt);
  const Lit p = ~out_learnt[0];

      if(lbd<=lbLBDMinimizingClause){
      MYFLAG++;
      for(int i = 1;i<out_learnt.size();i++) permDiff[var(out_learnt[i])] = MYFLAG;
      vec<Watcher>&  wbin  = watchesBin[p];
      int nb = 0;
      for(int k = 0;k<wbin.size();k++) {
	const Lit imp = wbin[k].blocker;
	if(permDiff[var(imp)]==MYFLAG && value(imp)==l_True) {
	  nb++;
	  permDiff[var(imp)]= MYFLAG-1;
#ifdef CLS_EXTRA_INFO
	  extraInfo = extraInfo >= ca[wbin[k].cref].extraInformation() ? extraInfo : ca[wbin[k].cref].extraInformation();
#endif
	}
      }
      int l = out_learnt.size()-1;
      if(nb>0) {
	nbReducedClauses++;
	for(int i = 1;i<out_learnt.size()-nb;i++) {
	  if(permDiff[var(out_learnt[i])]!=MYFLAG) {
	    const Lit p = out_learnt[l];
	    out_learnt[l] = out_learnt[i];
	    out_learnt[i] = p;
	    l--;i--;
	  }
	}
	out_learnt.shrink(nb);
      }
    }
}

// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
    if (decisionLevel() > level){
        for (int c = trail.size()-1; c >= trail_lim[level]; c--){
            Var      x  = var(trail[c]);
            assigns [x] = l_Undef;
	    vardata [x].dom = lit_Undef; // reset dominator
	    vardata [x].reason = CRef_Undef; // TODO for performance this is not necessary, but for assertions and all that!
            if (phase_saving > 1  ||   ((phase_saving == 1) && c > trail_lim.last())  ) // TODO: check whether publication said above or below: workaround: have another parameter value for the other case!
                polarity[x] = sign(trail[c]);
            insertVarOrder(x); }
        qhead = trail_lim[level];
	realHead = qhead;
        trail.shrink(trail.size() - trail_lim[level]);
        trail_lim.shrink(trail_lim.size() - level);
    } }


//=================================================================================================
// Major methods:


Lit Solver::pickBranchLit()
{
    Var next = var_Undef;

    // Random decision:
    if (drand(random_seed) < random_var_freq && !order_heap.empty()){
        next = order_heap[irand(random_seed,order_heap.size())];
        if (value(next) == l_Undef && decision[next])
            rnd_decisions++; }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || !decision[next])
        if (order_heap.empty()){
            next = var_Undef;
            break;
        }else
            next = order_heap.removeMin();

    const Lit returnLit =  next == var_Undef ? lit_Undef : mkLit(next, rnd_pol ? drand(random_seed) < 0.5 : polarity[next]);
    if( decisionLevel() == 0 ) dontTrustPolarity ++;
    return (config.opt_dontTrustPolarity && decisionLevel() == dontTrustPolarity) ? ~returnLit : returnLit;
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

int Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel,unsigned int &lbd, vec<CRef>& otfssClauses,uint64_t& extraInfo)
{
    isBiAsserting = false; // yet, the currently learned clause is not bi-asserting
    int pathC = 0;
    Lit p     = lit_Undef;
    int pathLimit = 0; // for bi-asserting clauses

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
        assert(confl != CRef_Undef && "there needs to be something to be resolved"); // (otherwise should be UIP)
         if( confl == CRef_Undef ) {
 	  cerr << "c SPECIALLY BUILD-IN FAIL-CHECK in analyze" << endl;
 	  exit( 35 );
 	}
        Clause& c = ca[confl];
	if( config.opt_ecl_debug || config.opt_rer_debug ) cerr << "c resolve on " << p << "(" << index << ") with [" << confl << "]" << c << " -- calculated currentSize: " << currentSize <<  endl;
	int clauseReductSize = c.size();
	// Special case for binary clauses
	// The first one has to be SAT
	if( p != lit_Undef && c.size()==2 && value(c[0])==l_False) {
	  assert(value(c[1])==l_True);
	  Lit tmp = c[0];
	  c[0] =  c[1], c[1] = tmp;
	}
	
	resolvedWithLarger = (c.size() == 2) ? resolvedWithLarger : resolvedWithLarger + 1; // how often do we resolve with a clause whose size is larger than 2?
	
	if (c.learnt() && dynamicDataUpdates){
	    if( config.opt_cls_act_bump_mode == 0 ) claBumpActivity(c);
	    else clssToBump.push( confl );
       }

	if( config.opt_update_lbd == 1 && dynamicDataUpdates ) { // update lbd during analysis, if allowed
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

#ifdef CLS_EXTRA_INFO // if resolution is done, then take also care of the participating clause!
	extraInfo = extraInfo >= c.extraInformation() ? extraInfo : c.extraInformation();
#endif
       
        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            Lit q = c[j];
	    if( config.opt_learn_debug ) cerr << "c level for " << q << " is " << level(var(q)) << endl;
            if (!seen[var(q)] && level(var(q)) > 0){
                currentSize ++;
                if( dynamicDataUpdates ) varsToBump.push( var(q) );
		if( config.opt_learn_debug ) cerr << "c set seen for " << q << endl;
                seen[var(q)] = 1;
                if (level(var(q)) >= decisionLevel()) {
                    pathC++;
#ifdef UPDATEVARACTIVITY
		if( config.opt_updateLearnAct ) {
		    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
		    if((reason(var(q))!= CRef_Undef)  && ca[reason(var(q))].learnt()) 
		      lastDecisionLevel.push(q);
		}
#endif

		} else {
                    out_learnt.push(q);
		    isOnlyUnit = false; // we found a literal from another level, thus the multi-unit clause cannot be learned
		}
	    } else { 
	      if( level(var(q)) == 0 ) clauseReductSize --; // this literal does not count into the size of the clause!
		if (units == 0 && seen[var(q)] && config.opt_biAsserting ) { 
		  if( pathLimit == 0 ) biAssertingPreCount ++;	// count how often learning produced a bi-asserting clause
		  pathLimit = 1; // store that the current learnt clause is a biasserting clause!
		}
	    }

        }
        
	if( currentSize + 1 == clauseReductSize ) { // OTFSS, but on the reduct!
	  if( config.opt_otfss && ( !c.learnt() // apply otfss only for clauses that are considered to be interesting!
	      || ( config.opt_otfssL && c.learnt() && c.lbd() <= config.opt_otfssMaxLBD ) ) ) {
	    otfssClauses.push( confl );
	    if(config.debug_otfss) cerr << "c OTFSS can remove literal " << p << " from " << c << endl;
	  }
	}
        
        if( !isOnlyUnit && units > 0 ) break; // do not consider the next clause, because we cannot continue with units
        
        // Select next clause to look at:
        while (!seen[var(trail[index--])]) {} // cerr << "c check seen for literal " << (sign(trail[index]) ? "-" : " ") << var(trail[index]) + 1 << " at index " << index << " and level " << level( var( trail[index] ) )<< endl;
        p     = trail[index+1];
	lastConfl = confl;
        confl = reason(var(p));
	if( config.opt_learn_debug ) cerr << "c reset seen for " << p << endl;
        seen[var(p)] = 0;
        pathC--;
	currentSize --;

	// do unit analysis only, if the clause did not become larger already!
	if( config.opt_allUipHack > 0  && pathC <= 0 && isOnlyUnit && out_learnt.size() == 1+units ) { 
	  learntUnit ++; 
	  units ++; // count current units
	  out_learnt.push( ~p ); // store unit
	  if( config.opt_allUipHack == 1 ) break; // for now, stop at the first unit! // TODO collect all units
	  pathLimit = 0;	// do not use bi-asserting learning once we found one unit clause
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
    learnedLHBRs = (resolvedWithLarger > 1) ? learnedLHBRs : learnedLHBRs + 1; // count the number of clauses that would have been hyper binary resolvents
    
    
    // if we do not use units, add asserting literal to learned clause!
    if( units == 0 ) {
      out_learnt[0] = ~p; // add the last literal to the clause
      if( pathC > 0 ) { // in case of bi-asserting clauses, the remaining literals have to be collected
	// look for second literal of this level
	while (!seen[var(trail[index--])]);
	p = trail[index+1];
	out_learnt.push( ~p );
      }
    } else { 
      // process learnt units!
      // clear seen
      for( int i = units+1; i < out_learnt.size() ; ++ i ) seen[ var(out_learnt[i]) ] = 0;
      out_learnt.shrink( out_learnt.size() - 1 - units );  // keep units+1 elements!
      
      assert( out_learnt.size() > 1 && "there should have been a unit" );
      out_learnt[0] = out_learnt.last(); out_learnt.pop(); // close gap in vector
      // printf("c first unit is %d\n", var( out_learnt[0] ) + 1 );
      
      out_btlevel = 0; // jump back to level 0!
      
      // clean seen, if more literals have been added
      if( !isOnlyUnit ) while (index >= trail_lim[ decisionLevel() - 1 ] ) seen[ var(trail[index--]) ] = 0;
      
      lbd = 1; // for glucoses LBD score
      return units; // for unit clauses no minimization is necessary
    }

    currentSize ++; // the literal "~p" has been added additionally
    if( currentSize != out_learnt.size() ) cerr << "c different sizes: clause=" << out_learnt.size() << ", counted=" << currentSize << " and collected vector: " << out_learnt << endl;
    assert( currentSize == out_learnt.size() && "counted literals has to be equal to actual clause!" );
    
    if( otfssClauses.size() > 0 && otfssClauses.last() == lastConfl ) {
      if(config.debug_otfss) cerr << "c current learnt clause " << out_learnt << " subsumes otfss clause " << ca[otfssClauses.last()] << endl;
      if( ca[otfssClauses.last()].learnt() ) { 
	if( ca[otfssClauses.last()].mark() == 0 ){
	  addCommentToProof("remove, because subsumed by otfss clause", true );
	  addToProof(ca[otfssClauses.last()], true); // add this to proof, if enabled, and clause still known
	}
	ca[otfssClauses.last()].mark(1);                 // remove this clause!
      }
      otfssClauses.pop(); // do not work on otfss clause, if current learned clause subsumes it anyways!
    }
    
    bool doMinimizeClause = true; // created extra learnt clause? yes -> do not minimize
    if( out_learnt.size() > decisionLevel() ) { // is it worth to check for decisionClause?
      lbd = computeLBD(out_learnt);
      if( lbd > (config.opt_learnDecPrecent * decisionLevel() + 99 ) / 100 ) {
	// instead of learning a very long clause, which migh be deleted very soon (idea by Knuth, already implemented in lingeling(2013)
	for (int j = 0; j < out_learnt.size(); j++) seen[var(out_learnt[j])] = 0;    // ('seen[]' is now cleared)
	out_learnt.clear();
	for( int i = 0; i + 1 < decisionLevel(); ++ i ) {
	  out_learnt.push( ~ trail[ trail_lim[i] ] );
	}
	out_learnt.push( ~p ); // instead of last decision, add UIP!
	out_learnt[ out_learnt.size() -1 ] = out_learnt[0];
	out_learnt[0] = ~p; // move 1st UIP literal to the front!
	learnedDecisionClauses ++;
	if( config.opt_printDecisions > 2) cerr << endl << "c learn decisionClause " << out_learnt << endl << endl;
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
                    if (!seen[var(c[k])] && level(var(c[k])) > 0){
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
    out_learnt.shrink(i - j);
    tot_literals += out_learnt.size();


    /* ***************************************
      Minimisation with binary clauses of the asserting clause
      First of all : we look for small clauses
      Then, we reduce clauses with small LBD.
      Otherwise, this can be useless
     */
    if( out_learnt.size()<=lbSizeMinimizingClause ) {
      minimisationWithBinaryResolution(out_learnt); // code in this method should execute below code until determining correct backtrack level
    }
    
    } // end working on usual learnt clause (minimize etc.)
    
    // Find correct backtrack level:
    //
    // yet, the currently learned clause is not bi-asserting (bi-asserting ones could be turned into asserting ones by minimization
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else{
        int max_i = 1;
        // Find the first literal assigned at the next-highest level:
	if( config.opt_biAsserting ) {
	  int currentLevel = level(var(out_learnt[max_i]));
	  for (int i = 1; i < out_learnt.size(); i++){
	    if( level(var(out_learnt[i])) == decisionLevel() ) { // if there is another literal of the decision level (other than the first one), then the clause is bi-asserting
	      isBiAsserting = true;
	    } // the level of the literals of the current level should not become the backtracking level, hence, this literal is not moved to this position
	    else if (level(var(out_learnt[max_i])) == decisionLevel() || level(var(out_learnt[i])) > level(var(out_learnt[max_i]))) max_i = i; // use any literal, as long as the backjump level is the same as the current level
	  }
	} else {
	  for (int i = 2; i < out_learnt.size(); i++){
	    if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
	      max_i = i;
	  }
	}
        // Swap-in this literal at index 1:
        const Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
	if( out_learnt.size() == 2 && isBiAsserting ) out_btlevel = 0; // for binary bi-asserting clauses, jump back to level 0 always!
        else out_btlevel       = level(var(p));	// otherwise, use the level of the variable that is moved to the front!
    }

    assert( out_btlevel < decisionLevel() && "there should be some backjumping" );
    
    // Compute LBD
    lbd = computeLBD(out_learnt);
    lbd = isBiAsserting ? lbd + 1 : lbd; // for bi-asserting clauses the LBD has to be one larger (approximation), because it is not known whether the one literal would glue the other one

  
#ifdef UPDATEVARACTIVITY
  // UPDATEVARACTIVITY trick (see competition'09 companion paper)
  if(lastDecisionLevel.size()>0 && dynamicDataUpdates) {
    for(int i = 0;i<lastDecisionLevel.size();i++) {
      if(ca[reason(var(lastDecisionLevel[i]))].lbd()<lbd)
	varsToBump.push( var(lastDecisionLevel[i]) ) ;
    }
    lastDecisionLevel.clear();
  } 
#endif	    

    for (int j = 0; j < analyze_toclear.size(); j++) {
      if( config.opt_learn_debug ) cerr << "c reset seen for " << analyze_toclear[j] << endl;
      seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
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
            if (!seen[var(p)] && level(var(p)) > 0){
                if (reason(var(p)) != CRef_Undef && (abstractLevel(var(p)) & abstract_levels) != 0){ // can be used for minimization
                    seen[var(p)] = 1;
                    analyze_stack.push(p);
                    analyze_toclear.push(p);
                }else{
                    for (int j = top; j < analyze_toclear.size(); j++)
                        seen[var(analyze_toclear[j])] = 0;
                    analyze_toclear.shrink(analyze_toclear.size() - top);
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

    seen[var(p)] = 1;

    for (int i = trail.size()-1; i >= trail_lim[0]; i--){
        Var x = var(trail[i]);
        if (seen[x]){
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
                        seen[var(c[j])] = 1;
            }  

            seen[x] = 0;
        }
    }

    seen[var(p)] = 0;
}


void Solver::uncheckedEnqueue(Lit p, CRef from)
{
  /*
   *  Note: this code is also executed during extended resolution, so take care of modifications performed there!
   */
  
    if( dynamicDataUpdates && config.opt_agility_restart_reject ) { // only if technique is enabled:
     // cerr << "c old: " << agility << " lit: " << p << " sign: " << sign(p) << " pol: " << polarity[var(p)] << endl;
      agility = agility * agility_decay + ( sign(p) != polarity[ var(p) ] ? (1.0 - agility_decay) : 0 );
     // cerr << "c new: " << agility << " ... decay: " << agility_decay << endl;
    }
  
    assert(value(p) == l_Undef && "cannot enqueue a wrong value");
    assigns[var(p)] = lbool(!sign(p));
    /** include variableExtraInfo here, if required! */
    vardata[var(p)] = mkVarData(from, decisionLevel());
    
    if( config.opt_hack > 0 )
      trailPos[ var(p) ] = (int)trail.size(); /// modified learning, important: before trail.push()!

    // prefetch watch lists
    if(config.opt_prefetch) __builtin_prefetch( & watches[p] );
    if(config.opt_printDecisions > 1 ) {cerr << "c enqueue " << p; if( from != CRef_Undef ) cerr << " because of " <<  ca[from]; cerr << endl;}
      
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
CRef Solver::propagate()
{
    // if( config.opt_printLhbr ) cerr << endl << "c called propagate" << endl;
  
  
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll();
    watchesBin.cleanAll();
    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        if( config.opt_learn_debug ) cerr << "c propagate literal " << p << endl;
        realHead = qhead;
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;

	if( config.opt_LHBR > 0 && vardata[var(p)].dom == lit_Undef ) {
	  vardata[var(p)].dom = p; // literal is its own dominator, if its not implied due to a binary clause
	  // if( config.opt_printLhbr ) cerr << "c undominated literal " << p << " is dominated by " << p << " (because propagated)" << endl; 
	}
	
	    // First, Propagate binary clauses 
	vec<Watcher>&  wbin  = watchesBin[p];
	
	for(int k = 0;k<wbin.size();k++) {
	  
	  Lit imp = wbin[k].blocker;
	  
	  assert( ca[ wbin[k].cref ].size() == 2 && "in this list there can only be binary clauses" );
	  
	  if( config.opt_learn_debug ) cerr << "c checked binary clause " << ca[wbin[k].cref ] << " with implied literal having value " << toInt(value(imp)) << endl;
	  
	  if(value(imp) == l_False) {
	    if( !config.opt_long_conflict ) return wbin[k].cref;
	    // else
	    confl = wbin[k].cref;
	    break;
	  }
	  
	  if(value(imp) == l_Undef) {
	    uncheckedEnqueue(imp,wbin[k].cref);
	    if( config.opt_LHBR > 0 ) {
	      vardata[ var(imp) ].dom = (config.opt_LHBR == 1 || config.opt_LHBR == 3) ? p : vardata[ var(p) ].dom ; // set dominator
	      // if( config.opt_printLhbr ) cerr << "c literal " << imp << " is dominated by " << p << " (because propagated in binary)" << endl;  
	    }
	  } else {
	    // hack
	      // consider variation only, if the improvement options are enabled!
	      if( (config.opt_hack > 0 ) && reason(var(imp)) != CRef_Undef) { // if its not a decision
		const int implicantPosition = trailPos[ var(imp) ];
		bool fail = false;
	       if( value( p ) != l_False || trailPos[ var(p) ] > implicantPosition ) { fail = true; }

		// consider change only, if the order of positions is correct, e.g. impl realy implies p, otherwise, we found a cycle
		if( !fail ) {
		  if( config.opt_hack_cost ) { // size based cost
		    if( vardata[var(imp)].cost > 2  ) { // 2 is smaller than old reasons size
		      if( config.opt_dbg ) cerr << "c for literal " << imp << " replace reason " << vardata[var(imp)].reason << " with " << wbin[k].cref << endl;
		      vardata[var(imp)].reason = wbin[k].cref;
		      vardata[var(imp)].cost = 2;
		      
		    } 
		  } else { // lbd based cost
		    int thisCost = ca[wbin[k].cref].lbd();
		    if( vardata[var(imp)].cost > thisCost  ) { // 2 is smaller than old reasons size
		      if( config.opt_dbg ) cerr << "c for literal " << imp << " replace reason " << vardata[var(imp)].reason << " with " << wbin[k].cref << endl;
		      vardata[var(imp)].reason = wbin[k].cref;
		      vardata[var(imp)].cost = thisCost;
		    } 
		  }
		} else {
		  // could be a cycle here, or this clause is satisfied, but not a reason clause!
		}
	      }
	  }
	    
	}
    
	if( false && config.opt_printLhbr ) { // debug output for lhbr
	  cerr << "c watch list before propagating literal " << p << endl;
	  int count = 0;
	  Watcher        *wi, *wend;
	  for (wi = (Watcher*)ws, wend = wi + ws.size();  wi != end;) {
	    const CRef     cr        = wi->cref;
            Clause&  c         = ca[cr];
	    cerr << "c [" << count << "] (" << cr << ") " << c << endl;
	    ++ count; wi ++;
	  }
	}

        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;)
	{
	    if( config.opt_learn_debug ) cerr << "c check clause " << ca[i->cref] << endl;
	    assert( ca[ i->cref ].size() > 2 && "in this list there can only be clauses with more than 2 literals" );
	    
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True){
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            const CRef cr = i->cref;
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
	    if( c[1] != false_lit ) { // build in exit
	      cerr << "c wrong literal order in the clause!" << endl;
	      cerr << "c clause (" << cr << "): " << c << endl;
	      assert(c[1] == false_lit && "wrong literal order in the clause!");
	      exit(37);
	    }
            assert(c[1] == false_lit && "wrong literal order in the clause!");
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True) // satisfied clause
	    {
	      // consider variation only, if the improvement options are enabled!
	      if( (config.opt_hack > 0 ) && reason(var(first)) != CRef_Undef) { // if its not a decision
		const int implicantPosition = trailPos[ var(first) ];
		bool fail = false;
		for( int i = 1; i < c.size(); ++ i ) {
		  if( value( c[i] ) != l_False || trailPos[ var(c[i]) ] > implicantPosition ) { fail = true; break; }
		}

		// consider change only, if the order of positions is correct, e.g. impl realy implies p, otherwise, we found a cycle
		if( !fail ) {
		  
		  if( config.opt_hack_cost ) { // size based cost
		    if( vardata[var(first)].cost > c.size()  ) { // 2 is smaller than old reasons size -> update vardata!
		      if( config.opt_dbg ) cerr << "c for literal " << c[0] << " replace reason " << vardata[var(first)].reason << " with " << cr << endl;
		      vardata[var(first)].reason = cr;
		      vardata[var(first)].cost = c.size();
		    }
		  } else { // lbd based cost
		    int thisCost = c.lbd();
		    if( vardata[var(first)].cost > thisCost  ) { // 2 is smaller than old reasons size -> update vardata!
		      if( config.opt_dbg ) cerr << "c for literal " << c[0] << " replace reason " << vardata[var(first)].reason << " with " << cr << endl;
		      vardata[var(first)].reason = cr;
		      vardata[var(first)].cost = thisCost;
		    }
		  }
		} else {
		  // could be a cycle here, or this clause is satisfied, but not a reason clause!
		}
	      } else { // could do inverse arcs here!
		
	      }
	      
	      *j++ = w; continue; } // same as goto NextClause;

	      
	    Lit commonDominator = (config.opt_LHBR > 0 && lhbrs < config.opt_LHBR_max) ? vardata[var(false_lit)].dom : lit_Error; // inidicate whether lhbr should still be performed
	    lhbrtests = commonDominator == lit_Error ? lhbrtests : lhbrtests + 1;
	    // if( config.opt_printLhbr ) cerr << "c common dominator for clause " << c << " : " << commonDominator << endl; 
	    
            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False)
		{
                    c[1] = c[k]; c[k] = false_lit;
		    if( config.opt_learn_debug ) cerr << "c new watched literal for clause " << ca[cr] << " is " << c[1] <<endl;
                    watches[~c[1]].push(w);
                    goto NextClause; 
		} // no need to indicate failure of lhbr, because remaining code is skipped in this case!
                else { // lhbr analysis! - any literal c[k] culd be removed from the clause, because it is not watched at the moment!
		  assert( value(c[k]) == l_False && "for lhbr all literals in the clause need to be set already" );
		  if( commonDominator != lit_Error ) { // do only, if its not broken already - and if limit is not reached
		    // TODO: currently, only lastUIP dominator is used, or "common" dominator, if thre exists any
		    commonDominator = ( commonDominator == lit_Undef ? vardata[var(c[k])].dom : 
				( commonDominator != vardata[var(c[k])].dom ? lit_Error : commonDominator ) );
		    // if( config.opt_printLhbr ) cerr << "c common dominator: " << commonDominator << " after visiting " << c[k] << endl; 
		  }
		}
		
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
		if( config.opt_learn_debug ) cerr << "c current clause is unit clause: " << ca[cr] << endl;
                uncheckedEnqueue(first, cr);
		if( config.opt_LHBR > 0 ) vardata[ var(first) ].dom = (config.opt_LHBR == 1 || config.opt_LHBR == 3) ? first : vardata[ var(first) ].dom ; // set dominator for this variable!
		
		// if( config.opt_printLhbr ) cerr << "c final common dominator: " << commonDominator << endl;
		
		// check lhbr!
		if( commonDominator != lit_Error && commonDominator != lit_Undef ) {
		  lhbrs ++;
		  l1lhbrs = decisionLevel() == 1 ? l1lhbrs + 1 : l1lhbrs;
		  oc.clear();
		  oc.push(first);
		  oc.push(~commonDominator);
		  
		  addCommentToProof("added by LHBR");
		  addToProof( oc ); // if drup is enabled, add this clause to the proof!
		  
		  // if commonDominator is element of the clause itself, delete the clause (hyper-self-subsuming resolution)
		  bool willSubsume = false;
		  if( config.opt_LHBR_sub ) { // check whether the new clause subsumes the other
		    for( int k = 1; k < c.size(); ++ k ) if ( c[k] == ~commonDominator ) { willSubsume = true; break; }
		  }
		  if( willSubsume )
		  { // created a binary clause that subsumes this clause  FIXME: actually replace this clause here instead of creating a new one! adapt code below!
		    if( c.mark() == 0 ){
		      addCommentToProof("Subsumed by LHBR clause", true);
		      addToProof(c,true);  // remove this clause from the proof, if not done already
		    }
		    c.mark(1); 
		    // the bigger clause is not needed any more
		    lhbr_sub ++;
		  } else { // no subsumption
		    // continue with LHBR!
		    lhbr_news ++;
		    l1lhbr_news = decisionLevel() == 1 ? l1lhbr_news + 1 : l1lhbr_news;
		  }
		  // a new clause is required
		  bool clearnt = c.learnt();
		  CRef cr2 = ca.alloc(oc, clearnt ); // add the new clause - now all references could be invalid!
		  if( clearnt ) { ca[cr2].setLBD(1); learnts.push(cr2); ca[cr2].activity() = c.activity(); }
		  else clauses.push(cr2);
		  attachClause(cr2);
		  vardata[var(first)].reason = cr2; // set the new clause as reason
		  vardata[ var(first) ].dom = (config.opt_LHBR == 1 || config.opt_LHBR == 3) ? commonDominator : vardata[ var(commonDominator) ].dom ; // set NEW dominator for this variable!
		  if( config.opt_printLhbr ) {
		    cerr << "c added new clause: " << ca[cr2] << ", that is " << (clearnt ? "learned" : "original" )  << " as position " << (clearnt ? clauses.size() - 1: learnts.size() - 1  ) << " with reference " << cr2 << endl;
		    cerr << "cls: " << clauses.size() << " vs. ls: " << learnts.size() << endl;
		  }
		  goto NextClause; // do not use references any more, because ca.alloc has been performed!
		} 
		
		if( c.mark() == 0 && dynamicDataUpdates && config.opt_update_lbd == 0) { // if LHBR did not remove this clause
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
        ws.shrink(i - j); // remove all duplciate clauses!
	
	if( false && config.opt_printLhbr ) {
	  cerr << "c watch list after propagating literal " << p << endl;
	  int count = 0;
	  for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;) {
	    const CRef     cr        = i->cref;
            Clause&  c         = ca[cr];
	    cerr << "c [" << count << "] (" << cr << ") " << c << endl;
	    ++ count; ++i;
	  }
	}
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
  reduceDBTime.start();
  resetRestrictedExtendedResolution(); // whenever the clause database is touched, forget about current RER step
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

  const int delStart = (int) (config.opt_keep_worst_ratio * (double)learnts.size()); // keep some of the bad clauses!
  for (i = j = 0; i < learnts.size(); i++){
    Clause& c = ca[learnts[i]];
    if (i >= delStart && c.lbd()>2 && c.size() > 2 && c.canBeDel() &&  !locked(c) && (i < limit)) {
      removeClause(learnts[i]);
      nbRemovedClauses++;
    }
    else {
      if(!c.canBeDel()) limit++; //we keep c, so we can delete an other clause
      c.setCanBeDel(true);       // At the next step, c can be delete
      learnts[j++] = learnts[i];
    }
  }
  learnts.shrink(i - j);
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
    cs.shrink(i - j);
}


void Solver::rebuildOrderHeap()
{
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if (decision[v] && value(v) == l_Undef)
            vs.push(v);
    order_heap.build(vs);
}


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
    watchesBin.cleanAll();
    
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
    resetRestrictedExtendedResolution(); // whenever a restart is done, drop current RER step
    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    unsigned int nblevels;
    bool blocked=false;
    starts++;
    for (;;){
	propagationTime.start();
        CRef confl = propagate();
	propagationTime.stop();
	
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
	      otfssCls.clear();
	      
	      uint64_t extraInfo = 0;
	      analysisTime.start();
	      // perform learnt clause derivation
	      int ret = analyze(confl, learnt_clause, backtrack_level,nblevels,otfssCls,extraInfo);
	      analysisTime.stop();
#ifdef CLS_EXTRA_INFO
	      maxResHeight = extraInfo;
#endif
	      if( config.opt_rer_debug ) cerr << "c analyze returns with " << ret << " , jumpLevel " << backtrack_level << " and set of literals " << learnt_clause<< endl;
     
	      // OTFSS TODO put into extra method!
	      lbool otfssResult = otfssProcessClauses(backtrack_level); // takes care of cancelUntil backtracking!
	      if( otfssResult == l_False ) return l_False; // during reducing an OTFSS clause, UNSAT could be shown
	      bool backTrackedBeyondAsserting = (otfssResult == l_True);
	      
	      // add the new clause(s) to the solver, perform more analysis on them
	      if( ret > 0 ) { // multiple learned clauses
		if( l_False == handleMultipleUnits( learnt_clause ) ) return l_False;
	      } else { // treat usual learned clause!
		if( l_False == handleLearntClause( learnt_clause, backTrackedBeyondAsserting, nblevels, extraInfo ) ) return l_False;
	      }
	    
	    if( dynamicDataUpdates ) {
	      varDecayActivity();
	      claDecayActivity();
	    }

	    conflictsSinceLastRestart ++;
           
        }else{ // there has not been a conflict
	  if( restartSearch(nof_conflicts, conflictC) ) return l_Undef; // perform a restart

	  // Handle Simplification Here!
	  //
          // Simplify the set of problem clauses - but do not do it each iteration!
	  if (simplifyIterations > config.opt_simplifyInterval && decisionLevel() == 0 && !simplify()) {
	    simplifyIterations = 0;
	    if( verbosity > 1 ) fprintf(stderr,"c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
	    return l_False;
	  }
	  if( decisionLevel() == 0  ) simplifyIterations ++;
	  
	  if( !withinBudget() ) return l_Undef; // check whether we can still do conflicts
	  
	  // Perform clause database reduction !
	  //
	  clauseRemoval(); // check whether learned clauses should be removed
	      
	    // Simple Inprocessing (deduction techniques that use the solver object
	    //
	    // if this point is reached, check whether interleaved Clause Strengthening could be scheduled (have heuristics!)
	    if( getExtendedResolution() && config.opt_interleavedClauseStrengthening && conflicts != lastICSconflicts && conflicts % config.opt_ics_interval == 0 ) {
	      if( !interleavedClauseStrengthening () ) { // TODO: have some schedule here!
		return l_False;
	      }
	    }
	    
            Lit next = lit_Undef;
            while (decisionLevel() < assumptions.size()){
                // Perform user provided assumption:
                Lit p = assumptions[decisionLevel()];

		if (value(p) == l_True){
                    // Dummy decision level:
                    newDecisionLevel();
                }else if (value(p) == l_False){
                    analyzeFinal(~p, conflict);
                    return l_False;
                }else{
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
	    if( config.opt_laTopUnit != -1 && topLevelsSinceLastLa >= config.opt_laTopUnit && maxLaNumber != -1) { maxLaNumber ++; topLevelsSinceLastLa = 0 ; }
            if(config.hk && (maxLaNumber == -1 || (las < maxLaNumber)) ) { // perform LA hack -- only if max. nr is not reached?
	      int hl = decisionLevel();
	      if( hl == 0 ) if( --untilLa == 0 ) {laStart = true; if(config.dx)cerr << "c startLA" << endl;}
	      if( laStart && hl == config.opt_laLevel ) {
		if( !laHack(learnt_clause) ) return l_False;
		topLevelsSinceLastLa = 0;
	      }
	    }
            
            // Increase decision level and enqueue 'next'
            newDecisionLevel();
	    if(config.opt_printDecisions > 0) printf("c decide %s%d at level %d\n", sign(next) ? "-" : "", var(next) + 1, decisionLevel() );
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
  const bool agilityReject = (config.opt_agility_restart_reject && agility > config.opt_agility_rejectLimit); // would reject if a restart is done now?

  { 
    // dynamic glucose restarts
    if( config.opt_restarts_type == 0 ) {
      // Our dynamic restart, see the SAT09 competition compagnion paper 
      if (
	  ( lbdQueue.isvalid() && ((lbdQueue.getavg()*K) > (sumLBD / conflicts)))
	  || (config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound )// if thre have been too many conflicts
	) {
	
	// reject by agility?
	if( agilityReject ) {
	  agility_rejects ++;
	  lbdQueue.fastclear(); // force solver to wait for another lbdQueue.maxsize conflicts!
	} else {
	  // increase current limit, if this has been the reason for the restart!!
	  if( (config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound ) ) { 
	    intervalRestart++;conflictsSinceLastRestart = (double)conflictsSinceLastRestart * (double)config.opt_rMaxInc; 
	  }
	  conflictsSinceLastRestart = 0;
	  lbdQueue.fastclear();
	  progress_estimate = progressEstimate();
	  const int restartLevel = config.opt_restart_level == 0 ? 0 : getRestartLevel(); // use matching trail, or reused trail mechanism!
	  cancelUntil(restartLevel);
	  return true;
	}
      }
    } else { // usual static luby or geometric restarts
      if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()) {
	
	  if( withinBudget() && agilityReject ) {
	    agility_rejects ++;
	    nof_conflicts += config.opt_agility_limit_increase;
	  } else {
	    progress_estimate = progressEstimate();
	    const int restartLevel = config.opt_restart_level == 0 ? 0 : getRestartLevel(); // use matching trail, or reused trail mechanism!
	    cancelUntil(restartLevel);
	    // cerr << "c restart after " << conflictC << " conflicts - limit: " << nof_conflicts << endl;
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
	  if( value( bLit ) == l_Undef ) { // only if not set already
	    if ( j == 0 || k == 0) L2units ++; else L3units ++; // stats
	    if( decisionLevel() != 0 ) cancelUntil(0);
	    uncheckedEnqueue( bLit );
	    addCommentToProof("added by uhd probing:"); addUnitToProof(bLit); // not sure whether DRUP can always find this
	  } else if (value( bLit ) == l_False ) return true; // found a contradiction
	} else {
	  if( ( big->getStart(bLit) < big->getStart(aLit) && big->getStop(aLit) < big->getStop(bLit) ) 
	  ||  ( big->getStart(~aLit) < big->getStart(~bLit) && big->getStop(~bLit) < big->getStop(~aLit) ) ){
	    if( value( aLit ) == l_Undef ) { // only if not set already
	    if ( j == 0 || k == 0) L2units ++; else L3units ++; // stats
	    if( decisionLevel() != 0 ) cancelUntil(0);
	    uncheckedEnqueue( aLit);
	    addCommentToProof("added by uhd probing:"); addUnitToProof(aLit);
	  } else if (value( aLit ) == l_False ) return true; // found a contradiction
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
	    
	    if( value( minLit ) == l_Undef ) {
	      L4units ++;
	      uncheckedEnqueue( minLit );
	    } else if (value(minLit) == l_False ) return true;
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



void Solver::fm(uint64_t*p,bool mo){ // for negative, add bit patter 10, for positive, add bit pattern 01!
  for(Var v=0;v<nVars();++v) p[v]=(p[v]<<2); // move two bits, so that the next assignment can be put there
if(!mo) for(int i=trail_lim[0];i<trail.size();++i){
    Lit l=trail[i];
    p[var(l)]+=(sign(l)?2:1);
  }
}

bool Solver::laHack(vec<Lit>& toEnqueue ) {
  assert(decisionLevel() == config.opt_laLevel && "can perform LA only, if level is correct" );
  laTime = cpuTime() - laTime;

  uint64_t hit[]   ={5,10,  85,170, 21845,43690, 1431655765,2863311530,  6148914691236517205, 12297829382473034410ull}; // compare numbers for lifted literals
  uint64_t hitEE0[]={9, 6, 153,102, 39321,26214, 2576980377,1717986918, 11068046444225730969ull, 7378697629483820646}; // compare numbers for l == dec[0] or l != dec[0]
  uint64_t hitEE1[]={0, 0, 165, 90, 42405,23130, 2779096485,1515870810, 11936128518282651045ull, 6510615555426900570}; // compare numbers for l == dec[1]
  uint64_t hitEE2[]={0, 0,   0,  0, 43605,21930, 2857740885,1437226410, 12273903644374837845ull, 6172840429334713770}; // compare numbers for l == dec[2]
  uint64_t hitEE3[]={0, 0,   0,  0,     0,    0, 2863289685,1431677610, 12297735558912431445ull, 6149008514797120170}; // compare numbers for l == dec[3]
  uint64_t hitEE4[]={0, 0,   0,  0,     0,    0,          0,         0, 12297829381041378645ull, 6148914692668172970}; // compare numbers for l == dec[4] 
  // TODO: remove white spaces, output, comments and assertions!
  uint64_t p[nVars()];
  memset(p,0,nVars()*sizeof(uint64_t)); // TODO: change sizeof into 4!
  vec<char> bu;
  polarity.copyTo(bu);  
  uint64_t pt=~0; // everything set -> == 2^64-1
  if(config.dx) cerr << "c initial pattern: " << pt << endl;
  Lit d[5];
  for(int i=0;i<config.opt_laLevel;++i) d[i]=trail[ trail_lim[i] ]; // get all decisions into dec array

  if(config.tb){ // use tabu
    bool same = true;
    for( int i = 0 ; i < config.opt_laLevel; ++i ){
      for( int j = 0 ; j < config.opt_laEvery; ++j )
	if(var(d[i])!=var(hstry[j]) ) same = false; 
    }
    if( same ) { laTime = cpuTime() - laTime; return true; }
    for( int i = 0 ; i < config.opt_laLevel; ++i ) hstry[i]=d[i];
  }
  las++;
  
  int bound=1<<config.opt_laLevel, failedTries = 0;
  if(config.dx) cerr << "c do LA until " << bound << " starting at level " << decisionLevel() << endl;
  fm(p,false); // fill current model
  for(uint64_t i=1;i<bound;++i){ // for each combination
    cancelUntil(0);
    newDecisionLevel();
    for(int j=0;j<config.opt_laLevel;++j) uncheckedEnqueue( (i&(1<<j))!=0 ? ~d[j] : d[j] ); // flip polarity of d[j], if corresponding bit in i is set -> enumerate all combinations!
    bool f = propagate() != CRef_Undef; // for tests!
    if(config.dx) { cerr << "c propagated iteration " << i << " : " ;  for(int j=0;j<config.opt_laLevel;++j) cerr << " " << ( (i&(1<<j))!=0 ? ~d[j] : d[j] ) ; cerr << endl; }
    if(config.dx) { cerr << "c corresponding trail: "; if(f) cerr << " FAILED! "; else  for( int j = trail_lim[0]; j < trail.size(); ++ j ) cerr << " "  << trail[j]; cerr << endl; }
    fm(p,f);
    uint64_t m=3;
    if(f) {pt=(pt&(~(m<<(2*i)))); failedTries ++; }
    if(config.dx) cerr << "c this propafation [" << i << "] failed: " << f << " current match pattern: " << pt << "(inv: " << ~pt << ")" << endl;
    if(config.dx) { cerr << "c cut: "; for(int j=0;j<2<<config.opt_laLevel;++j) cerr << ((pt&(1<<j))  != (uint64_t)0 ); cerr << endl; }
  }
  cancelUntil(0);
  
  // for(int i=0;i<opt_laLevel;++i) cerr << "c value for literal[" << i << "] : " << d[i] << " : "<< p[ var(d[i]) ] << endl;
  
  int t=2*config.opt_laLevel-2;
  // evaluate result of LA!
  bool foundUnit=false;
  if(config.dx) cerr << "c pos hit: " << (pt & (hit[t])) << endl;
  if(config.dx) cerr << "c neg hit: " << (pt & (hit[t+1])) << endl;
  toEnqueue.clear();
  bool doEE = ( (failedTries * 100)/ bound ) < config.opt_laEEp; // enough EE candidates?!
  if( config.dx) cerr << "c tries: " << bound << " failed: " << failedTries << " percent: " <<  ( (failedTries * 100)/ bound ) << " doEE: " << doEE << " current laEEs: " << laEEvars << endl;
  for(Var v=0; v<nVars(); ++v ){
    if(value(v)==l_Undef){ // l_Undef == 2
      if( (pt & p[v]) == (pt & (hit[t])) ){  foundUnit=true;toEnqueue.push( mkLit(v,false) );laAssignments++;} // pos consequence
      else if( (pt & p[v]) == (pt & (hit[t+1])) ){foundUnit=true;toEnqueue.push( mkLit(v,true)  );laAssignments++;} // neg consequence
      else if ( doEE  ) { 
	analyze_stack.clear(); // get a new set of literals!
	if( var(d[0]) != v ) {
	  if( (pt & p[v]) == (pt & (hitEE0[t])) ) analyze_stack.push( ~d[0] );
	  else if ( (pt & p[v]) == (pt & (hitEE0[t+1])) ) analyze_stack.push( d[0] );
	}
	if( var(d[1]) != v && hitEE1[t] != 0 ) { // only if the field is valid!
	  if( (pt & p[v]) == (pt & (hitEE1[t])) ) analyze_stack.push( ~d[1] );
	  else if ( (pt & p[v]) == (pt & (hitEE1[t+1])) ) analyze_stack.push( d[1] );
	}
	if( var(d[2]) != v && hitEE2[t] != 0 ) { // only if the field is valid!
	  if( (pt & p[v]) == (pt & (hitEE2[t])) ) analyze_stack.push( ~d[2] );
	  else if ( (pt & p[v]) == (pt & (hitEE2[t+1])) ) analyze_stack.push( d[2] );
	}
	if( var(d[3]) != v && hitEE3[t] != 0 ) { // only if the field is valid!
	  if( (pt & p[v]) == (pt & (hitEE3[t])) ) analyze_stack.push( ~d[3] );
	  else if ( (pt & p[v]) == (pt & (hitEE3[t+1])) ) analyze_stack.push( d[3] );
	}
	if( var(d[4]) != v && hitEE4[t] != 0 ) { // only if the field is valid!
	  if( (pt & p[v]) == (pt & (hitEE4[t])) ) analyze_stack.push( ~d[4] );
	  else if ( (pt & p[v]) == (pt & (hitEE4[t+1])) ) analyze_stack.push( d[4] );
	}
	if( analyze_stack.size() > 0 ) {
	  analyze_toclear.clear();
	  analyze_toclear.push(lit_Undef);analyze_toclear.push(lit_Undef);
	  laEEvars++;
	  laEElits += analyze_stack.size();
	  for( int i = 0; i < analyze_stack.size(); ++ i ) {
	    if( config.dx) {
	    cerr << "c EE [" << i << "]: " << mkLit(v,false) << " <= " << analyze_stack[i] << ", " << mkLit(v,true) << " <= " << ~analyze_stack[i] << endl;
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
	    }
	    
	    for( int pol = 0; pol < 2; ++ pol ) { // encode a->b, and b->a
	      analyze_toclear[0] = pol == 0 ? ~analyze_stack[i]  : analyze_stack[i];
	      analyze_toclear[1] = pol == 0 ?     mkLit(v,false) :    mkLit(v,true);
	      CRef cr = ca.alloc(analyze_toclear, config.opt_laEEl ); // create a learned clause?
	      if( config.opt_laEEl ) { 
		ca[cr].setLBD(2);
		learnts.push(cr);
		if(dynamicDataUpdates) claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 0 ? 1 : (config.opt_cls_act_bump_mode == 1) ? analyze_toclear.size() : 2 )  ); // bump activity based on its size); }
	      }
	      else clauses.push(cr);
	      attachClause(cr);
	      if( config.dx) cerr << "c add as clause: " << ca[cr] << endl;
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
    vector< vector< Lit > > clauses;
    vector<Lit> tmp;
    
    if( config.opt_laLevel != 5 ) {
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
      clauses.clear();
      tmp.clear();

      int litList[] = {0,1,2,3,4,5,-1,0,1,2,3,5,-1,0,1,2,4,5,-1,0,1,2,5,-1,0,1,3,4,5,-1,0,1,3,5,-1,0,1,4,5,-1,0,1,5,-1,0,2,3,4,5,-1,0,2,3,5,-1,0,2,4,5,-1,0,2,5,-1,0,3,4,5,-1,0,3,5,-1,0,4,5,-1,0,5,-1,1,2,3,4,5,-1,1,2,3,5,-1,1,2,4,5,-1,1,2,5,-1,1,3,4,5,-1,1,3,5,-1,1,4,5,-1,1,5,-1,2,3,4,5,-1,2,3,5,-1,2,4,5,-1,2,5,-1,3,4,5,-1,3,5,-1,4,5,-1,5,-1};
      int cCount = 0 ;
      tmp.clear();
      assert(config.opt_laLevel == 5 && "current proof generation only works for level 5!" );
      for ( int j = 0; true; ++ j ) { // TODO: count literals!
	int k = litList[j];
	if( k == -1 ) { clauses.push_back(tmp);
	  // cerr << "c write " << tmp << endl;
	  tmp.clear(); 
	  cCount ++;
	  if( cCount == 32 ) break;
	  continue; 
	}
	if( k == 5 ) tmp.push_back( l );
	else tmp.push_back( d[k] );
      }

      // write all clauses to proof -- including the learned unit
      addCommentToProof("added by lookahead");
      for( int j = 0; j < clauses.size() ; ++ j ){
	if(  0  ) cerr << "c write clause [" << j << "] " << clauses[ j ] << endl;
	addToProof(clauses[j]);
      }
      // delete all redundant clauses
      addCommentToProof("removed redundant lookahead clauses",true);
      for( int j = 0; j+1 < clauses.size() ; ++ j ){
	assert( clauses[j].size() > 1 && "the only unit clause in the list should not be removed!" );
	addToProof(clauses[j],true);
      }
    }
    
  }

  toEnqueue.clear();
  
  if( propagate() != CRef_Undef ){laTime = cpuTime() - laTime; return false;}
  
  // done with la, continue usual search, until next time something is done
  bu.copyTo(polarity); // restore polarities from before!
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
	    if( config.opt_init_pol == 1 ) polarity[v] = jw[v] > 0 ? 0 : 1;
	    else if( config.opt_init_pol == 2 ) polarity[v] = jw[v] > 0 ? 1 : 0;
	    else if( config.opt_init_pol == 3 ) polarity[v] = moms[v] > 0 ? 1 : 0;
	    else if( config.opt_init_pol == 4 ) polarity[v] = moms[v] > 0 ? 0 : 1;
	    else if( config.opt_init_pol == 5 ) polarity[v] = irand(random_seed,100) > 50 ? 1 : 0;
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
	polarity[ v-1 ] = sign(thisL);
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
      learnts.shrink(i - j);
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
    
    // probing during search:
    if( config.opt_uhdProbe > 0 ) {
      assert( big == 0 && "cannot be initialized already" );
      big = new Coprocessor::BIG();
      big->create( ca, nVars(), clauses, learnts );
      big->removeDuplicateEdges( nVars() );
      big->generateImplied( nVars(), add_tmp );
      if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
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
    
    initCegar(assumptions, currentSDassumptions, solves);
    
    printSearchHeader();
    
    sdSearchTime.start();
    do {
      sdLastIterTime.reset(); sdLastIterTime.start();
      if (verbosity >= 1) printf("c start solving with %d assumptions\n", assumptions.size() );
      while (status == l_Undef){
	
	double rest_base = 0;
	if( config.opt_restarts_type != 0 ) // set current restart limit
	  rest_base = config.opt_restarts_type == 1 ? luby(config.opt_restart_inc, curr_restarts) : pow(config.opt_restart_inc, curr_restarts);
	
	// re-shuffle BIG, if a sufficient number of restarts is reached
	if( big != 0 && config.opt_uhdRestartReshuffle > 0 && curr_restarts - lastReshuffleRestart >= config.opt_uhdRestartReshuffle ) {
	  big->generateImplied( nVars(), add_tmp );
	  if( config.opt_uhdProbe > 2 ) big->sort( nVars() ); // sort all the lists once
	  lastReshuffleRestart = curr_restarts; // update number of last restart
	}
	
	status = search(rest_base * config.opt_restart_first); // the parameter is useless in glucose - but interesting for the other restart policies
	if (!withinBudget()) break;
	  curr_restarts++;
	  
	  status = inprocess(status);
      }
      sdLastIterTime.stop();
      
      // another cegr iteration
      if( cegarNextIteration( assumptions, currentSDassumptions, status ) ) continue;
      break; // if non of the cegar methods found something, stop here!
    } while ( true ); // if nothing special happens, a single search call is sufficient
    sdSearchTime.stop();
    totalTime.stop();
    
    if( big != 0 ) { big->BIG::~BIG(); big = 0; } // clean up!
    
    //
    // print statistic output
    //
    if (verbosity >= 1)
      printf("c =========================================================================================================\n");
    
    if (verbosity >= 1 || config.opt_solve_stats) {
#if defined TOOLVERSION && TOOLVERSION < 400

#else
	    const double overheadC = totalTime.getCpuTime() - ( propagationTime.getCpuTime() + analysisTime.getCpuTime() + extResTime.getCpuTime() + preprocessTime.getCpuTime() + inprocessTime.getCpuTime() ); 
	    const double overheadW = totalTime.getWallClockTime() - ( propagationTime.getWallClockTime() + analysisTime.getWallClockTime() + extResTime.getWallClockTime() + preprocessTime.getWallClockTime() + inprocessTime.getWallClockTime() );
	    printf("c Tinimt-Ratios: ratioCpW: %.2lf ,overhead/Total %.2lf %.2lf \n", 
		   totalTime.getCpuTime()/totalTime.getWallClockTime(), overheadC / totalTime.getCpuTime(), overheadW / totalTime.getWallClockTime() );
	    printf("c Timing(cpu,wall, in s): total: %.2lf %.2lf ,prop: %.2lf %.2lf ,analysis: %.2lf %.2lf ,ext.Res.: %.2lf %.2lf ,reduce: %.2lf %.2lf ,overhead %.2lf %.2lf\n",
		   totalTime.getCpuTime(), totalTime.getWallClockTime(), propagationTime.getCpuTime(), propagationTime.getWallClockTime(), analysisTime.getCpuTime(), analysisTime.getWallClockTime(), extResTime.getCpuTime(), extResTime.getWallClockTime(), reduceDBTime.getCpuTime(), reduceDBTime.getWallClockTime(),
		   overheadC, overheadW );
	    printf("c PP-Timing(cpu,wall, in s): preprocessing: %.2lf %.2lf ,inprocessing: %.2lf %.2lf\n",
		   preprocessTime.getCpuTime(), preprocessTime.getWallClockTime(), inprocessTime.getCpuTime(), inprocessTime.getWallClockTime() );
            printf("c Learnt At Level 1: %d  Multiple: %d Units: %d\n", l1conflicts, multiLearnt,learntUnit);
	    printf("c LAs: %lf laSeconds %d LA assigned: %d tabus: %d, failedLas: %d, maxEvery %d, eeV: %d eeL: %d \n", laTime, las, laAssignments, tabus, failedLAs, maxBound, laEEvars, laEElits );
	    printf("c lhbr: %d (l1: %d ),new: %d (l1: %d ),tests: %d ,subs: %d ,byLearning: %d\n", lhbrs, l1lhbrs,lhbr_news,l1lhbr_news,lhbrtests,lhbr_sub, learnedLHBRs);
	    printf("c otfss: %d (l1: %d ),cls: %d ,units: %d ,binaries: %d ,jumpedHigher: %d\n", otfsss, otfsssL1,otfssClss,otfssUnits,otfssBinaries,otfssHigherJump);
	    printf("c learning: %ld cls, %lf avg. size, %lf avg. LBD, %ld maxSize, %ld proof-depth\n", 
		   (int64_t)totalLearnedClauses, 
		   sumLearnedClauseSize/totalLearnedClauses, 
		   sumLearnedClauseLBD/totalLearnedClauses,
		  (int64_t)maxLearnedClauseSize,
		   (int64_t)maxResDepth
		  );
	    printf("c ext.cl.l.: %d outOf %d ecls, %d maxSize, %.2lf avgSize, %.2lf totalLits\n",
		   extendedLearnedClauses,extendedLearnedClausesCandidates,maxECLclause, extendedLearnedClauses == 0 ? 0 : ( totalECLlits / (double)extendedLearnedClauses), totalECLlits);
	    printf("c res.ext.res.: %d rer, %d rerSizeCands, %d sizeReject, %d patternReject, %d bloomReject, %d maxSize, %.2lf avgSize, %.2lf totalLits\n",
		   rerLearnedClause, rerLearnedSizeCandidates, rerSizeReject, rerPatternReject, rerPatternBloomReject, maxRERclause, rerLearnedClause == 0 ? 0 : (totalRERlits / (double) rerLearnedClause), totalRERlits );
	    printf("c i.cls.strengthening: %.2lf seconds, %d calls, %d candidates, %d droppedBefore, %d shrinked, %d shrinkedLits\n", icsTime.getCpuTime(), icsCalls, icsCandidates, icsDroppedCandidates, icsShrinks, icsShrinkedLits );
	    printf("c bi-asserting: %d pre-Mini, %d post-Mini, %.3lf rel-pre, %.3lf rel-post\n", biAssertingPreCount, biAssertingPostCount, 
		   totalLearnedClauses == 0 ? 0 : (double) biAssertingPreCount / (double)totalLearnedClauses,
		   totalLearnedClauses == 0 ? 0 : (double) biAssertingPostCount / (double)totalLearnedClauses
		  );
	    printf("c decisionClauses: %d\n", learnedDecisionClauses );
	    printf("c IntervalRestarts: %d\n", intervalRestart);
	    printf("c partial restarts: %d saved decisions: %d saved propagations: %d recursives: %d\n", rs_partialRestarts, rs_savedDecisions, rs_savedPropagations, rs_recursiveRefinements );
	    printf("c agility restart rejects: %d\n", agility_rejects );
	    printf("c uhd probe: %lf s, %d L2units, %d L3units, %d L4units\n", bigBackboneTime.getCpuTime(), L2units, L3units, L4units );
	    printf("c OGS rewrite: %lf s, %d steps, %d assumptions, %d sdClauses, %d sdLits, %d failedCalls, %d failedAssumptions, %lf searchTime, %lf lastIterTime, \n", sdTime.getCpuTime(), sdSteps, sdAssumptions, sdClauses, sdLits, sdFailedCalls, sdFailedAssumptions, sdSearchTime.getCpuTime(), sdLastIterTime.getCpuTime() );
	    printf("c cegarBVA : %lf s, %d steps, %d cbVariables, %d cbReduction, %d cbFailedCalls, %d cegarClauses, %d cbReaddCls\n", cbTime.getCpuTime(), 
		   cbSteps, 
	    cbLits, 
	    cbReduction, 
	    cbFailedCalls, 
	    cbClauses, 
	    cbReintroducedClauses);
#endif
    }

    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
	if( model.size() > solveVariables ) model.shrink( model.size() - solveVariables ); // if SD has been used, nobody knows about these variables, so remove them before doing anything else next
	
	if( coprocessor != 0 && (useCoprocessorPP || useCoprocessorIP) ) coprocessor->extendModel(model);
	
    }else if (status == l_False && conflict.size() == 0)
        ok = false;
    cancelUntil(0);
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
    watchesBin.cleanAll();
    for (int v = 0; v < nVars(); v++)
        for (int s = 0; s < 2; s++){
            Lit p = mkLit(v, s);
            // printf(" >>> RELOCING: %s%d\n", sign(p)?"-":"", var(p)+1);
            vec<Watcher>& ws = watches[p];
            for (int j = 0; j < ws.size(); j++)
                ca.reloc(ws[j].cref, to);
	    
            vec<Watcher>& ws2 = watchesBin[p];
            for (int j = 0; j < ws2.size(); j++)
                ca.reloc(ws2[j].cref, to);
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
    learnts.shrink ( learnts.size() - keptClauses );

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
    clauses.shrink ( clauses.size() - keptClauses );
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


bool Solver::extendedClauseLearning( vec< Lit >& currentLearnedClause, unsigned int& lbd, uint64_t& extraInfo )
{
  if( ! config.opt_extendedClauseLearning ) return false; // not enabled -> do nothing!
  if( lbd > config.opt_ecl_maxLBD ) return false; // do this only for interesting clauses
  if( currentLearnedClause.size() < config.opt_ecl_minSize ) return false; // do this only for clauses, that seem to be relevant to be split! (have a better heuristic here!)
  if( (double)extendedLearnedClauses * config.opt_ecl_every > conflicts ) return false; // do not consider this clause!

  // stats
  extendedLearnedClausesCandidates ++;
  maxECLclause = maxECLclause >= currentLearnedClause.size() ? maxECLclause : currentLearnedClause.size();
  totalECLlits += currentLearnedClause.size();
  
  // resort the clause, s.t. the smallest two literals are at the back!
  int smallestLevel = decisionLevel(); // there need to be literals at/below the current level!
  int litsAtSmallest = 0; // the asserting literals should not have a level at the moment!
  bool foundUnit = false, assertingClause = true;
  
  if( config.opt_ecl_debug) cerr << "c transform current learned clause " << currentLearnedClause << endl;
  
  for( int i = 1 ; i < currentLearnedClause.size(); ++ i ) { // assume the level of the very first literal is the highest anyways!
    const int lev = level( var( currentLearnedClause[i] )) ; 
    if( lev == -1 ) { // handle literals without levels - there should be only one!
      if( foundUnit ) { assertingClause = false; } // is it a problem to work with non-asserting clauses?
      foundUnit = true;
      continue;
    } else if(lev < smallestLevel ) {
      litsAtSmallest = 1; // reached new level!
      const Lit tmp = currentLearnedClause[i]; // swap the literal to the front!
      currentLearnedClause[i] = currentLearnedClause[ litsAtSmallest ];
      currentLearnedClause[ litsAtSmallest ] = tmp;
      smallestLevel = lev;
    } else if(lev == smallestLevel )  { // move the smallest literals to the front (except very first), keep track of their number!
      const Lit tmp = currentLearnedClause[i];
      litsAtSmallest ++;
      currentLearnedClause[i] = currentLearnedClause[ litsAtSmallest ];
      currentLearnedClause[ litsAtSmallest ] = tmp;
    } else {
      // literal level is higher -> nothing to be done! 
    }
  }
  // swap last two literals to second and third position
  // swap literal with second highest level to second position
  Lit tmp = currentLearnedClause[1];
  currentLearnedClause[1] = currentLearnedClause[ currentLearnedClause.size() -2 ];
  currentLearnedClause[ currentLearnedClause.size() -2 ] = tmp;
  tmp = currentLearnedClause[2];
  currentLearnedClause[2] = currentLearnedClause[ currentLearnedClause.size() -1 ];
  currentLearnedClause[ currentLearnedClause.size() -1 ] = tmp;
  int pos = 1; int lev = level( var(currentLearnedClause[1]) );
  for( int i = 2 ; i < currentLearnedClause.size(); ++ i ) if( level( var(currentLearnedClause[i])) > lev ) { pos = i; lev = level( var(currentLearnedClause[i])) ; }
  tmp = currentLearnedClause[1];
  currentLearnedClause[1] = currentLearnedClause[ pos ];
  currentLearnedClause[ pos ] = tmp;
  
  // do not process this clause further, if the smallest level is not low enough!
  // either, compared to the absolute parameter
  // or (relativ) compared to the current decision level
  if( (config.opt_ecl_smallLevel > 0) ? (smallestLevel > config.opt_ecl_smallLevel) : ( - config.opt_ecl_smallLevel < (double)smallestLevel / (double)decisionLevel()  ) ) return false;
  
  // process the learned clause!
  if( config.opt_ecl_debug) cerr << "c into " << currentLearnedClause << " | with listAtSmallest= " << litsAtSmallest << endl;
  if( config.opt_ecl_debug) cerr << "c detailed: " << endl;
  for( int i = 0 ; i < currentLearnedClause.size(); ++ i  ) {
    if( config.opt_ecl_debug) cerr << "c " << i <<  " : " << currentLearnedClause[i] << " @ " << level( var(currentLearnedClause[i] )) << endl;
  }
  
  if( litsAtSmallest < 2 ){
    if( config.opt_ecl_debug) cerr << "c reject " << endl;
    return false; // do only work on the class, if there are (more than) 2 literals on the smallest level!
  }

  oc.clear(); // this is the vector for adding the new extension ... still need to think about whether to add the full extension ...

  const Lit l1 = currentLearnedClause[ currentLearnedClause.size() - 1 ];
  const Lit l2 = currentLearnedClause[ currentLearnedClause.size() - 2 ];
  if (level(var(l1)) == -1 || level(var(l2)) == -1 ) {
     if( config.opt_ecl_debug) cerr << "c reject " << endl;
    return false; // perform only, if lowest two literals are still assigned!
  }
  
  const Var x = newVar(true,true,'e'); // this is the fresh variable!
  vardata[x].level = level(var(l1));

  assigns[x] = lbool(true);
  
  oc.push( mkLit(x,false) );
  oc.push( l1 ); // has to have an order?
  oc.push( l2 );
  // this is the first clause, the second and third literals should be mapped to false
  // thus, this clause is going to be the reason clause for the literal 'x'
  // and should be assigned directly after the position of the two literals literal! (Huang performed a restart, but this is not wanted here, due to side effects)
  if( config.opt_ecl_debug) cerr << "c before trail: " << trail << endl;
  int ui = trail.size() - 1;
  trail.push(mkLit(x,false));
  //for( int i = 0 ; i < trail.size(); ++ i ) if( trail[i] == ~l1 ) {cerr << "c found l1(" << l1 << ") at " << i << endl; break;}
  //for( int i = 0 ; i < trail.size(); ++ i ) if( trail[i] == ~l2 ) {cerr << "c found l2(" << l2 << ") at " << i << endl; break;}
  

  while( ui > 0 && trail[ui] != ~l1 && trail[ui] != ~l2 ) {
    const Lit tmp = trail[ui]; trail[ui] = trail[ui+1]; trail[ui+1] = tmp; // swap!
    if( config.opt_hack > 0 ) trailPos[ var( tmp ) ] ++; // memorize that this literal is pushed up now
    ui --; // go down trail until one of the two literals is hit
  } // now, x is on the right position (immediately behind the latter of the two literals
  if( config.opt_hack > 0 )  trailPos[ x ] = ui + 1; // memorize position of new variable
  
  if( config.opt_ecl_debug) {
    cerr << "c after trail: " ;
    for( int i = 0 ; i < trail.size(); ++ i ) {
      cerr << " " << trail[i] << "@" << level(var(trail[i]));
    }
    cerr << endl;
    cerr << "c perform extension, new var " << x+1 << ", added at trail position " << ui << "/" << trail.size() << " and at level " << level( var(l2) ) << " with current jump-level " << decisionLevel() << endl;
  }
  
  if( config.opt_ecl_debug) cerr << "c CONSIDER: the level of the two literals could be differennt, then the LBD of the learned clause would be reduced!" << endl;
  int ml = level(var(l1)) > level(var(l2)) ? level(var(l1)) : level(var(l2));
  if( config.opt_ecl_debug) cerr << "c before (extL: " << level(var(l1)) << ", decL: " << decisionLevel() << ") trail lim: " << trail_lim << endl;
  for( int i = ml; i < decisionLevel(); ++i ) {
    if( config.opt_ecl_debug) cerr << "c inc dec pos for level " << i << " from " << trail_lim[i] << " to " << trail_lim[i]+1 << endl;
    trail_lim[i] ++;
  }
  if( config.opt_ecl_debug) cerr << "c after trail lim: " << trail_lim << endl;
  
  if( config.opt_ecl_full && !config.opt_ecl_as_learned && config.opt_ecl_as_replaceAll > 0 ) { // replace disjunction everywhere in the formula
    // here, the disjunction could also by replaced by ~x in the whole formula
    disjunctionReplace( l1, l2, mkLit(x,true), config.opt_ecl_as_replaceAll > 1, false);
  }
  
  if( config.opt_ecl_debug) cerr << "c add clause " << oc << endl;
  CRef cr = ca.alloc(oc, config.opt_ecl_as_learned); // add clause as non-learned clause 
  ca[cr].setLBD(1); // all literals are from the same level!
  if( config.opt_ecl_debug) cerr << "c add clause " << ca[cr] << endl;
  nbDL2++; // stats
#ifdef CLS_EXTRA_INFO
  ca[cr].setExtraInformation( defaultExtraInfo() );
#endif
  if( config.opt_ecl_as_learned ) { // if parameter says its a learned clause ...
    learnts.push(cr);
    if(dynamicDataUpdates) claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 1 ? oc.size() : 1 ) ); // LBD is also always 1 !
  } else clauses.push(cr);
  attachClause(cr);
  // set reason!
  vardata[x].reason = cr;
  if( config.opt_ecl_debug ) cerr << "c ref for new reason: " << cr << endl;

  if( config.opt_ecl_full ) { // parameter to add full extension
    oc.clear(); // for not x, not l1
    oc.push( mkLit(x,true) );
    oc.push( ~l1 );
    CRef icr = ca.alloc(oc, config.opt_ecl_as_learned); // add clause as non-learned clause 
    ca[icr].setLBD(1); // all literals are from the same level!
    if( config.opt_ecl_debug) cerr << "c add clause " << ca[icr] << endl;
    nbDL2++; nbBin ++; // stats
    if( config.opt_ecl_as_learned ) { // add clause
      learnts.push(icr);
      if( dynamicDataUpdates ) claBumpActivity(ca[icr], (config.opt_cls_act_bump_mode == 1 ? oc.size() : 1 ) ); // LBD is also always 1 !
    } else clauses.push(icr);
    attachClause(icr);
    oc[1] = ~l2; // for not x, not l2
    CRef cr = ca.alloc(oc, config.opt_ecl_as_learned); // add clause as non-learned clause 
    ca[cr].setLBD(1); // all literals are from the same level!
    if( config.opt_ecl_debug) cerr << "c add clause " << ca[cr] << endl;
    nbDL2++; nbBin ++; // stats
    if( config.opt_ecl_as_learned ) { // add clause
      learnts.push(cr);
      if( dynamicDataUpdates ) claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 1 ? oc.size() : 1 ) ); // LBD is also always 1 !);
    } else clauses.push(cr);
    attachClause(cr);
  }

  // set the activity of the fresh variable (Huang used average of the two literals)
  double newAct = 0;
  const double& a1 = activity[var(l1)];
  const double& a2 = activity[var(l2)];
  if( config.opt_ecl_newAct == 0 ) {
   newAct =  a1 + a2; newAct /= 2; 
  } else if( config.opt_ecl_newAct == 1 ) {
    newAct = ( a1 > a2 ? a1 : a2 );
  } else if( config.opt_ecl_newAct == 2 ) {
    newAct = ( a1 > a2 ? a2 : a1 );
  } else if( config.opt_ecl_newAct == 3 ) {
    newAct = a1 + a2;
  } else if( config.opt_ecl_newAct == 4 ) {
    newAct = a1 * a2; newAct = sqrt( newAct );
  } 
  activity[x] = newAct;
  // from bump activity code - scale and insert/update
  if ( newAct > 1e100 ) {
      for (int i = 0; i < nVars(); i++) activity[i] *= 1e-100;
      var_inc *= 1e-100; 
  }
  // Update order_heap with respect to new activity:
  if (order_heap.inHeap(x)) order_heap.decrease(x);
  
  // finally, remove last two lits from learned clause and replace them with the negation of the fresh variable!
  currentLearnedClause.shrink(1);
  assert( currentLearnedClause.size() > 1 && "we do not want to work on unit clauses" );
  currentLearnedClause[ currentLearnedClause.size() -1 ] = mkLit(x,true); // add negated clause
  
  if( config.opt_ecl_debug) cerr << "c final learned clause: " << currentLearnedClause << endl;
  
  // lbd remains the same
  // extra info is set to default, because fresh variable has been introduced
  extraInfo = defaultExtraInfo();
  extendedLearnedClauses++;
  return true;
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
	while (next == var_Undef || value(next) != l_Undef || !decision[next])
	    if (order_heap.empty()){
		next = var_Undef;
		break;
	    } else
		next = order_heap.removeMin();
	
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
	  while (more == var_Undef || value(more) != l_Undef || !decision[more])
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

Solver::rerReturnType Solver::restrictedExtendedResolution( vec< Lit >& currentLearnedClause, unsigned int& lbd, uint64_t& extraInfo )
{
  if( ! config.opt_restrictedExtendedResolution ) return rerFailed;
  if( currentLearnedClause.size() < config.opt_rer_minSize ||
     currentLearnedClause.size() > config.opt_rer_maxSize ||
     lbd < config.opt_rer_minLBD ||
     lbd > config.opt_rer_maxLBD ) return rerFailed;
  if( (double)rerLearnedClause * config.opt_rer_every > conflicts ) return rerFailed; // do not consider this clause!
  
  // passed the filters
  if( rerLits.size() == 0 ) { // init 
    rerCommonLits.clear();rerCommonLitsSum=0;
    for( int i = 1; i < currentLearnedClause.size(); ++ i ) {
      rerCommonLits.push( currentLearnedClause[i] );
      rerCommonLitsSum += toInt(currentLearnedClause[i]);
    }
    rerLits.push( currentLearnedClause[0] );
    sort( rerCommonLits ); // TODO: have insertionsort/mergesort here!
    // rerFuseClauses is updated in search later!
    //cerr << "c init as [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : " << currentLearnedClause << endl;
    return rerMemorizeClause; // tell search method to include new clause into list
  } else {
    if( currentLearnedClause.size() != 1+rerCommonLits.size() ) {
      //cerr << "c reject size" << endl;
      rerSizeReject ++;
      resetRestrictedExtendedResolution();
      return rerFailed; // clauses in a row do not fit the window
    } else { // size fits, check lits!
      // sort, if more than 2 literals
      if( currentLearnedClause.size() > 2 ) sort( &( currentLearnedClause[2] ), currentLearnedClause.size() - 2  ); // do not touch the second literal in the clause! check it separately!
      bool found = false;
      for( int i = 0 ; i < rerCommonLits.size(); ++ i ) {
	if ( rerCommonLits[i] == currentLearnedClause[1] ) { found = true; break;}
      }
      if( ! found ) {
	resetRestrictedExtendedResolution();
	//cerr << "c reject patter" << endl;
	rerPatternReject ++;
	return rerFailed;
      }
      // Bloom-Filter
      int64_t thisLitSum = 0;
      for( int i = 0 ; i < currentLearnedClause.size(); ++ i ) {
	thisLitSum += toInt( currentLearnedClause[i] );
      }
      if( thisLitSum != rerCommonLitsSum ) {
	resetRestrictedExtendedResolution();
	rerPatternBloomReject ++;
	return rerFailed;
      }
      found = false; // for the other literals pattern
      // check whether all remaining literals are in the clause
      int i = 0; int j = 2;
      while ( i < rerCommonLits.size() && j < currentLearnedClause.size() ) {
	if( rerCommonLits[i] == currentLearnedClause[j] ) {
	  i++; j++;
	} else if ( rerCommonLits[i] == currentLearnedClause[1] ) {
	  ++i;
	} else { // literal currentLearnedClause[j] is not in common literals!
	  resetRestrictedExtendedResolution();
	  //cerr << "c reject patter" << endl;
	  rerPatternReject ++;
	  return rerFailed;
	}
      }
      // clauses match
      rerLits.push( currentLearnedClause[0] ); // store literal
      
      if( rerLits.size() >= config.opt_rer_windowSize ) {
	// perform RER step 
	// add all the RER clauses with the fresh variable (and set up the new variable properly!
	const Var x = newVar(true,true,'r');
	vardata[x].level = level(var(currentLearnedClause[0]));
	// do not assign a value, because it will be undone anyways!
	
	// delete the current decision level as well, so that the order of the reason clauses can be set right!
	assert( decisionLevel() > 0 && "can undo a decision only, if it didnt occur at level 0" );
	if( true && config.opt_rer_debug ) {
	  cerr << "c trail: " ;
	  for( int i = 0 ; i < trail.size(); ++ i ) {
	      cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?"; if( reason(var(trail[i]) ) == CRef_Undef ) cerr << "U"; else cerr <<reason(var(trail[i]));
	    } cerr << endl;
	  cerr << "c trail_lim: " << trail_lim << endl;
	  cerr << "c decision level: " << decisionLevel() << endl;
	  for( int i = 0 ; i < decisionLevel() ; ++i ) cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl;
	}
	const Lit lastDecisoin = trail [ trail_lim[ decisionLevel() - 1 ] ];
	if( config.opt_rer_debug ) cerr << "c undo decision level " << decisionLevel() << ", and re-add the related decision literal " << lastDecisoin << endl;
	rerOverheadTrailLits += trail.size(); // store how many literals have been removed from the trail to set the order right!
	cancelUntil( decisionLevel() - 1 );
	if( config.opt_rer_debug ) {
	  if( config.opt_rer_debug ) cerr << "c intermediate decision level " << decisionLevel() << endl;
	  for( int i = 0 ; i < decisionLevel() ; ++i ) cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl;
	}
	rerOverheadTrailLits -= trail.size();
	// detach all learned clauses from fused clauses
	for( int i = 0; i < rerFuseClauses.size(); ++ i ) {
	  assert( rerFuseClauses[i] != reason( var( ca[rerFuseClauses[i]][0] ) ) && "from a RER-CDCL point of view, these clauses cannot be reason clause" );
	  assert( rerFuseClauses[i] != reason( var( ca[rerFuseClauses[i]][1] ) ) && "from a RER-CDCL point of view, these clauses cannot be reason clause" );
	  // ca[rerFuseClauses[i]].mark(1); // mark to be deleted!
	  removeClause(rerFuseClauses[i]); // drop this clause!
	}
	
	// we do not need a reason here, the new learned clause will do!
	oc.clear(); oc.push( mkLit(x,true) ); oc.push(lit_Undef);
	for( int i = 0 ; i < rerLits.size(); ++ i ) {
	  oc[1] = rerLits[i];
	  CRef icr = ca.alloc(oc, config.opt_rer_as_learned); // add clause as non-learned clause 
	  ca[icr].setLBD(1); // all literals are from the same level!
	  if( config.opt_rer_debug) cerr << "c add clause " << ca[icr] << endl;
	  nbDL2++; nbBin ++; // stats
	  if( config.opt_rer_as_learned ) { // add clause
	    learnts.push(icr);
	    if( dynamicDataUpdates ) claBumpActivity(ca[icr], (config.opt_cls_act_bump_mode == 1 ? oc.size() : 1 ));
	  } else clauses.push(icr);
	  attachClause(icr); // all literals should be unassigned
	}
	if( config.opt_rer_full ) { // also add the other clause?
	  oc.clear(); oc.push(mkLit(x,false));
	  for( int i = 0; i < rerLits.size(); ++i ) oc.push( ~rerLits[i] );
	  int pos = 1;
	  for( int i = 2; i < oc.size(); ++ i ) if ( level(var(oc[i])) > level(var(oc[pos])) ) pos = i; // get second highest level!
	  { const Lit tmp = oc[pos]; oc[pos] = oc[1]; oc[1] = tmp; } // swap highest level literal to second position
	  CRef icr = ca.alloc(oc, config.opt_rer_as_learned); // add clause as non-learned clause 
	  ca[icr].setLBD(rerLits.size()); // hard to say, would have to be calculated ... TODO
	  if( config.opt_rer_debug) cerr << "c add clause " << ca[icr] << endl;
	  if( config.opt_rer_as_learned ) { // add clause
	    learnts.push(icr);
	    if( dynamicDataUpdates ) claBumpActivity(ca[icr], (config.opt_cls_act_bump_mode == 0 ? 1 : ( config.opt_cls_act_bump_mode == 1 ? oc.size() : rerLits.size() )) );
	  } else clauses.push(icr);
	  attachClause(icr); // at least the first two literals should be unassigned!
	  if( !config.opt_rer_as_learned ) {
	    // here, the disjunction could also by replaced by ~x in the whole formula, if the window is binary
	    if ( config.opt_rer_as_replaceAll > 0 && rerLits.size() == 2 ) disjunctionReplace( ~rerLits[0], ~rerLits[1], mkLit(x,true), (config.opt_rer_as_replaceAll > 1), false); // if there would be a binary clause, this case would not happen
	  }
	}
	// set the activity of the new variable
	double newAct = 0;
	if( config.opt_ecl_newAct == 0 ) {
	  for( int i = 0; i < rerLits.size(); ++ i ) newAct += activity[ var( rerLits[i] ) ];
	  newAct /= (double)rerLits.size(); 
	} else if( config.opt_ecl_newAct == 1 ) {
	  for( int i = 0; i < rerLits.size(); ++ i ) // max
	    newAct = newAct >= activity[ var( rerLits[i] ) ] ? newAct : activity[ var( rerLits[i] ) ];
	} else if( config.opt_ecl_newAct == 2 ) {
	  newAct = activity[ var( rerLits[0] ) ];
	  for( int i = 1; i < rerLits.size(); ++ i ) // min
	    newAct = newAct > activity[ var( rerLits[i] ) ] ? activity[ var( rerLits[i] ) ] : newAct ;
	} else if( config.opt_ecl_newAct == 3 ) {
	  for( int i = 0; i < rerLits.size(); ++ i ) // sum
	    newAct += activity[ var( rerLits[i] ) ];
	} else if( config.opt_ecl_newAct == 4 ) {
	  for( int i = 0; i < rerLits.size(); ++ i ) // geo mean
	    newAct += activity[ var( rerLits[i] ) ];
	  newAct = pow(newAct,1.0/(double)rerLits.size());
	} 
	activity[x] = newAct;
	// from bump activity code - scale and insert/update
	if ( newAct > 1e100 ) {
	    for (int i = 0; i < nVars(); i++) activity[i] *= 1e-100;
	    var_inc *= 1e-100; 
	}
	// Update order_heap with respect to new activity:
	if (order_heap.inHeap(x)) order_heap.decrease(x);
	
	// code from search method - enqueue the last decision again!
	newDecisionLevel();
	uncheckedEnqueue( lastDecisoin ); // this is the decision that has been done on this level before!
	if( config.opt_rer_debug ) {
	  cerr << "c new decision level " << decisionLevel() << endl;
	  for( int i = 0 ; i < decisionLevel() ; ++i ) cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl;
	}
	
	// modify the current learned clause accordingly!
	currentLearnedClause[0] = mkLit(x,false);
	// stats
	if( config.opt_rer_debug ) {
	  cerr << "c close with [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : ";
	  for( int i =0; i < currentLearnedClause.size(); ++i ) cerr << " " << currentLearnedClause[i] << "@" << level(var(currentLearnedClause[i]));
	  cerr << endl;
	}
	rerLearnedClause ++; rerLearnedSizeCandidates ++; 
	if( config.opt_rer_debug ) {
	  cerr << endl << "c accepted current pattern with lits " << rerLits << " - start over" << endl << endl;
	  cerr << "c trail: " ;
	  for( int i = 0 ; i < trail.size(); ++ i ) {
	      cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?"; if( reason(var(trail[i]) ) == CRef_Undef ) cerr << "U"; else cerr <<reason(var(trail[i]));
	  } cerr << endl;
	}
	resetRestrictedExtendedResolution(); // done with the current pattern
	maxRERclause = maxRERclause >= currentLearnedClause.size() ? maxRERclause : currentLearnedClause.size();
	totalRERlits += currentLearnedClause.size();
	return rerDontAttachAssertingLit;
      } else {
	if( config.opt_rer_debug ) cerr << "c add as [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : " << currentLearnedClause << endl;
	rerLearnedSizeCandidates ++;
	return rerMemorizeClause; // add the next learned clause to the database as well!
      }
    }
  }
  return rerFailed;
}

void Solver::resetRestrictedExtendedResolution()
{
  rerCommonLits.clear();
  rerCommonLitsSum = 0;
  rerLits.clear();
  rerFuseClauses.clear();
}

void Solver::disjunctionReplace( Lit p, Lit q, const Lit x, bool inLearned, bool inBinary)
{
  
  for( int m = 0 ; m < (inLearned?2:1); ++ m ) {
    const vec<CRef>& cls = (m==0 ? clauses : learnts );
    
    for( int i = 0 ; i < cls.size(); ++ i ) { // check all clauses of the current set
      Clause& c = ca[cls[i]];
      if( c.mark() != 0 ) continue; // do not handle clauses that are marked for being deleted!
      if( !inBinary && c.size() <= 2 ) continue; // skip binary clauses, if doable -- TODO: for rer check whether it is relevant to check binary clauses! for ecl its not!
      int firstHit = 0;
      for(  ; firstHit < c.size(); ++ firstHit ) {
	const Lit& l = c[firstHit];
	if( l == p || l == q ) break;
	else if( l == ~p || l == ~q ) { firstHit = -1; break;}
      }
      if( firstHit == -1 ) continue; // do not handle this clause - tautology found

      if( c[firstHit] == q ) { const Lit tmp = q; q = p; p = tmp; }
      int secondHit = firstHit + 1;
      for( ; secondHit < c.size(); ++ secondHit ) {
	const Lit& l = c[secondHit];
	if( l == q ) break;
	else if( l == ~q ) {secondHit = -1; break; }
      }
      if( secondHit == -1 || secondHit == c.size() ) continue; // second literal not found, or complement of other second literal found

      // found both literals in the clause ...
      if( c.size() == 2 ) {
	assert( decisionLevel() == 0 && "can add a unit only permanently, if we are currently on level 0!" );
	removeClause( cls[i] );
	uncheckedEnqueue( x );
	continue; // nothing more to be done!
      } else { // TODO: could be implemented better (less watch moving!)
	// rewrite clause
	// reattach clause if neccesary
	assert( (firstHit != 0 || decisionLevel () == 0 ) && "a reason clause should not be rewritten, such that the first literal is moved!" );
	if( firstHit < 2 ) detachClause( cls[i], true ); // not always necessary to remove the watches!
	else {
	  if( c.learnt() ) learnts_literals --;
	  else clauses_literals--;
	}
	c[firstHit] = x;
	c[secondHit] = c[ c.size() - 1 ];
	c.shrink(1);
	assert( c.size() > 1 && "do not produce unit clauses!" );
	if( firstHit < 2 ) attachClause( cls[i] );
      }

    }
  }
}

bool Solver::interleavedClauseStrengthening()
{
  // TODO: have dynamic updates of the limits, so that a good time/result ratio can be reached!
  icsCalls ++;
  MethodClock thisMethodTime( icsTime ); // clock that measures how long the procedure took
  // freeze current state
  vec<Lit> trailCopy;
  trail.copyTo( trailCopy );
  vec<int> trailLimCopy;
  trail_lim.copyTo( trailLimCopy );
  vec<char> polarityCopy;
  polarity.copyTo(polarityCopy);
  const int oldVars = nVars();
  const int oldLearntsSize = learnts.size();
  // backtrack to level 0
  //if(config.opt_ics_debug) for( int i = 0 ; i < trailLimCopy.size(); ++i ) cerr << "c decision " << trailCopy[ trailLimCopy[i] ]  << "@" << i+1 << endl;
  cancelUntil( 0 );
  
  // disable dynamic updates via parameter?
  const bool oldDynUpdates = dynamicDataUpdates;
  if( !config.opt_ics_dynUpdate ) {
    dynamicDataUpdates = false;
  }
  
  // perform reducer algorithm for some of the last good learned clauses - also adding the newly learnt clauses
  int backtrack_level; unsigned int nblevels;	// helper variable (more or less borrowed from search method
  uint64_t extraInfo;			// helper variable (more or less borrowed from search method
  vec<Lit> learnt_clause;		// helper variable (more or less borrowed from search method
  // do the loop
  const int end = learnts.size();
  int start = 0, count = 0;
  double lbdSum = 0, sizeSum = 0;
  // calculate avgs. to be able to  reject clauses -- drop clauses that are not "usual" (mark() != 0)
  for( int i = learnts.size() - 1; i >= 0; -- i ) {
    const Clause& c = ca [ learnts[i] ];
    if( c.mark() != 0 ) continue; // do not consider this clause (seems to be deleted already, must not be part of a watched list any more ... )
    lbdSum += c.lbd();
    sizeSum += c.size();
    if( ++count > config.opt_ics_processLast ) { start = i; break; } // stop the scan here, and start ICS with this clause!
  }
  const double lbdCount = count;
  const double sizeLimit = (sizeSum / lbdCount ) * config.opt_ics_SIZEpercent; // clauses with a size smaller than this are dropped
  const double lbdLimit = (lbdSum / lbdCount ) * config.opt_ics_LBDpercent; // clauses with an LBD smaller than this are dropped
  
  for( int i = start; i < (config.opt_ics_shrinkNew ? learnts.size() : end) ; ++ i ) { // do not process the new clauses, nor keep them ... 
    Clause& c = ca [ learnts[i] ];
    if( c.mark() != 0 ) continue; // do not consider this clause (seems to be deleted already, must not be part of a watched list any more ... )
    if( c.size() > sizeLimit || c.lbd() > lbdLimit ){ icsDroppedCandidates++; continue; } // do not consider this clause!
    icsCandidates ++; // stats - store how many learned clauses have been tested
    if(config.opt_ics_debug) cerr << "c ICS on " << c << endl;
    // if(config.opt_ics_debug) cerr << "c current trail: " << trail << endl;
    detachClause( learnts[i], true ); // remove the clause from the solver, to be able to rewrite it
    for( int j = 0 ; j < c.size(); ++ j ) {
      if( value( c[j] ) == l_True ) { c.mark(1); break; } // do not use clauses that are satisfied!
    }
    if( c.mark() != 0 ) continue; // this clause is removed from the solver!

    // TODO: could sort the literals somehow here ...
    int k = 0; // number of literals to keep in the clause
    int j = 0;
    bool droppedLit = false;
    int dropPosition = -1;
    for( ; j < c.size(); ++ j ) {	// check each literal and enqueue it negated
      if( config.opt_ics_debug ) cerr << "c check lit " << j << "/" << c.size() << ": " << c[j] << " with value " << toInt( value(c[j]) ) << endl;
      if( value( c[j] ) == l_True ) { // just need to keep all previous and this literal
	c[k++] = c[j];
	if( config.opt_ics_debug ) cerr << "c interrupt because of sat.lit, current trail: " << trail << endl;
	break; // this literals does not need to be kept! // TODO: what if the clause is already satisfied, could be stopped as well!
      } else if ( value( c[j] ) == l_False ) {
	droppedLit = true;
	dropPosition = j; // dropped the literal here
	if( config.opt_ics_debug ) cerr << "c jump over false lit: " << c[j] << endl;
	continue; // can drop this literal
      }
      c[k++] = c[j]; // keep the current literal!
      newDecisionLevel(); // we are working on decision level 1, so nothing breaks
      uncheckedEnqueue( ~c[j] );
      CRef confl = propagate();
      if( confl != CRef_Undef ) { // found a conflict, handle it (via usual, simple, conflict analysis)
	learnt_clause.clear(); otfssCls.clear(); // prepare for analysis
	int ret = analyze(confl, learnt_clause, backtrack_level,nblevels,otfssCls,extraInfo);	
	cancelUntil( 0 );
	if( ret == 0 ) {
	  addCommentToProof("learnt clause");
	  addToProof( learnt_clause );
	  if (learnt_clause.size() == 1){
	      assert( decisionLevel() == 0 && "enequeue unit clause on decision level 0!" );
	      topLevelsSinceLastLa ++;
  #ifdef CLS_EXTRA_INFO
		      vardata[var(learnt_clause[0])].extraInfo = extraInfo;
  #endif
	      if( value(learnt_clause[0]) == l_Undef ) { // propagate unit clause!
		uncheckedEnqueue(learnt_clause[0]);
		if( propagate() != CRef_Undef ) {
		  if( config.opt_ics_debug ) cerr << "c ICS cannot propagate the unit of a learned unit clause " << learnt_clause[0] << endl;
		  return false;
		}
		nbUn ++;
	      }
	      else if (value(learnt_clause[0]) == l_False ) {
		if( config.opt_ics_debug ) cerr << "c ICS learned a falsified unit clause " << learnt_clause[0] << endl;
		return false; // otherwise, we have a top level conflict here!
	      }
	  }else{
	    const CRef cr = ca.alloc(learnt_clause, true);
	    ca[cr].setLBD(nblevels); 
  #ifdef CLS_EXTRA_INFO
	    ca[cr].setExtraInformation(extraInfo);
  #endif
	    learnts.push(cr); // this is the learned clause only, has nothing to do with the other clause!
	    attachClause(cr); // for now, we'll also use this clause!
	    if(config.opt_ics_debug) cerr << "c ICS learn clause [" << cr << "] " << ca[cr] << endl;
	  }
	} else {
	  // what to do here?
	  assert( false && "this method is not handled properly!" );
	}
	break; // we're done for this clause for now. ... what happens if we added the current learned clause now and repeat the process for the clause? there might be more reduction! -> TODO: shuffe clause and have parameter!
      } // end if conflict during propagate
    } // have checked all literals of the current clause ...
    // refresh reference! there might have been a ca.alloc!
    Clause& d = ca[ learnts[i] ];
    cancelUntil( 0 ); // to be on the safe side, if there has not been a conflict before
    // shrink the clause and add it back again!
    if(config.opt_ics_debug) cerr << "c ICS looked at " << j << " literals, and kept " << k << " with a size of " << d.size() << endl; 
    if( droppedLit || k < j ) { // actually, something has been done
      icsShrinks ++; icsShrinkedLits += (d.size() - k ); // stats -- store the success of shrinking
      d.shrink( d.size() - k );
    }
    if(config.opt_ics_debug) cerr << "c ICS return modified clause: " << d << endl;
    
    if( d.size() > 1 ) attachClause( learnts[i] ); // unit clauses do not need to be added!
    else if( d.size() == 1  ) { // if not already done, propagate this new clause!
      if( value( d[0] ) == l_Undef ) {
	uncheckedEnqueue(d[0]);
	if( propagate() != CRef_Undef ) {
	  if( config.opt_ics_debug ) cerr << "c ICS return false, because unit " << d[0] << " cannot be propagated" << endl;
	  return false;
	}
      } else if ( value( d[0] ) == l_False ) {
	if( config.opt_ics_debug ) cerr << "c ICS learned falsified unit " << d[0]<< endl;
	return false; // should not happen!
      }
    } else if( d.size() == 0 ) {
      if( config.opt_ics_debug ) cerr << "c ICS learned an empty clause [" << learnts[i] << "]" << endl;
      return false; // unsat, since c is empty
    }
  }
  
  // remove the newly learned clauses
  if( !config.opt_ics_keepLearnts ) {
    for( int j = oldLearntsSize; j < learnts.size(); ++ j ) {
      removeClause( learnts[j], true ); // remvoes a clause "lazily"
    }
    learnts.shrink( learnts.size() - oldLearntsSize );
    assert( learnts.size() == oldLearntsSize && "remove exactly all the clauses that have been added before" );
  }
  
  // role back solver state again
  assert( oldVars == nVars() && "no variables should have been added during executing this algorithm!" );
  
  for( int i = 0 ; i < trailLimCopy.size(); ++i ) {
    newDecisionLevel();
   // if(config.opt_ics_debug) cerr << "c enqueue " << trailCopy[ trailLimCopy[i] ]  << "@" << i+1 << endl;
    if( value( trailCopy[ trailLimCopy[i] ] ) == l_False ) break; // stop here, because the next decision has to be different! (and the search will take care of that!)
    else if( value( trailCopy[ trailLimCopy[i] ] ) == l_Undef ) uncheckedEnqueue( trailCopy[ trailLimCopy[i] ] );
    CRef confl = propagate();
    if( confl != CRef_Undef ) { // handle conflict. conflict at top-level -> return false!, else backjump, and continue with good state!
      cancelUntil( decisionLevel () - 1 );
//       else {
// 	cerr << "c during creating the trail again, an error has been found - cannot set level below the current value -- tried to enqueue decision literal " << trailCopy[ trailLimCopy[i] ] << " as " << i + 1 << "th decision " << endl;
// 	return false;
//       }
    }
  }
  polarityCopy.copyTo( polarity );
  dynamicDataUpdates = oldDynUpdates; // reverse the state!
  lastICSconflicts = conflicts;
  // return as if nothing has happened
  if( config.opt_ics_debug ) cerr << "c finished ICS" << endl;
  return true;
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


void Solver::giveNewSDAssumptions(vec< Lit >& assumptions, vec< Lit >& conflict_clause)
{
  sort( conflict_clause );
  int i = 0, j = 0;
  int keptLits = 0;
  while ( i < assumptions.size() && j < conflict_clause.size() ) 
  {
    if( var(assumptions[i]) == var(conflict_clause[j]) ) {
      j++; ++i;
    } else if ( var(assumptions[i]) < var(conflict_clause[j]) ) {
      assumptions[ keptLits ++ ] = assumptions[i];
      ++i ;
    } else if ( var(conflict_clause[j]) < var(assumptions [i]) ) {
      j++;
    }
  }
  // remove all the literals that occur in the conflict clause
  assumptions.shrink(  assumptions.size() - keptLits );
}

void Solver::substituteDisjunctions(vec< Lit >& assumptions)
{
  cerr << "c start substituteDisjunctions" << endl;
  
  assert( decisionLevel() == 0 && "should only be done on level 0" );
  
  const bool methodDebug = false;
  
  assumptions.clear();
  MethodClock methodTime( sdTime );

  // fill local data structures
  Coprocessor::MarkArray markArray;
  markArray.resize( nVars() * 2 );
  vec<Lit> tmpLiterals;
  uint32_t* lCount = (uint32_t*) malloc( sizeof( uint32_t ) * 2 * nVars() );
  
  uint32_t index = cegarLiteralHeap->size();
  // init
  for( Var v = 0 ; v < nVars(); ++ v ) // only add literals that could lead to reduction
  {
    if( cegarOcc[ toInt( mkLit(v,false)) ] > 3 ) if( !cegarLiteralHeap->inHeap(toInt(mkLit(v,false))) ) cegarLiteralHeap->insert( toInt( mkLit(v,false) ));
    if( cegarOcc[ toInt( mkLit(v,true) ) ] > 3 ) if( !cegarLiteralHeap->inHeap(toInt(mkLit(v,true))) )  cegarLiteralHeap->insert( toInt( mkLit(v,true)  ));
  }
  
  while (cegarLiteralHeap->size() > 0 && sdSteps < config.opt_sdLimit && !asynch_interrupt ) // as long as there is something to do
  {

    const Lit literal = toLit( (*cegarLiteralHeap)[0] );
    assert( cegarLiteralHeap->inHeap( toInt(literal) ) && "item from the heap has to be on the heap");
    cegarLiteralHeap->removeMin();
    
    if( cegarOcc[ toInt(literal) ] < 4 ) {
      if( methodDebug ) if( cegarOccs[ toInt(literal) ].size() > 0 ) cerr << "c reject lit " << literal << " with " << cegarOccs[ toInt(literal) ].size() << " occurrences" << endl;
      continue;	// results in reduction only if the literal occurs more than 4 times!
    }
    
    markArray.nextStep();
    tmpLiterals. clear();
    markArray.setCurrentStep( toInt(literal) );
    tmpLiterals.push(literal);
    vector< CRef >& Fliteral = cegarOccs[ toInt(literal) ];

    // remove all satisfied clauses from the formula
    for( uint32_t i = 0 ; i < Fliteral.size(); ++ i ) 
    {
      const Clause& clause = ca[ Fliteral[i] ];
      if( clause.mark() != 0 ) { // can only happen in the very first iteration
	Fliteral[i] = Fliteral[Fliteral.size()-1];
	Fliteral.pop_back();
	--i;
      }
    }
    
    uint32_t orLiterals = 1;			// number of literals that participate in the OR-pattern == n
    uint32_t usedClauses = Fliteral.size(); 	// first clauses in list that have the OR-pattern == k
    
    // while find more!
    while( true ) 
    {
      memset( lCount, 0, sizeof( uint32_t ) * 2 * nVars() ); // reset to 0
      
      // get all partner literals!
      for( uint32_t i = 0 ; i < usedClauses; ++ i ) 
      {
	const Clause& clause = ca[ Fliteral[i] ];
	if( clause.mark() != 0 ) { // can only happen in the very first iteration
	  Fliteral[i] = Fliteral[usedClauses-1];
	}
	if( orLiterals > 1 ) { // no need to check for the first literal, because it has to be in the list
	  // remove the clause from the work-set, because it does not contain the used pair any more
	  uint32_t presentLiterals = 0;
	  for( uint32_t j = 0 ; j<clause.size(); ++j )
	  {
	    const Lit& l = clause[j];
	    if( markArray.isCurrentStep( toInt(l) ) ) presentLiterals++;
	  }
	  if( presentLiterals < orLiterals ) {
	    CRef tmp = Fliteral[i];
	    Fliteral[i] = Fliteral[usedClauses-1];
	    Fliteral[usedClauses-1] = tmp;
	    usedClauses --;
	    i--;
	    continue;
	  }
	}
	
	for( uint32_t j = 0 ; j<clause.size(); ++j )
	{
	  sdSteps ++;
	  const Lit& l = clause[j];
	  if( cegarOccs[ toInt(l) ].size() < 3 ) continue; // this line guarantees, that each found matching occurs at least 3 times!
	  // do not count literals that already belong to the OR-gate
	  if( markArray.isCurrentStep( toInt(l) ) ) continue;
	  // count occurrence
	  tmpLiterals.push( l );
	  lCount[ toInt(l) ] ++;
	}
      }
      
      // sort tmp-literals according to count
      for( uint32_t i = orLiterals ; i < tmpLiterals.size(); ++ i ) {
	Lit ref = tmpLiterals[i];
	uint32_t j = i + 1;
	for( ; j < tmpLiterals.size(); ++ j ) {
	  const Lit lj = tmpLiterals[j];
	  if( lj == ref ) { tmpLiterals[j] = tmpLiterals[tmpLiterals.size()-1]; tmpLiterals.pop(); j--; continue; }
	  if( lCount[ toInt(ref) ] < lCount[ toInt(lj)] )
	  {
	    ref = lj;
	  }
	}
      }
      if( tmpLiterals.size() == orLiterals ) break; // no more partners
      if( lCount[ toInt(tmpLiterals[orLiterals]) ] < 3 ) break; // ensure that the matching appears at least 4 times

      // add the next literal to the or-gate
      if( methodDebug ) cerr << "c [CP2-ROR] add literal " << tmpLiterals[orLiterals] << "[" << lCount[ toInt(tmpLiterals[orLiterals])] << "] to OR-gate" << endl;
      markArray.setCurrentStep( toInt( tmpLiterals[orLiterals] ) );
      orLiterals ++;
      if( methodDebug ) cerr << "c before orLits: " << orLiterals << " tmpLits: " << tmpLiterals.size() << endl;
      assert( tmpLiterals.size() >= orLiterals && "there needs to be at least one more literal in the literals");
      tmpLiterals.shrink_( tmpLiterals.size() - orLiterals ); // do not call destructor!
      if( methodDebug ) cerr << "c after  orLits: " << orLiterals << " tmpLits: " << tmpLiterals.size() << endl;
    } // end while more literals
    
    // TODO: introduce parameter that controls whether larger pairs should be replaced?
    if( orLiterals == 1 ) {
      if( methodDebug ) cerr << "c reject literal " << literal << " because of orLit: " << orLiterals << endl;
      continue; // there is no OR-gate that can be introduced
    } else if( methodDebug ) cerr << "c usedClauses for " << literal << " : " << usedClauses << endl;

    // check reduction, abort if there is no reduction
    int32_t n = orLiterals;
    int32_t k = usedClauses;
    if( (k*n) - (n+1+3*k) <= 0 ) continue;
    
    if( methodDebug ) cerr << "c [CP2-ROR] found matching with reduction: " << (k*n) - (n+1+3*k) << endl;
    if( methodDebug ) cerr << "c [CP2-ROR] used clauses for current OR-gate: " << usedClauses << endl;

    // get next variable
    Var newVariable = newVar();
    if( methodDebug ) cerr << "c added variable " << newVariable << endl;
    // reserve space for the new variable in all the data structures
    cegarOccs.push_back( vector<CRef>() ); cegarOccs.push_back( vector<CRef>() );
    cegarOcc.push(0);cegarOcc.push(0);
    cegarLiteralHeap->addNewElement(nVars() * 2);
    markArray.resize( nVars() * 2 );
    lCount = (uint32_t*) realloc( lCount, sizeof( uint32_t ) * nVars() * 2 );
    
    // replace OR-gate in all the participating clauses
    for( uint32_t i = 0 ; i < usedClauses; ++ i )
    {
      detachClause( cegarOccs[ toInt(literal) ][i], true ); // remove clause from watch lists
      Clause& clause = ca[ cegarOccs[ toInt(literal) ][i] ];
      if( clause.mark() != 0 ) continue;
      assert( clause.size() >= orLiterals && "can only rewrite clauses where all the literals occur" );
      if( methodDebug ){ cerr << "c [CP2-ROR] rewrite clause " << ca [ cegarOccs[ toInt(literal) ][i]] << endl; }
      const Lit posNew = Lit( mkLit(newVariable,false) ); // new literal
      int replacsInClause = 0;
      for( uint32_t j = 0 ; j < clause.size(); ++ j )
      {
	const Lit tj = clause[j] ;
	if( markArray.isCurrentStep( toInt(tj) ) ) {
	  if( tj != literal ) {
	    if( methodDebug ){ cerr << "c [CP2-ROR] remove " << ca[ cegarOccs[ toInt(literal) ][i] ] << " from list of literal " << tj << " position " << j << endl; }
	    // delete the current clause from the list
	    for( int k = 0; k < cegarOccs[ toInt(tj) ].size(); k++ ) {
	      if( cegarOccs[ toInt(tj) ][k] == cegarOccs[ toInt(literal) ][i] ) {
		cegarOccs[ toInt(tj) ][k] = cegarOccs[ toInt(tj) ][ cegarOccs[ toInt(tj) ].size() - 1 ];
		cegarOccs[ toInt(tj) ].pop_back();
		break;
	      }
	    }
	  }
	  if( replacsInClause == 0 ) {
	    clause[j] = posNew;
	    replacsInClause ++;
	  } else {
	    clause.removePositionUnsorted(j);
	  }
	  cegarOcc[ toInt(tj) ] --; 
	  if( cegarOcc[ toInt(tj) ] > 0 ) cegarLiteralHeap->update( toInt(tj) ); 
	  j--;
	}
      }
      
      if( ca[ cegarOccs[ toInt(literal) ][i] ].size() == 1 ) {
	uncheckedEnqueue( ca[ cegarOccs[ toInt(literal) ][i] ][0] ); // enqueue the unit clause
      } else {
	attachClause( cegarOccs[ toInt(literal) ][i] ); // add the clause to the watch lists again
	if( cegarLiteralHeap->inHeap( toInt( posNew ) ) ) cegarLiteralHeap->update( toInt(posNew) );
	else cegarLiteralHeap->insert( toInt(posNew) );
      }
      
      // add new literal to clause, add clause to list of new literal
      cegarOcc[ toInt( posNew ) ] ++;
      cegarOccs[ toInt(posNew) ].push_back( cegarOccs[ toInt(literal) ][i] ); // store clause in the new list
      if( methodDebug ){ cerr << "c [CP2-ROR] final clause: "<< ca [ cegarOccs[ toInt(literal)][i] ] << endl; }
    }
    // remove that clauses also from the list of the literal
    for( uint32_t i = 0 ; i < usedClauses; ++ i ){
      cegarOccs[ toInt(literal) ][index] = cegarOccs[ toInt(literal) ][ cegarOccs[ toInt(literal) ].size() - 1 ];
      cegarOccs[ toInt(literal) ].pop_back();
    }
    sdClauses += usedClauses;
    sdLits += orLiterals;
    
    // collect the literals, that could be set to true to try the CEGAR rewriting
    assumptions.push( mkLit(newVariable, false) ); // 
    
    // add the new clauses
    if( methodDebug ) cerr << "c create clause with orLits: " << tmpLiterals << endl;
    tmpLiterals.shrink_( tmpLiterals.size() - orLiterals ); // remove all the literals from the last iteration
    tmpLiterals.push( mkLit(newVariable, true) ); // add the new variable
    CRef ref = ca.alloc(tmpLiterals, false); // no learnt clause!
    // add clause to formula, and solver structures
    attachClause( ref );
    clauses.push( ref );
    for( int i = 0 ; i < ca[ ref ].size() ; ++ i ) {
      const Lit tl = ca[ ref ][i];
      cegarOccs[ toInt(tl) ] . push_back ( ref );
      if( cegarLiteralHeap->inHeap( toInt( tl ) ) ) cegarLiteralHeap->update( toInt(tl) );
      else cegarLiteralHeap->insert( toInt(tl) );
    }
    if( methodDebug ) { cerr << "c added clause " << ca[ref] << endl; }
    
//     if( !rOrNoBlocked )
//     {
//       Lit lits[2];
//       lits[0] = Lit(newVariable, POS);
//       for( uint32_t i = 0 ; i < orLiterals; ++ i ) {
// 	lits[1] = tmpLiterals[i].complement();
// 	CL_REF ref = search.gsa.create(  lits, 2, false  );
// 	if( !addClause( ref ) ) { solution = UNSAT; return didSomething; }  
// 	formula->push_back(ref);
//       }
//     }
    
    if( methodDebug ) {
      cerr << "c [CP2-ROR] replaced OR-gate with " << orLiterals << " elements:";
      for( uint32_t i = 0 ; i < orLiterals; ++ i ) cerr << " " << tmpLiterals[i];
      cerr << endl;
    }
  }
  
  free( lCount );
  if( methodDebug ) cerr << "c finish SD with " << sdSteps << " steps" << endl;
  sdAssumptions = assumptions.size();
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
  if( config.opt_printDecisions > 2 ) {
    printf("c conflict at level %d\n", decisionLevel() );
    cerr << "c conflict clause: " << ca[confl] << endl;
    cerr << "c trail: " ;
    for( int i = 0 ; i < trail.size(); ++ i ) {
      cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?"; if( reason(var(trail[i]) ) == CRef_Undef ) cerr << "U"; else cerr <<reason(var(trail[i]));
    } cerr << endl;
  }
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
    if( config.opt_printDecisions > 1 ) cerr << "c enqueue multi-learned literal " << learnt_clause[i] << " at level " <<  decisionLevel() << endl;
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
  rerReturnType rerClause = rerFailed;
  if( doAddVariablesViaER && !isBiAsserting ) { // be able to block adding variables during search by the solver itself, do not apply rewriting to biasserting clauses!
    extResTime.start();
    bool ecl = extendedClauseLearning( learnt_clause, nblevels, extraInfo );
    if( ! ecl ) { // only if not ecl, rer could be tested!
      rerClause = restrictedExtendedResolution( learnt_clause, nblevels, extraInfo ); 
    } else {
      resetRestrictedExtendedResolution(); // otherwise, we just failed ... TODO: could simply jump over that clause ...
    }
    extResTime.stop();
  }

  lbdQueue.push(nblevels);
  sumLBD += nblevels;
  // write learned clause to DRUP!
  addCommentToProof("learnt clause");
  addToProof( learnt_clause );
  // store learning stats!
  totalLearnedClauses ++ ; sumLearnedClauseSize+=learnt_clause.size();sumLearnedClauseLBD+=nblevels;
  maxLearnedClauseSize = learnt_clause.size() > maxLearnedClauseSize ? learnt_clause.size() : maxLearnedClauseSize;

  if (learnt_clause.size() == 1){
      assert( decisionLevel() == 0 && "enequeue unit clause on decision level 0!" );
      topLevelsSinceLastLa ++;
  #ifdef CLS_EXTRA_INFO
      vardata[var(learnt_clause[0])].extraInfo = extraInfo;
  #endif
      if( value(learnt_clause[0]) == l_Undef ) {uncheckedEnqueue(learnt_clause[0]);nbUn++;}
      else if (value(learnt_clause[0]) == l_False ) return l_False; // otherwise, we have a top level conflict here!
      if( config.opt_printDecisions > 1 ) cerr << "c enqueue learned unit literal " << learnt_clause[0]<< " at level " <<  decisionLevel() << " from clause " << learnt_clause << endl;
  }else{
    CRef cr = ca.alloc(learnt_clause, true);
    if( rerClause == rerMemorizeClause ) rerFuseClauses.push( cr ); // memorize this clause reference for RER
    ca[cr].setLBD(nblevels); 
    if(nblevels<=2) nbDL2++; // stats
    if(ca[cr].size()==2) nbBin++; // stats
  #ifdef CLS_EXTRA_INFO
    ca[cr].setExtraInformation(extraInfo);
  #endif
    learnts.push(cr);
    attachClause(cr);
    
    if( dynamicDataUpdates ) claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 0 ? 1 : (config.opt_cls_act_bump_mode == 1) ? learnt_clause.size() : nblevels )  ); // bump activity based on its size
    if( rerClause != rerDontAttachAssertingLit ) { // attach unit only, if  rer does allow it
      if( !backtrackedBeyond ) {	// attach the unit only, if the backjumping level is the same as otfss level (s.t. we are still on the asserting level), and if not biasserting!

	if( !isBiAsserting ) {
	  if (value(learnt_clause[0]) == l_Undef) {
	    uncheckedEnqueue(learnt_clause[0], cr); // this clause is only unit, if OTFSS jumped to the same level!
	    if( config.opt_printDecisions > 1  ) cerr << "c enqueue literal " << learnt_clause[0] << " at level " <<  decisionLevel() << " from learned clause " << learnt_clause << endl;
	  } else if ( value(learnt_clause[0]) == l_False ) return l_False; // due to OTFSS there might arose a conflict already ...
	} else { 
	  biAssertingPostCount++;
	  isBiAsserting = false; // handled the current conflict clause, set this flag to false again
	}
      }
    }
    
    if( analyzeNewLearnedClause( cr ) )   // check whether this clause can be used to imply new backbone literals!
    {
      ok = false; // found a contradtion
      return l_False;	// interupt search!
    }
    
  }
  return l_Undef;
}

lbool Solver::otfssProcessClauses(int backtrack_level)
{
      if(config.debug_otfss) cerr << "c conflict at level " << decisionLevel() << " analyze will proceed at level " << backtrack_level << endl;
      int otfssBtLevel = backtrack_level;
      int enqueueK = 0 ; // number of clauses in the vector that need to be enqueued when jumping to the current otfssBtLevel
      otfsss = otfssCls.size() > 0 ? otfsss + 1 : otfsss;
      otfsssL1 = (decisionLevel() == 1 && otfssCls.size() > 0) ? otfsssL1 + 1 : otfsssL1;
      otfssClss += otfssCls.size();
      for( int i = 0 ; i < otfssCls.size() ; ++ i ) {
	Clause& c = ca[otfssCls[i]]; // when the first literal is removed, all literals of c are falsified! (has been reason for first literal)
	// TODO: does not work with DRUP yet!
	const int l1 = decisionLevel();
	int l2=0, movePosition = 2;
	assert( level(var(c[1])) == decisionLevel() && "this clause was used at the very last level to become reason for its first literal!" );
	for( int j = 2 ; j < c.size(); ++ j ) { // get the two highest levels in the clause!
	  int cl = level(var(c[j])); 
	  assert( cl <= l1 && "there cannot be a literal with a higher level than the current decision level" );
	  if(cl > l2) { l2 = cl; movePosition = j;} // found second highest level -> move literal to position
	}
	if( movePosition > 2 ) { const Lit tmpL = c[2]; c[2] = c[movePosition]; c[movePosition] = tmpL;} // move literal with second highest level to position 2 (if available)
	assert( (l1 != 0 || l2 != 0) && "one of the two levels has to be non-zero!" );
	if( l1 > l2 ) { // this clause is unit at level l2!
	  if( l2 < otfssBtLevel || l2 == 0) {  
	    if(config.debug_otfss) cerr << "c clear all memorized clauses, jump to new level " << l2 << "( from " << otfssBtLevel << ")" << endl;
	    otfssBtLevel = l2; enqueueK = 0; 
	   // we will lower the level now, so none of the previously enqueued clauses is unit any more!
	    if(config.debug_otfss) cerr << "c memorize clause " << c << " as unit at level " << l2 << " current BTlevel: " << otfssBtLevel << endl;
	    otfssCls[enqueueK++] = otfssCls[i]; // memorize that this clause has to be enqueued, if the level is not altered!
	    if(config.debug_otfss) { cerr << "c clause levels: "; for( int k = 0 ; k < c.size(); ++ k ) cerr << " " << level(var(c[k])); cerr << endl;}
	  }
	} else if( l1 == l2 ) l2 --; // reduce the backtrack level to get this clause right, even if its not a unit clause!
	if( l2 < otfssBtLevel ) { if(config.debug_otfss) cerr << "c clear all memorized clauses, jump to new level " << l2 << endl;
	  otfssBtLevel = l2; enqueueK = 0; }
	// finally, modify clause and get all watch structures into a good shape again!
	const Lit remLit = c[0]; // this literal will be removed finally!
	if( c.size() > 3 ) {
	  assert( level(var(c[1]) ) == l1 && "if there is a unit literal, this literal is the other watched literal!" );
	  assert( level(var(c[2]) ) >= l2 && "the second literal can be used as other watched literal for the reduced clause" );
	  // remove from watch list of first literal
	  if( config.opt_fast_rem ) removeUnSort(watches[~c[0]], Watcher(otfssCls[i], c[1])); // strict fast deletion?
	  else remove(watches[~c[0]], Watcher(otfssCls[i], c[1])); // strict deletion!
	  // add clause to list of third literal
	  watches[~c[2]].push(Watcher(otfssCls[i], c[1]));
	  c[0] = c[1]; c[1] = c[2]; c.removePositionUnsorted(2); // move the two literals with the highest levels forward!
	} else if( c.size() == 2 ) { // clause becomes unit, no need to attach it again!
	  assert( otfssBtLevel == 0 && "if we found a single unit, backtracking has to be performed to level 0!" );
	  const Lit tmp = c[0]; c[0] = c[1]; c[1] = tmp; // set the clause to be able to be removed adequately - unit will be enqued anyways!
	  c.mark(1);
	  otfssUnits ++;
	  // the clause is a unit, handle it!!
	  // assert( c.size() == 1 && "this has to be a unit clause");
	  assert( otfssBtLevel == 0 && "if its abinary clause, we need to jump back to level 0!" );
	  if( otfssBtLevel != 0 ) {
	    cerr << "c build in bug-check -- otfss backjump level set wrong" << endl;
	    exit(36);
	  }
	} else { // c.size() == 3
	  detachClause(otfssCls[i],true); // remove from watch lists for long clauses!
	  c[0] = c[1]; c.removePositionUnsorted(1); // shrink clause to binary clause!
	  attachClause(otfssCls[i]); // add to watch list for binary clauses (no extra constraints on clause literals!)
	  otfssBinaries++;
	}
	addCommentToProof("remove literal by OTFSS");
	addToProof(c);             // for RUP
	addToProof(c,true,remLit); // for DRUP
      }
      
      otfssHigherJump = otfssBtLevel < backtrack_level ? otfssHigherJump + 1 : otfssHigherJump; // stats
      if(config.debug_otfss) if( enqueueK > 0 ) cerr << "c jump to otfss level " << otfssBtLevel << endl;
      
      cancelUntil(otfssBtLevel); // backtrack - this level is at least as small as the usual level!
      
      // enqueue all clauses that need to be enqueued!
      for( int i = 0; i < enqueueK; ++ i ) {
	const Clause& c = ca[otfssCls[i]];
	if(config.debug_otfss) cerr << "c enqueue literal " << c[0] << " of OTFSS clause " << c << endl;
	if( value(c[0]) == l_Undef ) uncheckedEnqueue(c[0]); // it can happen that multiple clauses imply the same literal!
	else if ( decisionLevel() == 0 && value(c[0]) == l_Undef ) return l_False; 
	// else not necessary, because unit propagation will find the new conflict!
	if( config.opt_printDecisions > 2 ) cerr << "c enqueue OTFSS literal " << c[0]<< " at level " <<  decisionLevel() << " from clause " << c << endl;
      }
  return backtrack_level > otfssBtLevel ? l_True : l_Undef; // not yet backtracked, 
}

void Solver::printSearchProgress()
{
	if (verbosity >= 1 && (verbEveryConflicts == 0 || conflicts % verbEveryConflicts==0) ){
	    printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% %0.3lf | \n", 
		   (int)starts,(int)nbstopsrestarts, (int)(conflicts/starts), 
		   (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
		   (int)nbReduceDB, nLearnts(), (int)nbDL2,(int)nbRemovedClauses, progressEstimate()*100, agility);
	  }
}