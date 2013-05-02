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

// to be able to read var files
#include "coprocessor-src/VarFileParser.h"

using namespace Minisat;

//=================================================================================================
// Options:

static const char* _cat = "CORE";
static const char* _cr = "CORE -- RESTART";
static const char* _cred = "CORE -- REDUCE";
static const char* _cm = "CORE -- MINIMIZE";



static DoubleOption opt_K                 (_cr, "K",           "The constant used to force restart",            0.8,     DoubleRange(0, false, 1, false));           
static DoubleOption opt_R                 (_cr, "R",           "The constant used to block restart",            1.4,     DoubleRange(1, false, 5, false));           
static IntOption     opt_size_lbd_queue     (_cr, "szLBDQueue",      "The size of moving average for LBD (restarts)", 50, IntRange(10, INT32_MAX));
static IntOption     opt_size_trail_queue     (_cr, "szTrailQueue",      "The size of moving average for trail (block restarts)", 5000, IntRange(10, INT32_MAX));

static IntOption     opt_first_reduce_db     (_cred, "firstReduceDB",      "The number of conflicts before the first reduce DB", 4000, IntRange(0, INT32_MAX));
static IntOption     opt_inc_reduce_db     (_cred, "incReduceDB",      "Increment for reduce DB", 300, IntRange(0, INT32_MAX));
static IntOption     opt_spec_inc_reduce_db     (_cred, "specialIncReduceDB",      "Special increment for reduce DB", 1000, IntRange(0, INT32_MAX));
static IntOption    opt_lb_lbd_frozen_clause      (_cred, "minLBDFrozenClause",        "Protect clauses if their LBD decrease and is lower than (for one turn)", 30, IntRange(0, INT32_MAX));

static IntOption     opt_lb_size_minimzing_clause     (_cm, "minSizeMinimizingClause",      "The min size required to minimize clause", 30, IntRange(3, INT32_MAX));
static IntOption     opt_lb_lbd_minimzing_clause     (_cm, "minLBDMinimizingClause",      "The min LBD required to minimize clause", 6, IntRange(3, INT32_MAX));


static DoubleOption  opt_var_decay         (_cat, "var-decay",   "The variable activity decay factor",            0.95,     DoubleRange(0, false, 1, false));
static DoubleOption  opt_clause_decay      (_cat, "cla-decay",   "The clause activity decay factor",              0.999,    DoubleRange(0, false, 1, false));
static DoubleOption  opt_random_var_freq   (_cat, "rnd-freq",    "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true));
static DoubleOption  opt_random_seed       (_cat, "rnd-seed",    "Used by the random variable selection",         91648253, DoubleRange(0, false, HUGE_VAL, false));
static IntOption     opt_ccmin_mode        (_cat, "ccmin-mode",  "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2));
static IntOption     opt_phase_saving      (_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
static BoolOption    opt_rnd_init_act      (_cat, "rnd-init",    "Randomize the initial activity", false);
/*
static IntOption     opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
static DoubleOption  opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
*/
static DoubleOption  opt_garbage_frac      (_cat, "gc-frac",     "The fraction of wasted memory allowed before a garbage collection is triggered",  0.20, DoubleRange(0, false, HUGE_VAL, false));

static IntOption     opt_allUipHack        ("MODS", "alluiphack", "learn all unit UIPs at any level", 0, IntRange(0, 2) );
static BoolOption    opt_uipHack           ("MODS", "uiphack",      "learn more UIPs at decision level 1", false);
static IntOption     opt_uips              ("MODS", "uiphack-uips", "learn at most X UIPs at decision level 1 (0=all)", 0, IntRange(0, INT32_MAX));


static IntOption     opt_hack              ("REASON",    "hack",      "use hack modifications", 0, IntRange(0, 3) );
static BoolOption    opt_hack_cost         ("REASON",    "hack-cost", "use size cost", true );
static BoolOption    opt_dbg               ("REASON",    "dbg",       "debug hack", false );

static BoolOption    opt_long_conflict     ("REASON",    "longConflict", "if a binary conflict is found, check for a longer one!", false);

// extra 
static IntOption     opt_act               ("INIT", "actIncMode", "how to inc 0=lin, 1=geo", 0, IntRange(0, 1) );
static DoubleOption  opt_actStart          ("INIT", "actStart",   "highest value for first variable", 1024, DoubleRange(0, false, HUGE_VAL, false));
static DoubleOption  pot_actDec            ("INIT", "actDec",     "decrease per element (sub, or divide)",1/0.95, DoubleRange(0, false, HUGE_VAL, true));
static StringOption  actFile               ("INIT", "actFile",    "increase activities of those variables");
static BoolOption    opt_pol               ("INIT", "polMode",    "invert provided polarities", false );
static StringOption  polFile               ("INIT", "polFile",    "use these polarities");
static BoolOption    opt_printDecisions    ("INIT", "printDec",   "print decisions", false );

static IntOption     opt_rMax       ("MODS", "rMax",        "initial max. interval between two restarts (-1 = off)", -1, IntRange(-1, INT32_MAX) );
static DoubleOption  opt_rMaxInc    ("MODS", "rMaxInc",     "increase of the max. restart interval per restart",  1.1, DoubleRange(1, true, HUGE_VAL, false));

static BoolOption    dx                    ("MODS", "laHackOutput","output info about LA", false);
static BoolOption    hk                    ("MODS", "laHack",      "enable lookahead on level 0", false);
static BoolOption    tb                    ("MODS", "tabu",        "do not perform LA, if all considered LA variables are as before", false);
static BoolOption    opt_laDyn             ("MODS", "dyn",         "dynamically set the frequency based on success", false);
static IntOption     opt_laMaxEvery        ("MODS", "hlaMax",      "maximum bound for frequency", 50, IntRange(0, INT32_MAX) );
static IntOption     opt_laLevel           ("MODS", "hlaLevel",    "level of look ahead", 5, IntRange(0, 5) );
static IntOption     opt_laEvery           ("MODS", "hlaevery",    "initial frequency of LA", 1, IntRange(0, INT32_MAX) );
static IntOption     opt_laBound           ("MODS", "hlabound",    "max. nr of LAs (-1 == inf)", -1, IntRange(-1, INT32_MAX) );
static IntOption     opt_laTopUnit         ("MODS", "hlaTop",      "allow another LA after learning another nr of top level units (-1 = never)", -1, IntRange(-1, INT32_MAX));

static BoolOption    opt_prefetch("MODS", "prefetch", "prefetch watch list, when literal is enqueued", false);

//=================================================================================================
// Constructor/Destructor:


Solver::Solver() :

    // DRUP output file
    output (0)
    // Parameters (user settable):
    //
    , verbosity        (0)
    , K              (opt_K)
    , R              (opt_R)
    , sizeLBDQueue   (opt_size_lbd_queue)
    , sizeTrailQueue   (opt_size_trail_queue)
    , firstReduceDB  (opt_first_reduce_db)
    , incReduceDB    (opt_inc_reduce_db)
    , specialIncReduceDB    (opt_spec_inc_reduce_db)
    , lbLBDFrozenClause (opt_lb_lbd_frozen_clause)
    , lbSizeMinimizingClause (opt_lb_size_minimzing_clause)
    , lbLBDMinimizingClause (opt_lb_lbd_minimzing_clause)
  , var_decay        (opt_var_decay)
  , clause_decay     (opt_clause_decay)
  , random_var_freq  (opt_random_var_freq)
  , random_seed      (opt_random_seed)
  , ccmin_mode       (opt_ccmin_mode)
  , phase_saving     (opt_phase_saving)
  , rnd_pol          (false)
  , rnd_init_act     (opt_rnd_init_act)
  , garbage_frac     (opt_garbage_frac)


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
  , currentRestartIntervalBound(opt_rMax)
  , intervalRestart(0)
  
  // LA hack
  ,laAssignments(0)
  ,tabus(0)
  ,las(0)
  ,failedLAs(0)
  ,maxBound(0)
  ,laTime(0)
  ,maxLaNumber(opt_laBound)
  ,topLevelsSinceLastLa(0)
  ,untilLa(opt_laEvery)
  ,laBound(opt_laEvery)
  ,laStart(false)
  
  // preprocessor
  , coprocessor(0)
  , useCoprocessor(true)
{MYFLAG=0;}



Solver::~Solver()
{
    if (coprocessor != 0)
        delete coprocessor;
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
    vec<Lit>    oc;
    oc.clear();
    Lit p; int i, j, flag = 0;
    for (i = j = 0, p = lit_Undef; i < ps.size(); i++) {
        oc.push(ps[i]);
        if (value(ps[i]) == l_True || ps[i] == ~p || value(ps[i]) == l_False)
          flag = 1;
    }

    for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        if (value(ps[i]) == l_True || ps[i] == ~p)
            return true;
        else if (value(ps[i]) != l_False && ps[i] != p)
            ps[j++] = p = ps[i];
    ps.shrink(i - j);

    // print to DRUP
    if ( flag && (output != NULL)) {
      for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        fprintf(output, "%i ", (var(ps[i]) + 1) * (-2 * sign(ps[i]) + 1));
      fprintf(output, "0\n");

      fprintf(output, "d ");
      for (i = j = 0, p = lit_Undef; i < oc.size(); i++)
        fprintf(output, "%i ", (var(oc[i]) + 1) * (-2 * sign(oc[i]) + 1));
      fprintf(output, "0\n");
    }

    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1){
        uncheckedEnqueue(ps[0]);
        return ok = (propagate() == CRef_Undef);
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
    if(c.size()==2) {
      watchesBin[~c[0]].push(Watcher(cr, c[1]));
      watchesBin[~c[1]].push(Watcher(cr, c[0]));
    } else {
      watches[~c[0]].push(Watcher(cr, c[1]));
      watches[~c[1]].push(Watcher(cr, c[0]));
    }
    if (c.learnt()) learnts_literals += c.size();
    else            clauses_literals += c.size(); }




void Solver::detachClause(CRef cr, bool strict) {
    const Clause& c = ca[cr];
    
    assert(c.size() > 1);
    if(c.size()==2) {
      if (strict){
        remove(watchesBin[~c[0]], Watcher(cr, c[1]));
        remove(watchesBin[~c[1]], Watcher(cr, c[0]));
      }else{
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watchesBin.smudge(~c[0]);
        watchesBin.smudge(~c[1]);
      }
    } else {
      if (strict){
        remove(watches[~c[0]], Watcher(cr, c[1]));
        remove(watches[~c[1]], Watcher(cr, c[0]));
      }else{
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watches.smudge(~c[0]);
        watches.smudge(~c[1]);
      }
    }
    if (c.learnt()) learnts_literals -= c.size();
    else            clauses_literals -= c.size(); }


void Solver::removeClause(CRef cr) {
  
  Clause& c = ca[cr];

  // tell DRUP that clause has been deleted
  if (output != NULL) {
    fprintf(output, "d ");
    for (int i = 0; i < c.size(); i++)
      fprintf(output, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
    fprintf(output, "0\n");
  }

  detachClause(cr);
  // Don't leave pointers to free'd memory!
  if (locked(c)) vardata[var(c[0])].reason = CRef_Undef;
  c.mark(1); 
   ca.free(cr);
}


bool Solver::satisfied(const Clause& c) const {
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True)
            return true;
    return false; }


// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
    if (decisionLevel() > level){
        for (int c = trail.size()-1; c >= trail_lim[level]; c--){
            Var      x  = var(trail[c]);
            assigns [x] = l_Undef;
            if (phase_saving > 1 || (phase_saving == 1) && c > trail_lim.last())
                polarity[x] = sign(trail[c]);
            insertVarOrder(x); }
        qhead = trail_lim[level];
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

    return next == var_Undef ? lit_Undef : mkLit(next, rnd_pol ? drand(random_seed) < 0.5 : polarity[next]);
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

int Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel,unsigned int &lbd)
{
    int pathC = 0;
    Lit p     = lit_Undef;

    int units = 0;
    bool isOnlyUnit = true;
    lastDecisionLevel.clear();  // must clear before loop, because alluip can abort loop and would leave filled vector
    
    
    // Generate conflict clause:
    //
    out_learnt.push();      // (leave room for the asserting literal)
    int index   = trail.size() - 1;

    do{
        assert(confl != CRef_Undef); // (otherwise should be UIP)
        Clause& c = ca[confl];

	// Special case for binary clauses
	// The first one has to be SAT
	if( p != lit_Undef && c.size()==2 && value(c[0])==l_False) {
	  
	  assert(value(c[1])==l_True);
	  Lit tmp = c[0];
	  c[0] =  c[1], c[1] = tmp;
	}
	
       if (c.learnt())
            claBumpActivity(c);

        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            Lit q = c[j];

            if (!seen[var(q)] && level(var(q)) > 0){
                varBumpActivity(var(q));
                seen[var(q)] = 1;
                if (level(var(q)) >= decisionLevel()) {
                    pathC++;
#ifdef UPDATEVARACTIVITY
		    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
		    if((reason(var(q))!= CRef_Undef)  && ca[reason(var(q))].learnt()) 
		      lastDecisionLevel.push(q);
#endif

		} else {
                    out_learnt.push(q);
		    // if( isOnlyUnit && units > 0 ) printf("c stopUnits because of variable %d\n", var(q) + 1 );
		    isOnlyUnit = false; // we found a literal from another level, thus the multi-unit clause cannot be learned
		}
	    }

        }
        
        if( !isOnlyUnit && units > 0 ) break; // do not consider the next clause, because we cannot continue with units
        
        // Select next clause to look at:
        while (!seen[var(trail[index--])]); // cerr << "c check seen for literal " << (sign(trail[index]) ? "-" : " ") << var(trail[index]) + 1 << " at index " << index << " and level " << level( var( trail[index] ) )<< endl;
        p     = trail[index+1];
//	cerr << "c get the reason for literal " << (sign(p) ? "-" : " ") << var(p) + 1 << endl;
        confl = reason(var(p));
        seen[var(p)] = 0;
        pathC--;

	// do unit analysis only, if the clause did not become larger already!
	if( opt_allUipHack > 0  && pathC <= 0 && isOnlyUnit && out_learnt.size() == 1+units ) { 
	  learntUnit ++; 
	  units ++; // count current units
	  // printf(" c found units: %d, this var: %d\n", units, var(~p)+1 );
	  out_learnt.push( ~p ); // store unit
	  if( opt_allUipHack == 1 ) break; // for now, stop at the first unit! // TODO collect all units
	}
	
	// do stop here
    } while (
       //if no unit clause is learned, and the first UIP is hit
         (units == 0 && pathC > 0)
      // or 1stUIP is unit, but the current learned clause would be bigger, and there are still literals on the current level
      || (isOnlyUnit && units > 0 && index >= trail_lim[ decisionLevel() - 1] ) 
      
    );
    
    assert( out_learnt.size() > 0 && "there has to be some learnt clause" );
    
        // if we do not use units, add asserting literal to learned clause!
    if( units == 0 ) out_learnt[0] = ~p;
    else { 
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
      return units;
    }

    // Simplify conflict clause:
    //
    int i, j;
    out_learnt.copyTo(analyze_toclear);
    if (ccmin_mode == 2){
        uint32_t abstract_level = 0;
        for (i = 1; i < out_learnt.size(); i++)
            abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)

        for (i = j = 1; i < out_learnt.size(); i++)
            if (reason(var(out_learnt[i])) == CRef_Undef || !litRedundant(out_learnt[i], abstract_level))
                out_learnt[j++] = out_learnt[i];
        
    }else if (ccmin_mode == 1){
        for (i = j = 1; i < out_learnt.size(); i++){
            Var x = var(out_learnt[i]);

            if (reason(x) == CRef_Undef)
                out_learnt[j++] = out_learnt[i];
            else{
                Clause& c = ca[reason(var(out_learnt[i]))];
                for (int k = 1; k < c.size(); k++)
                    if (!seen[var(c[k])] && level(var(c[k])) > 0){
                        out_learnt[j++] = out_learnt[i];
                        break; }
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
    if(out_learnt.size()<=lbSizeMinimizingClause) {
      // Find the LBD measure                                                                                                         
      lbd = 0;
      MYFLAG++;
      for(int i=0;i<out_learnt.size();i++) {

	int l = level(var(out_learnt[i]));
	if (permDiff[l] != MYFLAG) {
	  permDiff[l] = MYFLAG;
	  lbd++;
	}
      }


      if(lbd<=lbLBDMinimizingClause){
      MYFLAG++;
      
      for(int i = 1;i<out_learnt.size();i++) {
	permDiff[var(out_learnt[i])] = MYFLAG;
      }

      vec<Watcher>&  wbin  = watchesBin[p];
      int nb = 0;
      for(int k = 0;k<wbin.size();k++) {
	Lit imp = wbin[k].blocker;
	if(permDiff[var(imp)]==MYFLAG && value(imp)==l_True) {
	  /*      printf("---\n");
		  printClause(out_learnt);
		  printf("\n");
		  
		  printClause(*(wbin[k].clause));printf("\n");
	  */
	  nb++;
	  permDiff[var(imp)]= MYFLAG-1;
	}
      }
      int l = out_learnt.size()-1;
      if(nb>0) {
	nbReducedClauses++;
	for(int i = 1;i<out_learnt.size()-nb;i++) {
	  if(permDiff[var(out_learnt[i])]!=MYFLAG) {
	    Lit p = out_learnt[l];
	    out_learnt[l] = out_learnt[i];
	    out_learnt[i] = p;
	    l--;i--;
	  }
	}
	
	//    printClause(out_learnt);
	//printf("\n");
	out_learnt.shrink(nb);
      
	/*printf("nb=%d\n",nb);
	  printClause(out_learnt);
	  printf("\n");
	*/
      }
    }
    }
    // Find correct backtrack level:
    //
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else{
        int max_i = 1;
        // Find the first literal assigned at the next-highest level:
        for (int i = 2; i < out_learnt.size(); i++)
            if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
                max_i = i;
        // Swap-in this literal at index 1:
        Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
        out_btlevel       = level(var(p));
    }


  // Find the LBD measure 
  lbd = 0;
  MYFLAG++;
  for(int i=0;i<out_learnt.size();i++) {
    
    int l = level(var(out_learnt[i]));
    if (permDiff[l] != MYFLAG) {
      permDiff[l] = MYFLAG;
      lbd++;
    }
  }


  
#ifdef UPDATEVARACTIVITY
  // UPDATEVARACTIVITY trick (see competition'09 companion paper)
  if(lastDecisionLevel.size()>0) {
    for(int i = 0;i<lastDecisionLevel.size();i++) {
      if(ca[reason(var(lastDecisionLevel[i]))].lbd()<lbd)
	varBumpActivity(var(lastDecisionLevel[i]));
    }
    lastDecisionLevel.clear();
  } 
#endif	    



    for (int j = 0; j < analyze_toclear.size(); j++) seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
    return 0;
}


// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool Solver::litRedundant(Lit p, uint32_t abstract_levels)
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

        for (int i = 1; i < c.size(); i++){
            Lit p  = c[i];
            if (!seen[var(p)] && level(var(p)) > 0){
                if (reason(var(p)) != CRef_Undef && (abstractLevel(var(p)) & abstract_levels) != 0){
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
    assert(value(p) == l_Undef);
    assigns[var(p)] = lbool(!sign(p));
    vardata[var(p)] = mkVarData(from, decisionLevel());
    
    if( opt_hack > 0 )
      trailPos[ var(p) ] = (int)trail.size(); /// modified learning, important: before trail.push()!

    // prefetch watch lists
    if(opt_prefetch) __builtin_prefetch( & watches[p] );
      
    trail.push_(p);
}


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

/// print elements of a vector
template <typename T>
inline std::ostream& operator<<(std::ostream& other, const std::vector<T>& data ) 
{
  for( int i = 0 ; i < data.size(); ++ i )
    other << " " << data[i];
  return other;
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
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll();
    watchesBin.cleanAll();
    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;

	
	    // First, Propagate binary clauses 
	vec<Watcher>&  wbin  = watchesBin[p];
	
	for(int k = 0;k<wbin.size();k++) {
	  
	  Lit imp = wbin[k].blocker;
	  
	  if(value(imp) == l_False) {
	    if( !opt_long_conflict ) return wbin[k].cref;
	    // else
	    confl = wbin[k].cref;
	    break;
	  }
	  
	  if(value(imp) == l_Undef) {
	    //printLit(p);printf(" ");printClause(wbin[k].cref);printf("->  ");printLit(imp);printf("\n");
	    uncheckedEnqueue(imp,wbin[k].cref);
	  } else {
	    // hack
	      // consider variation only, if the improvement options are enabled!
	      if( (opt_hack > 0 ) && reason(var(imp)) != CRef_Undef) { // if its not a decision
		const int implicantPosition = trailPos[ var(imp) ];
		bool fail = false;
	       if( value( p ) != l_False || trailPos[ var(p) ] > implicantPosition ) { fail = true; }

		// consider change only, if the order of positions is correct, e.g. impl realy implies p, otherwise, we found a cycle
		if( !fail ) {
		  if( opt_hack_cost ) { // size based cost
		    if( vardata[var(imp)].cost > 2  ) { // 2 is smaller than old reasons size
		      if( opt_dbg ) cerr << "c for literal " << imp << " replace reason " << vardata[var(imp)].reason << " with " << wbin[k].cref << endl;
		      vardata[var(imp)].reason = wbin[k].cref;
		      vardata[var(imp)].cost = 2;
		      
		    } 
		  } else { // lbd based cost
		    int thisCost = ca[wbin[k].cref].lbd();
		    if( vardata[var(imp)].cost > thisCost  ) { // 2 is smaller than old reasons size
		      if( opt_dbg ) cerr << "c for literal " << imp << " replace reason " << vardata[var(imp)].reason << " with " << wbin[k].cref << endl;
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
    


        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;){
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True){
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            CRef     cr        = i->cref;
            Clause&  c         = ca[cr];
            Lit      false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True){
	      
	      // consider variation only, if the improvement options are enabled!
	      if( (opt_hack > 0 ) && reason(var(first)) != CRef_Undef) { // if its not a decision
		const int implicantPosition = trailPos[ var(first) ];
		bool fail = false;
		for( int i = 1; i < c.size(); ++ i ) {
		  if( value( c[i] ) != l_False || trailPos[ var(c[i]) ] > implicantPosition ) { fail = true; break; }
		}

		// consider change only, if the order of positions is correct, e.g. impl realy implies p, otherwise, we found a cycle
		if( !fail ) {
		  
		  if( opt_hack_cost ) { // size based cost
		    if( vardata[var(first)].cost > c.size()  ) { // 2 is smaller than old reasons size -> update vardata!
		      if( opt_dbg ) cerr << "c for literal " << c[0] << " replace reason " << vardata[var(first)].reason << " with " << cr << endl;
		      vardata[var(first)].reason = cr;
		      vardata[var(first)].cost = c.size();
		    }
		  } else { // lbd based cost
		    int thisCost = c.lbd();
		    if( vardata[var(first)].cost > thisCost  ) { // 2 is smaller than old reasons size -> update vardata!
		      if( opt_dbg ) cerr << "c for literal " << c[0] << " replace reason " << vardata[var(first)].reason << " with " << cr << endl;
		      vardata[var(first)].reason = cr;
		      vardata[var(first)].cost = thisCost;
		    }
		  }
		  
		   
		} else {
		  // could be a cycle here, or this clause is satisfied, but not a reason clause!
		}
	      }
	      
	      *j++ = w; continue; }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False){
                    c[1] = c[k]; c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause; }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False){
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            }else {
                uncheckedEnqueue(first, cr);
	  
#ifdef DYNAMICNBLEVEL		    
		// DYNAMIC NBLEVEL trick (see competition'09 companion paper)
		if(c.learnt()  && c.lbd()>2) { 
		  MYFLAG++;
		  unsigned  int nblevels =0;
		  for(int i=0;i<c.size();i++) {
		    int l = level(var(c[i]));
		    if (permDiff[l] != MYFLAG) {
		      permDiff[l] = MYFLAG;
		      nblevels++;
		    }
		    
		    
		  }
		  if(nblevels+1<c.lbd() ) { // improve the LBD
		    if(c.lbd()<=lbLBDFrozenClause) {
		      c.setCanBeDel(false); 
		    }
		      // seems to be interesting : keep it for the next round
		    c.setLBD(nblevels); // Update it
		  }
		}
#endif
		
	    }
        NextClause:;
        }
        ws.shrink(i - j);
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
 
  int     i, j;
  nbReduceDB++;
  sort(learnts, reduceDB_lt(ca));

  // We have a lot of "good" clauses, it is difficult to compare them. Keep more !
  if(ca[learnts[learnts.size() / RATIOREMOVECLAUSES]].lbd()<=3) nbclausesbeforereduce +=specialIncReduceDB; 
  // Useless :-)
  if(ca[learnts.last()].lbd()<=5)  nbclausesbeforereduce +=specialIncReduceDB; 
  
  
  // Don't delete binary or locked clauses. From the rest, delete clauses from the first half
  // Keep clauses which seem to be usefull (their lbd was reduce during this sequence)

  int limit = learnts.size() / 2;

  for (i = j = 0; i < learnts.size(); i++){
    Clause& c = ca[learnts[i]];
    if (c.lbd()>2 && c.size() > 2 && c.canBeDel() &&  !locked(c) && (i < limit)) {
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
}


void Solver::removeSatisfied(vec<CRef>& cs)
{
  
    int i, j;
    for (i = j = 0; i < cs.size(); i++){
        Clause& c = ca[cs[i]];
        if (c.size()>=2 && satisfied(c)) // A bug if we remove size ==2, We need to correct it, but later.
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
    assert(decisionLevel() == 0);

    if (!ok || propagate() != CRef_Undef)
        return ok = false;

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


/** modified conflict analysis, which finds all UIPs after level 1 conflicts
 * 
 */
void Solver::analyzeOne( CRef confl, vec<Lit>& learntUnits)
{
    learntUnits.clear();
    assert( decisionLevel() == 1 && "works only on first decision level!" );
    // genereate learnt clause - extract selected number of units!
    int pathC = 0;
    Lit p     = lit_Undef;
    unsigned uips = 0;
    unsigned loops = 0;
    int index = trail.size() - 1;

    do{
        assert(confl != CRef_Undef && "the current literal has to have a reason");
        loops ++;
        if( confl != CRef_Undef ) {
	  Clause& c = ca[confl];
	  for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
	      Lit q = c[j];
	      if( c.size() == 2 && q == p ) q = c[0]; // in glucose, binary clauses are not ordered!
	      if (!seen[var(q)] && level(var(q)) > 0){
		  varBumpActivity(var(q));
		  seen[var(q)] = 1;
		  if (level(var(q)) >= decisionLevel()) pathC++;
	      }
	  }
	}
	assert ( (loops != 1 || pathC > 1) && "in the first iteration there have to be at least 2 literals of the decision level!" );
	
        while (!seen[var(trail[index--])]);
        p     = trail[index+1];
        confl = reason(var(p));
        seen[var(p)] = 0;
        pathC--;
        // this works only is the decision level is 1
        if( pathC <= 0 ) {
          assert( loops > 1 && "learned clause can occur only if already 2 clauses have been resolved!" );
           uips ++;
           learntUnits.push(~p);

	   // learned enough uips - return. if none has been found, return false!
           if( opt_uips != 0 && uips >= opt_uips ) {
             // reset seen literals - index points to position before current UIP
             for( int i = 0 ; i < index + 1; ++ i ) seen[ var(trail[i]) ] = 0;
             return;
           }
        }

	
    }while (index >= trail_lim[0] ); // pathC > 0 finds unit clauses

    // add uip in form of decision!  
    if( learntUnits.size() == 0 || learntUnits.last() != ~p ) learntUnits.push(~p);
  
  assert( learntUnits.size() > 0 && "there has to be at least one UIP (decision literal)" );
  return;
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
    for (;;){
        CRef confl = propagate();
        if (confl != CRef_Undef){
            // CONFLICT
	  conflicts++; conflictC++;
	  if( opt_printDecisions ) printf("c conflict at level %d\n", decisionLevel() );

	  if (verbosity >= 1 && conflicts%verbEveryConflicts==0){
	    printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% |\n", 
		   (int)starts,(int)nbstopsrestarts, (int)(conflicts/starts), 
		   (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
		   (int)nbReduceDB, nLearnts(), (int)nbDL2,(int)nbRemovedClauses, progressEstimate()*100);
	  }
	  if (decisionLevel() == 0) {
	    return l_False;
	    
	  }

	  trailQueue.push(trail.size());
	  if( conflicts>LOWER_BOUND_FOR_BLOCKING_RESTART && lbdQueue.isvalid()  && trail.size()>R*trailQueue.getavg()) {
	    lbdQueue.fastclear();
	    nbstopsrestarts++;
	    if(!blocked) {lastblockatrestart=starts;nbstopsrestartssame++;blocked=true;}
	  }

	    l1conflicts = ( decisionLevel() != 1 ? l1conflicts : l1conflicts + 1 );
	    
	    if( decisionLevel() == 1 && opt_uipHack) {
	      // learn a bunch of unit clauses!
	      analyzeOne(confl, learnt_clause );

	      nblevels = 1;
	      lbdQueue.push(nblevels);
	      sumLBD += nblevels;
	      cancelUntil(0);

	      for( int i = 0 ; i < learnt_clause.size(); ++ i )
		uncheckedEnqueue(learnt_clause[i]);
	      

          // write learned unit clauses to DRUP!
          if (output != NULL) {
	          for( int i = 0 ; i < learnt_clause.size(); ++ i )
              for (int i = 0; i < learnt_clause.size(); i++)
                fprintf(output, "%i 0\n" , (var(learnt_clause[i]) + 1) *
                            (-2 * sign(learnt_clause[i]) + 1) );
          }


	      nbUn+=learnt_clause.size(); // stats
	      if(nblevels<=2) nbDL2++; // stats

	      multiLearnt = ( learnt_clause.size() > 1 ? multiLearnt + 1 : multiLearnt );
	      topLevelsSinceLastLa ++;
	      
	    } else {
	      learnt_clause.clear();

	      int ret = analyze(confl, learnt_clause, backtrack_level,nblevels);
	      if( ret > 0 ) { // multiple learned clauses
		assert( backtrack_level == 0 && "found units, have to jump to level 0!" );
		cancelUntil(backtrack_level);
		lbdQueue.push(1);sumLBD += 1; // unit clause has one level
		for( int i = 0 ; i < learnt_clause.size(); ++ i ) // add all units to current state
		  { uncheckedEnqueue(learnt_clause[i]);  }
		  
          // write learned unit clauses to DRUP!
          if (output != NULL) {
	          for( int i = 0 ; i < learnt_clause.size(); ++ i )
              for (int i = 0; i < learnt_clause.size(); i++)
                fprintf(output, "%i 0\n" , (var(learnt_clause[i]) + 1) *
                            (-2 * sign(learnt_clause[i]) + 1) );
          }
		  
		multiLearnt = ( learnt_clause.size() > 1 ? multiLearnt + 1 : multiLearnt ); // stats
		topLevelsSinceLastLa ++;
	      } else { // treat usual learned clause!

		lbdQueue.push(nblevels);
		sumLBD += nblevels;

		cancelUntil(backtrack_level);

          // write learned clause to DRUP!
          if (output != NULL) {
              for (int i = 0; i < learnt_clause.size(); i++)
                fprintf(output, "%i " , (var(learnt_clause[i]) + 1) *
                            (-2 * sign(learnt_clause[i]) + 1) );
              fprintf(output, "0\n");
          }

		if (learnt_clause.size() == 1){
		    topLevelsSinceLastLa ++;
		    uncheckedEnqueue(learnt_clause[0]);nbUn++;
		    if( opt_printDecisions ) printf("c enqueue literal $s%d at level %d\n", sign(learnt_clause[0]) ? "-" : "", var(learnt_clause[0]) + 1, decisionLevel() );
		}else{
		  CRef cr = ca.alloc(learnt_clause, true);
		  ca[cr].setLBD(nblevels); 
		  if(nblevels<=2) nbDL2++; // stats
		  if(ca[cr].size()==2) nbBin++; // stats

		  learnts.push(cr);
		  attachClause(cr);
		  
		  claBumpActivity(ca[cr]);
		  uncheckedEnqueue(learnt_clause[0], cr);
		  if( opt_printDecisions ) printf("c enqueue literal %s%d at level %d\n", sign(learnt_clause[0]) ? "-" : "", var(learnt_clause[0]) + 1, decisionLevel() );
		}

	      }
	    }
            varDecayActivity();
            claDecayActivity();

	    conflictsSinceLastRestart ++;
           
        }else{
	  // Our dynamic restart, see the SAT09 competition compagnion paper 
	  if (
	      ( lbdQueue.isvalid() && ((lbdQueue.getavg()*K) > (sumLBD / conflicts)))
	      || (opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound )// if thre have been too many conflicts
	     ) {
	    
	    // increase current limit, if this has been the reason for the restart!!
	    if( (opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound ) ) { intervalRestart++;conflictsSinceLastRestart = (double)conflictsSinceLastRestart * (double)opt_rMaxInc; }
	    
	    conflictsSinceLastRestart = 0;
	    lbdQueue.fastclear();
	    progress_estimate = progressEstimate();
	    cancelUntil(0);
	    return l_Undef; }


           // Simplify the set of problem clauses:
	  if (decisionLevel() == 0 && !simplify()) {
	    printf("c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
	    return l_False;
	  }
	    // Perform clause database reduction !
	    if(conflicts>=curRestart* nbclausesbeforereduce) 
	      {
	
		assert(learnts.size()>0);
		curRestart = (conflicts/ nbclausesbeforereduce)+1;
		reduceDB();
		nbclausesbeforereduce += incReduceDB;
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
		  printf("c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
		  // Model found:
		  return l_True;
		}
            }
            
            // if sufficiently many new top level units have been learned, trigger another LA!
	    if( opt_laTopUnit != -1 && topLevelsSinceLastLa >= opt_laTopUnit && maxLaNumber != -1) { maxLaNumber ++; topLevelsSinceLastLa = 0 ; }
            if(hk && (maxLaNumber == -1 || (las < maxLaNumber)) ) { // perform LA hack -- only if max. nr is not reached?
	      int hl = decisionLevel();
	      if( hl == 0 ) if( --untilLa == 0 ) {laStart = true; if(dx)cerr << "c startLA" << endl;}
	      if( laStart && hl == opt_laLevel ) {
		if( !laHack(learnt_clause) ) return l_False;
		topLevelsSinceLastLa = 0;
	      }
	    }
            
            // Increase decision level and enqueue 'next'
            newDecisionLevel();
	    if(opt_printDecisions) printf("c decide %s%d at level %d\n", sign(next) ? "-" : "", var(next) + 1, decisionLevel() );
            uncheckedEnqueue(next);
        }
    }
}


void Solver::fm(uint64_t*p,bool mo){ // for negative, add bit patter 10, for positive, add bit pattern 01!
for(Var v=0;v<nVars();++v) p[v]=(p[v]<<2); // move two bits, so that the next assignment can be put there

if(!mo) for(int i=trail_lim[0];i<trail.size();++i){
  Lit l=trail[i];
  p[var(l)]+=(sign(l)?2:1);
} //TODO: check whether it is worth to have the extra variable!

//cerr << "c move!" << endl;
// TODO: can be removed!
//for(Var v=0;v<nVars();++v) cerr << "c update " << v + 1 << " to " << p[v] << endl;

}

bool Solver::laHack(vec<Lit>& toEnqueue ) {
  assert(decisionLevel() == opt_laLevel && "can perform LA only, if level is correct" );
  laTime = cpuTime() - laTime;

   uint64_t hit[]={5,10,85,170,21845,43690,1431655765,2863311530,6148914691236517205,12297829382473034410};
  // TODO: remove white spaces, output, comments and assertions!
  uint64_t p[nVars()];
  memset(p,0,nVars()*sizeof(uint64_t)); // TODO: change sizeof into 4!
  vec<char> bu;
  polarity.copyTo(bu);  
  uint64_t pt=~0; // everything set -> == 2^32-1
  if(dx) cerr << "c initial pattern: " << pt << endl;
  Lit d[5];
  for(int i=0;i<opt_laLevel;++i) d[i]=trail[ trail_lim[i] ]; // get all decisions into dec array

  if(tb){ // use tabu
    bool same = true;
    for( int i = 0 ; i < opt_laLevel; ++i ){
      for( int j = 0 ; j < opt_laEvery; ++j )
	if(var(d[i])!=var(hstry[j]) ) same = false; 
    }
    if( same ) { laTime = cpuTime() - laTime; return true; }
    for( int i = 0 ; i < opt_laLevel; ++i ) hstry[i]=d[i];
  }
  las++;
  
  int bound=1<<opt_laLevel;
  if(dx) cerr << "c do LA until " << bound << " starting at level " << decisionLevel() << endl;
  fm(p,false); // fill current model
  for(uint64_t i=1;i<bound;++i){ // for each combination
    cancelUntil(0);
    newDecisionLevel();
    for(int j=0;j<opt_laLevel;++j) uncheckedEnqueue((i&(1<<j))!=0?~d[j]:d[j]); // flip polarity of d[j], if corresponding bit in i is set -> enumberate all combinations!
    bool f=propagate() != CRef_Undef;
    if(dx) cerr << "c propagated iteration " << i << endl;
    fm(p,f);
    uint64_t m=3;
    if(f)pt=(pt&(~(m<<(2*i))));
    if(dx) cerr << "c this propafation [" << i << "] failed: " << f << " current match pattern: " << pt << "(inv: " << ~pt << ")" << endl;
    if(dx) { cerr << "c cut: "; for(int j=0;j<2<<opt_laLevel;++j) cerr << ((pt&(1<<j))  != (uint64_t)0 ); cerr << endl; }
  }
  cancelUntil(0);
  int t=2*opt_laLevel-2;
  // evaluate result of LA!
  bool foundUnit=false;
  if(dx) cerr << "c pos hit: " << (pt & (hit[t])) << endl;
  if(dx) cerr << "c neg hit: " << (pt & (hit[t+1])) << endl;
  toEnqueue.clear();
  for(Var v=0; v<nVars(); ++v ){
    if(value(v)==l_Undef){ // l_Undef == 2
      if( (pt & p[v]) == (pt & (hit[t])) ){  foundUnit=true;toEnqueue.push( mkLit(v,false) );laAssignments++;} // pos consequence
      if( (pt & p[v]) == (pt & (hit[t+1])) ){foundUnit=true;toEnqueue.push( mkLit(v,true)  );laAssignments++;} // neg consequence

      // TODO: can be cut!
      if(dx) if( (pt & p[v]) == (pt & (hit[t])) )   { cerr << "c pos " << v+1 << endl; }
      if(dx) if( (pt & p[v]) == (pt & (hit[t+1])) ) { cerr << "c neg " << v+1 << endl; }
      if(dx) if( p[v] != 0 ) cerr << "p[" << v+1 << "] = " << p[v] << endl;
    } // use brackets needs less symbols than writing continue!
  }
  
  // enqueue new units
  for( int i = 0 ; i < toEnqueue.size(); ++ i ) uncheckedEnqueue( toEnqueue[i] );

  // TODO: apply schema to have all learned unit clauses in DRUP! -> have an extra vector!
  vector< vector< Lit > > clauses;
  vector<Lit> tmp;
  
  if(  0  ) cerr << "c LA " << las << " write clauses: " << endl;
  if (output != NULL) {
     // construct look-ahead clauses
    for( int i = 0 ; i < toEnqueue.size(); ++ i ){
      // cerr << "c write literal " << i << " from LA " << las << endl;
      const Lit l = toEnqueue[i];
      clauses.clear();
      tmp.clear();

      int litList[] = {0,1,2,3,4,5,-1,0,1,2,3,5,-1,0,1,2,4,5,-1,0,1,2,5,-1,0,1,3,4,5,-1,0,1,3,5,-1,0,1,4,5,-1,0,1,5,-1,0,2,3,4,5,-1,0,2,3,5,-1,0,2,4,5,-1,0,2,5,-1,0,3,4,5,-1,0,3,5,-1,0,4,5,-1,0,5,-1,1,2,3,4,5,-1,1,2,3,5,-1,1,2,4,5,-1,1,2,5,-1,1,3,4,5,-1,1,3,5,-1,1,4,5,-1,1,5,-1,2,3,4,5,-1,2,3,5,-1,2,4,5,-1,2,5,-1,3,4,5,-1,3,5,-1,4,5,-1,5,-1};
      int cCount = 0 ;
      tmp.clear();
      assert(opt_laLevel == 5 && "current proof generation only works for level 5!" );
      for ( int j = 0; true; ++ j ) { // TODO: count literals!
	int k = litList[j];
	if( k == -1 ) { clauses.push_back(tmp);
	  cerr << "c write " << tmp << endl;
	  tmp.clear(); 
	  cCount ++;
	  if( cCount == 32 ) break;
	  continue; 
	}
	if( k == 5 ) tmp.push_back( l );
	else tmp.push_back( d[k] );
      }

      // write all clauses to proof -- including the learned unit
      for( int j = 0; j < clauses.size() ; ++ j ){
	if(  0  ) cerr << "c write clause [" << j << "] " << clauses[ j ] << endl;
	for (int i = 0; i < clauses[j].size(); i++)
	  fprintf(output, "%i " , (var(clauses[j][i]) + 1) * (-2 * sign(clauses[j][i]) + 1) );
	fprintf(output, "0\n");
      }
      // delete all redundant clauses
      for( int j = 0; j+1 < clauses.size() ; ++ j ){
	assert( clauses[j].size() > 1 && "the only unit clause in the list should not be removed!" );
	fprintf(output, "d ");
	for (int i = 0; i < clauses[j].size(); i++)
	  fprintf(output, "%i " , (var(clauses[j][i]) + 1) * (-2 * sign(clauses[j][i]) + 1) );
	fprintf(output, "0\n");
      }
    }
    
  }

  toEnqueue.clear();
  
  if( propagate() != CRef_Undef ){laTime = cpuTime() - laTime; return false;}
  
  // done with la, continue usual search, until next time something is done
  bu.copyTo(polarity); // restore polarities from before!
  if(opt_laDyn){
    if(foundUnit)laBound=opt_laEvery; 		// reset down to specified minimum
    else {if(laBound<opt_laMaxEvery)laBound++;}	// increase by one!
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


// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_()
{
    model.clear();
    conflict.clear();
    if (!ok) return l_False;

    lbdQueue.initSize(sizeLBDQueue);


    trailQueue.initSize(sizeTrailQueue);
    sumLBD = 0;
    
    solves++;
    
    
    lbool   status        = l_Undef;
    nbclausesbeforereduce = firstReduceDB;
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
printf("c ==================================[ Search Statistics (every %6d conflicts) ]=========================\n",verbEveryConflicts);
      printf("c |                                                                                                       |\n"); 

      printf("c |          RESTARTS           |          ORIGINAL         |              LEARNT              | Progress |\n");
      printf("c |       NB   Blocked  Avg Cfc |    Vars  Clauses Literals |   Red   Learnts    LBD2  Removed |          |\n");
      printf("c =========================================================================================================\n");

    }
    
    // parse for variable polarities from file!
    if( polFile ) { // read polarities from file, initialize phase polarities with this value!
      string filename = string(polFile);
      Coprocessor::VarFileParser vfp( filename );
      vector<int> polLits;
      vfp.extract( polLits );
      for( int i = 0 ; i < polLits.size(); ++ i ) {
	const Var v = polLits[i] > 0 ? polLits[i] : - polLits[i];
	if( v - 1 >= nVars() ) continue; // other file might contain more variables
	Lit thisL = mkLit(v-1, polLits[i] < 0 );
	if( opt_pol ) thisL = ~thisL;
	polarity[ v-1 ] = sign(thisL);
      }
      cerr << "c adopted poarity of " << polLits.size() << " variables" << endl;
    }
    
    
    // parse for activities from file!
    if( actFile ) { // set initial activities
      string filename = string(actFile);
      Coprocessor::VarFileParser vfp( filename );
      vector<int> actVars;
      vfp.extract(actVars);
      
      double thisValue = opt_actStart;
      for( int i = 0 ; i < actVars.size(); ++ i ) {
	const Var v = actVars[i]-1;
	if( v >= nVars() ) continue; // other file might contain more variables
	activity[ v] = thisValue;
	thisValue = (opt_act == 0 ? thisValue - pot_actDec : thisValue / pot_actDec );
      }
      cerr << "c adopted activity of " << actVars.size() << " variables" << endl;
      rebuildOrderHeap();
    }
    

    if( status == l_Undef ) {
	  // restart, triggered by the solver
	  if( coprocessor == 0 && useCoprocessor) coprocessor = new Coprocessor::Preprocessor(this); // use number of threads from coprocessor
          if( coprocessor != 0 && useCoprocessor) status = coprocessor->preprocess();
         if (verbosity >= 1) printf("c =========================================================================================================\n");
    }
    
    // Search:
    int curr_restarts = 0;
    while (status == l_Undef){
      status = search(0); // the parameter is useless in glucose, kept to allow modifications

        if (!withinBudget()) break;
        curr_restarts++;
	
	if( status == l_Undef ) {
	  // restart, triggered by the solver
	  if( coprocessor == 0 && useCoprocessor)  coprocessor = new Coprocessor::Preprocessor(this); // use number of threads from coprocessor
          if( coprocessor != 0 && useCoprocessor) status = coprocessor->inprocess();
	}
	
    }

    if (verbosity >= 1)
      printf("c =========================================================================================================\n");
    
    if (verbosity >= 1) {
            printf("c Learnt At Level 1: %d  Multiple: %d Units: %d\n", l1conflicts, multiLearnt,learntUnit);
	    printf("c LAs: %d laSeconds %lf LA assigned: %d tabus: %d, failedLas: %d, maxEvery %d\n", laTime, las, laAssignments, tabus, failedLAs, maxBound );
	    printf("c IntervalRestarts: %d\n", intervalRestart);
    }

    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
	
	if( coprocessor != 0 && useCoprocessor) coprocessor->extendModel(model);
	
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
    for (int i = 0; i < learnts.size(); i++)
        ca.reloc(learnts[i], to);

    // All original:
    //
    for (int i = 0; i < clauses.size(); i++)
        ca.reloc(clauses[i], to);
}


void Solver::garbageCollect()
{
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() >= ca.wasted() ? ca.size() - ca.wasted() : 0); //FIXME security-workaround, for CP3 (due to inconsistend wasted-bug)

    relocAll(to);
    if (verbosity >= 2)
        printf("|  Garbage collection:   %12d bytes => %12d bytes             |\n", 
               ca.size()*ClauseAllocator::Unit_Size, to.size()*ClauseAllocator::Unit_Size);
    to.moveTo(ca);
}
