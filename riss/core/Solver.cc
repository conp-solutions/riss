/***************************************************************************************[Solver.cc]
 Glucose -- Copyright (c) 2009, Gilles Audemard, Laurent Simon
                CRIL - Univ. Artois, France
                LRI  - Univ. Paris Sud, France

Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
Glucose are exactly the same as Minisat on which it is based on. (see below).

---------------

Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Copyright (c) 2012-2014, Norbert Manthey, LGPL v2, see LICENSE

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
#include "riss/utils/VarFileParser.h"

#include "riss/core/EnumerateMaster.h"

using namespace Coprocessor;
using namespace std;

namespace Riss
{

//=================================================================================================
// Constructor/Destructor:


Solver::Solver(CoreConfig* externalConfig, const char* configName) :    // CoreConfig& _config
    privateConfig(externalConfig == 0 ? new CoreConfig(configName == 0 ? "" : configName) : externalConfig)
    , deleteConfig(externalConfig == 0)
    , config(* privateConfig)
    // DRUP output file
    , proofFile(0)

    // setup search configuration as code to fill struct
    , verbosity(config.opt_verb)
    , verbEveryConflicts(100000)

    , posInAllClauses(true)
    , negInAllClauses(true)

    , random_var_freq(config.opt_random_var_freq)
    , random_seed(config.opt_random_seed)
    , rnd_pol(random_var_freq > 0)               // if there is a random variable frequency, allow random decisions
    , rnd_init_act(config.opt_rnd_init_act)
    , garbage_frac(config.opt_garbage_frac)

    // Statistics: (formerly in 'SolverStats')
    //
    , nbRemovedClauses(0), nbReducedClauses(0), nbDL2(0), nbBin(0), nbUn(0), nbReduceDB(0)
    , solves(0), starts(0), decisions(0), rnd_decisions(0), propagations(0), conflicts(0), nbstopsrestarts(0), nbstopsrestartssame(0), lastblockatrestart(0)
    , dec_vars(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0)
    , performSimplificationNext(0)
    , nbLCM(0), nbLitsLCM(0), nbConflLits(0), nbLCMattempts(0), nbLCMsuccess(0), npLCMimpDrop(0), nbRound1Lits(0), nbRound2Lits(0), nbLCMfalsified(0)
    , curRestart(1)



    , ok(true)
    , cla_inc(1)
    , var_inc(1)
    , watches(WatcherDeleted(ca))
//  , watchesBin            (WatcherDeleted(ca))

    , reverseMinimization(config.opt_use_reverse_minimization)  // reverse minimization hack
    , earlyAssumptionConflict(config.opt_earlyAssumptionConflict)

    , eqInfo(this)

    , qhead(0)
    , realHead(0)
    , simpDB_assigns(-1)
    , simpDB_props(0)
    , order_heap(VarOrderLt(activity))
    , progress_estimate(0)
    , remove_satisfied(true)

    // removal setup
    , max_learnts(config.opt_max_learnts)
    , learntsize_factor(config.opt_learnt_size_factor)
    , learntsize_inc(config.opt_learntsize_inc)
    , learntsize_adjust_start_confl(config.opt_learntsize_adjust_start_confl)
    , learntsize_adjust_inc(config.opt_learntsize_adjust_inc)
    , learntsize_adjust_confl(config.opt_learntsize_adjust_start_confl)
    , learntsize_adjust_cnt(config.opt_learntsize_adjust_start_confl)

    , preprocessCalls(0)
    , inprocessCalls(0)

    // Resource constraints:
    //
    , conflict_budget(-1)
    , propagation_budget(-1)
    , asynch_interrupt(false)

    // IPASIR
    , terminationCallbackState(0)
    , terminationCallbackMethod(0)
    , learnCallbackState(0)
    , learnCallbackLimit(0)
    , learnCallback(0)
    , learnCallbackBuffer(0)

    // Online proof checking class
    , onlineDratChecker(config.opt_checkProofOnline != 0 ? new OnlineProofChecker(dratProof) : 0)

    // UIP hack
    , l1conflicts(0)
    , multiLearnt(0)
    , learntUnit(0)

    // restart interval hack
    , conflictsSinceLastRestart(0)
    , currentRestartIntervalBound(config.opt_rMax)
    , intervalRestart(0)

    // LA hack
    , laAssignments(0)
    , tabus(0)
    , las(0)
    , failedLAs(0)
    , maxBound(0)
    , laTime(0)
    , maxLaNumber(config.opt_laBound)
    , topLevelsSinceLastLa(0)
    , laEEvars(0)
    , laEElits(0)
    , untilLa(config.opt_laEvery)
    , laBound(config.opt_laEvery)
    , laStart(false)

    , startedSolving(false)

    , useVSIDS(config.opt_vsids_start)

    , simplifyIterations(0)
    , learnedDecisionClauses(0)
    , doAddVariablesViaER(config.opt_restrictedExtendedResolution)

    , totalLearnedClauses(0)
    , sumLearnedClauseSize(0)
    , sumLearnedClauseLBD(0)
    , maxLearnedClauseSize(0)

    , rerExtractedGates(0)
    , rerITEtries(0)
    , rerITEsuccesses(0)
    , rerITErejectS(0)
    , rerITErejectT(0)
    , rerITErejectF(0)
    , maxResDepth(0)

    // extended resolution rewrite
    , erRewriteRemovedLits(0)
    , erRewriteClauses(0)

    // restricted extended resolution
    , rerCommonLitsSum(0)
    , rerLearnedClause(0)
    , rerLearnedSizeCandidates(0)
    , rerSizeReject(0)
    , rerPatternReject(0)
    , rerPatternBloomReject(0)
    , maxRERclause(0)
    , rerOverheadTrailLits(0)
    , totalRERlits(0)

    // interleaved clause strengthening
    , lastICSconflicts(-1)
    , icsCalls(0)
    , icsCandidates(0)
    , icsDroppedCandidates(0)
    , icsShrinks(0)
    , icsShrinkedLits(0)
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

    // bi-asserting learned clauses
    , isBiAsserting(false)
    , allowBiAsserting(false)
    , lastBiAsserting(0)
    , biAssertingPostCount(0)
    , biAssertingPreCount(0)

    // UHLE for learnt clauses
    , searchUHLEs(0)
    , searchUHLElits(0)

    , pq_order(config.opt_pq_order)   // Contrasat

    , impl_cl_heap()

    // MiPiSAT
    //
    , probing_step(0)
    , probing_step_width(config.opt_probing_step_width)
    , probing_limit(config.opt_probing_limit)

    // cir minisat
    //
    , cir_bump_ratio(config.opt_cir_bump)
    , cir_count(0)

    // 999 MS hack
    , activityBasedRemoval(config.opt_act_based)
    , lbd_core_threshold(config.opt_lbd_core_thresh)
    , learnts_reduce_fraction(config.opt_l_red_frac)

    // preprocessor
    , coprocessor(nullptr)
    , useCoprocessorPP(config.opt_usePPpp)
    , useCoprocessorIP(config.opt_usePPip)

    // communication to other solvers that might be run in parallel
    , sharingTimePoint(config.sharingType)
    , communication(0)

    , useNaiveBacktracking(config.opt_dpll)
    , enumerationClient(this)
{
    // EMA for dynamic restart schedules
    slow_interpretationSizes.reinit(config.opt_restart_ema_trailslow);              // collect all conflict levels
    slow_LBDs.reinit(config.opt_restart_ema_lbdslow);   // collect all clause LBDs
    recent_LBD.reinit(config.opt_restart_ema_lbdfast);

    restartSwitchSchedule.initialize(config.opt_rswitch_isize);

    // Parameters (user settable):
    //
    searchconfiguration.K = config.opt_K;
    searchconfiguration.R = config.opt_R;
    searchconfiguration.sizeLBDQueue = config.opt_size_lbd_queue;
    searchconfiguration.sizeTrailQueue = config.opt_size_trail_queue;

    searchconfiguration.firstReduceDB = config.opt_first_reduce_db;
    searchconfiguration.incReduceDB = config.opt_inc_reduce_db;
    searchconfiguration.specialIncReduceDB = config.opt_spec_inc_reduce_db;
    searchconfiguration.lbLBDFrozenClause = config.opt_lb_lbd_frozen_clause;

    searchconfiguration.lbSizeMinimizingClause = config.opt_lb_size_minimzing_clause;
    searchconfiguration.lbLBDMinimizingClause = config.opt_lb_lbd_minimzing_clause;
    searchconfiguration.uhle_minimizing_size = config.uhle_minimizing_size;
    searchconfiguration.uhle_minimizing_lbd  = config.uhle_minimizing_lbd;
    searchconfiguration.use_reverse_minimization = config.opt_use_reverse_minimization; // has to be set in the reverseminimization object as well!
    searchconfiguration.lbSizeReverseClause =      config.reverse_minimizing_size;
    searchconfiguration.lbLBDReverseClause =       config.lbLBDreverseClause;

    searchconfiguration.var_decay = config.opt_var_decay_start;
    searchconfiguration.var_decay_start = config.opt_var_decay_start;
    searchconfiguration.var_decay_end = config.opt_var_decay_stop;
    searchconfiguration.var_decay_inc = config.opt_var_decay_inc;
    searchconfiguration.var_decay_distance = config.opt_var_decay_dist;
    searchconfiguration.clause_decay = config.opt_clause_decay;

    searchconfiguration.ccmin_mode = config.opt_ccmin_mode;

    searchconfiguration.phase_saving = config.opt_phase_saving;
    searchconfiguration.restarts_type = config.opt_restarts_type;

    // communication
    communicationClient.receiveEE        = config.opt_receiveEquivalences;
    communicationClient.refineReceived   = config.opt_refineReceivedClauses;
    communicationClient.resendRefined    = config.opt_resendRefinedClauses;
    communicationClient.doReceive        = config.opt_receiveData;
    communicationClient.sendAll          = config.opt_sendAll;
    communicationClient.useDynamicLimits = config.opt_dynLimit;
    communicationClient.keepLonger       = config.opt_keepLonger;
    communicationClient.lbdFactor        = config.opt_recLBDfactor;

    MYFLAG = 0;
    hstry[0] = lit_Undef; hstry[1] = lit_Undef; hstry[2] = lit_Undef; hstry[3] = lit_Undef; hstry[4] = lit_Undef; hstry[5] = lit_Undef;

    if (onlineDratChecker != 0) { onlineDratChecker->setVerbosity(config.opt_checkProofOnline); }

//     cerr << "c sizes: solver: " << sizeof(Solver) << " comm: " << sizeof(Communicator) << endl;

    if ((const char*)config.search_schedule != 0) {
        configScheduler.initConfigs(searchconfiguration, string(config.search_schedule), config.sscheduleGrowFactor, config.scheduleDefaultConflicts, config.scheduleConflicts);    // setup configuration
    }
}



Solver::~Solver()
{
    if (big != 0)         { big->BIG::~BIG(); delete big; big = 0; }   // clean up!
    if (coprocessor != 0) { delete coprocessor; coprocessor = 0; }
    if (deleteConfig) { delete privateConfig; privateConfig = 0; }
    if (learnCallbackBuffer != 0) { delete [] learnCallbackBuffer; learnCallbackBuffer = 0; }
}



// Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(bool sign, bool dvar, char type)
{
    int v = nVars();
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true));

    varFlags. push(VarFlags(sign));

//     assigns  .push(l_Undef);
    vardata  .push(mkVarData(CRef_Undef, -1));
    //activity .push(0);
    activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
//     seen     .push(0);
    lbd_marker  .resize(2 * v + 2); // add space for the next variable

    trail    .capacity(v + 1);
    setDecisionVar(v, dvar);

    if (config.opt_rer_rewriteNew || config.opt_rer_extractGates) {
        erRewriteInfo. push(LitPair()); erRewriteInfo. push(LitPair());       // for the two new literals, add empty infos
    }

    // get space for reverse data structures watches
    if (reverseMinimization.enabled) {
        reverseMinimization.assigns.push(l_Undef);
        reverseMinimization.trail.capacity(v + 1);
    }

    // space for replacement info
    assert(v == eqInfo.replacedBy.size() && "new variables have to match the size");
    eqInfo.replacedBy.push(mkLit(v, false));

    Var compressionVar = compression.newVar();
    assert((compressionVar == 0 || compressionVar == v) && "variable that has been added to compression should match the currently added variable");

    if (config.opt_litPairDecisions > 0) {
        decisionLiteralPairs.push(LitPairPair());    // add another pair per literal
        decisionLiteralPairs.push(LitPairPair());    // add another pair per literal
    }

    return v;
}

void Solver::reserveVars(Var v)
{
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true));
//    watchesBin  .init(mkLit(v, false));
//    watchesBin  .init(mkLit(v, true ));

//     assigns  .capacity(v+1);
    vardata  .capacity(v + 1);
    //activity .push(0);
    activity .capacity(v + 1);
//     seen     .capacity(v+1);
    lbd_marker  .capacity(2 * v + 2);
    varFlags. capacity(v + 1);
    trail    .capacity(v + 1);

    if (config.opt_rer_rewriteNew || config.opt_rer_extractGates) {
        erRewriteInfo. growTo(2 * v);     // for the two new literals, add empty infos
    }

    // get space for reverse data structures watches
    if (reverseMinimization.enabled) {
        reverseMinimization.assigns.capacity(v + 1);
        reverseMinimization.trail.capacity(v + 1);
    }

    eqInfo.replacedBy.capacity(v + 1);

    if (config.opt_litPairDecisions > 0) { decisionLiteralPairs.capacity(2 * v); }  // create sufficient capacity

}



bool Solver::addClause_(vec< Lit >& ps, bool noRedundancyCheck)
{
    assert(decisionLevel() == 0);
    if (!ok) { return false; }

    // Check if clause is satisfied and remove false/duplicate literals:
    if (! noRedundancyCheck) { sort(ps); }  // sort only, if necessary

    // analyze for DRUP - add if necessary!
    Lit p; int i, j, flag = 0;
    if (outputsProof()) {
        oc.clear();
        for (i = j = 0, p = lit_Undef; i < ps.size(); i++) {
            oc.push(ps[i]);
            if (value(ps[i]) == l_True || ps[i] == ~p || value(ps[i]) == l_False) {
                flag = 1;
            }
        }
    }

    bool somePositive = false;
    bool someNegative = false;
    if (!config.opt_hpushUnit) {   // do not analyzes clauses for being satisfied or simplified
        for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
            if (value(ps[i]) == l_True || ps[i] == ~p) { // noRedundancyCheck breaks the second property, which is ok, as it also not fails
                return true;
            } else if (value(ps[i]) != l_False && ps[i] != p) {
                ps[j++] = p = ps[i]; // assigning p is not relevant for noRedundancyCheck
                somePositive = somePositive || !sign(p);
                someNegative = someNegative || sign(p);
            }
        ps.shrink_(i - j);
    } else { // delay units, but still remove tautologies!
        for (i = j = 0, p = lit_Undef; i < ps.size(); i++) {
            if (ps[i] == ~p) { // noRedundancyCheck breaks the second property, which is ok, as it also not fails
                return true;
            } else if (ps[i] != p) {
                ps[j++] = p = ps[i]; // assigning p is not relevant for noRedundancyCheck
                somePositive = somePositive || !sign(p);
                someNegative = someNegative || sign(p);
            }
        }
        ps.shrink_(i - j);
    }

    // add to Proof that the clause has been changed
    if (flag &&  outputsProof()) {
        addCommentToProof("reduce due to assigned literals, or duplicates");
        addToProof(ps);
        addToProof(oc, true);
    } else if (outputsProof() && config.opt_verboseProof == 2) {
        cerr << "c added usual clause " << ps << " to solver" << endl;
    }

    if (ps.size() == 0) {
        return ok = false;
    } else if (ps.size() == 1) {
        if (config.opt_hpushUnit) {
            if (value(ps[0]) == l_False) { return ok = false; }
            if (value(ps[0]) == l_True) { return true; }
        }
        uncheckedEnqueue(ps[0]);
        if (!config.opt_hpushUnit) { return ok = (propagate(true) == CRef_Undef); }
        else { return ok; }
    } else {
        CRef cr = ca.alloc(ps, false);
        clauses.push(cr);
        attachClause(cr);
        assert(ps.size() > 1 && "this should not be a unit clause");
        // to feed polarity heuristic
        posInAllClauses = posInAllClauses && somePositive; // memorize for the whole formula
        negInAllClauses = negInAllClauses && someNegative;

        if (config.opt_litPairDecisions > 0 && ps.size() > 2) {  // collect literals only for larger clauses (not binary!)
            for (int i = 0 ; i < ps.size(); ++i) {
                LitPairPair& lp = decisionLiteralPairs[ toInt(ps [i]) ];
                if (lp.p.replaceWith != lit_Undef && lp.q.replaceWith != lit_Undef) { continue; }  // this literal already has enough literals stored
                if (lp.p.replaceWith == lit_Undef) {
                    lp.p.replaceWith = ps [(i + 1) % ps.size() ]; // store next two literals
                    lp.p.otherMatch  = ps [(i + 2) % ps.size() ]; // store next two literals
                } else {
                    assert(lp.q.replaceWith == lit_Undef && "this case is left over");
                    lp.q.replaceWith = ps [(i + 1) % ps.size() ]; // store next two literals
                    lp.q.otherMatch  = ps [(i + 2) % ps.size() ]; // store next two literals
                }
            }
        }
    }

    return true;
}

bool Solver::addClause(const Clause& ps)
{
    if (ps.size() == 0) {
        return ok = false;
    } else if (ps.size() == 1) {
        if (config.opt_hpushUnit) {
            if (value(ps[0]) == l_False) { return ok = false; }
            if (value(ps[0]) == l_True) { return true; }
        }
        uncheckedEnqueue(ps[0]);
        if (!config.opt_hpushUnit) { return ok = (propagate() == CRef_Undef); }
        else { return ok; }
    } else {
        CRef cr = ca.alloc(ps, ps.learnt());
        if (!ps.learnt()) { clauses.push(cr); }
        else { learnts.push(cr); }
        attachClause(cr);
    }

    return true;
}

lbool Solver::integrateNewClause(vec<Lit>& clause)
{
    DOUT(if (config.opt_dbg) cerr << "c [local] add clause " << clause << " at level " << decisionLevel() << endl;);
    DOUT(if (config.opt_dbg) cerr << "c [local] ok: " << ok << " trail: " << trail << endl;);

    if (clause.size() == 0) {
        ok = false;     // solver state is false
        return l_False; // adding the empty clause results in an unsatisfiable formula
    }

    if (decisionLevel() == 0) {  // perform propagation if we are on level 0
        return addClause_(clause) ? l_True : l_False;
    }

    // analyze the current clause
    int satLits = 0, unsatLits = 0, undefLits = 0;

    // make sure we have enough variables
    Lit maxLit = clause[0];
    for (int i = 0 ; i < clause.size(); ++ i) {
        // make sure we have enough space
        maxLit = clause[i] < maxLit ? maxLit : clause[i];
        if (nVars() < var(maxLit)) {
            const Var nv = newVar();
        }
        // examine the clause
        lbool truthvalue = value(clause[i]);
        if (truthvalue != l_False) {
            Lit tmp = clause[undefLits + satLits]; clause[undefLits + satLits] = clause[i]; clause[i] = tmp;
            undefLits = (truthvalue == l_Undef) ? undefLits + 1 : undefLits;
            satLits = (truthvalue == l_True) ? satLits + 1 : satLits;
        } else {
            unsatLits ++;
        }
    }

    DOUT(if (config.opt_dbg) cerr << "c [local] sat: " << satLits << " unsat: " << unsatLits << " undefLits: " << undefLits << endl;);

    // clause is not satisfied, and not "free" enough
    int backtrack_level = decisionLevel();
    int highestLevelVars = 0;
    if (undefLits + satLits < 2) {  // we need to do something to watch the clause safely
        if (undefLits + satLits < 1) { // apply backtracking

            DOUT(if (config.opt_dbg) {
            cerr << "c [local] detailed2: ";
            for (int i = 0 ; i < clause.size(); ++ i) {
                    cerr << " " << clause[i] << "@" << level(var(clause[i])) << "t" << (value(clause[i]) == l_True) << "f" << (value(clause[i]) == l_False);
                }
                cerr << " " << endl;
            });
            int higehest_level = 0;
            backtrack_level = -1;
            for (int i = 0 ; i < clause.size(); ++ i) {
                assert(value(clause[i]) == l_False && "all literals in the clause have to be unsatisfiable");
                assert(higehest_level > backtrack_level && "some literals have to be undefined after backtracking");
                if (level(var(clause[i])) > higehest_level) {                     // found new highest level in the clause
                    DOUT(if (config.opt_dbg) cerr << "c " << clause[i] << " sets bt to " << backtrack_level << " and highest to " << level(var(clause[i])) << endl;);
                    backtrack_level = higehest_level;                               // the other level is the new backtrack level
                    higehest_level = level(var(clause[i]));                         // store new highest level
                    Lit tmp = clause[0]; clause[0] = clause[i]; clause[i] = tmp;    // move highest level variable to front!
                    highestLevelVars = 1;                                           // count variables for this level
                } else if (level(var(clause[i])) > backtrack_level) {
                    if (level(var(clause[i])) == higehest_level) {                 // found another variable of the highest level
                        DOUT(if (config.opt_dbg) cerr << "c move literal " << clause[i] << "@" << level(var(clause[i])) << " to position " << highestLevelVars << endl;);
                        Lit tmp = clause[highestLevelVars]; clause[highestLevelVars] = clause[i]; clause[i] = tmp;  // move highest level variable to front!
                        highestLevelVars ++;      // and count
                    } else {
                        DOUT(if (config.opt_dbg) cerr << "c " << clause[i] << " updates bt from " << backtrack_level << " to " << level(var(clause[i])) << " highest: " << higehest_level << endl;);
                        backtrack_level = level(var(clause[i]));
                    }
                }
            }
            for (int i = 1; i < clause.size(); ++ i) {                             // move literal of backtrack level forward
                if (level(var(clause[i])) == backtrack_level) {
                    const Lit tmp = clause[i]; clause[i] = clause[1]; clause[1] = tmp; // move the literal forward
                    break;                                                             // stop looking for more variables
                }
            }
        } else {
            assert(value(clause[0]) != l_False && "shuffling above moved only free literal to front");
            backtrack_level = 0;
            for (int i = 1; i < clause.size(); ++ i) {           // find actual backtrack level
                if (level(var(clause[i])) > backtrack_level) {
                    backtrack_level = level(var(clause[i])) ;             // store level
                    Lit tmp = clause[1]; clause[1] = clause[i]; clause[i] = tmp;  // move literal to front
                }
                assert(level(var(clause[1])) >= level(var(clause[i])) && "highest level on second position in clause");
            }
        }
    }

    // jump back (if necessary)
    if (backtrack_level == -1) {  // cannot backtrack beyond level 0 -> clause cannot be added to the formula
        assert(value(clause[0]) == l_False && "clause to be integrated is falsified");
        ok = false;
        return l_False;
    }

    DOUT(if (config.opt_dbg) cerr << "c [local] decisionLevel: " << decisionLevel() << " backtracklevel: " << backtrack_level << endl;);

    cancelUntil(backtrack_level); // backtrack

    DOUT(if (config.opt_dbg) {
    cerr << "c [local] detailed2: ";
    for (int i = 0 ; i < clause.size(); ++ i) {
            cerr << " " << clause[i] << "@" << level(var(clause[i])) << "t" << (value(clause[i]) == l_True) << "f" << (value(clause[i]) == l_False);
        }
        cerr << " " << endl;
    });

    assert(value(clause[0]) != l_False && "first literal has to be free now");

    // add the clause to the local data structures
    CRef cr = CRef_Undef;                // for unit clauses
    if (clause.size() > 1) {             // if clause is larger, add nicely to two-watched-literal structures
        cr = ca.alloc(clause, false);
        clauses.push(cr);
        attachClause(cr);
        DOUT(if (config.opt_dbg) cerr << "c new reason clause[ " << cr << " ]: " << ca[cr] << endl
             << "c  1st lit: " << ca[cr][0] << " value: " << value(ca[cr][0]) << " level: " << level(var(ca[cr][0])) << endl
             << "c  2nd lit: " << ca[cr][1] << " value: " << value(ca[cr][1]) << " level: " << level(var(ca[cr][1])) << endl;);
    } else {
        assert(decisionLevel() == 0 && "unit can only be added at decision level 0");
    }

    if ((undefLits == 1 && satLits == 0)                            // clause was unit before backjumping already
            || (undefLits == 0 && satLits == 0 && highestLevelVars == 1)  // or clause became unit after backjumping
       ) {
        DOUT(if (config.opt_dbg) cerr << "c [local] enqueue unit " << clause[0] << endl;);
        assert((clause.size() == 1 || cr != CRef_Undef) && "always enqueue with a reason");
        uncheckedEnqueue(clause[0], cr);                              // then continue with unit propagation
        assert((clause.size() <= 1 || level(var(clause[0])) == level(var(clause[1]))) && "always watch two literals of the same level (conflicting)");
    } else {
        DOUT(if (config.opt_dbg && clause.size() == 1) cerr << "c did not enqueue unit clause " << clause << " at level " << decisionLevel() << " satisfied: " << (value(clause[0]) == l_True) <<  " falsified: " << (value(clause[0]) == l_False) << endl;);
    }

    DOUT(if (config.opt_dbg) cerr << "c [local] succeeded at level " << decisionLevel() << endl;);
    DOUT(if (config.opt_dbg) cerr << "c [local] trail " << trail << endl;);
    DOUT(if (config.opt_dbg) cerr << "c [local] trail " << trail.size() << " prop_head: " << qhead << endl;);

    return l_True;
}

int Solver::integrateAssumptions(vec<Lit>& nextAssumptions)
{
    // current level is 0, or there have not been assumptions in the last call to search
    if (decisionLevel() == 0) { return 0; }  // value below would always be l_Undef

    DOUT(if (config.opt_dbg) {
    cerr << "c integrate assumptions: " << nextAssumptions << std::endl
         << "c trail: " << trail << std::endl
         << "c current assumptions: " << assumptions << std::endl;
});

    int keep = 0;
    while (keep < nextAssumptions.size() && keep < assumptions.size()) {
        // std::cerr << "c integrate [" << keep << "] " << nextAssumptions[keep] << " vs " << assumptions[keep] << " with value " << value( assumptions[keep] ) << std::endl;
        const Lit& assumeLit = nextAssumptions[keep];
        // check that assumptions match, the assumption is satisfied, and the trail matches as well (would have been set as decision on the given level)
        if (assumeLit == assumptions[keep] && value(assumeLit) == l_True && level(var(assumeLit)) <= keep) { ++ keep; }
        else { break; }
    }

    DOUT(if (config.opt_dbg) std::cerr << "integrate new assumptions " << nextAssumptions << " with old assumptions " << assumptions << " and trail " << trail << " on level " << decisionLevel() << ", jump back to " << keep << std::endl;);
    cancelUntil(keep);
    return keep;
}

void Solver::attachClause(CRef cr)
{
    const Clause& c = ca[cr];
    assert(c.size() > 1 && "cannot watch unit clauses!");
    assert(c.mark() == 0 && "satisfied clauses should not be attached!");

    if (c.size() == 2) {
        watches[~c[0]].push(Watcher(cr, c[1], 0)); // add watch element for binary clause
        watches[~c[1]].push(Watcher(cr, c[0], 0)); // add watch element for binary clause
    } else {
        watches[~c[0]].push(Watcher(cr, c[1], 1));
        watches[~c[1]].push(Watcher(cr, c[0], 1));
    }

    if (c.learnt()) { learnts_literals += c.size(); }
    else            { clauses_literals += c.size(); }
}




void Solver::detachClause(CRef cr, bool strict)
{
    const Clause& c = ca[cr];

//     cerr << "c detach clause " << cr << " which is " << ca[cr] << endl;

    // assert(c.size() > 1 && "there should not be unit clauses - on the other hand, LHBR and OTFSS could create unit clauses");
//     if( c.size() == 1 ) {
//       cerr << "c extra bug - unit clause is removed" << endl;
//       exit( 36 );
//     }

    const int watchType = c.size() == 2 ? 0 : 1; // have the same code only for different watch types!
    if (strict) {
        if (config.opt_fast_rem) {
            removeUnSort(watches[~c[0]], Watcher(cr, c[1], watchType));
            removeUnSort(watches[~c[1]], Watcher(cr, c[0], watchType));
        } else {
            remove(watches[~c[0]], Watcher(cr, c[1], watchType)); // linear (touchs all elements)!
            remove(watches[~c[1]], Watcher(cr, c[0], watchType)); // linear (touchs all elements)!
        }
    } else {
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watches.smudge(~c[0]);
        watches.smudge(~c[1]);
    }

    if (c.learnt()) { learnts_literals -= c.size(); }
    else            { clauses_literals -= c.size(); }
}


void Solver::removeClause(Riss::CRef cr, bool strict)
{

    Clause& c = ca[cr];

    // tell DRUP that clause has been deleted, if this was not done before already!
    if (c.mark() == 0) {
        addCommentToProof("delete via clause removal", true);
        // assert(false && "stop here for debugging");
        addToProof(c, true); // clause has not been removed yet
    }
    DOUT(if (config.opt_learn_debug) cerr << "c remove clause [" << cr << "]: " << c << endl;);

    detachClause(cr, strict);
    // Don't leave pointers to free'd memory!
    if (locked(c)) {
        vardata[var(c[0])].reason = CRef_Undef;
    }
    c.mark(1);
    ca.free(cr);
}


bool Solver::satisfied(const Clause& c) const
{

    // quick-reduce option
    if (config.opt_quick_reduce) { // Check clauses with many literals is time consuming
        return (value(c[0]) == l_True) || (value(c[1]) == l_True);
    }

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True) {
            return true;
        }
    return false;
}



/******************************************************************
 * Minimisation with binary reolution
 ******************************************************************/
bool Solver::minimisationWithBinaryResolution(vec< Lit >& out_learnt, unsigned int& lbd, unsigned& dependencyLevel)
{

    // Find the LBD measure
    // const unsigned int lbd = computeLBD(out_learnt,out_learnt.size());
    const Lit p = ~out_learnt[0];

    if (lbd <= searchconfiguration.lbLBDMinimizingClause) {
        lbd_marker.nextStep();
        for (int i = 1; i < out_learnt.size(); i++) { lbd_marker.setCurrentStep(var(out_learnt[i])); }
        const vec<Watcher>&  wbin  = watches[p]; // const!
        int nb = 0;
        for (int k = 0; k < wbin.size(); k++) {
            if (!wbin[k].isBinary()) { continue; }   // has been looping on binary clauses only before!
            const Lit imp = wbin[k].blocker();
            if (lbd_marker.isCurrentStep(var(imp)) && value(imp) == l_True) {
                nb++;
                lbd_marker.reset(var(imp));
                #ifdef PCASSO
                dependencyLevel = dependencyLevel >= ca[wbin[k].cref()].getPTLevel() ? dependencyLevel : ca[wbin[k].cref()].getPTLevel();
                #endif
            }
        }
        int l = out_learnt.size() - 1;
        if (nb > 0) {
            nbReducedClauses++;
            for (int i = 1; i < out_learnt.size() - nb; i++) {
                if (! lbd_marker.isCurrentStep(var(out_learnt[i]))) {
                    const Lit p = out_learnt[l];
                    out_learnt[l] = out_learnt[i];
                    out_learnt[i] = p;
                    l--; i--;
                }
            }
            out_learnt.shrink_(nb);
            return true; // literals have been removed from the clause
        } else { return false; } // no literals have been removed
    } else { return false; } // no literals have been removed
}


/******************************************************************
 * Minimisation with binary implication graph
 ******************************************************************/
bool Solver::searchUHLE(vec<Lit>& learned_clause, unsigned int& lbd, unsigned& dependencyLevel)
{
    #ifdef PCASSO
    // TODO implement dependencyLevel correctly!
    #endif
    if (lbd <= searchconfiguration.uhle_minimizing_lbd) { // should not touch the very first literal!
        const Lit p = learned_clause[0]; // this literal cannot be removed!
        const uint32_t cs = learned_clause.size(); // store the size of the initial clause
        Lit Splus  [cs];      // store sorted literals of the clause
        DOUT(if (config.opt_learn_debug) cerr << "c minimize with UHLE: " << learned_clause << endl;);
        for (uint32_t ci = 0 ; ci  < cs; ++ ci) { Splus [ci] = learned_clause[ci]; }

        {
            // sort the literals according to the time stamp they have been found
            const uint32_t s = cs;
            for (uint32_t j = 1; j < s; ++j) {
                const Lit key = Splus[j];
                const uint32_t keyDsc = big->getStart(key);
                int32_t i = j - 1;
                while (i >= 0 && big->getStart(Splus[i]) > keyDsc) {
                    Splus[i + 1] = Splus[i]; i--;
                }
                Splus[i + 1] = key;
            }
        }

        // apply UHLE for the literals of the clause
        uint32_t pp = cs;
        uint32_t finished = big->getStop(Splus[cs - 1]);
        Lit finLit = Splus[cs - 1];
        for (pp = cs - 1 ; pp > 0; -- pp) {
            const Lit l = Splus[ pp - 1];
            const uint32_t fin = big->getStop(l);
            if (fin > finished) {
                for (int i = 1 ; i < learned_clause.size(); ++ i) {   // remove the literal l from the current learned claus, do not remove the asserting literal!
                    if (learned_clause[i] == l) {
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
        Lit Sminus [csn];   // store the complementary literals sorted to their discovery in the BIG
        for (uint32_t ci = 0 ; ci < csn; ++ ci) {
            Sminus[ci] = ~learned_clause[ci];
        }
        {
            // insertion sort for discovery of complementary literals
            const uint32_t s = csn;
            for (uint32_t j = 1; j < s; ++j) {
                const Lit key = Sminus[j];
                const uint32_t keyDsc = big->getStart(key);
                int32_t i = j - 1;
                while (i >= 0 && big->getStart(Sminus[i]) > keyDsc) {
                    Sminus[i + 1] = Sminus[i]; i--;
                }
                Sminus[i + 1] = key;
            }
        }

        // run UHLE for the complementary literals
        finished = big->getStop(Sminus[0]);
        finLit = Sminus[ 0 ];
        for (uint32_t pn = 1; pn < csn; ++ pn) {
            const Lit l = Sminus[ pn ];
            const uint32_t fin = big->getStop(l);
            if (fin < finished) {   // remove the complementary literal from the clause!
                for (int i = 1 ; i < learned_clause.size(); ++ i) {   // remove the literal l from the current learned clause, do not remove the asserting literal
                    if (learned_clause[i] == ~l) {
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
        searchUHLElits += (cs - learned_clause.size());
        if (cs != learned_clause.size()) {
            DOUT(if (config.opt_learn_debug) cerr << "c UHLE result: " << learned_clause << endl;)
                // some literals have been removed
            {
                return true;
            }
        } else { return false; } // no literals have been removed
    } else { return false; }// no literals have been removed
}

/** check whether there is an AND-gate that can be used to simplify the given clause
 */
bool Solver::erRewrite(vec<Lit>& learned_clause, unsigned int& lbd, unsigned& dependencyLevel)
{
    #ifdef PCASSO
    // TODO implement dependencyLevel correctly!
    #endif
    if (lbd <= config.erRewrite_lbd) {
        if (config.opt_rer_extractGates || (config.opt_rer_rewriteNew && config.opt_rer_windowSize == 2)) {
            if ((config.opt_rer_rewriteNew && !config.opt_rer_as_learned)
                    || config.opt_rer_extractGates
               ) {
                rerRewriteArray.resize(nVars() * 2);   // get enough space
                rerRewriteArray.nextStep();
                // get all literals marked
                for (int i = 0; i < learned_clause.size(); ++ i) { rerRewriteArray.setCurrentStep(toInt(learned_clause[i])); }

                // rewrite the learned clause by replacing a disjunction of two literals with the
                // corresponding ER-literal (has to be falsified as well)
                const int cs = learned_clause.size();
                DOUT(if (config.opt_rer_debug) cerr << "c check erRewrite for clause " << learned_clause << endl;);
                // seen vector is still valid
                for (int i = 1; i < learned_clause.size(); ++ i) {
                    const Lit& l1 = learned_clause[i];
                    const Lit& otherLit = erRewriteInfo[ toInt(l1) ].otherMatch;
                    if (otherLit == lit_Undef) { continue; }  // there has been no rewriting with this literal yet
                    if (! varFlags[ var(otherLit) ].seen) { continue; }    // the literal for rewriting is not present in the clause, because the variable is not present
                    if (rerRewriteArray.isCurrentStep(toInt(otherLit))) { continue; }       // check whether the other literal is present
                    if (rerRewriteArray.isCurrentStep(toInt(~erRewriteInfo[ toInt(l1) ].replaceWith))) { continue; }       // do not add complementary literals

                    // check whether the other literal is present
                    for (int j = 1; j < learned_clause.size(); ++ j) {
                        if (i == j) { continue; }  // do not check the literal with itself
                        if (learned_clause[j] == otherLit) {  // found the other match
                            DOUT(if (config.opt_rer_debug) cerr << "c rewrite " << learned_clause << " pos " << i << " and " << j << " lits: " << learned_clause[i] << " and " << learned_clause[j] << " ADDING LITERAL: " << erRewriteInfo[ toInt(l1) ].replaceWith << endl;);
                            learned_clause[i] = erRewriteInfo[ toInt(l1) ].replaceWith; // replace
                            rerRewriteArray.setCurrentStep(toInt(erRewriteInfo[ toInt(l1) ].replaceWith));     // remebmer that we also added this literal to the clause
                            learned_clause[j] = learned_clause[ learned_clause.size() - 1 ]; // delete the other literal
                            learned_clause.pop(); // by pushing forward, and pop_back
                            // assert( !hasComplementary(learned_clause) && !hasDuplicates(learned_clause) && "there should not be duplicate literals in the learned clause"  );
                            erRewriteRemovedLits ++;
                            --i; // repeat current literal, moved in literal might be a replacable literal as well!
                            break; // done with the current literal!
                        }
                    }
                }
                if (cs > learned_clause.size()) {

                    rerRewriteArray.resize(2 * nVars());
                    rerRewriteArray.nextStep();

                    int keptLits = 0;
                    for (int k = 0 ; k < learned_clause.size(); ++ k) {
                        if (!rerRewriteArray.isCurrentStep(toInt(learned_clause[k]))) {      // keep each literal once
                            rerRewriteArray.setCurrentStep(toInt(learned_clause[k]));     // memorize this literal
                            learned_clause[keptLits++] = learned_clause[k];               // keep literals
                        } // else drop literal automatically
                    }
                    learned_clause.shrink_(learned_clause.size() - keptLits);   // remove duplicate literals

                    // assert( !hasComplementary(learned_clause) && !hasDuplicates(learned_clause) && "there should not be duplicate literals in the learned clause"  );

                    erRewriteClauses ++;
                    return true;
                } else { return false; }
            }
        }
    }
    return false;
}

// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level)
{
    if (decisionLevel() > level) {
        DOUT(if (config.opt_learn_debug) cerr << "c call cancel until " << level << " move propagation head from " << qhead << " to " << trail_lim[level] << endl;);
        for (int c = trail.size() - 1; c >= trail_lim[level]; c--) {
            Var      x  = var(trail[c]);
            varFlags [x].assigns = l_Undef;
            vardata [x].dom = lit_Undef; // reset dominator
            vardata [x].reason.setReason(CRef_Undef);   // TODO for performance this is not necessary, but for assertions and all that!
            if (searchconfiguration.phase_saving > 1  || ((searchconfiguration.phase_saving == 1) && c > trail_lim.last())) {   // TODO: check whether publication said above or below: workaround: have another parameter value for the other case!
                varFlags[x].polarity = sign(trail[c]);
            }
            insertVarOrder(x);
        }
        qhead = trail_lim[level];
        realHead = trail_lim[level];
        trail.shrink_(trail.size() - trail_lim[level]);
        trail_lim.shrink_(trail_lim.size() - level);
    }
}


//=================================================================================================
// Major methods:


Lit Solver::pickBranchLit()
{
    Var next = var_Undef;

    DOUT(
    if (config.opt_ordered_branch) {
    Var v = 0;
    for (; v < nVars(); ++v) {
            if (value(v) == l_Undef) { return mkLit(v); }
        }
    }
    )

    // NuSMV: PREF MOD
    // Selection from preferred list
    for (int i = 0; i < preferredDecisionVariables.size(); i++) {
        if (value(preferredDecisionVariables[i]) == l_Undef) {
            next = preferredDecisionVariables[i];
        }
    }
    // NuSMV: PREF MOD END

    // test simple version of BCD based decision heuristic
    if (config.opt_litPairDecisions > 0 && decisionLevel() > 0) {
        assert(trail_lim.size() > 0 && "there has to be a decision already");
        int refDecisionLevel = 0;
        while (refDecisionLevel < decisionLevel() && trail_lim[refDecisionLevel] < trail.size()) {    // as long as there are other reference decisions, take care of assumptions and "empty assumption decision literals"
            assert(trail_lim[refDecisionLevel] < trail.size() && "decision literals have to be located in the trail");
            Lit decideDecision = trail [ trail_lim[refDecisionLevel] ] ;
            DOUT(if (config.opt_printDecisions > 1) cerr << endl << "c reference decision: " << decideDecision << " @ " << refDecisionLevel << endl ;);
            LitPairPair& lp = decisionLiteralPairs[ toInt(decideDecision) ];
            DOUT(if (config.opt_printDecisions > 1) { // print state of reference literal lists when picking the decision
            cerr << "c pairs/sat/undefs: " << lp.p.otherMatch << " " << (lp.p.otherMatch != lit_Undef ? value(lp.p.otherMatch) == l_True : false)           << " " << (lp.p.otherMatch != lit_Undef ? value(lp.p.otherMatch) == l_Undef : false)
                     << " " << lp.p.replaceWith << " " << (lp.p.replaceWith != lit_Undef ? value(lp.p.replaceWith) == l_True : false) << " " << (lp.p.replaceWith != lit_Undef ? value(lp.p.replaceWith) == l_Undef : false)
                     << " -- "
                     << lp.q.otherMatch  << " " << (lp.q.otherMatch != lit_Undef  ? value(lp.q.otherMatch)  == l_True : false)        << " " << (lp.q.otherMatch != lit_Undef  ? value(lp.q.otherMatch)  == l_Undef : false)
                     << " " << lp.q.replaceWith << " " << (lp.q.replaceWith != lit_Undef ? value(lp.q.replaceWith) == l_True : false) << " " << (lp.q.replaceWith != lit_Undef ? value(lp.q.replaceWith) == l_Undef : false)
                     << endl;
            }
                );
            refDecisionLevel ++;
            if (lp.p.otherMatch == lit_Undef || lp.q.otherMatch == lit_Undef) { continue; }  // there is no clause stored for the given literal
            if (lp.q.otherMatch != lit_Undef && value(lp.q.otherMatch)  != l_True && value(lp.q.replaceWith) == l_Undef) { assert(lp.p.replaceWith != lit_Undef); return  lp.q.replaceWith; }        // return a free literal
            if (lp.q.replaceWith != lit_Undef &&  value(lp.q.replaceWith) != l_True && value(lp.q.otherMatch) == l_Undef) { assert(lp.p.otherMatch != lit_Undef); return lp.q.otherMatch; }
            assert(lp.p.otherMatch != lit_Undef && lp.p.replaceWith != lit_Undef && "case left, both lits have to be set");
            if (value(lp.p.otherMatch)  != l_True && value(lp.p.replaceWith) == l_Undef) { assert(lp.p.replaceWith != lit_Undef); return lp.p.replaceWith; }
            if (value(lp.p.replaceWith) != l_True && value(lp.p.otherMatch) == l_Undef) { assert(lp.p.otherMatch != lit_Undef); return lp.p.otherMatch; }
        }
    }

    // Random decision:
    if (
        // NuSMV: PREF MOD
        next == var_Undef &&
        // NuSMV: PREF MOD END
        drand(random_seed) < random_var_freq && !order_heap.empty()) {
        next = order_heap[irand(random_seed, order_heap.size())];
        if (value(next) == l_Undef && varFlags[next].decision) {
            rnd_decisions++;
        }
    }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || ! varFlags[next].decision)
        if (order_heap.empty()) {
            next = var_Undef;
            break;
        } else {
            next = order_heap.removeMin();
        }

    // first path is usually chosen, one if
    if (next != var_Undef && !posInAllClauses && !negInAllClauses) {
        bool assignFalse = varFlags[next].polarity;           // usual phase saving
        if (decisionLevel() < config.opt_phase_bit_level) {   // bit phase saving
            assignFalse = curr_restarts >> (decisionLevel() % config.opt_phase_bit_number) & 1;
            if (config.opt_phase_bit_invert) { assignFalse = !assignFalse; }
        }
        return mkLit(next, rnd_pol ? drand(random_seed) < random_var_freq : assignFalse);
    } else {
        return next == var_Undef ? lit_Undef :
               (posInAllClauses ? mkLit(next, false) : mkLit(next, true));
    }
}


void Solver::updateMetricsDuringAnalyze(const Lit p, const CRef cr, Clause& c, bool& foundFirstLearnedClause, unsigned& dependencyLevel)
{
    // Special case for binary clauses
    // The first one has to be SAT
    if (p != lit_Undef && c.size() == 2 && value(c[0]) == l_False) {
        assert(value(c[1]) == l_True);
        Lit tmp = c[0];
        c[0] =  c[1], c[1] = tmp;
    }
    if (sharingTimePoint == 2 && (c.learnt() || c.isCoreClause()) && !c.wasUsedInAnalyze()) {   // share clauses only, if they are used during resolutions in conflict analysis
        #ifdef PCASSO
        updateSleep(&c, c.size(), c.getPTLevel());  // share clause including level information
        #else
        updateSleep(&c, c.size());
        #endif
        c.setUsedInAnalyze();
    }
    if (!foundFirstLearnedClause) {  // dynamic adoption only until first learned clause!
        if (c.learnt()) {
            if (config.opt_cls_act_bump_mode == 0) { claBumpActivity(c); }
            else { clssToBump.push(cr); }
        }

        if (config.opt_update_lbd == 1) {    // update lbd during analysis, if allowed
            if (c.learnt()  && c.lbd() > 2) {
                unsigned int nblevels = computeLBD(c, c.size());
                if (nblevels + 1 < c.lbd() || config.opt_lbd_inc) {  // improve the LBD (either LBD decreased,or option is set)
                    if (c.lbd() <= searchconfiguration.lbLBDFrozenClause) {
                        c.setCanBeDel(false);
                    }
                    // seems to be interesting : keep it for the next round
                    c.setLBD(nblevels); // Update it
                    if (c.lbd() < lbd_core_threshold) { // turn learned clause into core clause and keep it for ever
                        // move clause from learnt to original, NOTE: clause is still in the learnt vector, has to be treated correctly during garbage collect/inprocessing
                        clauses.push(cr);
                        c.learnt(false);
                        // to prevent learnt clauses participated in revert conflict analyses
                        // from being dropped immediately (by fast-paced periodic clause database reduction)
                        // they are marked as protected
                        c.setCoreClause(true);
                    }
                } else if (config.opt_rem_inc_lbd && nblevels > c.lbd()) { c.setCanBeDel(true); }
            }
        }
    }
    #ifdef PCASSO // if resolution is done, then take also care of the participating clause!
    dependencyLevel = dependencyLevel >= c.getPTLevel() ? dependencyLevel : c.getPTLevel();
    #endif

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

int Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel, unsigned int& lbd,  unsigned& dependencyLevel)
{
    isBiAsserting = false; // yet, the currently learned clause is not bi-asserting
    int pathC = 0;
    Lit p     = lit_Undef;
    int pathLimit = 0; // for bi-asserting clauses

    bool foundFirstLearnedClause = false;
    int units = 0; // stats
    bool isOnlyUnit = true;
    lastDecisionLevel.clear();  // must clear before loop, because alluip can abort loop and would leave filled vector
    int currentSize = 0;        // count how many literals are inside the resolvent at the moment! (for otfss)

    varsToBump.clear(); clssToBump.clear(); // store info for bumping

    Solver::ReasonStruct currentReason(confl);

    DOUT(if (config.opt_learn_debug) cerr << endl << "c found conflict clause " << ca[confl] << endl;);

    // Generate conflict clause:
    //
    bool didNotResetBiasserting = true; // yet we did not reset learning a bi-asserting clause
    out_learnt.push();                  // (leave room for the asserting literal)
    int index   = trail.size() - 1;

    do {
        DOUT(if (config.opt_learn_debug) cerr << "c enter loop with lit " << p << endl;);

        int clauseSize = 2, clauseReductSize = 2;
        Clause* c = 0;
        if (! currentReason.isBinaryClause()) {
            c = & ca[currentReason.getReasonC()];
            clauseSize = c->size();
            clauseReductSize = c->size();
            DOUT(if (config.opt_rer_debug) cerr << "c resolve on " << p << "(" << index << "/" << trail.size() << ") with [" << currentReason.getReasonC() << "]" << *c << " -- calculated currentSize: " << currentSize << " pathLimit: " << pathLimit <<  endl;);
            updateMetricsDuringAnalyze(p, currentReason.getReasonC(), *c, foundFirstLearnedClause, dependencyLevel);   // process the learned clause
        } else {
            DOUT(if (config.opt_rer_debug) cerr << "c resolve on " << p << "(" << index << "/" << trail.size() << ") with lit " << currentReason.getReasonL() << "] pathLimit: " << pathLimit <<  endl;);
        }

        for (int j = (p == lit_Undef) ? 0 : 1; j < clauseSize; j++) {
            const Lit& q = currentReason.isBinaryClause() ? currentReason.getReasonL() : (*c)[j]; // get reason literal
            DOUT(if (config.opt_learn_debug) cerr << "c level for " << q << " is " << level(var(q)) << endl;);
            // TODO display reason in the line above!
            if (!varFlags[var(q)].seen && level(var(q)) > 0) { // variable is not in the clause, and not on top level
                currentSize ++;
                if (!foundFirstLearnedClause) { varsToBump.push(var(q)); }
                DOUT(if (config.opt_learn_debug) cerr << "c set seen for " << q << endl;);
                varFlags[var(q)].seen = 1;
                if (level(var(q)) >= decisionLevel()) {
                    pathC++;
                    #ifdef UPDATEVARACTIVITY
                    if (!foundFirstLearnedClause && config.opt_updateLearnAct) {  // should be set similar to no_LBD as its used this way in the hack solver
                        if (! reason(var(q)).isBinaryClause()) {
                            const CRef& r = reason(var(q)).getReasonC();
                            // UPDATEVARACTIVITY trick (see competition'09 companion paper)
                            // VSIDS scores of variables at the current decision level is aditionally
                            // bumped if they are propagated by core learnt clauses (similar to glucose)
                            if (r != CRef_Undef && (ca[r].learnt() || ca[r].isCoreClause())) { // either core clause, as some learnt clauses are moved ) {
                                DOUT(if (config.opt_learn_debug) cerr << "c add " << q << " to last decision level" << endl;);
                                lastDecisionLevel.push(q);
                            }
                        }
                    }
                    #endif

                } else {
                    out_learnt.push(q);
                    isOnlyUnit = false; // we found a literal from another level, thus the multi-unit clause cannot be learned
                }
            } else {
                if (level(var(q)) == 0) { clauseReductSize --; }  // this literal does not count into the size of the clause!
                if (units == 0 && varFlags[var(q)].seen && allowBiAsserting) {
                    if (pathLimit == 0 && didNotResetBiasserting) {
                        if (pathLimit == 0) { biAssertingPreCount ++; }    // count how often learning produced a bi-asserting clause
                        pathLimit = 1; // store that the current learnt clause is a biasserting clause!
                    } else {
                        didNotResetBiasserting = false; // we want to learn 1-empowering biasserting clauses, hence, we can tolerate only a single merge resolution step
                        pathLimit = 0;                 // reset path limit to go for usual 1-empowering clause
                    }
                }
            }
            if (currentReason.isBinaryClause()) { break; }  // only one iteration!
        }

        // OTFSS is possible here
        if (!foundFirstLearnedClause && currentSize + 1 == clauseReductSize) {  // OTFSS, but on the reduct!
            if (c != 0 && p != lit_Undef && config.opt_otfss && (!c->learnt()  // apply otfss only for clauses that are considered to be interesting, and not to the conflict itself! // TODO find another way to not apply to the conflict clause
                    || (config.opt_otfssL && c->learnt() && c->lbd() <= config.opt_otfssMaxLBD))) {
                DOUT(if (config.debug_otfss) cerr << "c OTFSS can remove literal " << p << " from " << c << endl;);
                #ifdef PCASSO
                // TODO add dependency to otfss info
                #else
                otfss.revealedClause ++;
                assert(p != lit_Undef && "cannot add OTFSS clause for the conflict!");
                otfss.info.push({ currentReason.getReasonC(), p });   // add pair to vector to be processed afterwards
                #endif
            }
        }
        if (!isOnlyUnit && units > 0) { break; }   // do not consider the next clause, because we cannot continue with units

        // Select next clause to look at:
        while (! varFlags[ var(trail[index--]) ].seen) {}    // cerr << "c check seen for literal " << (sign(trail[index]) ? "-" : " ") << var(trail[index]) + 1 << " at index " << index << " and level " << level( var( trail[index] ) )<< endl;
        p     = trail[index + 1];
        currentReason = reason(var(p));
        DOUT(if (config.opt_learn_debug) cerr << "c reset seen for " << p << endl;);
        varFlags[var(p)].seen = 0;
        pathC--;
        currentSize --;

        // do unit analysis only, if the clause did not become larger already!
        if (config.opt_allUipHack > 0  && pathC <= 0 && isOnlyUnit && out_learnt.size() == 1 + units) {
            learntUnit ++;
            units ++; // count current units
            out_learnt.push(~p);   // store unit
            DOUT(if (config.opt_learn_debug) cerr << "c learn unit clause " << ~p << " with pathLimit=" << pathLimit << endl;);
            if (config.opt_allUipHack == 1) { break; }   // for now, stop at the first unit! // TODO collect all units
            pathLimit = 0;    // do not use bi-asserting learning once we found one unit clause
            foundFirstLearnedClause = true; // we found a first clause, hence, stop all heuristical updates for the following steps
        }

        // do stop here
    } while (
        //if no unit clause is learned, and the first UIP is hit, or a bi-asserting clause is hit
        (units == 0 && pathC > pathLimit)
        // or 1stUIP is unit, but the current learned clause would be bigger, and there are still literals on the current level
        || (isOnlyUnit && units > 0 && index >= trail_lim[ decisionLevel() - 1])
    );
    // Note: biasserting clauses can also be unit clauses!
    assert(out_learnt.size() > 0 && "there has to be some learnt clause");

    // if we do not use units, add asserting literal to learned clause!
    if (units == 0) {
        out_learnt[0] = ~p; // add the last literal to the clause
        if (pathC > 0) {   // in case of bi-asserting clauses, the remaining literals have to be collected
            // look for second literal of this level
            while (! varFlags[var(trail[index--])].seen);
            p = trail[index + 1];
            out_learnt.push(~p);
        }
    } else {
        // process learnt units!
        // clear seen
        for (int i = units + 1; i < out_learnt.size() ; ++ i) { varFlags[ var(out_learnt[i]) ].seen = 0; }
        out_learnt.shrink_(out_learnt.size() - 1 - units);    // keep units+1 elements!

        assert(out_learnt.size() > 1 && "there should have been a unit");
        out_learnt[0] = out_learnt.last(); out_learnt.pop(); // close gap in vector
        // printf("c first unit is %d\n", var( out_learnt[0] ) + 1 );

        out_btlevel = 0; // jump back to level 0!

        // clean seen, if more literals have been added
        if (!isOnlyUnit) while (index >= trail_lim[ decisionLevel() - 1 ]) { varFlags[ var(trail[index--]) ].seen = 0; }

        lbd = 1; // for glucoses LBD score
        return units; // for unit clauses no minimization is necessary
    }

    currentSize ++; // the literal "~p" has been added additionally
    DOUT(if (currentSize != out_learnt.size()) { cerr << "c different sizes: clause=" << out_learnt.size() << ", counted=" << currentSize << " and collected vector: " << out_learnt << endl; });
    assert(currentSize == out_learnt.size() && "counted literals has to be equal to actual clause!");

    DOUT(if (config.opt_rer_debug) cerr << "c learned clause (before mini, biAsserting: " << (int)(pathLimit != 0) << "): " << out_learnt << endl;);

    bool doMinimizeClause = true; // created extra learnt clause? yes -> do not minimize
    lbd = computeLBD(out_learnt, out_learnt.size());
    bool recomputeLBD = false; // current lbd is valid
    if (decisionLevel() > 0 && out_learnt.size() > decisionLevel() && out_learnt.size() > config.opt_learnDecMinSize && config.opt_learnDecPrecent != -1) {  // is it worth to check for decisionClause?
        if (lbd > (config.opt_learnDecPrecent * decisionLevel() + 99) / 100) {
            // instead of learning a very long clause, which migh be deleted very soon (idea by Knuth, already implemented in lingeling(2013)
            for (int j = 0; j < out_learnt.size(); j++) { varFlags[var(out_learnt[j])].seen = 0; }    // ('seen[]' is now cleared)
            out_learnt.clear();

            for (int i = 0; i < decisionLevel(); ++i) {
                if ((i == 0 || trail_lim[i] != trail_lim[i - 1]) && trail_lim[i] < trail.size()) { // no dummy level caused by assumptions ...
                    out_learnt.push(~trail[ trail_lim[i] ]);    // get the complements of all decisions into dec array
                }
            }
            DOUT(if (config.opt_printDecisions > 2 || config.opt_learn_debug  || config.opt_rer_debug) cerr << endl << "c current decision stack: " << out_learnt << endl ;);
            const Lit tmpLit = out_learnt[ out_learnt.size() - 1 ]; //
            out_learnt[ out_learnt.size() - 1 ] = out_learnt[0]; // have first decision as last literal
            out_learnt[0] = tmpLit; // ~p; // new implied literal is the negation of the asserting literal ( could also be the last decision literal, then the learned clause is a decision clause) somehow buggy ...
            learnedDecisionClauses ++;
            DOUT(if (config.opt_printDecisions > 2 || config.opt_learn_debug  || config.opt_rer_debug) cerr << endl << "c learn decisionClause " << out_learnt << endl << endl;);
            doMinimizeClause = false;
            analyze_toclear.clear(); // already cleared everything
        }
    }

    // control minimization based on clause size
    if (config.opt_minimize_max_size != 0 && out_learnt.size() > config.opt_minimize_max_size) {
        doMinimizeClause = false;
        out_learnt.copyTo(analyze_toclear); // take care of the conesquences of not performing minimization
    }
    DOUT(if (config.opt_learn_debug) cerr << endl << "c found learned clause after usual resolution: " << out_learnt << endl;);

    if (doMinimizeClause) {
        // Simplify conflict clause:
        //
        int i, j;
        uint64_t minimize_dependencyLevel = dependencyLevel;
        out_learnt.copyTo(analyze_toclear);
        if (searchconfiguration.ccmin_mode == 2) {
            uint32_t abstract_level = 0;
            for (i = 1; i < out_learnt.size(); i++) {
                abstract_level |= abstractLevel(var(out_learnt[i]));    // (maintain an abstraction of levels involved in conflict)
            }


            for (i = j = 1; i < out_learnt.size(); i++) {
                minimize_dependencyLevel = dependencyLevel;
                if (reason(var(out_learnt[i])).getReasonC() == CRef_Undef) {
                    out_learnt[j++] = out_learnt[i]; // keep, since we cannot resolve on decisino literals
                } else if (!litRedundant(out_learnt[i], abstract_level, dependencyLevel)) {
                    dependencyLevel = minimize_dependencyLevel; // not minimized, thus, keep the old value
                    out_learnt[j++] = out_learnt[i]; // keep, since removing the literal would probably introduce new levels
                }
            }

        } else if (searchconfiguration.ccmin_mode == 1) {
            for (i = j = 1; i < out_learnt.size(); i++) {
                Var x = var(out_learnt[i]);

                #ifdef PCASSO
                assert(! reason(x).isBinaryClause() && "pcasso does not support implicit binary clause reasons");
                #endif

                if (!reason(x).isBinaryClause() && reason(x).getReasonC() == CRef_Undef) {
                    out_learnt[j++] = out_learnt[i];
                } else if (reason(x).isBinaryClause()) {  // handle case with binary clause
                    const Lit reasonLit = reason(x).getReasonL();
                    if (! varFlags[var(reasonLit)].seen && level(var(reasonLit)) > 0) {
                        out_learnt[j++] = out_learnt[i];
                    }
                } else {
                    Clause& c = ca[reason(var(out_learnt[i])).getReasonC()];
                    int k = ((c.size() == 2) ? 0 : 1); // bugfix by Siert Wieringa
                    for (; k < c.size(); k++) {
                        if (! varFlags[var(c[k])].seen && level(var(c[k])) > 0) {
                            out_learnt[j++] = out_learnt[i];
                            break;
                        }
                    }
                    #ifdef PCASSO
                    if (k == c.size()) {
                        dependencyLevel = dependencyLevel >= c.getPTLevel() ? dependencyLevel : c.getPTLevel();
                    }
                    #endif
                }
            }
        } else {
            i = j = out_learnt.size();
        }

        max_literals += out_learnt.size();
        out_learnt.shrink_(i - j);
        tot_literals += out_learnt.size();
        if (i != j) { recomputeLBD = true; }   // necessary to recompute LBD here!
        DOUT(if (config.opt_learn_debug) cerr << endl << "c found learned clause after MINISAT minimization: " << out_learnt << endl;);


        /* ***************************************
        Minimisation with binary clauses of the asserting clause
        First of all : we look for small clauses
        Then, we reduce clauses with small LBD.
        Otherwise, this can be useless
             */

        if (out_learnt.size() <= searchconfiguration.lbSizeMinimizingClause) {
            if (recomputeLBD) { lbd = computeLBD(out_learnt, out_learnt.size()); }  // update current lbd, such that the following method can decide next whether it wants to apply minimization to the clause
            recomputeLBD = minimisationWithBinaryResolution(out_learnt, lbd, dependencyLevel); // code in this method should execute below code until determining correct backtrack level
            DOUT(if (config.opt_learn_debug) cerr << endl << "c found learned clause after GLUCOSE minimization: " << out_learnt << endl;);
        }

        // assert( !hasComplementary(out_learnt) && !hasDuplicates(out_learnt) && "there should not be duplicate literals in the learned clause"  );

        if (out_learnt.size() <= searchconfiguration.uhle_minimizing_size) {
            if (recomputeLBD) { lbd = computeLBD(out_learnt, out_learnt.size()); }  // update current lbd, such that the following method can decide next whether it wants to apply minimization to the clause
            recomputeLBD = searchUHLE(out_learnt, lbd, dependencyLevel);
            DOUT(if (config.opt_learn_debug) cerr << endl << "c found learned clause after UNHIDE minimization: " << out_learnt << endl;);
        }

        // assert( !hasComplementary(out_learnt) && !hasDuplicates(out_learnt) && "there should not be duplicate literals in the learned clause"  );

        if (searchconfiguration.use_reverse_minimization && out_learnt.size() <= searchconfiguration.lbSizeReverseClause) {
            if (recomputeLBD) { lbd = computeLBD(out_learnt, out_learnt.size()); }  // update current lbd, such that the following method can decide next whether it wants to apply minimization to the clause
            recomputeLBD = reverseLearntClause(out_learnt, lbd, dependencyLevel);
            DOUT(if (config.opt_learn_debug) cerr << endl << "c found learned clause after REVERSE minimization: " << out_learnt << endl;);
        }

        // assert( !hasComplementary(out_learnt) && !hasDuplicates(out_learnt) && "there should not be duplicate literals in the learned clause"  );

        // rewrite clause only, if one of the two systems added information
        if (config.opt_rer_as_replaceAll && out_learnt.size() <= config.erRewrite_size) {   // rewrite disjunctions with information taken from RER
            if (recomputeLBD) { lbd = computeLBD(out_learnt, out_learnt.size()); }  // update current lbd, such that the following method can decide next whether it wants to apply minimization to the clause
            recomputeLBD = erRewrite(out_learnt, lbd, dependencyLevel);
        }

        // assert( !hasComplementary(out_learnt) && !hasDuplicates(out_learnt) && "there should not be duplicate literals in the learned clause"  );

    } // end working on usual learnt clause (minimize etc.)


    DOUT(if (config.opt_rer_debug) cerr << "c learned clause (after minimize): " << out_learnt << endl;);
    // Find correct backtrack level:
    //
    // yet, the currently learned clause is not bi-asserting (bi-asserting ones could be turned into asserting ones by minimization
    if (out_learnt.size() == 1) {
        out_btlevel = 0;
    } else {
        int max_i = 1;
        int decLevelLits = 1;
        // Find the first literal assigned at the next-highest level:
        if (config.opt_biAsserting) {
            int currentLevel = level(var(out_learnt[max_i]));
            for (int i = 1; i < out_learnt.size(); i++) {
                if (level(var(out_learnt[i])) == decisionLevel()) {  // if there is another literal of the decision level (other than the first one), then the clause is bi-asserting
                    isBiAsserting = true;
                    // move this literal to the next free position at the front!
                    const Lit tmp = out_learnt[i];
                    out_learnt[i] = out_learnt[decLevelLits];
                    out_learnt[decLevelLits] = tmp;
                    if (max_i == decLevelLits) { max_i = i; }  // move the literal with the second highest level correctly
                    decLevelLits ++;
                } // the level of the literals of the current level should not become the backtracking level, hence, this literal is not moved to this position
                else if (level(var(out_learnt[max_i])) == decisionLevel() || level(var(out_learnt[i])) > level(var(out_learnt[max_i]))) { max_i = i; } // use any literal, as long as the backjump level is the same as the current level
            }
        } else {
            for (int i = 2; i < out_learnt.size(); i++) {
                if (level(var(out_learnt[i])) > level(var(out_learnt[max_i]))) {
                    max_i = i;
                }
            }
        }
        // Swap-in this literal at index 1:
        const Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
        if (out_learnt.size() == 2 && isBiAsserting) { out_btlevel = 0; }  // for binary bi-asserting clauses, jump back to level 0 always!
        else { out_btlevel       = level(var(p)); }  // otherwise, use the level of the variable that is moved to the front!
    }

    assert(out_btlevel < decisionLevel() && "there should be some backjumping");

    // Compute LBD, if the current value is not the right value
    if (recomputeLBD) { lbd = computeLBD(out_learnt, out_learnt.size()); }

    lbd = isBiAsserting ? lbd + 1 : lbd; // for bi-asserting clauses the LBD has to be one larger (approximation), because it is not known whether the one literal would glue the other one

    #ifdef UPDATEVARACTIVITY
    // UPDATEVARACTIVITY trick (see competition'09 companion paper)
    if (lastDecisionLevel.size() > 0) {
        for (int i = 0; i < lastDecisionLevel.size(); i++) {
            if (ca[ reason(var(lastDecisionLevel[i])).getReasonC() ].lbd() < lbd) {
                DOUT(if (config.opt_learn_debug) cerr << "c add " << lastDecisionLevel[i] << " to bump, with " << ca[ reason(var(lastDecisionLevel[i])).getReasonC() ].lbd() << " vs " << lbd << endl;);
                varsToBump.push(var(lastDecisionLevel[i])) ;
            }
        }
        lastDecisionLevel.clear();
    }
    #endif

    for (int j = 0; j < analyze_toclear.size(); j++) {
        DOUT(if (config.opt_learn_debug) cerr << "c reset seen for " << analyze_toclear[j] << endl;);
        varFlags[var(analyze_toclear[j])].seen = 0;    // ('seen[]' is now cleared)
    }

    // bump the used clauses!
    for (int i = 0 ; i < clssToBump.size(); ++ i) {
        claBumpActivity(ca[ clssToBump[i] ], config.opt_cls_act_bump_mode == 1 ? out_learnt.size() : lbd);
    }
    for (int i = 0 ; i < varsToBump.size(); ++ i) {
        varBumpActivity(varsToBump[i], config.opt_var_act_bump_mode == 0 ? 1 : (config.opt_var_act_bump_mode == 1 ? out_learnt.size() : lbd));
    }

    for (int k = 0 ; k < out_learnt.size(); ++ k) {  // check for duplicates
        for (int m = k + 1; m < out_learnt.size(); ++m) { assert(out_learnt[k] != out_learnt[m] && "do not have duplicate literals in the clause"); }
    }

    return 0;

}


// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool Solver::litRedundant(Riss::Lit p, uint32_t abstract_levels, unsigned int& dependencyLevel)
{
    // TODO implement dependencyLevel properly for Pcasso into analysis!

    DOUT(if (config.opt_learn_debug) cerr << "c check redundancy for literal " << p << endl;);
    analyze_stack.clear(); analyze_stack.push(p);
    int top = analyze_toclear.size();
    while (analyze_stack.size() > 0) {
        assert(reason(var(analyze_stack.last())).isBinaryClause() || reason(var(analyze_stack.last())).getReasonC() != CRef_Undef);  // a reason must exist

        if (reason(var(analyze_stack.last())).isBinaryClause()) {

            // only use innermost part of below loops
            const Lit& p  = reason(var(analyze_stack.last())).getReasonL();
            analyze_stack.pop();

            if (!varFlags[var(p)].seen && level(var(p)) > 0) {
                if ((reason(var(p)).isBinaryClause() || reason(var(p)).getReasonC() != CRef_Undef) && (abstractLevel(var(p)) & abstract_levels) != 0) {  // can be used for minimization
                    varFlags[var(p)].seen = 1;
                    analyze_stack.push(p);
                    analyze_toclear.push(p);
                } else {
                    for (int j = top; j < analyze_toclear.size(); j++) {
                        varFlags[var(analyze_toclear[j])].seen = 0;
                    }
                    analyze_toclear.shrink_(analyze_toclear.size() - top);
                    return false;
                }
            }

        } else {
            Clause& c = ca[ reason(var(analyze_stack.last())).getReasonC() ]; analyze_stack.pop();
            if (c.size() == 2 && value(c[0]) == l_False) {
                assert(value(c[1]) == l_True);
                Lit tmp = c[0];
                c[0] =  c[1], c[1] = tmp;
            }
            #ifdef PCASSO // if minimization is done, then take also care of the participating clause!
            dependencyLevel = dependencyLevel >= c.getPTLevel() ? dependencyLevel : c.getPTLevel();
            #endif
            for (int i = 1; i < c.size(); i++) {
                Lit p  = c[i];
                if (!varFlags[var(p)].seen && level(var(p)) > 0) {
                    if ((reason(var(p)).isBinaryClause() || reason(var(p)).getReasonC() != CRef_Undef) && (abstractLevel(var(p)) & abstract_levels) != 0) {  // can be used for minimization
                        varFlags[var(p)].seen = 1;
                        analyze_stack.push(p);
                        analyze_toclear.push(p);
                    } else {
                        for (int j = top; j < analyze_toclear.size(); j++) {
                            varFlags[var(analyze_toclear[j])].seen = 0;
                        }
                        analyze_toclear.shrink_(analyze_toclear.size() - top);
                        return false;
                    }
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

    if (decisionLevel() == 0) {
        return;
    }

    varFlags[var(p)].seen = 1;

    for (int i = trail.size() - 1; i >= trail_lim[0]; i--) {
        Var x = var(trail[i]);
        if (varFlags[x].seen) {
            if (reason(x).isBinaryClause()) {  // handle binary reasons properly
                if (level(var(reason(x).getReasonL())) > 0) {
                    varFlags[ var(reason(x).getReasonL()) ].seen = 1;
                }
            } else {
                if (reason(x).getReasonC() == CRef_Undef) {
                    assert(level(x) > 0);
                    out_conflict.push(~trail[i]);
                } else {
                    Clause& c = ca[ reason(x).getReasonC() ];
                    // Bug in case of assumptions due to special data structures for Binary.
                    // Many thanks to Sam Bayless (sbayless@cs.ubc.ca) for discover this bug.
                    for (int j = ((c.size() == 2) ? 0 : 1); j < c.size(); j++)
                        if (level(var(c[j])) > 0) {
                            varFlags[var(c[j])].seen = 1;
                        }
                }
            }

            varFlags[x].seen = 0;
        }
    }

    varFlags[var(p)].seen = 0;
}


void Solver::analyzeFinal(const Solver::ReasonStruct& conflictingClause, vec< Lit >& out_conflict, const Lit otherLit)
{
    out_conflict.clear();

    if (decisionLevel() == 0) {
        return;
    }

    if (!conflictingClause.isBinaryClause()) {
        // saw all literals of this clause (if they are not top level)
        const Clause& c = ca[conflictingClause.getReasonC()];
        for (int i = 0 ; i < c.size(); ++ i) {
            if (level(var(c[i])) > 0) {
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c mark " << var(c[i]) << std::endl;);
                varFlags[var(c[i])].seen = 1;
            }
        }
    } else {
        for (int i = 0 ; i < 2; ++ i) {
            const Var v = var((i == 0 ? otherLit : conflictingClause.getReasonL()));
            if (level(v) > 0) {
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c mark " << v << std::endl;);
                varFlags[v].seen = 1;
            }
        }
    }

    const int minIndex = trail_lim.size() < 1 ? 0 : trail_lim[0];
    for (int i = trail.size() - 1; i >= minIndex; i--) {
        Var x = var(trail[i]);
        if (varFlags[x].seen) {

            if (reason(x).isBinaryClause()) {  // handle binary reasons properly
                if (level(var(reason(x).getReasonL())) > 0) {
                    varFlags[ var(reason(x).getReasonL()) ].seen = 1;
                }
            } else {
                if (reason(x).getReasonC() == CRef_Undef) {
                    assert(level(x) > 0);
                    out_conflict.push(~trail[i]);
                } else {
                    Clause& c = ca[ reason(x).getReasonC() ];
                    // Bug in case of assumptions due to special data structures for Binary.
                    // Many thanks to Sam Bayless (sbayless@cs.ubc.ca) for discover this bug.
                    for (int j = ((c.size() == 2) ? 0 : 1); j < c.size(); j++)
                        if (level(var(c[j])) > 0) {
                            varFlags[var(c[j])].seen = 1;
                        }
                }
            }
            varFlags[x].seen = 0;
        }
    }

    if (!conflictingClause.isBinaryClause()) {
        // saw all literals of this clause (if they are not top level)
        const Clause& c = ca[conflictingClause.getReasonC()];
        for (int i = 0 ; i < c.size(); ++ i) {
            if (level(var(c[i])) > 0) {
                varFlags[var(c[i])].seen = 0;
            }
        }
    } else {
        for (int i = 0 ; i < 2; ++ i) {
            const Var v = var((i == 0 ? otherLit : conflictingClause.getReasonL()));
            if (level(v) > 0) {
                varFlags[v].seen = 0;
            }
        }
    }
}


void Solver::uncheckedEnqueue(Lit p, Riss::CRef from, bool addToProof, const unsigned dependencyLevel)
{
    /*
     *  Note: this code is also executed during extended resolution, so take care of modifications performed there!
     */
    if (addToProof) {    // whenever we are at level 0, add the unit to the proof (might introduce duplicates, but this way all units are covered
        assert(decisionLevel() == 0 && "proof can contain only unit clauses, which have to be created on level 0");
        addCommentToProof("add unit clause that is created during adding the formula");
        addUnitToProof(p);
    }

    assert(value(p) == l_Undef && "cannot enqueue a wrong value");
    varFlags[var(p)].assigns = lbool(!sign(p));
    /** include variableExtraInfo here, if required! */
    vardata[var(p)] = mkVarData(from, decisionLevel());
    vardata[var(p)].position = (int)trail.size(); // to sort learned clause for extra analysis

    // prefetch watch lists
    // __builtin_prefetch( & watchesBin[p], 1, 0 ); // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    __builtin_prefetch(& watches[p], 1, 0);   // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    DOUT(if (config.opt_printDecisions > 1) {cerr << "c unchecked enqueue " << p; if (from != CRef_Undef) { cerr << " because of [" << from << "] " <<  ca[from]; } cerr << endl;});

    trail.push_(p);
}

void Solver::uncheckedEnqueue(Lit p, Riss::Lit fromLit, bool addToProof, const unsigned dependencyLevel)
{
    /*
     *  Note: this code is also executed during extended resolution, so take care of modifications performed there!
     */
    if (addToProof) {    // whenever we are at level 0, add the unit to the proof (might introduce duplicates, but this way all units are covered
        assert(decisionLevel() == 0 && "proof can contain only unit clauses, which have to be created on level 0");
        addCommentToProof("add unit clause that is created during adding the formula");
        addUnitToProof(p);
    }

    assert(value(p) == l_Undef && "cannot enqueue a wrong value");
    varFlags[var(p)].assigns = lbool(!sign(p));
    /** include variableExtraInfo here, if required! */
    vardata[var(p)] = mkVarData(fromLit, decisionLevel());
    vardata[var(p)].position = (int)trail.size(); // to sort learned clause for extra analysis

    // prefetch watch lists
    // __builtin_prefetch( & watchesBin[p], 1, 0 ); // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    __builtin_prefetch(& watches[p], 1, 0);   // prefetch the watch, prepare for a write (1), the data is highly temoral (0)
    DOUT(if (config.opt_printDecisions > 1) {cerr << "c unchecked enqueue " << p << " implied by " << fromLit << endl;});

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
    assert((decisionLevel() == 0 || !duringAddingClauses) && "clauses can only be added at level 0!");
    // if( config.opt_printLhbr ) cerr << endl << "c called propagate" << endl;
    DOUT(if (config.opt_learn_debug) cerr << "c call propagate with " << qhead << " for " <<  trail.size() << " lits: " << trail << endl;);

    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll(); clssToBump.clear();

    const bool no_long_conflict = !config.opt_long_conflict;
    const bool update_lbd = config.opt_update_lbd == 0;
    const bool share_clauses = sharingTimePoint == 1 && communication != 0;

    while (qhead < trail.size()) {
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        DOUT(if (config.opt_learn_debug) cerr << "c propagate literal " << p << endl;);
        realHead = qhead;
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;

        // propagate longer clauses here!
        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;) {
            if (i->isBinary()) {   // handle binary clauses
                const Lit& imp = i->blocker();
                assert(ca[ i->cref() ].size() == 2 && "in this list there can only be binary clauses");
                DOUT(if (config.opt_learn_debug) cerr << "c checked binary clause " << ca[i->cref() ] << " with implied literal having value " << (value(imp)) << endl;);
                if (value(imp) == l_False) {
                    if (no_long_conflict) {  // stop on the first conflict we see?
                        confl = i->cref();              // store the conflict
                        while (i < end) { *j++ = *i++; }    // move the remaining elements forward
                        ws.shrink_(i - j);              // remove all duplciate clauses
                        goto FinishedPropagation;       // jump to end of method, so that the statistics can be updated correctly
                    }
                    confl = i->cref(); // store intermediate conflict to be evaluated later
                } else if (value(imp) == l_Undef) { // enqueue the implied literal
                    uncheckedEnqueue(imp, p, duringAddingClauses); // the reason why the literal "imp" is implied is the literal "p" (which is currently propagated)
                }
                *j++ = *i++; // keep the element in the list
                continue;
            }

            DOUT(if (config.opt_learn_debug) cerr << "c check clause [" << i->cref() << "]" << ca[i->cref()] << endl;);
            #ifndef PCASSO // PCASS reduces clauses during search without updating the watch lists ...
//            assert(ca[ i->cref() ].size() > 2 && "in this list there can only be clauses with more than 2 literals"); (RATE also shrinks clauses during using the solver object)
            #endif

            // Try to avoid inspecting the clause:
            const Lit blocker = i->blocker();
            if (value(blocker) == l_True) { // keep binary clauses, and clauses where the blocking literal is satisfied
                *j++ = *i++; continue;
            }

            // Make sure the false literal is data[1]:
            const CRef cr = i->cref();
            Clause&  c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit) {
                c[0] = c[1], c[1] = false_lit;
            }
            assert(c[1] == false_lit && "wrong literal order in the clause!");
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            #ifndef PCASSO // Pcasso reduces clauses without updating the watch lists
//            assert(c.size() > 2 && "at this point, only larger clauses should be handled!"); (RATE also shrinks clauses during using the solver object)
            #endif
            const Watcher& w     = Watcher(cr, first, 1); // updates the blocking literal
            if (first != blocker && value(first) == l_True) { // satisfied clause

                *j++ = w; continue;
            } // same as goto NextClause;

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False) {
                    c[1] = c[k]; c[k] = false_lit;
                    DOUT(if (config.opt_learn_debug) cerr << "c new watched literal for clause " << ca[cr] << " is " << c[1] << endl;);
                    watches[~c[1]].push(w);
                    goto NextClause;
                } // no need to indicate failure of lhbr, because remaining code is skipped in this case!


            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (share_clauses && (c.learnt() || c.isCoreClause()) && !c.wasPropagated()) {  // share clauses only, if they are propagated (see Simon&Audemard SAT 2014)
                #ifdef PCASSO
                updateSleep(&c, c.size(), c.getPTLevel());  // share clause including level information
                #else
                updateSleep(&c, c.size());  // shorter clauses are shared immediately!
                #endif
                c.setPropagated();
            }
            if (value(first) == l_False) {
                confl = cr; // independent of opt_long_conflict -> overwrite confl!
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end) {
                    *j++ = *i++;
                }
            } else {
                DOUT(if (config.opt_learn_debug) cerr << "c current clause is unit clause: " << ca[cr] << endl;);
                uncheckedEnqueue(first, cr, duringAddingClauses);

                // if( config.opt_printLhbr ) cerr << "c final common dominator: " << commonDominator << endl;

                if (update_lbd && c.mark() == 0) { // if LHBR did not remove this clause
                    int newLbd = computeLBD(c, c.size());
                    if (newLbd < c.lbd() || config.opt_lbd_inc) {  // improve the LBD (either LBD decreased,or option is set)
                        if (c.lbd() <= searchconfiguration.lbLBDFrozenClause) {
                            c.setCanBeDel(false);   // LBD of clause improved, so that its not considered for deletion
                        }
                        c.setLBD(newLbd);
                    } else if (config.opt_rem_inc_lbd && newLbd > c.lbd()) { c.setCanBeDel(true); }
                }
            }
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!

    }
FinishedPropagation:
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
struct reduceDB_lbd_lt {
    ClauseAllocator& ca;
    reduceDB_lbd_lt(ClauseAllocator& ca_) : ca(ca_) {}
    bool operator()(CRef x, CRef y)
    {

        // Main criteria... Like in MiniSat we keep all binary clauses
        if (ca[x].size() > 2 && ca[y].size() == 2) { return 1; }

        if (ca[y].size() > 2 && ca[x].size() == 2) { return 0; }
        if (ca[x].size() == 2 && ca[y].size() == 2) { return 0; }

        // Second one  based on literal block distance
        if (ca[x].lbd() > ca[y].lbd()) { return 1; }
        if (ca[x].lbd() < ca[y].lbd()) { return 0; }


        // Finally we can use old activity or size, we choose the last one
        return ca[x].activity() < ca[y].activity();
        //return x->size() < y->size();

        //return ca[x].size() > 2 && (ca[y].size() == 2 || ca[x].activity() < ca[y].activity()); }
    }
};

// struct that has been used inside Minisat for removal
struct reduceDB_act_lt {
    ClauseAllocator& ca;
    reduceDB_act_lt(ClauseAllocator& ca_) : ca(ca_) {}
    bool operator()(CRef x, CRef y)
    {

        return (!ca[x].learnt() || !ca[y].learnt()) || // do not continue, if one of the clauses is not learnt
               (ca[x].size() > 2 && (ca[y].size() == 2 || ca[x].activity() < ca[y].activity()));
    }
};

void Solver::reduceDB()
{
    DOUT(if (config.opt_removal_debug > 0)  cerr << "c reduceDB ..." << endl;);
    reduceDBTime.start();
    resetRestrictedExtendedResolution(); // whenever the clause database is touched, forget about current RER step
    int     i, j;
    nbReduceDB++;

    if (big != 0) {   // build a new BIG that is valid on the current
        big->recreate(ca, nVars(), clauses, learnts);
        big->removeDuplicateEdges(nVars());
        big->generateImplied(nVars(), add_tmp);
        if (config.opt_uhdProbe > 2) { big->sort(nVars()); }     // sort all the lists once
    }

    double  extra_lim = cla_inc / learnts.size();    // Remove any clause below this activity

    if (! activityBasedRemoval) { sort(learnts, reduceDB_lbd_lt(ca)); }    // sort size 2 and lbd 2 to the back!
    else {

        // automatically choose between activity and LBD based removal!
        bool useAct = true;
        if (config.opt_avg_size_lbd_ratio != 0) {  // only perform, if operation is allowed
            double avgLBD = 0, stddevLBD = 0, avgSIZE = 0, stddevSIZE = 0;
            double count = 0;
            for (int i = 0 ; i < learnts.size(); ++ i) {  // calc avg and stddev incrementally in one round
                Clause& c = ca[ learnts[i] ];
                if (c.mark() == 0 && c.learnt()) {
                    count ++;
                    const double deltaLBD = c.lbd() - avgLBD;
                    avgLBD = avgLBD + deltaLBD / count;
                    stddevLBD = stddevLBD + deltaLBD * (c.lbd() - avgLBD);
                    const double deltaSIZE = c.size() - avgSIZE;
                    avgSIZE = avgSIZE + deltaSIZE / count;
                    stddevSIZE = stddevSIZE + deltaSIZE * (c.size() - avgSIZE);
                }
            }
            stddevLBD = (count > 1) ? stddevLBD / (count - 1) : 0.0;
            stddevSIZE = (count > 1) ? stddevSIZE / (count - 1) : 0.0;

            // choose value based on ratio. negative ratio means that comparison is in the oposite direction
            useAct = false;
            if (config.opt_avg_size_lbd_ratio > 0 && stddevLBD * config.opt_avg_size_lbd_ratio > stddevSIZE) {
                useAct = true;
            } else if (config.opt_avg_size_lbd_ratio < 0 && stddevLBD * -config.opt_avg_size_lbd_ratio < stddevSIZE) {
                useAct = true;
            }

        }
        if (useAct) { sort(learnts, reduceDB_act_lt(ca)); }
        else { sort(learnts, reduceDB_lbd_lt(ca)); }
    }


    // We have a lot of "good" clauses, it is difficult to compare them. Keep more !
    if (learnts.size() > 0 && ca[learnts[learnts.size() / RATIOREMOVECLAUSES]].lbd() <= 3) { nbclausesbeforereduce += searchconfiguration.specialIncReduceDB; }
    // Useless :-)
    if (learnts.size() > 0 && ca[learnts.last()].lbd() <= 5)  { nbclausesbeforereduce += searchconfiguration.specialIncReduceDB; }


    // Don't delete binary or locked clauses. From the rest, delete clauses from the first half
    // Keep clauses which seem to be usefull (their lbd was reduce during this sequence)

    int limit = learnts.size() * learnts_reduce_fraction;
    DOUT(if (config.opt_removal_debug > 1) cerr << "c reduce limit: " << limit << endl;) ;

    const int delStart = (int)(config.opt_keep_worst_ratio * (double)learnts.size());  // keep some of the bad clauses!
    for (i = j = 0; i < learnts.size(); i++) {
        Clause& c = ca[learnts[i]];
        if (! c.isCoreClause()) {
            if (i >= delStart
                    && (c.lbd() > 2 || activityBasedRemoval)
                    && c.size() > 2
                    && c.canBeDel()
                    &&  !locked(c)
                    && (i < limit)) {
                removeClause(learnts[i]);
                nbRemovedClauses++;
                DOUT(if (config.opt_removal_debug > 2) cerr << "c remove clause " << c << endl;);
            } else {
                if (!c.canBeDel()) { limit++; } //we keep c, so we can delete an other clause
                c.setCanBeDel(true);       // At the next step, c can be delete
                DOUT(if (config.opt_removal_debug > 2) cerr << "c keep clause " << c << " due to set 'canBeDel' flag" << endl;);
                learnts[j++] = learnts[i];
            }
        } else {
            // core-learnt clauses are removed from this vector
            assert((!c.isCoreClause() || !c.learnt()) && "for all core-learnt clauses the learnt flag should have been erased");
        }
    }
    // FIXME: check whether old variant of removal works with the above code - otherwise include with parameter
    learnts.shrink_(i - j);
    DOUT(if (config.opt_removal_debug > 0) cerr << "c resulting learnt clauses: " << learnts.size() << endl;);
    checkGarbage();
    reduceDBTime.stop();
}


void Solver::removeSatisfied(vec<CRef>& cs)
{
    assert(qhead == trail.size() && "only perform in case trail is fully handled as proof already");
    int i, j;
    for (i = j = 0; i < cs.size(); i++) {
        Clause& c = ca[cs[i]];
        if (c.size() > 1 && satisfied(c)) { // do not remove unit clauses!
            removeClause(cs[i]);
        } else {
            cs[j++] = cs[i];
        }
    }
    cs.shrink_(i - j);
}


void Solver::rebuildOrderHeap()
{
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if (varFlags[v].decision && value(v) == l_Undef) {
            vs.push(v);
        }
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


void Solver::counterImplicationRestart()
{
    // number of times this function is executed
    cir_count++;

    // this function runs every three times
    // small intervalls (like 3) end in better performance
    if (cir_count % 3 > 0) {
        return;
    }

    int nv = nVars();
    vec<int> implied_num(nv + 1, 0);
    int max_imp = 0;

    // loop backwards over trail to set the indegree
    // of each variable and find maximal indegree
    for (int c = trail.size() - 1; c >= 0; c--) {
        int x = var(trail[c]);
        int lvl = level(x);

        if (reason(x).isBinaryClause()) {

            Var cl[2]; // pseudo clause structure for simple code
            cl[0] = x;
            cl[1] = var(reason(x).getReasonL());

            // loop over reason clause
            for (int j = 0; j < 2; j++) {
                Var v = cl[j];

                if (level(v) != lvl) {
                    implied_num[x]++;
                }
            }

            if (max_imp < implied_num[x]) {
                max_imp = implied_num[x];
            }

        } else {
            CRef r = reason(x).getReasonC();

            if (r == CRef_Undef) {
                continue;
            }

            Clause& cl = ca[r];

            // loop over reason clause
            for (int j = 0; j < cl.size(); j++) {
                int v = var(cl[j]);

                if (level(v) != lvl) {
                    implied_num[x]++;
                }
            }

            if (max_imp < implied_num[x]) {
                max_imp = implied_num[x];
            }

        }
    }

    for (int c = trail.size() - 1; c >= 0; c--) {
        int x = var(trail[c]);


        // all the VSIDS scores of the variables are bumped in
        // proportion to their indegrees. To bump the VSIDS score drastically, the constant
        // number of the \BUMPRATIO" needs to be relatively large.
        // A variable with a large indegree implies that this variable used to be the unit
        // variable in a large clause. So it is selected as
        // decision-variables at early depth of the search tree.
        if (implied_num[x] != 0) {
            varBumpActivity(x, var_inc * implied_num[x] * cir_bump_ratio / max_imp);
        }
    }
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

    assert(decisionLevel() == 0);

    if (!config.opt_hpushUnit || startedSolving) {   // push the first propagate until solving started
        if (!ok || propagate() != CRef_Undef) {
            return ok = false;
        }
    }

    if (nAssigns() == simpDB_assigns || (simpDB_props > 0)) {
        return true;
    }

    if (!activityBasedRemoval) {  // perform this check only if the option is used
        int i = 0; // to prevent g++ to throw a "i maybe used unintialized" warning
        int j = 0;
        for (; i < learnts.size(); i++) {
            if (!ca[learnts[i]].isCoreClause()) {
                learnts[j++] = learnts[i];
            }
        }
        learnts.shrink(i - j);
    }

    // Remove satisfied clauses:
    removeSatisfied(learnts);
    if (remove_satisfied) {      // Can be turned off.
        removeSatisfied(clauses);
    }
    checkGarbage();
    rebuildOrderHeap();

    simpDB_assigns = nAssigns();
    simpDB_props   = clauses_literals + learnts_literals;   // (shouldn't depend on stats really, but it will do for now)

    // Only perform inprocessing if the option is not zero.
    if (probing_step_width) {
        // Performed probing every "probing_step_width"-times
        probing_step = (probing_step + 1) % probing_step_width;

        if (probing_step == 0) {
            // If a contraction for any of the variables was found, the
            // formula is UNSAT
            if (!probingHighestActivity()) {
                // Mark the formula as unsatisfiable
                ok = false;

                return false;
            }
        }
    }
    return true;
}

bool Solver::probingHighestActivity()
{
    // Probe variables with highest activity
    for (int i = 0; i < fmin(order_heap.size(), probing_limit); ++i) {
        // Contradiction was found => UNSAT
        if (!probing(order_heap[i])) {
            return false;
        }
    }

    return true;
}

bool Solver::probing(Var v)
{

    cancelUntil(0);

    if (value(v) != l_Undef) {
        return true;
    }

    switch (probingLiteral(mkLit(v, true))) {
    // Contradiction => UNSAT
    case 2: return false;
    case 1: return true;
    }

    // no conflicting clause was found
    varFlags.copyTo(probing_oldAssigns);
    cancelUntil(0);

    // Probe negated literal
    switch (probingLiteral(mkLit(v, false))) {
    // Contradiction => UNSAT
    case 2: return false;
    case 1: return true;
    }

    // No conflict clause for positive and negative literal
    // found. Collect the commonly implied literals and enqueue them.

    probing_uncheckedLits.clear();

    // Walk though assignment stack for decision level 0
    for (int i = trail_lim[0]; i < trail.size(); ++i) {
        Lit l = trail[i];

        // if variable assignment has changed from positive to negative,
        // save literal as unchecked
        if (probing_oldAssigns[var(l)].assigns == l_True && !sign(l)) {
            probing_uncheckedLits.push(l);
        }

        // same as above, but from negative to positive
        if (probing_oldAssigns[var(l)].assigns == l_False && sign(l)) {
            probing_uncheckedLits.push(l);
        }
    }

    cancelUntil(0);

    // enqueue all implied literals
    for (int i = 0; i < probing_uncheckedLits.size(); i++) {
        enqueue(probing_uncheckedLits[i], CRef_Undef);
    }

    // Found conflicting clause => UNSAT
    if (propagate() != CRef_Undef) {
        return false;
    }

    return true;
}


int Solver::probingLiteral(Lit v)
{
    newDecisionLevel();

    // Unit propagation for literal
    uncheckedEnqueue(v);

    // Conflict for literal v
    if (propagate() != CRef_Undef) {
        // Perform backtrack to top level
        cancelUntil(0);

        // Unit propagation for negated literal
        uncheckedEnqueue(~v);

        // Contradiction: conflict literal "v" and "not v"
        if (propagate() != CRef_Undef) {
            return 2;
        }

        // Conflict for literal v
        return 1;
    }

    // No conflicting clause found
    return 0;
}

void Solver::enableDPLL()
{
    useNaiveBacktracking = true;
}


lbool Solver::dpllBacktracking()
{
    if (decisionLevel() == 0) { return l_False; }  // no more search space left
    assert(trail_lim.size() > 0 && "there are decisions that can be undone");
    const Lit impliedLit = ~ trail [ trail_lim[ trail_lim.size() - 1 ] ];
    DOUT(if (config.opt_learn_debug || config.opt_printDecisions > 0) cerr << "c DPLL backtracking: add " << impliedLit << " for backtracking (was decision " << ~impliedLit << "@" << decisionLevel() << ")" << endl;);
    cancelUntil(decisionLevel() - 1);            // undo last level
    uncheckedEnqueue(impliedLit, CRef_Undef);    // add the complement of the highest decision as implied literal (yet without reason clause)
    return l_Undef;
}

lbool Solver::conflictAnalysis(const CRef confl, vec<Lit>& learnt_clause)
{
    // run naive backtracking (dpll)
    if (useNaiveBacktracking) {
        return dpllBacktracking();
    }

    int backtrack_level;
    unsigned int nblevels;
    learnt_clause.clear();
    if (config.opt_biAsserting && lastBiAsserting + config.opt_biAssiMaxEvery <= conflicts) { allowBiAsserting = true; }  // use bi-asserting only once in a while!
    unsigned dependencyLevel = 0;
    analysisTime.start();
    // perform learnt clause derivation
    int ret = analyze(confl, learnt_clause, backtrack_level, nblevels, dependencyLevel);
    analysisTime.stop();
    allowBiAsserting = false;
    assert((!isBiAsserting || ret == 0) && "cannot be multi unit and bi asserting at the same time");
    DOUT(if (config.opt_rer_debug) cerr << "c analyze returns with " << ret << " , jumpLevel " << backtrack_level << " and set of literals " << learnt_clause << endl;);
    // OTFSS TODO put into extra method!
    bool backTrackedBeyondAsserting = false; // indicate whether learnt clauses can become unit (if no extra backtracking is performed, this stament is true)
    cancelUntil(backtrack_level);  // cancel trail so that learned clause becomes a unit clause
    // add the new clause(s) to the solver, perform more analysis on them
    if (ret > 0) {   // multiple learned clauses
        if (l_False == handleMultipleUnits(learnt_clause)) { return l_False; }
        updateSleep(&learnt_clause, learnt_clause.size(), true);   // share multiple unit clauses!
    } else { // treat usual learned clause!
        // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "there should not be duplicate literals in the learned clause"  );
        if (l_False == handleLearntClause(learnt_clause, backTrackedBeyondAsserting, nblevels, dependencyLevel)) { return l_False; }
    }

    return l_Undef;
}

void Solver::handleTopLevelUnits(const int& beforeTrail, int& proofTopLevels)
{
    assert(decisionLevel() == 0 && "should be called only on decision level 0");
    if (outputsProof()) {   // add the units to the proof that have been added by being propagated on level 0
        for (; proofTopLevels < trail.size(); ++ proofTopLevels) { addUnitToProof(trail[ proofTopLevels ]); }
    }
    // send all units that have been propagated on level 0! (other thread might not see the same clauses as this thread)
    const int unitsToShare = trail.size() - beforeTrail;
    if (unitsToShare > 0) {
        Lit* headPointer = & (trail[beforeTrail]);  // pointer to the actual element in the vector. as vectors use arrays to store data, the trick works
        #ifdef PCASSO
        updateSleep(&headPointer, unitsToShare, currentDependencyLevel(), true);   // give some value to the method, it will trace the dependencies for multi-units automatically
        #else
        updateSleep(&headPointer, unitsToShare, true);
        #endif
    }
}

lbool Solver::receiveInformation()
{
    // check for communication to the outside (for example in the portfolio solver)
    int result = updateSleep((vec<Lit>*)0x0, 0, 0);  // just receive
    if (-1 == result) {
        // interrupt via communication
        return l_Undef;
    } else if (result == 1) {
        // interrupt received a clause that leads to a conflict on level 0
        conflict.clear();
        return l_False;
    }
    return l_True;
}

Lit Solver::prefetchAssumption(const Lit p, int mode)
{
    const int start = decisionLevel() + 1;
    const int end = assumptions.size() - 1;

    // in case nothing is left
    if (mode == 0 || end <= start) { return p; }

    // check all assumptions
    if (mode == 4) {
        for (int i = start; i <= end; ++i) {
            if (value(assumptions[i]) == l_False) { return assumptions[i]; }
        }
        return p;
    }

    // check a single assumption
    Lit candidate = p;
    const int diff = end - start;
    // mode 1 to 3 act like a bloom filter
    switch (mode) {
    case 1: candidate = assumptions[ assumptions.size() - 1 ];
        break;
    case 2: candidate = assumptions[ start + rand() % (diff + 1) ]; // both start and end are valid indexes
        break;
    case 3: candidate = assumptions[ start + ((diff + 1) / 2) ];
        break;
    }
    // check whether our candidate is falsified
    if (value(candidate) == l_False) { return candidate; }

    // if no failed assumption was found, return the default
    return p;
}

Lit Solver::performSearchDecision(lbool& returnValue, vec<Lit>& tmp_Lits)
{
    Lit next = lit_Undef;
    bool checkedLookaheadAlready = false;
    while (next == lit_Undef) {
        while (decisionLevel() < assumptions.size()) {
            // Perform user provided assumption:
            Lit p = assumptions[decisionLevel()];

            if (config.opt_prefetch_assumption != 0) { p = prefetchAssumption(p, config.opt_prefetch_assumption); }
            if (value(p) == l_True) {
                // Dummy decision level: // do not have a dummy level here!!
                DOUT(if (config.opt_printDecisions > 0) cerr << "c have dummy decision level for assumptions" << endl;);
                newDecisionLevel();
            } else if (value(p) == l_False) {
                analyzeFinal(~p, conflict);
                returnValue = l_False;
                return lit_Error;
            } else {
                DOUT(if (config.opt_printDecisions > 0) cerr << "c use assumption as next decision : " << p << endl;);
                next = p;
                break;
            }
        }

        if (next == lit_Undef) {
            // New variable decision:
            decisions++;
            next = pickBranchLit();
            DOUT(if (config.opt_printDecisions > 1) cerr << "c pickBranchLit selects literal " << next << endl;);
            if (next != lit_Undef) { assert(value(next) == l_Undef && "decision variable has to be undefined"); }
            if (next == lit_Undef) {
                EnumerationClient::EnumerateState res = enumerationClient.processCurrentModel(next);
                if (res == EnumerationClient::oneModel) {
                    DOUT(if (config.opt_printDecisions > 1) cerr << "c enumeration opted to return SAT in search method (oneModel)" << endl;);
                    returnValue = l_True;
                    return lit_Error;
                } else if (res == EnumerationClient::goOn) {
                    DOUT(if (config.opt_printDecisions > 1) cerr << "c enumeration opted continue with search " << endl;);
                    return lit_Undef;
                } else if (res == EnumerationClient::stop) {
                    DOUT(if (config.opt_printDecisions > 1) cerr << "c enumeration opted to return SAT in search method (oneModel)" << endl;);
                    returnValue = l_True;
                    return lit_Error;
                } else {
                    DOUT(if (config.opt_printDecisions > 1) cerr << "c enumeration opted and unhandled result " << endl;);
                    assert(false && "this case should not be reached");
                    returnValue = l_Undef;
                    return lit_Error;
                }
            }
        }

        // if sufficiently many new top level units have been learned, trigger another LA!
        if (!checkedLookaheadAlready) {
            checkedLookaheadAlready = true; // memorize that we did the check in the first iteration
            if (config.opt_laTopUnit != -1 && topLevelsSinceLastLa >= config.opt_laTopUnit && maxLaNumber != -1) { maxLaNumber ++; topLevelsSinceLastLa = 0 ; }
            if (config.localLookAhead && (maxLaNumber == -1 || (las < maxLaNumber))) {  // perform LA hack -- only if max. nr is not reached?
                // if(config.opt_printDecisions > 0) cerr << "c run LA (lev=" << decisionLevel() << ", untilLA=" << untilLa << endl;
                int hl = decisionLevel();
                if (hl == 0) if (--untilLa == 0) { laStart = true; DOUT(if (config.localLookaheadDebug)cerr << "c startLA" << endl;); }
                if (laStart && hl == config.opt_laLevel) {
                    if (!laHack(tmp_Lits)) {
                        returnValue = l_False;
                        return lit_Error;
                    }
                    topLevelsSinceLastLa = 0;
                    //          cerr << "c drop decision literal " << next << endl;
                    if (!order_heap.inHeap(var(next))) { order_heap.insert(var(next)); }     // add the literal back to the heap!
                    next = lit_Undef;
                    continue; // after local look-ahead re-check the assumptions
                }
            }
        }
        assert(next != lit_Undef && value(next) == l_Undef && "the literal that is picked exists, and is unassigned");
    }
    assert(next != lit_Undef);

    // Increase decision level and enqueue  the literal we just selected 'next'
    newDecisionLevel();
    DOUT(if (config.opt_printDecisions > 0) printf("c decide %s%d at level %d\n", sign(next) ? "-" : "", var(next) + 1, decisionLevel()););
    uncheckedEnqueue(next);

    return next;
}


int Solver::simplifyLearntLCM(Clause& c, int vivificationConfig)
{
    int roundLits[3];
    int round = 0;
    roundLits[0] = c.size();
    DOUT(vec<Lit> originalClause; for (int i = 0 ; i < c.size(); ++i) originalClause.push(c[i]););
    while (vivificationConfig > 0 && c.size() > 1) {
        bool minimized = false;
        round ++;
        const int roundViviConfig = vivificationConfig % 5;
        vivificationConfig /= 5;

        assert(round <= 2 && "have at most 2 rounds");
        // if this round should not perform any simplification, jump to the next round (and work with reversed clause)
        if (roundViviConfig == 0) {roundLits[round] = c.size(); continue; }

        if (round == 2) { // reverse clause in second iteration!
            c.reverse();
        }
        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM round " << round << " with config " << roundViviConfig << " start with clause: " << c << std::endl;);

        Lit impliedLit = lit_Undef; // literal that is implied by the negation of some other literals
        int i = 0, j = 0, maxLevel = 0;
        ReasonStruct confl(CRef_Undef);

        assert(decisionLevel() == 0 && "LCM has to be performed on level 0");
        const int inputSize = c.size();
        for (i = 0, j = 0; i < c.size(); i++) {
            DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c look into lit " << c[i] << " [" << i << "/" << c.size() << "], wrote " << j << " literals already" << std::endl;);
            if (value(c[i]) == l_Undef) {
                newDecisionLevel();
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM enqueue literal " << ~c[i] << " with trail " << trail << std::endl;);
                uncheckedEnqueue(~c[i]);
                c[j++] = c[i];
                confl = propagate();
                if (confl.isBinaryClause() || confl.getReasonC() != CRef_Undef) {
                    DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM propagation failed with conflict " << ca[confl.getReasonC()] << std::endl;);
                    // F \bigvee \neg l_1 \land ... \land \neg l_{i-1} -> \bot \equiv F \land (l_1 \lor ... \lor l_{i-1})
                    // or something similar, based on conflict analysis with the clause we just got as conflict
                    break;
                }
            } else if (value(c[i]) == l_True) {
                if (roundViviConfig > 2) { // <- enable only in case conflict resultion is performed below. the actual break does not work!
                    impliedLit = c[i]; // store to be able to use it for conflict analysis afterwards
                    c[j++] = c[i];  // not necessary, as will be resolved out later anyways (or use as basis for conflict)?

                    // F \bigvee \neg l_1 \land ... \land \neg l_{i-1} -> \l_i \equiv
                    // F \bigvee \neg l_1 \land ... \land \neg l_{i-1} \land \neg l_i -> \bot, with confl = reason(l_i)
                    confl = reason(var(c[i]));
                    DOUT(
                    if (config.opt_lcm_dbg > 2) {
                    std::cerr << "c LCM with trail " << trail << " find implied literal " << impliedLit << "@" << level(var(impliedLit)) << " at " << decisionLevel() << std::endl;
                        if (confl.isBinaryClause()) { std::cerr << " LCM with reason lit " << confl.getReasonL() << std::endl; } //r
                        else { std::cerr << " with clause [" << confl.getReasonC() << "] " << ca[confl.getReasonC()] << std::endl; }
                    }
                    );
                    assert((confl.isBinaryClause() || confl.getReasonC() != CRef_Undef) && "the assignment to the literal has to have a reason");
                    break;
                } else {
                    DOUT(if (config.opt_lcm_dbg > 2) {
                    std::cerr << "c drop true literals " << c[i] << "@" << level(var(c[i])) << " with reason ";
                        ReasonStruct& r =  reason(var(c[i]));
                        if (r.isBinaryClause()) { std::cerr << " lit " << r.getReasonL() << std::endl; }
                        else {
                            if (r.getReasonC() == CRef_Undef) { std::cerr << " <decision> or <top level unit>" << std::endl; }
                            else { std::cerr << " cls[" << r.getReasonC() << "]: " << ca[r.getReasonC()] << std::endl; }
                        }
                    }
                        );
                    // keep the satisfied literal, because moving it away can fail!
                    c[j++] = c[i];
                    break; // this part until c[j] is subsumed by other a combination of other clauses in the formula (seems to be hyper-resolution to get the subsumed clause)
                }
            } else {
                assert(value(c[i]) == l_False && "there are only 3 case for a truth assignment");
                // F \land (-l1,...,-lk} -> {-li}
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c at level " << decisionLevel() << " drop falsified literal from clause: " << c[i] << "@" << level(var(c[i])) << std::endl;);
            }
        }

        DOUT(if (config.opt_lcm_dbg > 2) { std::cerr << "c remove last " << c.size() - j << " literals from " << c << std::endl; if (j > 0) std::cerr << "c last kept lit: " << c[j - 1] << std::endl; })
        c.shrink(c.size() - j);
        DOUT(for (int m = 0 ; m < c.size(); ++ m) {
        bool found = false;
        for (int n = 0; n < originalClause.size(); ++n) { if (c[m] == originalClause[n]) { found = true; break; } }
            if (!found) { std::cerr << "c shrinked clause " << c << "(" << c.size() << "," << c[m] << ") is not a subset of the original clause any more: " << originalClause << "(" << originalClause.size() << ")" << std::endl; }
            assert(found && "clause should be a subclause of the other, maybe with gaps");
        });
        DOUT(if (config.opt_lcm_dbg && outputsProof()) {
        // test adding and removing the current clause to the proof
        std::cerr << "c test adding and removing UP fail clause " << c << " to/from the proof, with impliedLit " << impliedLit << std::endl;

        if (config.opt_lcm_dbg > 4) {
                for (int z = 0 ; z < trail.size(); ++ z) {
                    ReasonStruct& r =  reason(var(trail[z]));
                    std::cerr << "c trail[" << z << "]: " << trail[z] << "@" << level(var(trail[z])) << " with reason ";
                    if (r.isBinaryClause()) { std::cerr << " lit " << r.getReasonL() << std::endl; }
                    else {
                        if (r.getReasonC() == CRef_Undef) { std::cerr << " <decision> or <top level unit>" << std::endl; }
                        else { std::cerr << " cls[" << r.getReasonC() << "]: " << ca[r.getReasonC()] << std::endl; }
                    }
                }
            }


            addToProof(c);
            addToProof(c, true);
            std::cerr << "c proof test succeeded" << std::endl;
        });
        #if 0
        if (impliedLit == lit_Undef) { c.shrink(c.size() - j); }
        else {
            std::cerr << "c shrink " << c << " with implied lit " << impliedLit << " at position " << j << std::endl;
            c.shrink(c.size() - (j + 1)); // keep literal litImplied!
            std::cerr << "c result after shrinking: " << c << std::endl;
            assert(c.last() == impliedLit && "implied lit has to be kept in case this clause cannot be reduced further");
        }
        #endif

        if (roundViviConfig > 1 && (confl.isBinaryClause() || confl.getReasonC() != CRef_Undef)) {
            conflict.clear(); // use the conflict vector, as it's a class member. make sure to clear it afterwards again!
            DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM run LCM analysis with impliedLit " << impliedLit << std::endl;);

            DOUT(if (config.opt_lcm_dbg > 1 && !confl.isBinaryClause()) std::cerr << "c LCM        conflict clause: " << ca[confl.getReasonC()] << " with trail " << trail << std::endl;);
            analyzeFinal(confl, conflict, impliedLit);
            if (impliedLit != lit_Undef) {
                conflict.push(impliedLit); // add the final literal of the clause here, has to be done after analyzeFinal!
            }
            if (conflict.size() < c.size()) {
                nbConflLits += c.size() - conflict.size();
                assert(c.size() >= conflict.size() && "shrink should not increase size");
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c started with reduced clause " << c << " and now turn it into learned clause " << conflict << std::endl;);
                c.shrink(c.size() - conflict.size()); // shrink clause!
                for (int k = 0; k < c.size(); ++ k) { c[k] = conflict[k]; } // and actually use the literals of conflict
            } else if (false && impliedLit != lit_Undef && c.last() == impliedLit && j == c.size()) { // this is no valid operation, unless we actually did not shrink c above!
                // in case analysis did not result in removing a literal from c, at least drop the impliedLit as in usual vivification
                assert(c[j - 1] == impliedLit && c.size() >= j && "until here, the position of impliedLit should be fixed");
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c started with reduced clause " << c << " and now turn it into learned clause " << conflict << std::endl;);
                c[j - 1] = c.last(); // drop the literal "impliedLit" from the clause
                c.pop();
                npLCMimpDrop ++;
            } else {
                DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c rejected produced conflict " << conflict << " for " << c << std::endl;);
            }
            conflict.clear();

            // analyzeFinal will place the literals in reverse order in c. Make sure we can convert this back
            if (roundViviConfig > 3) { c.reverse(); }
        }

        if (c.size() < inputSize) {
            int nblevels = computeLBD(c, c.size());
            if (nblevels < c.lbd()) {
                c.setLBD(nblevels);
            }
            minimized = true;
        }
        cancelUntil(0);

        if (round == 2) { // reverse clause in second iteration!
            c.reverse();
        }
        DOUT(for (int m = 0 ; m < c.size(); ++ m) {
        bool found = false;
        for (int n = 0; n < originalClause.size(); ++ n) {
                if (c[m] == originalClause[n]) { found = true; break; }
            }
            if (!found) {
                std::cerr << "c shrinked clause " << c << " is not a subset of the original clause any more: " << originalClause << std::endl;
                assert(false && "new clause should be a subset of the original clause");
            }
        }
            );
        DOUT(if (config.opt_lcm_dbg && outputsProof()) {
        // test adding and removing the current clause to the proof
        std::cerr << "c test adding and removing clause " << c << " to/from the proof" << std::endl;
        addToProof(c);
            addToProof(c, true);
            std::cerr << "c proof test succeeded" << std::endl;
        });

        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM round " << round << " final clause: " << c << std::endl;);
        roundLits[round] = c.size();

        if (!minimized) { break; } // no need for a second iteration if no minimization was achieved in the first iteration
    }

    nbRound1Lits += roundLits[0] - roundLits[1];
    if (round > 1) { nbRound2Lits += roundLits[1] - roundLits[2]; }

    return c.lbd();
}

bool Solver::simplifyClause_viviLCM(const Riss::CRef cr, int LCMconfig, bool fullySimplify)
{
    assert(qhead == trail.size() && "make sure, we are working on level 0 and the PROOF is up to date!");
    Clause& c = ca[cr];
    bool keepClause = false;
    bool false_lit = false, sat = false;
    // in the first 2 positions, there should not be a falsified literal after propagation!
    int i = 2, j = 2;
    if (outputsProof()) { add_tmp.clear(); } // keep track of all literals of the clause to have a proper DRAT proof!

    if (c.size() > 2) {
        if (value(c[0]) == l_True || value(c[1]) == l_True) {
            sat = true;
        } else {
            for (i = 2; i < c.size(); i++) {
                if (value(c[i]) == l_True) {
                    sat = true;
                    break;
                }
            }
            if (!sat) {
                if (outputsProof()) for (int k = 0 ; k < c.size(); ++k) { add_tmp.push(c[k]); } // store full clause before any removal
                for (i = 2; i < c.size(); i++) {
                    if (value(c[i]) != l_False) {
                        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "LCM keep literal " << c[i] << std::endl;);
                        c[j++] = c[i]; // TODO FIXME: has to end up in drat proof!
                        if (value(c[i]) == l_True) {
                            sat = true;
                            break;
                        }
                    } else {
                        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "LCM drop literal " << c[i] << std::endl;);
                    }
                }
                nbLCMfalsified += (i - j);
                c.shrink(i - j);
            }
        }
        // a clause might become satisfiable during the analysis. remove such a clause!
        if (sat) {
            removeClause(cr, true); // this will also delete the clause from the DRAT proof!
            return false; // drop the clause!
        }
        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "LCM final clause: " << c << std::endl;);
    } else {
        if (value(c[0]) == l_True || value(c[1]) == l_True) {
            return true; // keep satisfied binary clauses, and do not continue with these clauses
        }
    }

    if (!config.opt_lcm_full && !fullySimplify) { // use efficiency filters?
        // if clause is in first half of sorted learned clauses, or has been processed in the past, ignore it
        return true;
    }

    int oldSize = c.size();
    detachClause(cr, true); // expensive, hence perform as late as possible

    nbLCMattempts ++;
    // simplifying the clause does not change it's memory location, hence c is still valid afterwards
    DOUT(if (config.opt_lcm_dbg > 2) std::cerr << std::endl  << std::endl  << std::endl  << std::endl  << std::endl << "c LCM test   clause index " << cr << " with literals: " << c << " and trail: " << trail << std::endl;);
    int newLBD = simplifyLearntLCM(c, LCMconfig);
    nbLitsLCM += oldSize - c.size();
    nbLCMsuccess = oldSize > c.size() ? nbLCMsuccess + 1 : nbLCMsuccess;
    DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM result clause index " << cr << " with literals: " << c << std::endl << std::endl  << std::endl  << std::endl  << std::endl;);

    if (outputsProof()) { // respect proof
        addCommentToProof("delete clause based on LCM, removed literals might be stronger than DRUP", true);
        if (add_tmp.size() > c.size() && c.size() >= 1) {
            addToProof(c);
            addToProof(add_tmp, true); // for now, keep this removed!
        }
    }

    // add clause back to the data structures
    if (c.size() > 1) {
        attachClause(cr);
        keepClause = true;

        if (c.learnt() && c.size() < oldSize) { // re-calculate LBD for the clause, if it became smaller
            if (newLBD < c.lbd()) {
                c.setLBD(newLBD);
            }
        }

        c.setLcmSimplified();
    } else if (c.size() == 1) {
        uncheckedEnqueue(c[0]); // this clause is already added to the proof
        int breforeTrail = trail.size();
        int beforePropTopLevelLits = trail.size();
        if (propagate() != CRef_Undef) {
            ok = false;
            return false;
        }
        if (decisionLevel() == 0) { handleTopLevelUnits(breforeTrail, beforePropTopLevelLits); }
        // free clause, as it's satisfiable now
        c.mark(1);
        ca.free(cr);
    } else {
        ok = false;
    }
    return keepClause;
}

bool Solver::simplifyLCM()
{
    /* some notes on theory:
     imply a negative literal:

    f, (l1 lm)
    f, -l1, ..., -le -> -lf
    f, -l1, ..., -le, -lg ... -lm -> -lf

    f, -l1, ..., -le, -lg ... -lm, lf -> \bot
    f -> l1, ...,le, -lf, lg, ... lm
    f -> C \setminus lf
    f,C \equiv f, C \setminus lf

    imply a positive literal:

    f, (l1 lm)
    f, -l1, ..., -le -> lf
    f, -l1, ..., -le, -lf -> lf \land -lf -> \bot
    f -> (l1, ..., le, lf)
    f,C \equiv f, (l1, ..., le, lf) -> subsume! == shrink case

    conflict:

    f,D, -l1, ..., -lf -> -D

    f,D \equiv f,D,(l1,...,lf), i.e. f,D \to (l1,...,lf)? (yes, see the above)

    f, -l1, ..., -le, -lf -> \bot
    f -> l1, ...,le, -lf, lg, ... lm
    f -> C \setminus lf
     * */

    assert(qhead == trail.size() && "make sure we are in a good state before LCM");

    // make sure we do not miss something
    if (!ok || propagate() != CRef_Undef) {
        return ok = false;
    }

    MethodClock lcmMethodClock(LCMTime);  // measure the time for the remainder of this function in the given clock

    nbLCM ++;
    assert(decisionLevel() == 0 && "run learned clause minimization only on level 0");
    removeSatisfied(clauses); // TODO: test whether actually necessary when being executed "right after" reduceDB()
    watches.cleanAll();

    int ci, cj;
    for (ci = 0, cj = 0; ci < learnts.size(); ci++) {
        const CRef cr = learnts[ci];
        Clause& c = ca[cr];
        if (c.mark()) { continue; } // this clause can be dropped
        if (c.size() < config.opt_lcm_min_size) {
            // do not look at clauses that have less than the given amount of literals
            learnts[cj++] = learnts[ci];
            continue;
        }

        bool keep = simplifyClause_viviLCM(cr, config.opt_lcm_style, ! c.wasLcmSimplified() && ci >= learnts.size() / 2);  // only run full LCM on new good clauses
        if (keep) { learnts[cj++] = learnts[ci]; }
        if (!ok) { break; } // stop in case we found an empty clause
    }
    // fill gaps unneeded space
    learnts.shrink(ci - cj);


    checkGarbage();
    return ok;
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
    bool blockNextRestart = false;
    starts++;
    int proofTopLevels = 0;
    if (trail_lim.size() == 0) { proofTopLevels = trail.size(); } else { proofTopLevels  = trail_lim[0]; }

    // simplify
    if (config.opt_lcm && (config.opt_lcm_full || (config.opt_lcm_style > 0 && performSimplificationNext > 0 && performSimplificationNext % config.opt_lcm_freq == 0))) {
        // from time to time we have to interfere with partial restarts, but LCM overrules, to be able to run once in a while
        if (decisionLevel() > 0) {
            cancelUntil(0);
        }
        performSimplificationNext = 0;
        sort(learnts, reduceDB_lbd_lt(ca));
        if (!simplifyLCM()) { return l_False; }
        performSimplificationNext = 0;
    }

    for (;;) {
        propagationTime.start();
        const int beforeTrail = trail.size();
        CRef confl = propagate();
        propagationTime.stop();

        if (decisionLevel() == 0) { handleTopLevelUnits(beforeTrail, proofTopLevels); }

        // in case of full LCM debugging, check for lcm in each round, on level 0, whenever there is no conflict
        DOUT(
        if (config.opt_lcm && (config.opt_lcm_full && decisionLevel() == 0 && confl == CRef_Undef && config.opt_lcm_style > 0)) {
        sort(learnts, reduceDB_lbd_lt(ca));
            if (!simplifyLCM()) { return l_False; }
            performSimplificationNext = 0;
        }
        );

        if (confl != CRef_Undef) { // CONFLICT

            conflicts++; conflictC++;
            printConflictTrail(confl);

            updateDecayAndVMTF(); // update dynamic parameters
            printSearchProgress(); // print current progress

            if (decisionLevel() == 0) { // top level conflict - stop!
                return l_False;
            }

            if (earlyAssumptionConflict && decisionLevel() < assumptions.size()) {
                // if used in incremental mode, abort as soon as we found a set of inconsistent assumptions (not only when another assumption cannot be set as decision)
                analyzeFinal(ReasonStruct(confl), conflict);
                return l_False;
            }

            updateBlockRestartAndRemovalHeuristic(blockNextRestart);   // update restart heuristic, decide about blocking restarts next time
            l1conflicts = (decisionLevel() != 1 ? l1conflicts : l1conflicts + 1);

            lbool analysisResult = conflictAnalysis(confl, learnt_clause);  // perform conflict analysis with all its functionality
            if (analysisResult != l_Undef) { return analysisResult; }           // if techniques on learned clauses reveal unsatisfiability of the formula, return this result

            // in case of full lcm debugging, restart after every conflict!
            DOUT(if (config.opt_lcm_full && config.opt_lcm_style > 0) cancelUntil(0););

            varDecayActivity();
            claDecayActivity();

        } else { // there has not been a conflict
            if (handleRestarts(nof_conflicts, conflictC)) { return l_Undef; }    // perform a restart

            // check for new models, continue with propagation if new clauses have been added
            if (enumerationClient.receiveModelBlockingClauses()) { continue; }

            // Handle Simplification Here!
            //
            // Simplify the set of problem clauses - but do not do it each iteration!
            if (decisionLevel() == 0) {
                if (simplifyIterations > config.opt_simplifyInterval && !simplify()) {
                    DOUT(if (config.opt_printDecisions > 0) cerr << "c ran simplify" << endl;);
                    simplifyIterations = 0;
                    return l_False;
                }

                simplifyIterations ++;
                if (processOtfss(otfss)) { return l_False ; }    // make sure we work on the correct clauses still, and we are on level 0 (collected before)
            }

            if (!withinBudget()) { return l_Undef; }   // check whether we can still do conflicts

            lbool receiveResult = receiveInformation();
            if (receiveResult != l_True) {
                cerr << "c interrupt search due to result of receiving information (" << receiveResult << ")" << endl;
                return receiveResult;
            }

            // Perform clause database reduction !
            //
            clauseRemoval(); // check whether learned clauses should be removed

            // Simple Inprocessing (deduction techniques that use the solver object
            //
            // if this point is reached, check whether interleaved Clause Strengthening could be scheduled (have heuristics!)
            if (config.opt_interleavedClauseStrengthening && conflicts != lastICSconflicts && conflicts % config.opt_ics_interval == 0) {
                DOUT(if (config.opt_printDecisions > 0) cerr << "c run ICS" << endl;);
                if (!interleavedClauseStrengthening()) {   // TODO: have some schedule here!
                    return l_False;
                }
            }

            // perform search decision
            lbool searchReturnCode = l_Undef;
            Lit next = performSearchDecision(searchReturnCode, learnt_clause) ;
            if (next == lit_Error) { return searchReturnCode; }               // we should terminate with the given type
            else if (next == lit_Undef) { goto SolverNextSearchIteration; }   // we should continue search without another decision
        }
    SolverNextSearchIteration:;
    }

    assert(false && "this point should not be reached");
    return l_Undef;
}
void Solver::updateBlockRestartAndRemovalHeuristic(bool& blockNextRestart)
{
    if (searchconfiguration.restarts_type == 0) { trailQueue.push(trail.size()); }                   // tell queue about current size of the interpretation before the conflict

    slow_interpretationSizes.update(trail.size());
    // block restart based on the current interpretation size (before conflict analysis and backtracking)
    if (conflicts > config.opt_restart_min_noBlock                     // do not block within the first
            && config.opt_allow_restart_blocking                             // is blocking restarts ok?
            && searchconfiguration.restarts_type == 0                        // block only with dynamic restarts
            && lbdQueue.isvalid()                                            // block only, if we did not already block 'recently'
       ) {
        if ((!config.opt_restarts_dyn_ema && trail.size() > searchconfiguration.R * trailQueue.getavg())         // glucose like: compare to value of trailQueue
                || (config.opt_restarts_dyn_ema && trail.size() > searchconfiguration.R * slow_interpretationSizes.getValue())      // EMA: compare to value of slowly evolving trail size EMA
           ) {
            lbdQueue.fastclear();
            nbstopsrestarts++;
            if (!blockNextRestart) {lastblockatrestart = starts; nbstopsrestartssame++; blockNextRestart = true;}
        }
    }

    conflictsSinceLastRestart ++;
    // update values for removal heuristic
    if (config.opt_reduceType == 1) {
        if (--learntsize_adjust_cnt == 0) {
            learntsize_adjust_confl *= learntsize_adjust_inc;
            learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
            max_learnts             *= learntsize_inc;
        }
    }
}
void Solver::EnumerationClient::assignClientID()
{
    assert(master != 0 && "master has to exist to receive an ID");
    clientID = master->assignClientID();
}

void Solver::EnumerationClient::enableBTbasedEnumeration()
{
    projectionType = BACKTRACKING;
}

bool Solver::EnumerationClient::isBTenumerating() const
{
    return master != 0 && projectionType == BACKTRACKING;
}

/** receive blocking clauses from enumeration master and add them to the formula of the current solver
 * Note: if the decision level has been changed, then clauses have been added lower than the decision level the solver has been working on
 @return false, if nothing has to be changed during search, true, if propoagation should be called next (instead of doing a decision)
 */
bool Solver::EnumerationClient::receiveModelBlockingClauses()
{
    if (master == nullptr) { return false; }  // nothing to be done
    if (solver->decisions < lastReceiveDecisions + receiveEveryDecisions) { return false; }
    if (!master->hasMoreModels(blockedModels)) { return false; }  // nothing to be received by master

    // todo: make sure we do not receive the same blocking clause multiple times (the one of our own model)

    // set number of decisions when receiving models
    lastReceiveDecisions = solver->decisions;
    int oldDecisionLevel = solver->decisionLevel();
    while (master->hasMoreModels(blockedModels)) {
        // models we have seen is the number we found plus the number of personally found models
        if (!master->reveiveModel(clientID, blockedModels, solver->model, blockingClause)) {
            blockedModels ++; // saw one more model - or at least asked for it
            continue;
        }
        blockedModels ++; // saw one more model to block it

        if (minimizeReceived != NONE) {
            // allowed to reduce the blocked clause?
            solver->initReverseMinimitaion(); // initialize reverse minimization
            // dummies for minimization
            unsigned int lbd = 0; unsigned dependencyLevel = 0;

            blockingClause.copyTo(minimizedClause);
            if (minimizeReceived == ALSOFROMBLOCKED) {
                solver->reverseLearntClause(minimizedClause, lbd, dependencyLevel, true);
            }
            blockingClause.clear();
            for (int i = 0 ; i < solver->model.size(); ++ i) {
                blockingClause.push(mkLit(i, solver->model[i] == l_False));
            }
            solver->reverseLearntClause(blockingClause, lbd, dependencyLevel, true);
            // use the shorter clause!
            if (minimizedClause.size() < blockingClause.size()) { minimizedClause.copyTo(blockingClause); }
        }

        // integrate the clause, with no previous knowledge
        int maxLevel = -1, max2Level = -1;
        integrateClause(blockingClause, maxLevel, max2Level);
    }

    // if the decision level has been changed, then clauses have been added lower than the decision level the solver has been working on
    return oldDecisionLevel != solver->decisionLevel();
}


Solver::EnumerationClient::EnumerateState
Solver::EnumerationClient::processCurrentModel(Lit& nextDecision)
{
    if (master == 0) { return oneModel; }  // we do not perform enumeration

    assert((!master->isShared() || projectionType == NAIVE) && "currently we do not support sophisticated enumeration in the parallel setting");

    // allowed to reduce the blocked clause?
    if (mtype != EnumerationClient::MinimizeType::NONE) { solver->initReverseMinimitaion(); }  // initialize reverse minimization

    int maxLevel = 0, max2Level = 0;
    int modelSize = solver->trail.size();
    // create blocking clause if allowed
    if (! master->usesProjection())  {
        // negate all decision variables
        createDecisionClause(blockingClause, maxLevel, max2Level);
        // the best (shortest) blocking clause is now stored in the vector blocking clause
    } else {
        // count all models by adding the negated decision clause
        // negate all variables from the projection
        createBlockingProjectionClause(blockingClause, maxLevel, max2Level);
        modelSize = blockingClause.size();
    }

    // dummies for minimization
    unsigned int lbd = 0; unsigned dependencyLevel = 0;

    // minimize blocking clause
    if (mtype == EnumerationClient::MinimizeType::ALSOFROMBLOCKED) {  // minimized blocking clause we just created
        blockingClause.copyTo(minimizedClause);
        solver->reverseLearntClause(minimizedClause, lbd, dependencyLevel, true);
        if (minimizedClause.size() < blockingClause.size()) {
            minimizedClause.swap(blockingClause);   // if the new clause is better, keep the new clause as blocking clause, in minimizedClause we store the current projection (if we use projection)
            // TODO collect statistics
            maxLevel = -1; max2Level = -1;
        }
    }

    bool integrateBlockingClause = true; // usually, the blocking clause should be added to the solver (except for backtracking-enumeration with projection in some cases)
    // if projection is not active, we are furthermore allowed to try to come up with a shorter blocking clause starting from the full model
    if (! master->usesProjection()) {
        if (mtype == EnumerationClient::MinimizeType::ONLYFROMFULL || mtype == EnumerationClient::MinimizeType::ALSOFROMBLOCKED) {
            solver->trail.copyTo(minimizedClause);
            for (int i = 0 ; i < minimizedClause.size(); ++ i) { minimizedClause[i] = ~minimizedClause[i]; }  // first, create a full blocking clause
            solver->reverseLearntClause(minimizedClause, lbd, dependencyLevel, true);
            if (minimizedClause.size() < blockingClause.size()) {
                minimizedClause.moveTo(blockingClause);   // if the new clause is better, keep the new clause as blocking clause
                // TODO collect statistics
                maxLevel = -1; max2Level = -1;
            }
        }
        // tell master about model, blocking clause, and the full model
        uint64_t modelhash = convertModel(solver->nVars(), solver->trail, solver->model);   // convert in client, such that the master must not perform the conversion in the critical section!
        bool newModel = master->addModel(clientID, solver->model, modelhash, & blockingClause, &solver->trail);
        if (newModel) { foundModels ++; }
        else { duplicateModels ++; }

        if (projectionType == BACKTRACKING) { solver->enableDPLL(); }

        assert(integrateBlockingClause && "communication with master does not change search space of client");
    } else {
        // model minimization is independent of enumeration type (TODO: analyze whether minimization works under projection at all ... )
        if (modelSize == blockingClause.size()) {  // we did not (successfully) minimize the projection clause, hence, the projection model was not moved to minimized clause
            blockingClause.copyTo(minimizedClause);
        }
        // the model has the opposite values as the blocking clause
        for (int i = 0 ; i < minimizedClause.size(); ++i) { minimizedClause[i] = ~ minimizedClause[i]; }
        uint64_t modelhash = convertModel(solver->nVars(), minimizedClause, solver->model);   // convert in client, such that the master must not perform the conversion in the critical section!
        bool newModel = master->addModel(clientID, solver->model, modelhash, & blockingClause, &solver->trail);    // tell master about model under projection, blocking clause, and the ful model
        if (newModel) { foundModels ++; }
        else { duplicateModels ++; }
        if (projectionType == BACKTRACKING) {  // use advanced enumeration with projection

            int maxProjectionLevel = 0;
            for (int i = 0 ; i < master->projectionSize(); ++ i) {
                const Var v = master->projectionVariable(i);
                maxProjectionLevel = maxProjectionLevel >= solver->level(v) ? maxProjectionLevel : solver->level(v);
            }

            if (maxProjectionLevel == 0) {  // if we are at level 0, we are done and just have to print the last solution (paper: algorithm 2, line 23)
                integrateBlockingClause = false;  // do not add this solution
                master->notifyReachedAllModels(); // tell master we found all solutions
            } else {

                if (maxProjectionLevel == projectionBacktrackingLevel) {  // highest decision level of projection variables in this solution are at the current projection backtracking level (paper: algorithm 2, line 24 -- 29)
                    const CRef cr = projectionReasonClauses[projectionBacktrackingLevel];   // blocking clause for the current backtracking level
                    solver->removeClause(cr, true);           // strictly remove the clause from the watch lists, and mark it to be deleted
                    const Lit blLit = projectionDecisionStack[projectionBacktrackingLevel]; // backtracking decision literal of the current level
                    integrateBlockingClause = false;             // do not add the current solution as it will be disallowed by the naive backtracking anyways
                    projectionBacktrackingLevel --;              // decrease backtracking level
                    solver->cancelUntil(projectionBacktrackingLevel);    // jump back to the backtracking level
                    solver->uncheckedEnqueue(~blLit);                    // add complement of last decision on this (now lower) level -> naive backtracking
                } else {
                    projectionBacktrackingLevel ++;
                    // TODO continue implementation here!
                    assert(false && "add missing functionality");
                }
            }
        }
    }

    assert((integrateBlockingClause || master->usesProjection()) && "we only changed the search space if we use projection");

    // add blocking clause to this solver without disturbing its search too much
    if (!solver->useNaiveBacktracking) {
        if (integrateBlockingClause) {
            CRef moreModelsPossible = integrateClause(blockingClause, maxLevel, max2Level);
            if (moreModelsPossible == CRef_Error) {
                cerr << "c stop after client found all models" << endl;
                master->notifyReachedAllModels();
                return stop;
            }
        }
    } else {
        if (master->usesProjection()) {
            // not yet implemented - apply naive backtracking in a way that the current model (under projection) cannot be found again during search
            assert(false && "not implemented yet");
            cerr << "c WARNING: cannot (yet) use projection-enumeration and naive backtracking together - abort" << endl;
            exit(1);
        } else {
            // simply perform backtracking
            solver->dpllBacktracking();
        }
    }

    bool enoughModels = master->foundEnoughModels();
    return enoughModels ? stop : goOn;
}

Solver::EnumerationClient::EnumerationClient(Solver* _solver)
    :   clientID(-1)
    , master(nullptr)
    , solver(_solver)
    , blockedModels(0)
    , lastReceiveDecisions(0)
    , projectionType(NAIVE)
    , projectionBacktrackingLevel(0)
    , mtype(ALSOFROMBLOCKED)
    , minimizeReceived(ALSOFROMBLOCKED)
    , foundModels(0)
    , duplicateModels(0)
    , receiveEveryDecisions(512)
{
}

uint64_t Solver::EnumerationClient::convertModel(uint32_t nVars, vec< Lit >& trail, vec< lbool >& model)
{
    // check whether model is already present
    uint64_t currentHash = 0;
    const int maxVar = trail.size() < nVars ? nVars : trail.size();
    model.clear();
    model.growTo(maxVar, l_Undef);
    for (int i = 0 ; i < trail.size(); ++ i) {
        currentHash += sign(trail[i]) ? ((uint64_t)var(trail[i])) << 32ull : var(trail[i]); // separate positive and negative literals, sum everything up
        model[var(trail[i])] = sign(trail[i]) ? l_False : l_True;        // add this model
    }
    return currentHash;
}


uint64_t Solver::EnumerationClient::getModels() const
{
    return foundModels;
}

uint64_t Solver::EnumerationClient::getDupModels() const
{
    return duplicateModels;
}


bool Solver::EnumerationClient::enoughModels(lbool searchstatus) const
{
    // true, if there is no master, or if there is a master and all models have been found
    if (searchstatus == l_True) {
        return master == 0 || master->foundEnoughModels();
    } else if (searchstatus == l_Undef) {
        if (master == 0) { return false; }   // we did not find a model yet (in sequential search)
        else {
            return master->isShared() && master->foundEnoughModels();
        }
    }
    // should actually never reach here, as searchstatus should not be l_False
    // when calling this method. But to make the compiler happy:
    return true;
}


Solver::EnumerationClient::~EnumerationClient()
{

    // delete master only in sequential setup
    if (master != 0 && ! master->isShared()) {
        delete master;
        master = 0;
    }
}

CRef Solver::EnumerationClient::integrateClause(vec< Lit >& clause, int maxLevel, int max2Level)
{
    if (maxLevel < 0 || max2Level < 0) {
        maxLevel = 0; max2Level = 0;
        for (int i = 0 ; i < clause.size(); ++ i) {
            int varLevel = solver->level(var(clause[i]));
            if (varLevel > maxLevel) { max2Level = maxLevel; maxLevel = varLevel; }
            else if (varLevel > max2Level) { max2Level = varLevel; }
        }
    }
    // modify clause so that the first two literals are l_Undef (after backjumping)
    int tmp = 0 ;
    for (int i = 0 ; i < clause.size(); ++ i) {
        if (solver->level(var(clause[i])) >= max2Level) {
            Lit tmpL = clause[i];
            clause[i] = clause[tmp];
            clause[tmp] = tmpL;
            tmp ++;
        }
    }
    bool succesful = true;
    // can clause be used for unit propagation on some level?
    if (max2Level > 1 && clause.size() > 2) {
        // create clause
        CRef cr = solver->ca.alloc(clause, false);
        solver->clauses.push(cr);
        solver->cancelUntil(max2Level - 1);
        assert(solver->value(clause[0]) == l_Undef && solver->value(clause[1]) == l_Undef && "first two literals have to be undefined");
        solver->attachClause(cr);
        return cr;
    } else { // restart and add clause (should be unary/conflict on level 0)
        solver->cancelUntil(0); // todo: can we get rid of the restart?
        succesful = solver->addClause_(clause);   // might have problems with the current model here
    }
    return succesful ? CRef_Undef : CRef_Error;
}


void Solver::setEnumnerationMaster(EnumerateMaster* master)
{
    enumerationClient.setMaster(master);

    enumerationClient.assignClientID();

    switch (master->minimizeBlocked()) {
    case 0: enumerationClient.mtype = EnumerationClient::NONE ; break;
    case 1: enumerationClient.mtype = EnumerationClient::ONLYFROMFULL ; break;
    case 2: enumerationClient.mtype = EnumerationClient::ALSOFROMBLOCKED ; break;
    default:
        assert(false && "this case is not handled here");
        break;
    }
    switch (master->minimizeReceived()) {
    case 0: enumerationClient.minimizeReceived = EnumerationClient::NONE ; break;
    case 1: enumerationClient.minimizeReceived = EnumerationClient::ONLYFROMFULL ; break;
    case 2: enumerationClient.minimizeReceived = EnumerationClient::ALSOFROMBLOCKED ; break;
    default:
        assert(false && "this case is not handled here");
        break;
    }
    enumerationClient.receiveEveryDecisions = master->checkNewModelsEvery();

    // use enumeration backtracking instead of naive clause blocking
    if (master->usesBacktrackingEnumeration()) {
        enumerationClient.enableBTbasedEnumeration();
    }
}


void Solver::initReverseMinimitaion()
{
    if (! reverseMinimization.enabled) {  // enable reverseMinimization to be able to use it
        reverseMinimization.enabled = true;
        reverseMinimization.assigns.growTo(nVars() + 1, l_Undef); // grow assignment
        reverseMinimization.trail.capacity(nVars()  + 1);       // allocate trail
    }
}

void Solver::EnumerationClient::createDecisionClause(vec< Lit >& clause, int& maxLevel, int& max2Level)
{
    clause.clear();
    assert(maxLevel == 0 && max2Level == 0 && "levels should be initialized correctly");
    for (int i = 0 ; i < solver->trail_lim.size(); ++ i) {
        const Lit l = ~ solver->trail[ solver->trail_lim[i] ];
        const int varLevel = solver->level(var(l));
        if (varLevel > maxLevel) { max2Level = maxLevel; maxLevel = varLevel; }
        else if (varLevel > max2Level) { max2Level = varLevel; }
        clause.push(l);
    }
}

void Solver::EnumerationClient::createBlockingProjectionClause(vec< Lit >& clause, int& maxLevel, int& max2Level)
{
    clause.clear();
    assert(maxLevel == 0 && max2Level == 0 && "levels should be initialized correctly");
    assert(master != nullptr && "can use this only if master is present");
    for (int i = 0 ; i < master->projectionSize(); ++ i) {
        if (solver->value(master->projectionVariable(i)) == l_Undef) { continue; }
        const Lit l = mkLit(master->projectionVariable(i), solver->value(master->projectionVariable(i)) == l_True ? true : false);
        const int varLevel = solver->level(var(l));
        if (varLevel > maxLevel) { max2Level = maxLevel; maxLevel = varLevel; }
        else if (varLevel > max2Level) { max2Level = varLevel; }
        clause.push(l);
    }
}


void Solver::clauseRemoval()
{
    if ((config.opt_reduceType == 1 && (learnts.size() - nAssigns() >= max_learnts))                                 // minisat style removal
            || (config.opt_reduceType == 0 && (conflicts >= curRestart * nbclausesbeforereduce && learnts.size() > 0))    // glucose style removal
            || (config.opt_reduceType == 2 && learnts.size() >= max_learnts)
       ) { // perform only if learnt clauses are present
        curRestart = config.opt_reduceType == 0 ? (conflicts / nbclausesbeforereduce) + 1 : curRestart; // update only during dynamic restarts
        reduceDB();
        nbclausesbeforereduce = config.opt_reduceType == 0 ? nbclausesbeforereduce + searchconfiguration.incReduceDB : nbclausesbeforereduce; // update only during dynamic restarts

        DOUT(if (config.opt_lcm_dbg > 2) std::cerr << "c LCM allow LCM once" << std::endl;);
        if (config.opt_lcm) { performSimplificationNext ++; } // after clause reduction, performing one more analysis of clauses is ok
    }
}


bool Solver::handleRestarts(int& nof_conflicts, const int conflictC)
{
    if (useNaiveBacktracking) { return false; }  // if we run DPLL style search, we should not perform restarts, as we would redo the whole search

    // handle restart heuristic switching first
    if (restartSwitchSchedule.heuristicSwitching()) {  // heuristic switching is enabled
        if (restartSwitchSchedule.reachedIntervalLimit(conflicts)) {    // we reached the limit
            // did we finish the whole intervale (currently using static schedules)
            if (restartSwitchSchedule.finishedFullInterval()) {
                // setup new interval
                restartSwitchSchedule.setupNextFullInterval(conflicts, config.opt_rswitch_interval_inc, config.opt_dynamic_rtype_ratio);
                searchconfiguration.restarts_type = 0; // trigger dynamic restarts
                searchconfiguration.var_decay = 0.95;  // use var decay value of 0.95
                searchconfiguration.var_decay_end = 0.95;  // use var decay value of 0.95
            } else {
                // the rest of the interval is reserved for a static schedule
                restartSwitchSchedule.setupSecondIntervalHalf();
                searchconfiguration.restarts_type = config.opt_alternative_rtype; // activate alternative restart type
                searchconfiguration.var_decay = 0.999;  // use var decay value of 0.999
            }
        }
    }

    // we should never perform restarts due to the selected strategy
    if (config.opt_restarts_type == 4) { return false; }

    // next decide about restart
    {
        // dynamic glucose restarts
        if (searchconfiguration.restarts_type == 0) {
            // Our dynamic restart, see the SAT09 competition compagnion paper
            if (
                (lbdQueue.isvalid()
                 && ((!config.opt_restarts_dyn_ema && (lbdQueue.getavg()     * searchconfiguration.K) > (sumLBD / (conflicts > 0 ? conflicts : 1)))  // use glucose structures
                     || (config.opt_restarts_dyn_ema && (recent_LBD.getValue() * searchconfiguration.K > slow_LBDs.getValue()))                        // or use EMA of LBDs
                    )
                )
                || (config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound) // if there have been too many conflicts
            ) {

                // increase current limit, if this has been the reason for the restart!!
                if ((config.opt_rMax != -1 && conflictsSinceLastRestart >= currentRestartIntervalBound)) {
                    intervalRestart++; conflictsSinceLastRestart = (double)conflictsSinceLastRestart * (double)config.opt_rMaxInc;
                }
                // do counter implication before partial restart
                if (cir_bump_ratio != 0) {
                    counterImplicationRestart();
                }
                conflictsSinceLastRestart = 0;
                lbdQueue.fastclear();
                progress_estimate = progressEstimate();
                int partialLevel = 0;
                if (config.opt_restart_level != 0) {
                    partialLevel = getRestartLevel();
                    if (partialLevel == -1) {
                        if (verbosity > 0) {
                            static bool didIt = false;
                            if (!didIt) { cerr << "c prevent search from restarting while we have SAT" << endl; didIt = false;}
                        }
                        return false; // we found that we should not restart, because we have a (partial) model
                    }
                }
                // do not jump beyond assumptions, as those will never change
                if (config.opt_assumprestart) { partialLevel = partialLevel < assumptions.size() ? assumptions.size() : partialLevel; }
                cancelUntil(partialLevel);
                return true;

            }
        } else { // static restarts (luby, geometric, constant)
            if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()) {
                // do counter implication restart before partial restart
                if (cir_bump_ratio != 0) {
                    counterImplicationRestart();
                }

                {
                    progress_estimate = progressEstimate();
                    int partialLevel = 0;
                    if (config.opt_restart_level != 0) {
                        partialLevel = getRestartLevel();
                        if (partialLevel == -1) {
                            if (verbosity > 0) {
                                static bool didIt = false;
                                if (!didIt) { cerr << "c prevent search from restarting while we have SAT" << endl; didIt = false;}
                            }
                            return false; // we found that we should not restart, because we have a (partial) model
                        }
                    }
                    // do not jump beyond assumptions, as those will never change
                    if (config.opt_assumprestart) { partialLevel = partialLevel < assumptions.size() ? assumptions.size() : partialLevel; }
                    cancelUntil(partialLevel);
                    return true;
                }
            }
        }
    }
    return false;
}


bool Solver::analyzeNewLearnedClause(const CRef& newLearnedClause)
{
    if (config.opt_uhdProbe == 0) { return false; }

    if (decisionLevel() == 0) { return false; }   // no need to analyze the clause, if its satisfied on the top level already!

    const Clause& clause = ca[ newLearnedClause ];

    MethodClock mc(bigBackboneTime);

    if (clause.size() == 2) {   // perform the probing algorithm based on a binary clause
        const Lit* aList = big->getArray(clause[0]);
        const Lit* bList = big->getArray(clause[1]);
        const int aSize = big->getSize(clause[0]) + 1;
        const int bSize = big->getSize(clause[1]) + 1;

        for (int j = 0 ; j < aSize; ++ j) {
            const Lit aLit = j == 0 ? clause[0] : aList[ j - 1];
            for (int k = 0; k < ((config.opt_uhdProbe > 1 || j == 0) ? bSize : 1); ++ k) {     // even more expensive method
                const Lit bLit = k == 0 ? clause[1] : bList[ k - 1];
                // a -> aLit -> bLit, and b -> bLit ; thus F \land (a \lor b) -> bLit, and bLit is a backbone!

                if ((big->getStart(aLit) < big->getStart(bLit) && big->getStop(bLit) < big->getStop(aLit))
                        // a -> aLit, b -> bLit, -bLit -> -aLit = aLit -> bLit -> F -> bLit
                        || (big->getStart(~bLit) < big->getStart(~aLit) && big->getStop(~aLit) < big->getStop(~bLit))) {
                    if (decisionLevel() != 0) { cancelUntil(0); }   // go to level 0, because a unit is added next
                    if (value(bLit) == l_Undef) {     // only if not set already
                        if (j == 0 || k == 0) { L2units ++; } else { L3units ++; } // stats
                        DOUT(if (config.opt_learn_debug) cerr << "c uhdPR bin(b) enqueue " << bLit << "@" << decisionLevel() << endl;);
                        uncheckedEnqueue(bLit);
                        addCommentToProof("added by uhd probing:"); addUnitToProof(bLit); // not sure whether DRUP can always find this
                    } else if (value(bLit) == l_False) {
                        DOUT(if (config.opt_learn_debug) cerr << "c contradiction on literal bin(b) " << bLit << "@" << decisionLevel() << " when checking clause " << clause << endl;);
                        return true; // found a contradiction on level 0! on higher decision levels this is not known!
                    }
                } else {
                    if ((big->getStart(bLit) < big->getStart(aLit) && big->getStop(aLit) < big->getStop(bLit))
                            || (big->getStart(~aLit) < big->getStart(~bLit) && big->getStop(~bLit) < big->getStop(~aLit))) {
                        if (decisionLevel() != 0) { cancelUntil(0); }   // go to level 0, because a unit is added next
                        if (value(aLit) == l_Undef) {     // only if not set already
                            if (j == 0 || k == 0) { L2units ++; } else { L3units ++; } // stats
                            DOUT(if (config.opt_learn_debug) cerr << "c uhdPR bin(a) enqueue " << aLit << "@" << decisionLevel() << endl;);
                            uncheckedEnqueue(aLit);
                            addCommentToProof("added by uhd probing:"); addUnitToProof(aLit);
                        } else if (value(aLit) == l_False) {
                            DOUT(if (config.opt_learn_debug) cerr << "c contradiction on literal bin(a) " << aLit << "@" << decisionLevel() << " when checking clause " << clause << endl;);
                            return true; // found a contradiction
                        }
                    }
                }

            }
        }
    } else if (clause.size() > 2 && clause.size() <= config.opt_uhdProbe) {
        bool oneDoesNotImply = false;
        for (int j = 0 ; j < clause.size(); ++ j) {
            if (big->getSize(clause[j]) == 0) { oneDoesNotImply = true; break; }
        }
        if (!oneDoesNotImply) {
            analyzePosition.assign(clause.size(), 0);   // initialize position of all big lists for the literals in the clause
            analyzeLimits.assign(clause.size(), 0);
            bool oneDoesNotImply = false;
            for (int j = 0 ; j < clause.size(); ++ j) {
                analyzeLimits[j] = big->getSize(clause[j]);   // initialize current imply test lits
                sort(big->getArray(clause[j]), big->getSize(clause[j]));     // sort all arrays (might be expensive)
            }

            bool allInLimit = true;

            // this implementation does not cover the case that all literals of a clause except one imply this one literal!
            int whileIteration = 0;
            while (allInLimit) {
                whileIteration ++;
                // find minimum literal
                bool allEqual = true;
                Lit minLit = big->getArray(clause[0])[ analyzePosition[0] ];
                int minPosition = 0;

                for (int j = 1 ; j < clause.size(); ++ j) {
                    if (big->getArray(clause[j])[ analyzePosition[j] ] < minLit) {
                        minLit = big->getArray(clause[j])[ analyzePosition[j] ];
                        minPosition = j;
                    }
                    if (big->getArray(clause[j])[ analyzePosition[j] ] != big->getArray(clause[j - 1])[ analyzePosition[j - 1] ]) { allEqual = false; }
                }

                if (allEqual) {   // there is a commonly implied literal
                    if (decisionLevel() != 0) { cancelUntil(0); }   // go to level 0, because a unit clause in added next
                    if (value(minLit) == l_Undef) {
                        L4units ++;
                        DOUT(if (config.opt_learn_debug) cerr << "c uhdPR long enqueue " << minLit << "@" << decisionLevel() << endl;);
                        uncheckedEnqueue(minLit);
                    } else if (value(minLit) == l_False) {
                        DOUT(if (config.opt_learn_debug) cerr << "c contradiction on commonly implied liteal " << minLit << "@" << decisionLevel() << " when checking clause " << clause << endl;);
                        return true;
                    }
                    for (int j = 0 ; j < clause.size(); ++ j) {
                        analyzePosition[j] ++;
                        if (analyzePosition[j] >= analyzeLimits[j]) { allInLimit = false; }   // stop if we dropped out of the list of implied literals!
                    }
                } else { // only move the literal of the minimum!
                    analyzePosition[minPosition] ++;
                    if (analyzePosition[minPosition] >= analyzeLimits[minPosition]) { allInLimit = false; }   // stop if we dropped out of the list of implied literals!
                }
            }

        }
    }
    return false;
}



void Solver::fillLAmodel(vec<LONG_INT>& pattern, const int steps, vec<Var>& relevantVariables, const bool moveOnly)   // for negative, add bit patter 10, for positive, add bit pattern 01!
{
    if (!moveOnly) {   // move and add pattern
        int keepVariables = 0;  // number of variables that are kept
        for (int i = 0 ; i < relevantVariables.size(); ++ i) {
            if (value(relevantVariables[i]) != l_Undef) {     // only if the variable is kept, move and add!
                relevantVariables[ keepVariables++ ] = relevantVariables[i];
                const Var& v = relevantVariables[i];    // the current kept variable
                const Lit l = mkLit(v, value(v) == l_False);   // the actual literal on the trail
                pattern[v] = (pattern[v] << (2 * steps));   // move the variables according to the number of failed propagations
                pattern[v] += (sign(l) ? 2 : 1); // add the pattern for the kept variable
            }
        }
        relevantVariables.shrink_(relevantVariables.size() - keepVariables);   // remove the variables that are not needed any more
    } else { // only move all the relevant variables
        for (int i = 0 ; i < relevantVariables.size(); ++ i) {
            const Var& v = relevantVariables[i];    // the current kept variable
            pattern[v] = (pattern[v] << (2 * steps));   // move the variables according to the number of failed propagations
        }
    }
}

bool Solver::laHack(vec<Lit>& toEnqueue)
{
    assert(decisionLevel() == config.opt_laLevel && "can perform LA only, if level is correct");
    laTime = cpuTime() - laTime;

    const LONG_INT hit[]   = {5, 10,  85, 170, 21845, 43690, 1431655765, 2863311530,  6148914691236517205, 12297829382473034410ull}; // compare numbers for lifted literals
    const LONG_INT hitEE0[] = {9, 6, 153, 102, 39321, 26214, 2576980377, 1717986918, 11068046444225730969ull, 7378697629483820646}; // compare numbers for l == dec[0] or l != dec[0]
    const LONG_INT hitEE1[] = {0, 0, 165, 90, 42405, 23130, 2779096485, 1515870810, 11936128518282651045ull, 6510615555426900570}; // compare numbers for l == dec[1]
    const LONG_INT hitEE2[] = {0, 0,   0,  0, 43605, 21930, 2857740885, 1437226410, 12273903644374837845ull, 6172840429334713770}; // compare numbers for l == dec[2]
    const LONG_INT hitEE3[] = {0, 0,   0,  0,     0,    0, 2863289685, 1431677610, 12297735558912431445ull, 6149008514797120170}; // compare numbers for l == dec[3]
    const LONG_INT hitEE4[] = {0, 0,   0,  0,     0,    0,          0,         0, 12297829381041378645ull, 6148914692668172970}; // compare numbers for l == dec[4]
    // FIXME have another line for level 6 here!

//  if(config.localLookaheadDebug) cerr << "c initial pattern: " << pt << endl;
    Lit d[6];
    int j = 0;
    for (int i = 0; i < config.opt_laLevel; ++i) {
        if ((i == 0 || trail_lim[i] != trail_lim[i - 1]) && trail_lim[i] < trail.size()) { // no dummy level caused by assumptions ...
            d[j++] = trail[ trail_lim[i] ];    // get all decisions into dec array
        }
    }
    const int actualLAlevel = j;

    DOUT(if (config.localLookaheadDebug) {
    cerr << "c LA decisions: ";
    for (int i = 0 ; i < actualLAlevel; ++ i) { cerr << " " << d[i] << " (vs. [" << trail_lim[i] << "] " << trail[ trail_lim[i] ] << " ) "; }
        cerr << endl;
    });

    if (config.tb) { // use tabu
        bool same = true;
        for (int i = 0 ; i < actualLAlevel; ++i) {
            for (int j = 0 ; j < actualLAlevel; ++j)
                if (var(d[i]) != var(hstry[j])) { same = false; }
        }
        if (same) { laTime = cpuTime() - laTime; return true; }
        for (int i = 0 ; i < actualLAlevel; ++i) { hstry[i] = d[i]; }
        for (int i = actualLAlevel; i < config.opt_laLevel; ++i) { hstry[i] = lit_Undef; }
    }
    las++;

    int bound = 1 << actualLAlevel, failedTries = 0;

    variablesPattern.growTo(nVars()); // have a pattern for all variables
    memset(&(variablesPattern[0]), 0, nVars()*sizeof(LONG_INT)); // initialized to 0
    varFlags.copyTo(backupSolverState);   // backup the current solver state
    LONG_INT patternMask = ~0; // current pattern mask, everything set -> == 2^64-1

    relevantLAvariables.clear();  // the current set of relevant variables is empty
    int start = 0; // first literal that has been used during propagation
    if (trail_lim.size() > 0) { start = trail_lim[0]; }
    // collect all the variables that are put on the trail here
    for (; start < trail.size(); ++start) { relevantLAvariables.push(var(trail[start])); }     // only these variables are relevant for LA, because more cannot be in the intersection
    start = 0;


    DOUT(if (config.localLookaheadDebug) cerr << "c do LA until " << bound << " starting at level " << decisionLevel() << endl;);
    fillLAmodel(variablesPattern, 0, relevantLAvariables); // fill current model
    int failedProbesInARow = 0;
    for (LONG_INT i = 1; i < bound; ++i) { // for each combination
        cancelUntil(0);
        newDecisionLevel();
        for (int j = 0; j < actualLAlevel; ++j) { uncheckedEnqueue((i & (1 << j)) != 0 ? ~d[j] : d[j]); }   // flip polarity of d[j], if corresponding bit in i is set -> enumerate all combinations!
        bool f = propagate() != CRef_Undef; // for tests!
//    if(config.localLookaheadDebug) { cerr << "c propagated iteration " << i << " : " ;  for(int j=0;j<actualLAlevel;++j) cerr << " " << ( (i&(1<<j))!=0 ? ~d[j] : d[j] ) ; cerr << endl; }
        DOUT(if (config.localLookaheadDebug) { cerr << "c corresponding trail: "; if (f) { cerr << " FAILED! "; } else  for (int j = trail_lim[0]; j < trail.size(); ++ j) { cerr << " "  << trail[j]; } cerr << endl; });

        if (f) {
            LONG_INT m = 3;
            patternMask = (patternMask & (~(m << (2 * i))));
            failedTries ++; // global counter
            failedProbesInARow ++; // local counter
        } else {
            fillLAmodel(variablesPattern, failedProbesInARow + 1, relevantLAvariables);
            failedProbesInARow = 0;
        }
//    if(config.localLookaheadDebug) cerr << "c this propafation [" << i << "] failed: " << f << " current match pattern: " << pt << "(inv: " << ~pt << ")" << endl;
        DOUT(if (config.localLookaheadDebug) { cerr << "c cut: "; for (int j = 0; j < 2 << actualLAlevel; ++j) { cerr << ((patternMask & (1 << j))  != (LONG_INT)0); } cerr << endl; });
    }
    if (failedProbesInARow > 0) { fillLAmodel(variablesPattern, failedProbesInARow, relevantLAvariables, true); }   // finally, moving all variables right
    cancelUntil(0);

    // for(int i=0;i<opt_laLevel;++i) cerr << "c value for literal[" << i << "] : " << d[i] << " : "<< p[ var(d[i]) ] << endl;

    int t = 2 * actualLAlevel - 2;
    // evaluate result of LA!
    bool foundUnit = false;
//  if(config.localLookaheadDebug) cerr << "c pos hit: " << (pt & (hit[t])) << endl;
//  if(config.localLookaheadDebug) cerr << "c neg hit: " << (pt & (hit[t+1])) << endl;
    toEnqueue.clear();
    bool doEE = ((failedTries * 100) / bound) < config.opt_laEEp;   // enough EE candidates?!
    DOUT(if (config.localLookaheadDebug) cerr << "c tries: " << bound << " failed: " << failedTries << " percent: " << ((failedTries * 100) / bound) << " doEE: " << doEE << " current laEEs: " << laEEvars << endl;);
    for (int variableIndex = 0 ; variableIndex < relevantLAvariables.size(); ++ variableIndex) { // loop only over the relevant variables
        const Var& v = relevantLAvariables[variableIndex];
        if (value(v) == l_Undef) { // l_Undef == 2
            if ((patternMask & variablesPattern[v]) == (patternMask & (hit[t]))) {
                foundUnit = true; toEnqueue.push(mkLit(v, false)); laAssignments++;
                // cerr << "c LA enqueue " << mkLit(v,false) << " (pat=" << hit[t] << ")" << endl;
            } // pos consequence
            else if ((patternMask & variablesPattern[v]) == (patternMask & (hit[t + 1]))) {
                foundUnit = true; toEnqueue.push(mkLit(v, true)); laAssignments++;
                // cerr << "c LA enqueue " << mkLit(v,true) << " (pat=" << hit[t] << ")" << endl;
            } // neg consequence
            else if (doEE) {
                analyze_stack.clear(); // get a new set of literals!
                if (var(d[0]) != v) {
                    if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE0[t]))) { analyze_stack.push(~d[0]); }
                    else if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE0[t + 1]))) { analyze_stack.push(d[0]); }
                }
                if (var(d[1]) != v && hitEE1[t] != 0) {   // only if the field is valid!
                    if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE1[t]))) { analyze_stack.push(~d[1]); }
                    else if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE1[t + 1]))) { analyze_stack.push(d[1]); }
                }
                if (var(d[2]) != v && hitEE2[t] != 0) {   // only if the field is valid!
                    if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE2[t]))) { analyze_stack.push(~d[2]); }
                    else if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE2[t + 1]))) { analyze_stack.push(d[2]); }
                }
                if (var(d[3]) != v && hitEE3[t] != 0) {   // only if the field is valid!
                    if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE3[t]))) { analyze_stack.push(~d[3]); }
                    else if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE3[t + 1]))) { analyze_stack.push(d[3]); }
                }
                if (var(d[4]) != v && hitEE4[t] != 0) {   // only if the field is valid!
                    if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE4[t]))) { analyze_stack.push(~d[4]); }
                    else if ((patternMask & variablesPattern[v]) == (patternMask & (hitEE4[t + 1]))) { analyze_stack.push(d[4]); }
                }
                if (analyze_stack.size() > 0) {
                    analyze_toclear.clear();
                    analyze_toclear.push(lit_Undef); analyze_toclear.push(lit_Undef);
                    laEEvars++;
                    laEElits += analyze_stack.size();
                    for (int i = 0; i < analyze_stack.size(); ++ i) {
                        DOUT(
                        if (config.localLookaheadDebug) {
                        cerr << "c EE [" << i << "]: " << mkLit(v, false) << " <= " << analyze_stack[i] << ", " << mkLit(v, true) << " <= " << ~analyze_stack[i] << endl;
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

                        for (int pol = 0; pol < 2; ++ pol) {   // encode a->b, and b->a
                            analyze_toclear[0] = pol == 0 ? ~analyze_stack[i]  : analyze_stack[i];
                            analyze_toclear[1] = pol == 0 ?     mkLit(v, false) :    mkLit(v, true);
                            CRef cr = ca.alloc(analyze_toclear, config.opt_laEEl);  // create a learned clause?
                            if (config.opt_laEEl) {
                                ca[cr].setLBD(2);
                                learnts.push(cr);
                                claBumpActivity(ca[cr], (config.opt_cls_act_bump_mode == 0 ? 1 : (config.opt_cls_act_bump_mode == 1) ? analyze_toclear.size() : 2));    // bump activity based on its size); }
                                if (config.opt_cls_act_bump_mode != 2) {
                                    claBumpActivity(ca[cr],                                                         // bump activity based on its
                                                    (config.opt_cls_act_bump_mode == 0 ? 1                          // constant
                                                     : (config.opt_cls_act_bump_mode == 1) ? 2  // size
                                                     : 1              // LBD
                                                    ));
                                } else {
                                    ca[cr].activity() = 2 < config.opt_size_bounded_randomized ?       // if clause size is less than SBR
                                                        2                                              // use size as activity
                                                        : config.opt_size_bounded_randomized + drand(random_seed);   // otherwise, use SBR
                                }
                            } else { clauses.push(cr); }
                            attachClause(cr);
                            DOUT(if (config.localLookaheadDebug) cerr << "c add as clause: " << ca[cr] << endl;);
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
    for (int i = 0 ; i < toEnqueue.size(); ++ i) { uncheckedEnqueue(toEnqueue[i]); }

    // TODO: apply schema to have all learned unit clauses in DRUP! -> have an extra vector!
    if (outputsProof()) {

        if (actualLAlevel != 5) {
            static bool didIT = false;
            if (! didIT) {
                cerr << "c WARNING: DRUP proof can produced only for la level 5!" << endl;
                didIT = true;
            }
        }

        // construct look-ahead clauses
        for (int i = 0 ; i < toEnqueue.size(); ++ i) {
            // cerr << "c write literal " << i << " from LA " << las << endl;
            const Lit l = toEnqueue[i];
            localLookAheadProofClauses.clear();
            localLookAheadTmpClause.clear();

            const int litList[] = {0, 1, 2, 3, 4, 5, -1, 0, 1, 2, 3, 5, -1, 0, 1, 2, 4, 5, -1, 0, 1, 2, 5, -1, 0, 1, 3, 4, 5, -1, 0, 1, 3, 5, -1, 0, 1, 4, 5, -1, 0, 1, 5, -1, 0, 2, 3, 4, 5, -1, 0, 2, 3, 5, -1, 0, 2, 4, 5, -1, 0, 2, 5, -1, 0, 3, 4, 5, -1, 0, 3, 5, -1, 0, 4, 5, -1, 0, 5, -1, 1, 2, 3, 4, 5, -1, 1, 2, 3, 5, -1, 1, 2, 4, 5, -1, 1, 2, 5, -1, 1, 3, 4, 5, -1, 1, 3, 5, -1, 1, 4, 5, -1, 1, 5, -1, 2, 3, 4, 5, -1, 2, 3, 5, -1, 2, 4, 5, -1, 2, 5, -1, 3, 4, 5, -1, 3, 5, -1, 4, 5, -1, 5, -1};
            int cCount = 0 ;
            localLookAheadTmpClause.clear();
            assert(actualLAlevel == 5 && "current proof generation only works for level 5!");
            for (int j = 0; true; ++ j) {   // TODO: count literals!
                int k = litList[j];
                if (k == -1) {
                    localLookAheadProofClauses.push_back(localLookAheadTmpClause);
                    // cerr << "c write " << tmp << endl;
                    localLookAheadTmpClause.clear();
                    cCount ++;
                    if (cCount == 32) { break; }
                    continue;
                }
                if (k == 5) { localLookAheadTmpClause.push_back(l); }
                else { localLookAheadTmpClause.push_back(d[k]); }
            }

            // write all clauses to proof -- including the learned unit
            addCommentToProof("added by lookahead");
            for (int j = 0; j < localLookAheadProofClauses.size() ; ++ j) {
                if (0) { cerr << "c write clause [" << j << "] " << localLookAheadProofClauses[ j ] << endl; }
                addToProof(localLookAheadProofClauses[j]);
            }
            // delete all redundant clauses
            addCommentToProof("removed redundant lookahead clauses", true);
            for (int j = 0; j + 1 < localLookAheadProofClauses.size() ; ++ j) {
                assert(localLookAheadProofClauses[j].size() > 1 && "the only unit clause in the list should not be removed!");
                addToProof(localLookAheadProofClauses[j], true);
            }
        }

    }

    toEnqueue.clear();

    if (propagate() != CRef_Undef) {laTime = cpuTime() - laTime; return false;}

    // done with la, continue usual search, until next time something is done
    for (int i = 0 ; i < backupSolverState.size(); ++ i) { varFlags[i].polarity = backupSolverState[i].polarity; }

    if (config.opt_laDyn) {
        if (foundUnit) { laBound = config.opt_laEvery; }    // reset down to specified minimum
        else {if (laBound < config.opt_laMaxEvery) { laBound++; }} // increase by one!
    }
    laStart = false; untilLa = laBound; // reset counter
    laTime = cpuTime() - laTime;

    if (!foundUnit) { failedLAs++; }
    maxBound = maxBound > laBound ? maxBound : laBound;
    return true;
}


double Solver::progressEstimate() const
{
    double  progress = 0;
    double  F = 1.0 / nVars();

    for (int i = 0; i <= decisionLevel(); i++) {
        int beg = i == 0 ? 0 : trail_lim[i - 1];
        int end = i == decisionLevel() ? trail.size() : trail_lim[i];
        progress += pow(F, i) * (end - beg);
    }

    return progress / nVars();
}

/** to create the luby series */
static double luby(double y, int x)
{

    // Find the finite subsequence that contains index 'x', and the
    // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x + 1; seq++, size = 2 * size + 1);

    while (size - 1 != x) {
        size = (size - 1) >> 1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

lbool Solver::initSolve(int solves)
{
    bool changedActivities = false; // indicate whether the decision heap has to be rebuild
    // reset the counters that guide the search (and stats)
    if (config.opt_reset_counters != 0 && solves % config.opt_reset_counters == 0) {
        nbRemovedClauses = 0; nbReducedClauses = 0;
        nbDL2 = 0; nbBin = 0; nbUn = 0; nbReduceDB = 0;
        starts = 0; decisions = 0; rnd_decisions = 0;
        propagations = 0; conflicts = 0; nbstopsrestarts = 0;
        nbstopsrestartssame = 0; lastblockatrestart = 0;
        las = 0; failedLAs = 0; maxBound = 0; maxLaNumber = config.opt_laBound;
        topLevelsSinceLastLa = 0; untilLa = config.opt_laEvery;

        curr_restarts = 0; // reset restarts
        restartSwitchSchedule.lubyRestarts = 0;
        restartSwitchSchedule.geometricRestarts = 0;
        restartSwitchSchedule.constantRestarts = 0;
        restartSwitchSchedule.resetInterval();

        // reset restart heuristic information for dynamic restarts
        slow_interpretationSizes.reset();
        slow_LBDs.reset();
        recent_LBD.reset();
        lbdQueue.fastclear();

        configScheduler.reset(searchconfiguration);
    }

    // initialize activities and polarities
    if (config.opt_init_act != 0 || config.opt_init_pol != 0) {
        if (solves == 1
                || (config.resetActEvery != 0 && solves % config.resetActEvery == 0)
                || (config.resetPolEvery != 0 && solves % config.resetPolEvery == 0)
           ) {
            double* jw = new double [nVars()]; // todo: could be done in one array with a struct!
            int32_t* moms = new int32_t [nVars()];
            memset(jw, 0, sizeof(double) * nVars());
            memset(moms, 0, sizeof(int32_t) * nVars());

            for (int i = 0 ; i < clauses.size(); ++ i) {
                const Clause& c = ca[clauses[i]];
                const double cs = 1 / (pow(2.0, c.size()));
                for (int j = 0 ; j < c.size(); ++ j) {
                    jw[ var(c[j]) ] = (sign(c[j]) ? jw[ var(c[j]) ]  - cs : jw[ var(c[j]) ] + cs);
                    moms[ var(c[j]) ] = (sign(c[j]) ? moms[ var(c[j]) ]  - 1 : moms[ var(c[j]) ] + 1);
                }
            }
            // set initialization based on calculated values
            for (Var v = 0 ; v < nVars(); ++ v) {
                if (solves == 1 || (config.resetActEvery != 0 && solves % config.resetActEvery == 0)) {
                    if (config.opt_init_act == 1) { activity[v] = v; }
                    else if (config.opt_init_act == 2) { activity[v] = pow(1.0 / searchconfiguration.var_decay_start, 2 * v / nVars()); }
                    else if (config.opt_init_act == 3) { activity[nVars() - v - 1] = v; }
                    else if (config.opt_init_act == 4) { activity[nVars() - v - 1] = pow(1.0 / searchconfiguration.var_decay_start, 2 * v / nVars()); }
                    else if (config.opt_init_act == 5) { activity[v] = drand(random_seed); }
                    else if (config.opt_init_act == 6) { activity[v] = jw[v] > 0 ? jw[v] : -jw[v]; }
                    changedActivities = true;
                }

                if (solves == 1 || (config.resetPolEvery != 0 && solves % config.resetPolEvery == 0)) {
                    if (config.opt_init_pol == 1) { varFlags[v].polarity = jw[v] > 0 ? 0 : 1; }
                    else if (config.opt_init_pol == 2) { varFlags[v].polarity = jw[v] > 0 ? 1 : 0; }
                    else if (config.opt_init_pol == 3) { varFlags[v].polarity = moms[v] > 0 ? 1 : 0; }
                    else if (config.opt_init_pol == 4) { varFlags[v].polarity = moms[v] > 0 ? 0 : 1; }
                    else if (config.opt_init_pol == 5) { varFlags[v].polarity = irand(random_seed, 100) > 50 ? 1 : 0; }
                    else if (config.opt_init_pol == 6) { varFlags[v].polarity = ~ varFlags[v].polarity; }
                }
            }
            delete [] moms;
            delete [] jw;
        }
    }


    // parse for variable polarities from file!
    if (solves == 1 && config.polFile) {   // read polarities from file, initialize phase polarities with this value!
        string filename = string(config.polFile);
        Riss::VarFileParser vfp(filename);
        vector<int> polLits;
        vfp.extract(polLits);
        for (int i = 0 ; i < polLits.size(); ++ i) {
            const Var v = polLits[i] > 0 ? polLits[i] : - polLits[i];
            if (v - 1 >= nVars()) { continue; }   // other file might contain more variables
            Lit thisL = mkLit(v - 1, polLits[i] < 0);
            if (config.opt_pol) { thisL = ~thisL; }
            varFlags[v - 1].polarity = sign(thisL);
        }
        cerr << "c adopted poarity of " << polLits.size() << " variables" << endl;
    }


    // parse for activities from file!
    if (solves == 1 && config.actFile) {   // set initial activities
        string filename = string(config.actFile);
        Riss::VarFileParser vfp(filename);
        vector<int> actVars;
        vfp.extract(actVars);

        double thisValue = config.opt_actStart;
        // reverse the order
        if (config.opt_act == 2 || config.opt_act == 3) for (int i = 0 ; i < actVars.size() / 2; ++ i) { int tmp = actVars[i]; actVars[i] = actVars[ actVars.size() - i - 1 ]; actVars[ actVars.size() - i - 1 ] = tmp; }
        for (int i = 0 ; i < actVars.size(); ++ i) {
            const Var v = actVars[i] - 1;
            if (v >= nVars()) { continue; }   // other file might contain more variables
            activity[ v] = thisValue;
            thisValue = ((config.opt_act == 0 || config.opt_act == 2) ? thisValue - config.pot_actDec : thisValue / config.pot_actDec);
        }
        cerr << "c adopted activity of " << actVars.size() << " variables" << endl;
        changedActivities = true;
    }

    if (changedActivities) { rebuildOrderHeap(); }



    //
    // incremental sat solver calls
    //
    if (config.intenseCleaningEvery != 0 && solves % config.intenseCleaningEvery == 0) {   // clean the learnt clause data base intensively
        int i = 0, j = 0;
        for (; i < learnts.size(); ++ i) {
            Clause& c = ca[ learnts[i] ];
            if (c.size() > config.incKeepSize || c.lbd() > config.incKeepLBD && !locked(c)) {   // remove clauses with too large size or lbd
                removeClause(learnts[i]);
            } else {
                learnts[j++] = learnts[i]; // move clause forward!
            }
        }
        learnts.shrink_(i - j);
    }

    return l_Undef;
}

void Solver::applyConfiguration()
{
    lbdQueue.clear();
    assert(searchconfiguration.sizeLBDQueue > 0 && "thera have to be some elements (at least one)");
    lbdQueue.initSize(searchconfiguration.sizeLBDQueue);

    trailQueue.clear();
    assert(searchconfiguration.sizeTrailQueue > 0 && "thera have to be some elements (at least one)");
    trailQueue.initSize(searchconfiguration.sizeTrailQueue);

    if (config.opt_reduceType == 1) {  // minisat style removal?
        max_learnts = nClauses() * learntsize_factor;
        if (max_learnts < config.opt_max_learnts) {
            max_learnts = config.opt_max_learnts;
        }

        learntsize_adjust_confl   = learntsize_adjust_start_confl;
        learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
    }

    nbclausesbeforereduce = searchconfiguration.firstReduceDB;
    sumLBD = 0;
}

void Solver::dumpAndExit(const char* filename, bool doExit, bool fullState)
{
    FILE* f = fopen(filename, "w");
    if (f == nullptr) {
        fprintf(stderr, "could not open file %s\n", filename), exit(1);
    }
    fprintf(f, "c CNF dumped by Riss\n");

    if (!okay()) {     // unsat
        fprintf(f, "p cnf 0 1\n0\n"); // print the empty clause
        fclose(f);
        return;
    }

    // count level 0 assignments
    int level0 = 0;
    for (int i = 0; i < trail.size(); ++i) {
        if (level(var(trail[i])) == 0) {
            ++level0;
        } else { break; }
    }
    // print header, if activated
    if (fullState) {
        int nCls = trail_lim.size() == 0 ? 0 : trail_lim[0];
        for (int i = 0; i < clauses.size(); ++i) {
            nCls = ca[ clauses[i] ].mark() ? nCls : nCls + 1;
        }
        for (int i = 0; i < learnts.size(); ++i) {
            nCls = ca[ learnts[i] ].mark() ? nCls : nCls + 1;
        }
        fprintf(f, "p cnf %u %i\n", (nVars()), nCls);
    } else { fprintf(f, "p cnf %u %i\n", (nVars()), level0 + clauses.size()); }

    // print assignments
    int relevantTrailLits = fullState ? (trail_lim.size() == 0 ? 0 : trail_lim[0]) : trail.size();
    for (int i = 0; i < relevantTrailLits; ++i) {
        if (level(var(trail[i])) == 0) {
            stringstream s;
            s << trail[i];
            fprintf(f, "%s 0\n", s.str().c_str());
        } else { break; } // stop after first level
    }
    // print clauses
    if (fullState) {
        for (int p = 0 ; p < 2; ++p) {
            vec<CRef>& clauseList = p == 0 ? clauses : learnts;
            for (int i = 0; i < clauseList.size(); ++i) {
                if (ca[clauseList[i]].mark()) { continue; }
                stringstream s;
                s << ca[ clauseList[i] ];
                fprintf(f, "%s 0\n", s.str().c_str());
            }
        }
    } else {
        for (int i = 0; i < clauses.size(); ++i) {
            stringstream s;
            s << ca[ clauses[i] ];
            fprintf(f, "%s 0\n", s.str().c_str());
        }
    }
    fclose(f);
    if (doExit) { exit(1); }
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_(const SolveCallType preprocessCall)
{
    lbool   status        = l_Undef;

    if (preprocessCall != afterSimplification) {

        // print formula of the call?
        if ((const char*)config.printOnSolveTo != 0) {
            dumpAndExit((const char*)config.printOnSolveTo);
        }
        totalTime.start();
        startedSolving = true;
        model.clear();
        conflict.clear();
        if (!ok) { return l_False; }
        applyConfiguration();
        solves++;

        lbool initValue = initSolve(solves);
        if (initValue != l_Undef)  { return initValue; }

        printHeader();

        if (preprocessCall == initializeOnly) { return status; }

        // preprocess
        if (status == l_Undef) {   // TODO: freeze variables of assumptions!
            status = preprocess();
            if (config.ppOnly || preprocessCall == simplificationOnly) { return status; }  // stop also if preprocessing should be done only
        }

    }

    if (verbosity >= 1) {
        printf("c | solve clauses: %12d  solve variables: %12d                                            |\n", nClauses(), nVars());
        printf("c =========================================================================================================\n");
    }

    // probing during search, or UHLE for learnt clauses
    if (config.opt_uhdProbe > 0 || (searchconfiguration.uhle_minimizing_size > 0 && searchconfiguration.uhle_minimizing_lbd > 0)) {
        if (big == 0) { big = new Coprocessor::BIG(); }   // if there is no big yet, create it!
        big->recreate(ca, nVars(), clauses, learnts);
        big->removeDuplicateEdges(nVars());
        big->generateImplied(nVars(), add_tmp);
        if (config.opt_uhdProbe > 2) { big->sort(nVars()); }     // sort all the lists once
    }

    DOUT(if (config.opt_learn_debug) {
    cerr << "c solver state after preprocessing" << endl;
    cerr << "c start solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts decision vars: " << order_heap.size() << endl;
        cerr << "c lits until level " << decisionLevel() << ": " ; for (int i = 0 ; i < trail.size(); ++ i) { cerr << " " << trail[i]; } cerr << endl;
        cerr << "c clauses: " << endl; for (int i = 0 ; i < clauses.size(); ++ i) { cerr << "c [" << clauses[i] << "]m: " << ca[clauses[i]].mark() << " == " << ca[clauses[i]] << endl; }
        cerr << "c assumptions: "; for (int i = 0 ; i < assumptions.size(); ++ i) { cerr << " " << assumptions[i]; } cerr << endl;
        cerr << "c solve with " << config.presetConfig() << endl;
        cerr << "c current decision level: " << decisionLevel() << endl;
    });

    //
    // Search:
    //
    curr_restarts = 0;
    lastReshuffleRestart = 0;

    // substitueDisjunctions
    // for now, works only if there are no assumptions!
    int solveVariables = nVars();
    int currentSDassumptions = 0;


    printSearchHeader();

    rerInitRewriteInfo();


    //if (verbosity >= 1) printf("c start solving with %d assumptions\n", assumptions.size() );
    while (status == l_Undef) {

        if (configScheduler.checkAndChangeSearchConfig(conflicts, searchconfiguration)) { applyConfiguration(); }  // if a new configuratoin was selected, update structures

        double rest_base = 0; // initially 0, as we want to use glucose restarts
        if (searchconfiguration.restarts_type != 0) { // set current restart limit -- the value is multiplied with the parameter "opt_restart_first" below
            rest_base = searchconfiguration.restarts_type == 1 ? luby(config.opt_restart_inc, restartSwitchSchedule.lubyRestarts) :
                        (searchconfiguration.restarts_type == 2 ? pow(config.opt_restart_inc, restartSwitchSchedule.geometricRestarts) : 1);
        }

        // re-shuffle BIG, if a sufficient number of restarts is reached
        if (big != 0 && config.opt_uhdRestartReshuffle > 0 && curr_restarts - lastReshuffleRestart >= config.opt_uhdRestartReshuffle) {
            if (nVars() > big->getVars()) {   // rebuild big, if new variables are present
                big->recreate(ca, nVars(), clauses, learnts);   // build a new BIG that is valid on the "new" formula!
                big->removeDuplicateEdges(nVars());
            }
            big->generateImplied(nVars(), add_tmp);
            if (config.opt_uhdProbe > 2) { big->sort(nVars()); }     // sort all the lists once
            lastReshuffleRestart = curr_restarts; // update number of last restart
        }

        status = search(rest_base * config.opt_restart_first); // the parameter is useless in glucose - but interesting for the other restart policies
        DOUT(if (config.opt_learn_debug || config.opt_printDecisions > 1) cerr << "c search returned with status " << status << endl;);
        if (!withinBudget()) {
            DOUT(if (config.opt_learn_debug || config.opt_printDecisions > 1) cerr << "c interrupt solving due to budget with status" << status << endl;);
            break;
        }
        if (enumerationClient.enoughModels(status)) {  // decide how to proceed based on the current status
            DOUT(if (config.opt_learn_debug || config.opt_printDecisions > 1) cerr << "c interrupt solving due to number of revealed models with status" << status << endl;);
            break; // stop if we found all the models we need
        }

        // increment restart counters based on restart type
        curr_restarts++;
        restartSwitchSchedule.lubyRestarts = searchconfiguration.restarts_type == 1 ? restartSwitchSchedule.lubyRestarts + 1 : restartSwitchSchedule.lubyRestarts;
        restartSwitchSchedule.geometricRestarts = searchconfiguration.restarts_type == 2 ? restartSwitchSchedule.geometricRestarts + 1 : restartSwitchSchedule.geometricRestarts;
        restartSwitchSchedule.constantRestarts = searchconfiguration.restarts_type == 3 ? restartSwitchSchedule.constantRestarts + 1 : restartSwitchSchedule.constantRestarts;

        status = inprocess(status);
    }

    if (status == l_False && config.opt_refineConflict) {
        DOUT(if (config.opt_learn_debug) cerr << "c run refine final conflict" << endl;);
        refineFinalConflict();
    }  // minimize final conflict clause

    totalTime.stop();

    DOUT(if (config.opt_learn_debug) {
    cerr << "c finish solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts and status " << (status == l_Undef ? "UNKNOWN" : (status == l_True ? "SAT" : "UNSAT")) << " and conflict " << conflict << endl;
        if (status == l_False) { cerr << "c conflict clause: " << conflict << endl; }
    });

    //
    // print statistic output
    //
    if (verbosity >= 1) {
        printf("c =========================================================================================================\n");
    }

    if (verbosity >= 1 || config.opt_solve_stats) {
        #if defined TOOLVERSION && TOOLVERSION < 400

        #else
        const double overheadC = totalTime.getCpuTime() - (propagationTime.getCpuTime() + analysisTime.getCpuTime() + extResTime.getCpuTime() + preprocessTime.getCpuTime() + inprocessTime.getCpuTime());
        const double overheadW = totalTime.getWallClockTime() - (propagationTime.getWallClockTime() + analysisTime.getWallClockTime() + extResTime.getWallClockTime() + preprocessTime.getWallClockTime() + inprocessTime.getWallClockTime());
        printf("c Tinimt-Ratios: ratioCpW: %.2lf ,overhead/Total %.2lf %.2lf \n",
               totalTime.getCpuTime() / totalTime.getWallClockTime(), overheadC / totalTime.getCpuTime(), overheadW / totalTime.getWallClockTime());
        printf("c Timing(cpu,wall, in s): total: %.2lf %.2lf ,prop: %.2lf %.2lf ,analysis: %.2lf %.2lf ,ext.Res.: %.2lf %.2lf ,reduce: %.2lf %.2lf ,overhead %.2lf %.2lf\n",
               totalTime.getCpuTime(), totalTime.getWallClockTime(), propagationTime.getCpuTime(), propagationTime.getWallClockTime(), analysisTime.getCpuTime(), analysisTime.getWallClockTime(), extResTime.getCpuTime(), extResTime.getWallClockTime(), reduceDBTime.getCpuTime(), reduceDBTime.getWallClockTime(),
               overheadC, overheadW);
        printf("c PP-Timing(cpu,wall, in s): preprocessing( %d ): %.2lf %.2lf ,inprocessing (%d ): %.2lf %.2lf\n",
               preprocessCalls, preprocessTime.getCpuTime(), preprocessTime.getWallClockTime(), inprocessCalls, inprocessTime.getCpuTime(), inprocessTime.getWallClockTime());
        printf("c Trivial Polarity: True: %d  False: %d\n", (int)posInAllClauses, (int) negInAllClauses);
        printf("c Learnt At Level 1: %d  Multiple: %d Units: %d\n", l1conflicts, multiLearnt, learntUnit);
        printf("c LAs: %lf laSeconds %d LA assigned: %d tabus: %d, failedLas: %d, maxEvery %d, eeV: %d eeL: %d \n", laTime, las, laAssignments, tabus, failedLAs, maxBound, laEEvars, laEElits);
        printf("c otfss: %d (l1: %d ),cls: %d ,units: %d ,binaries: %d, eagerCandidates: %d\n", otfss.otfsss, otfss.otfsssL1, otfss.otfssClss, otfss.otfssUnits, otfss.otfssBinaries, otfss.revealedClause);
        printf("c learning: %ld cls, %lf avg. size, %lf avg. LBD, %ld maxSize\n",
               (int64_t)totalLearnedClauses,
               sumLearnedClauseSize / totalLearnedClauses,
               sumLearnedClauseLBD / totalLearnedClauses,
               (int64_t)maxLearnedClauseSize
              );
        printf("c res.ext.res.: %d rer, %d rerSizeCands, %d sizeReject, %d patternReject, %d bloomReject, %d maxSize, %.2lf avgSize, %.2lf totalLits, %d gates\n",
               rerLearnedClause, rerLearnedSizeCandidates, rerSizeReject, rerPatternReject, rerPatternBloomReject, maxRERclause,
               rerLearnedClause == 0 ? 0 : (totalRERlits / (double) rerLearnedClause), totalRERlits, rerExtractedGates);
        printf("c ER rewrite: %d cls, %d lits\n", erRewriteClauses, erRewriteRemovedLits);
        printf("c i.cls.strengthening: %.2lf seconds, %d calls, %d candidates, %d droppedBefore, %d shrinked, %d shrinkedLits\n", icsTime.getCpuTime(), icsCalls, icsCandidates, icsDroppedCandidates, icsShrinks, icsShrinkedLits);
        printf("c bi-asserting: %ld pre-Mini, %ld post-Mini, %.3lf rel-pre, %.3lf rel-post\n", biAssertingPreCount, biAssertingPostCount,
               totalLearnedClauses == 0 ? 0 : (double) biAssertingPreCount / (double)totalLearnedClauses,
               totalLearnedClauses == 0 ? 0 : (double) biAssertingPostCount / (double)totalLearnedClauses
              );
        printf("c search-UHLE: %d attempts, %d rem-lits\n", searchUHLEs, searchUHLElits);
        printf("c revMin: %d tries %d succesful %d dropped %d cut %d conflicts\n", reverseMinimization.attempts, reverseMinimization.succesfulReverseMinimizations, reverseMinimization.revMindroppedLiterals, reverseMinimization.revMinConflicts, reverseMinimization.revMincutOffLiterals);
        printf("c decisionClauses: %d\n", learnedDecisionClauses);
        printf("c IntervalRestarts: %d\n", intervalRestart);
        printf("c partial restarts: %d saved decisions: %d saved propagations: %d recursives: %d\n", rs_partialRestarts, rs_savedDecisions, rs_savedPropagations, rs_recursiveRefinements);
        printf("c uhd probe: %lf s, %d L2units, %d L3units, %d L4units\n", bigBackboneTime.getCpuTime(), L2units, L3units, L4units);
        printf("c LCM: %lf s, %ld nbLCM, %ld LCMclsAttempts, %ld nbLCMclsSuccess, %ld npConflLCMlits, %ld nbLCMlits, %ld falsified, %ld positiveDrop, %ld litsR1, %ld litsR2\n",
               LCMTime.getCpuTime(), nbLCM, nbLCMattempts, nbLCMsuccess, nbConflLits, nbLitsLCM, nbLCMfalsified, npLCMimpDrop, nbRound1Lits, nbRound2Lits);
        #endif
    }

    if (status == l_True) {
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) { model[i] = value(i); }
        if (model.size() > solveVariables) { model.shrink_(model.size() - solveVariables); }     // if SD has been used, nobody knows about these variables, so remove them before doing anything else next

        if (false) {
            cerr << "c solver state after solving with solution" << endl;
            cerr << "c check clauses: " << endl;
            for (int i = 0 ; i < clauses.size(); ++ i) {
                int j = 0;
                const Clause& c = ca[clauses[i]];
                for (; j < c.size(); j++) {
                    if (model[ var(c[j]) ] == l_True && !sign(c[j])) { break; }
                    else if (model[ var(c[j]) ] == l_False && sign(c[j])) { break; }
                }
                if (j == c.size()) { cerr << "c unsatisfied clause [" << clauses[i] << "] m: " << ca[clauses[i]].mark() << " == " << ca[clauses[i]] << endl; }
            }
        }

        if (coprocessor != nullptr && (useCoprocessorPP || useCoprocessorIP)) {
            coprocessor->extendModel(model);
        }

    } else if (status == l_False && conflict.size() == 0) {
        ok = false;
    } else if (status == l_False) {
        DOUT(if (config.opt_learn_debug) cerr << "c stop search with conflict <" << conflict << ">" << endl;);
    }

    assert((status != l_Undef || !withinBudget() || asynch_interrupt) && "unknown should not happen here if there was no interrupt");

    if (!config.opt_savesearch || config.opt_refineConflict) { cancelUntil(0); }

    // cerr << "c finish solving with " << nVars() << " vars, " << nClauses() << " clauses and " << nLearnts() << " learnts and status " << (status == l_Undef ? "UNKNOWN" : ( status == l_True ? "SAT" : "UNSAT" ) ) << endl;

    return status;
}

void Solver::refineFinalConflict()
{
    if (assumptions.size() == 0 || conflict.size() < 2) { return; }   // nothing to be done
    assumptions.moveTo(refineAssumptions);                        // memorize original assumptions

//      cerr << "c conflict " << conflict << " at level " << decisionLevel() << endl;
//      cerr << "c trail: " << trail << endl;
//      cerr << "c ================================" << endl;
//      cerr << endl << endl << endl;

    cancelUntil(0);    // make sure we are on level 0 again
    assert(decisionLevel() == 0 && "run this routine only after the trail has been cleared already");

    const int conflictSize = conflict.size();

    // Solver::analyzeFinal adds assumptions in reverse order to the conflict clause, hence, add them in this order again
    assumptions.clear();
    for (int i = 0 ; i < conflict.size(); ++ i) { assumptions.push(~conflict[i]); }    // assumptions are reversed now
    // call the search routine once more, now with the modified assumptions
    lbool res = search(INT32_MAX);

//     // for debugging purposes, have a special exit
//     if( conflictSize > conflict.size() + 1 && conflict.size() > 1) exit (42);

//     cerr << "c refined conflict: " << conflict << endl;
//     cerr << "c trail: " << trail << endl;

    // move assumptions back
    refineAssumptions.moveTo(assumptions);

    // literals in conflict clause are reversed now. turn around the vector once more
    if (config.opt_refineConflictReverse) {
        int i = 0, j = conflict.size() - 1;
        while (i < j) {
            Lit tmp = conflict[i];
            conflict[i++] = conflict[j]; // last time i is used in loop, hence increase afterwards
            conflict[j--] = tmp;         // last time j is used in loop, hence increase afterwards
        }
    }
    cancelUntil(0);    // make sure we are on level 0 again
}

//=================================================================================================
// Writing CNF to DIMACS:
//
// FIXME: this needs to be rewritten completely.

static Var mapVar(Var x, vec<Var>& map, Var& max)
{
    if (map.size() <= x || map[x] == -1) {
        map.growTo(x + 1, -1);
        map[x] = max++;
    }
    return map[x];
}


void Solver::toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max)
{
    if (satisfied(c)) { return; }

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False) {
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max) + 1);
        }
    fprintf(f, "0\n");
}


void Solver::toDimacs(const char *file, const vec<Lit>& assumps)
{
    FILE* f = fopen(file, "wr");
    if (f == nullptr) {
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    }
    toDimacs(f, assumps);
    fclose(f);
}


void Solver::toDimacs(FILE* f, const vec<Lit>& assumps)
{
    // Handle case when solver is in contradictory state:
    if (!ok) {
        fprintf(f, "p cnf 1 2\n1 0\n-1 0\n");
        return;
    }

    vec<Var> map; Var max = 0;

    // Cannot use removeClauses here because it is not safe
    // to deallocate them at this point. Could be improved.
    int cnt = 0;
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]])) {
            cnt++;
        }

    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]])) {
            Clause& c = ca[clauses[i]];
            for (int j = 0; j < c.size(); j++)
                if (value(c[j]) != l_False) {
                    mapVar(var(c[j]), map, max);
                }
        }

    // Assumptions are added as unit clauses:
    cnt += assumptions.size();

    fprintf(f, "p cnf %d %d\n", max, cnt);

    for (int i = 0; i < assumptions.size(); i++) {
        assert(value(assumptions[i]) != l_False);
        fprintf(f, "%s%d 0\n", sign(assumptions[i]) ? "-" : "", mapVar(var(assumptions[i]), map, max) + 1);
    }

    for (int i = 0; i < clauses.size(); i++) {
        toDimacs(f, ca[clauses[i]], map, max);
    }

    if (verbosity > 0) {
        printf("Wrote %d clauses with %d variables.\n", cnt, max);
    }
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
        for (int s = 0; s < 2; s++) {
            Lit p = mkLit(v, s);
            // printf(" >>> RELOCING: %s%d\n", sign(p)?"-":"", var(p)+1);
            vec<Watcher>& ws = watches[p];
            for (int j = 0; j < ws.size(); j++) {
                ca.reloc(ws[j].cref(), to);
            }
        }

    // All reasons:
    //
    for (int i = 0; i < trail.size(); i++) {
        Var v = var(trail[i]);
        if (level(v) == 0) { vardata[v].reason = CRef_Undef; }   // take care of reason clauses for literals at top-level
        else if (!reason(v).isBinaryClause()
                 && reason(v).getReasonC() != CRef_Undef
                 && (ca[ reason(v).getReasonC() ].reloced() || locked(ca[reason(v).getReasonC()]))
                ) {
            vardata[v].reason.setReason(ca.relocC(vardata[v].reason.getReasonC(), to));
        }

    }

    // All learnt:
    //
    int keptClauses = 0;
    for (int i = 0; i < learnts.size(); i++) {
        if (!ca[ learnts[i] ].mark()) { // reloc only if not marked already
            ca.reloc(learnts[i], to);
            learnts[keptClauses++] = learnts[i]; // keep the clause only if its not marked!
        }
    }
    learnts.shrink_(learnts.size() - keptClauses);

    // All original:
    //
    keptClauses = 0;
    for (int i = 0; i < clauses.size(); i++) {
        if (!ca[ clauses[i]].mark()) { // reloc only if not marked already
            ca.reloc(clauses[i], to);
            clauses[keptClauses++] = clauses[i]; // keep the clause only if its not marked!
        }
    }
    clauses.shrink_(clauses.size() - keptClauses);
    // handle all clause pointers from OTFSS
    keptClauses = 0;
    for (int i = 0 ; i < otfss.info.size(); ++ i) {
        if (!ca[otfss.info[i].cr].mark()) { // keep only relevant clauses (checks mark() != 0 )
            ca.reloc(otfss.info[i].cr, to);
            otfss.info[keptClauses++] = otfss.info[i]; // keep the clause only if its not marked!
        }
    }
    otfss.info.shrink_(otfss.info.size() - keptClauses);
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

void Solver::buildReduct()
{
    cancelUntil(0);
    int keptClauses = 0;
    uint64_t remLits = 0;
    for (int j = 0; j < clauses.size(); ++ j) {
        int keptLits = 0;
        bool isSat = false;
        Clause& c = ca[ clauses[j] ];
        for (int k = 0 ; k < c.size(); ++ k) {
            if (value(c[k]) == l_True) { isSat = true; break; }
            else if (value(c[k]) != l_False) {
                c[ keptLits ++ ] = c[k];
            } else { remLits ++; } // literal is falsified
        }
        if (!isSat) {
            c.shrink(c.size() - keptLits);
            assert(c.size() != 1 && "propagation should have found this unit already");
            clauses[ keptClauses++ ] = clauses [j];
        }
    }
    DOUT(if (verbosity > 2) cerr << "c removed lits during reduct: " << remLits << " removed cls: " << clauses.size() - keptClauses << endl;);
    clauses.shrink_(clauses.size() - keptClauses);

}

int Solver::getRestartLevel()
{
    if (config.opt_restart_level == 0) { return 0; }
    else {
        // get decision literal that would be decided next:

        if (config.opt_restart_level >= 1) {

            bool repeatReusedTrail = false;
            Var next = var_Undef;
            int restartLevel = 0;
            do {
                repeatReusedTrail = false; // get it right this time?

                // Activity based selection
                while (next == var_Undef || value(next) != l_Undef || ! varFlags[next].decision) // found a yet unassigned variable with the highest activity among the unassigned variables
                    if (order_heap.empty()) {
                        next = var_Undef;
                        break;
                    } else {
                        next = order_heap.removeMin();    // get next element
                    }

                if (next == var_Undef) { return -1; }   // we cannot compare to any other variable, hence, we have SAT already
                // based on variable next, either check for reusedTrail, or matching Trail!
                // activity of the next decision literal
                const double cmpActivity = activity[ next ];
                restartLevel = 0;
                for (int i = 0 ; i < decisionLevel() ; ++i) {
                    if (activity[ var(trail[ trail_lim[i] ]) ] < cmpActivity) {
                        restartLevel = i;
                        break;
                    }
                }
                order_heap.insert(next);   // put the decision literal back, so that it can be used for the next decision

                if (config.opt_restart_level > 1 && restartLevel > 0) {   // check whether jumping higher would be "more correct"
                    cancelUntil(restartLevel);
                    Var more = var_Undef;
                    while (more == var_Undef || value(more) != l_Undef || ! varFlags[more].decision)
                        if (order_heap.empty()) {
                            more = var_Undef;
                            break;
                        } else {
                            more = order_heap.removeMin();
                        }

                    // actually, would have to jump higher than the current level!
                    if (more != var_Undef && activity[more] > var(trail[ trail_lim[ restartLevel - 1 ] ])) {
                        repeatReusedTrail = true;
                        next = more; // no need to insert, and get back afterwards again!
                        rs_recursiveRefinements ++;
                    } else {
                        order_heap.insert(more);
                    }
                }
            } while (repeatReusedTrail);
            // stats
            if (restartLevel > 0) {   // if a partial restart is done
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

void Solver::restrictedExtendedResolutionInitialize(const vec< Lit >& currentLearnedClause)
{
    DOUT(if (config.opt_rer_debug) cerr << "c init RER for " << currentLearnedClause << endl;);
    // init RER
    rerCommonLits.clear(); rerCommonLitsSum = 0;
    for (int i = 1; i < currentLearnedClause.size(); ++ i) {
        rerCommonLits.push(currentLearnedClause[i]);
        rerCommonLitsSum += toInt(currentLearnedClause[i]);
    }
    rerLits.push(currentLearnedClause[0]);
    sort(rerCommonLits);   // TODO: have insertionsort/mergesort here!
}

void Solver::rerInitRewriteInfo()
{
    if (!config.opt_rer_extractGates) { return; }

    bool changedActivities = false; // remember whether some activities have been modified
    for (int i = 0; i < clauses.size(); ++ i) {
        const Clause& c = ca[clauses[i]];
        if (c.size() != 3) { continue; }

        // check literal as output
        char hit[3];
        for (int j = 0 ; j < 3; ++ j) {
            const Lit o = c[j]; // clause [o, x, y], clauses to match: [-o,-x] and [-o,-y]
            hit[0] = 0;  hit[1] = 0;  hit[2] = 0; // init hit array
            hit[j] = 1;

            // check binary clauses in watch list
            const vec<Watcher>&  wbin  = watches[o];
            for (int k = 0; k < wbin.size(); k++) {
                if (!wbin[k].isBinary()) { continue; }
                const Lit& imp = wbin[k].blocker(); // (o -> imp) => clause [-o, imp]
                if (~imp == c[0]) { hit[0] = 1; }  // could have else here. TODO: what is better for branch prediction?
                if (~imp == c[1]) { hit[1] = 1; }
                if (~imp == c[2]) { hit[2] = 1; }
            }
            if (hit[0] && hit[1] && hit[2]) {  // all literals have been hit
                DOUT(if (config.opt_rer_debug) cerr << "c found gate with output " << o << " and clause " << c << endl;);
                rerExtractedGates ++;
                Lit l1, l2;
                int k = 0;
                for (; k < 3; ++ k) {
                    if (c[k] == o) { continue; }
                    DOUT(if (config.opt_rer_debug) cerr << "c select l1 with k=" << k << " to " << c[k] << endl;);
                    l1 = c[k++]; break;
                }
                for (; k < 3; ++ k) {
                    if (c[k] == o) { continue; }
                    DOUT(if (config.opt_rer_debug)  cerr << "c select l2 with k=" << k << " to " << c[k] << endl;);
                    l2 = c[k]; break;
                }
                if (l1 > l2) { Lit tmp = l1; l1 = l2; l2 = tmp; }  // l1 is the smaller literal
                assert((toInt(l1) + toInt(l2) + toInt(o) == toInt(c[0]) + toInt(c[1]) + toInt(c[2])) && "sums have to be the same");
                erRewriteInfo[ toInt(l1) ].otherMatch = l2;
                erRewriteInfo[ toInt(l1) ].replaceWith = ~o;   // resolve with the given clause results in having the original long clause again

                if (config.opt_rer_addInputAct != 0) {
                    activity[ var(l1) ] += config.opt_rer_addInputAct;
                    activity[ var(l2) ] += config.opt_rer_addInputAct;
                    changedActivities = true;
                }

            }
        }
    }

    if (changedActivities) { rebuildOrderHeap(); }  // make sure that the novel activities are used immediately

}

Solver::rerReturnType Solver::restrictedExtendedResolution(vec< Lit >& currentLearnedClause, unsigned int& lbd, unsigned& dependencyLevel)
{
    if (! config.opt_restrictedExtendedResolution) { return rerUsualProcedure; }
    DOUT(if (config.opt_rer_debug) cerr << "c analyze clause for RER" << endl;);
    if (currentLearnedClause.size() < config.opt_rer_minSize ||
            currentLearnedClause.size() > config.opt_rer_maxSize ||
            lbd < config.opt_rer_minLBD ||
            lbd > config.opt_rer_maxLBD) { return rerUsualProcedure; }
    if ((double)rerLearnedClause * config.opt_rer_every > conflicts) { return rerUsualProcedure; }  // do not consider this clause!

    // passed the size/LBD filters
    if (rerLits.size() == 0) {
        // initialize the structures for RER
        restrictedExtendedResolutionInitialize(currentLearnedClause);
        // rerFuseClauses is updated in search later!
        // cerr << "c init as [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : " << currentLearnedClause << endl;
        return rerMemorizeClause; // tell search method to include new clause into list

    } else {
        DOUT(if (config.opt_rer_debug) cerr << "c compare " << currentLearnedClause.size() << " vs " << 1 + rerCommonLits.size() << endl;);
        if (currentLearnedClause.size() != 1 + rerCommonLits.size()) {
            DOUT(if (config.opt_rer_debug) cerr << "c reject size" << endl;);
            rerSizeReject ++;
            resetRestrictedExtendedResolution();
            if (config.opt_rer_each) { restrictedExtendedResolutionInitialize(currentLearnedClause); return rerMemorizeClause; }    // initialize with the new clause
            else { return rerUsualProcedure; } // clauses in a row do not fit the window
        } else { // size fits, check lits!
            // sort, if more than 2 literals
//       cerr << "current learnt clause before sort: " << currentLearnedClause << endl;
            if (currentLearnedClause.size() > 2) { sort(&(currentLearnedClause[2]), currentLearnedClause.size() - 2); }       // do not touch the second literal in the clause! check it separately!
//       cerr << "current learnt clause after  sort: " << currentLearnedClause << endl;

            bool found = false;
            for (int i = 0 ; i < rerCommonLits.size(); ++ i) {
                if (rerCommonLits[i] == currentLearnedClause[1]) { found = true; break;}
            }
            if (! found) {
                rerReturnType thisReturn = rerUsualProcedure;
                if (config.opt_rer_ite && rerLits.size() == 1) {  // check whether half an ITE pattern can be matched
                    if (restrictedERITE(rerLits[0], rerCommonLits, currentLearnedClause) == rerDontAttachAssertingLit) {   // independent of the return value
                        thisReturn = rerDontAttachAssertingLit; // RER-ITE had success and found a clause that is implied on the decision level
                    }
                }
                resetRestrictedExtendedResolution();
                DOUT(if (config.opt_rer_debug) cerr << "c reject patter" << endl;);
                rerPatternReject ++;

                if (config.opt_rer_each && thisReturn == rerUsualProcedure) { restrictedExtendedResolutionInitialize(currentLearnedClause); return rerMemorizeClause; }    // initialize with the new clause
                else { return thisReturn; }
            }
            DOUT(if (config.opt_rer_debug) cerr << "c found match - check with more details" << endl;);
            // Bloom-Filter
            int64_t thisLitSum = 0;
            for (int i = 0 ; i < currentLearnedClause.size(); ++ i) {
                thisLitSum += toInt(currentLearnedClause[i]);
            }
            if (thisLitSum != rerCommonLitsSum) {
                rerReturnType thisReturn = rerUsualProcedure;
                if (config.opt_rer_ite && rerLits.size() == 1) {  // check whether half an ITE pattern can be matched
                    if (restrictedERITE(rerLits[0], rerCommonLits, currentLearnedClause) == rerDontAttachAssertingLit) {   // independent of the return value
                        thisReturn = rerDontAttachAssertingLit; // RER-ITE had success and found a clause that is implied on the decision level
                    }
                }
                resetRestrictedExtendedResolution();
                rerPatternBloomReject ++;

                if (config.opt_rer_each && thisReturn == rerUsualProcedure) {
                    restrictedExtendedResolutionInitialize(currentLearnedClause);
                    return rerMemorizeClause;
                } // initialize with the new clause
                else { return thisReturn; }
            }
            DOUT(if (config.opt_rer_debug) cerr << "c found match - passed bloom filter" << endl;);

            found = false; // for the other literals pattern
            // check whether all remaining literals are in the clause


            int i = 0; int j = 2;
            while (i < rerCommonLits.size() && j < currentLearnedClause.size()) {
//  cerr << "c compare " << rerCommonLits << " to " << currentLearnedClause[j] << " (or " << currentLearnedClause[1] << ")" << endl;
                if (rerCommonLits[i] == currentLearnedClause[j]) {
                    i++; j++;
                } else if (rerCommonLits[i] == currentLearnedClause[1]) {
                    ++i;
                } else { // literal currentLearnedClause[j] is not in common literals!
                    rerReturnType thisReturn = rerUsualProcedure;
                    if (config.opt_rer_ite && rerLits.size() == 1) {  // check whether half an ITE pattern can be matched
                        if (restrictedERITE(rerLits[0], rerCommonLits, currentLearnedClause) == rerDontAttachAssertingLit) {   // independent of the return value
                            thisReturn = rerDontAttachAssertingLit; // RER-ITE had success and found a clause that is implied on the decision level
                        }
                    }
                    resetRestrictedExtendedResolution();
                    DOUT(if (config.opt_rer_debug) cerr << "c reject patter" << endl;);
                    rerPatternReject ++;

                    if (config.opt_rer_each && thisReturn == rerUsualProcedure) { restrictedExtendedResolutionInitialize(currentLearnedClause); return rerMemorizeClause; }   // initialize with the new clause
                    else { return thisReturn; }
                }

            }
            DOUT(if (config.opt_rer_debug) cerr << "c the two clauses match!" << endl;);
            // clauses match
            rerLits.push(currentLearnedClause[0]);   // store literal

            if (rerLits.size() >= config.opt_rer_windowSize) {
                clearOtfss(otfss);   // avoid collision with otfss, hence, discard all collected otfss info

                // perform RER step
                // add all the RER clauses with the fresh variable (and set up the new variable properly!
                const Var x = newVar(true, true, 'r');
                vardata[x].level = level(var(currentLearnedClause[0]));
                // do not assign a value, because it will be undone anyways!

                // delete the current decision level as well, so that the order of the reason clauses can be set right!
                assert(decisionLevel() > 0 && "can undo a decision only, if it didnt occur at level 0");
                DOUT(if (config.opt_rer_debug) {
                cerr << "c trail: " ;
                for (int i = 0 ; i < trail.size(); ++ i) {
                        cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?";
                        if (!reason(var(trail[i])).isBinaryClause() && reason(var(trail[i])).getReasonC() == CRef_Undef) { cerr << "U"; }
                        else {
                            if (!reason(var(trail[i])).isBinaryClause()) { cerr << reason(var(trail[i])).getReasonC(); }
                            else { cerr << "L" << reason(var(trail[i])).getReasonL(); }
                        }
                    } cerr << endl;
                    cerr << "c trail_lim: " << trail_lim << endl;
                    cerr << "c decision level: " << decisionLevel() << endl;
                    for (int i = 0 ; i < decisionLevel() ; ++i) { cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl; }
                });
                const Lit lastDecisoin = trail [ trail_lim[ decisionLevel() - 1 ] ];
                DOUT(if (config.opt_rer_debug) cerr << "c undo decision level " << decisionLevel() << ", and re-add the related decision literal " << lastDecisoin << endl;);
                rerOverheadTrailLits += trail.size(); // store how many literals have been removed from the trail to set the order right!
                cancelUntil(decisionLevel() - 1);
                DOUT(if (config.opt_rer_debug) {
                if (config.opt_rer_debug) { cerr << "c intermediate decision level " << decisionLevel() << endl; }
                    for (int i = 0 ; i < decisionLevel() ; ++i) { cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl; }
                });
                rerOverheadTrailLits -= trail.size();
                // detach all learned clauses from fused clauses
                for (int i = 0; i < rerFuseClauses.size(); ++ i) {
                    assert(rerFuseClauses[i] != reason(var(ca[rerFuseClauses[i]][0])).getReasonC() && "from a RER-CDCL point of view, these clauses cannot be reason clause");
                    assert(rerFuseClauses[i] != reason(var(ca[rerFuseClauses[i]][1])).getReasonC() && "from a RER-CDCL point of view, these clauses cannot be reason clause");
                    // ca[rerFuseClauses[i]].mark(1); // mark to be deleted!
                    DOUT(if (config.opt_rer_debug) cerr << "c remove clause (" << i << ")[" << rerFuseClauses[i] << "] " << ca[ rerFuseClauses[i] ] << endl;);
                    removeClause(rerFuseClauses[i]); // drop this clause!
                }

                // rewrite the current formula before adding the definition of the new variable!
                if (config.opt_rer_full && !config.opt_rer_as_learned) {
                    // here, the disjunction could also by replaced by ~x in the whole formula, if the window is binary
                    if (config.opt_rer_as_replaceAll > 0 && rerLits.size() == 2) { disjunctionReplace(~rerLits[0], ~rerLits[1], mkLit(x, true), (config.opt_rer_as_replaceAll > 1), false); }   // if there would be a binary clause, this case would not happen
                }

                // we do not need a reason here, the new learned clause will do!
                oc.clear(); oc.push(mkLit(x, true)); oc.push(lit_Undef);
                for (int i = 0 ; i < rerLits.size(); ++ i) {
                    oc[1] = rerLits[i];
                    assert(!hasDuplicates(oc) && "no duplicate literals!");
                    CRef icr = ca.alloc(oc, config.opt_rer_as_learned); // add clause as non-learned clause
                    ca[icr].setLBD(1); // all literals are from the same level!
                    DOUT(if (config.opt_rer_debug) cerr << "c add clause [" << icr << "]" << ca[icr] << endl;);
                    nbDL2++; nbBin ++; // stats
                    if (config.opt_rer_as_learned) {  // add clause
                        learnts.push(icr);
                    } else { clauses.push(icr); }
                    attachClause(icr); // all literals should be unassigned
                }
                if (config.opt_rer_full) {  // also add the other clause?
                    oc.clear(); oc.push(mkLit(x, false));
                    for (int i = 0; i < rerLits.size(); ++i) { oc.push(~rerLits[i]); }
                    // assert( !hasComplementary(rerLits) && !hasDuplicates(rerLits) && "no duplicate literals!" );
                    int pos = 1;
                    for (int i = 2; i < oc.size(); ++ i) if (level(var(oc[i])) > level(var(oc[pos]))) { pos = i; }    // get second highest level!
                    { const Lit tmp = oc[pos]; oc[pos] = oc[1]; oc[1] = tmp; } // swap highest level literal to second position
                    CRef icr = ca.alloc(oc, config.opt_rer_as_learned); // add clause as non-learned clause
                    ca[icr].setLBD(rerLits.size()); // hard to say, would have to be calculated ... TODO
                    DOUT(if (config.opt_rer_debug) cerr << "c add clause [" << icr << "] " << ca[icr] << endl;);
                    if (config.opt_rer_as_learned) {  // add clause
                        learnts.push(icr);
                    } else { clauses.push(icr); }
                    attachClause(icr); // at least the first two literals should be unassigned!

                }
                // set the activity of the new variable
                double newAct = 0;
                if (config.opt_rer_newAct == 0) {
                    for (int i = 0; i < rerLits.size(); ++ i) { newAct += activity[ var(rerLits[i]) ]; }
                    newAct /= (double)rerLits.size();
                } else if (config.opt_rer_newAct == 1) {
                    for (int i = 0; i < rerLits.size(); ++ i) { // max
                        newAct = newAct >= activity[ var(rerLits[i]) ] ? newAct : activity[ var(rerLits[i]) ];
                    }
                } else if (config.opt_rer_newAct == 2) {
                    newAct = activity[ var(rerLits[0]) ];
                    for (int i = 1; i < rerLits.size(); ++ i) { // min
                        newAct = newAct > activity[ var(rerLits[i]) ] ? activity[ var(rerLits[i]) ] : newAct ;
                    }
                } else if (config.opt_rer_newAct == 3) {
                    for (int i = 0; i < rerLits.size(); ++ i) { // sum
                        newAct += activity[ var(rerLits[i]) ];
                    }
                } else if (config.opt_rer_newAct == 4) {
                    for (int i = 0; i < rerLits.size(); ++ i) { // geo mean
                        newAct += activity[ var(rerLits[i]) ];
                    }
                    newAct = pow(newAct, 1.0 / (double)rerLits.size());
                }
                activity[x] = newAct;
                // from bump activity code - scale and insert/update
                if (newAct > 1e100) {
                    for (int i = 0; i < nVars(); i++) { activity[i] *= 1e-100; }
                    var_inc *= 1e-100;
                }
                // Update order_heap with respect to new activity:
                if (order_heap.inHeap(x)) { order_heap.decrease(x); }

                // is rewrite enabled, then add information
                if (config.opt_rer_rewriteNew && config.opt_rer_full && !config.opt_rer_as_learned && config.opt_rer_windowSize == 2) { // as real clause, and full extension, and two ltis
                    erRewriteInfo[ toInt(~rerLits[0]) ].otherMatch = ~rerLits[1];
                    erRewriteInfo[ toInt(~rerLits[0]) ].replaceWith = mkLit(x, true);
                }

                // code from search method - enqueue the last decision again!
                newDecisionLevel();
                uncheckedEnqueue(lastDecisoin);   // this is the decision that has been done on this level before!
                DOUT(if (config.opt_rer_debug) {
                cerr << "c new decision level " << decisionLevel() << endl;
                    for (int i = 0 ; i < decisionLevel() ; ++i) { cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl; }
                });

                // modify the current learned clause accordingly!
                currentLearnedClause[0] = mkLit(x, false);
                // stats
                DOUT(if (config.opt_rer_debug) {
                cerr << "c close with [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : ";
                    for (int i = 0; i < currentLearnedClause.size(); ++i) { cerr << " " << currentLearnedClause[i] << "@" << level(var(currentLearnedClause[i])); }
                    cerr << endl;
                });
                rerLearnedClause ++; rerLearnedSizeCandidates ++;
                DOUT(if (config.opt_rer_debug) {
                cerr << endl << "c accepted current pattern with lits " << rerLits << " - start over" << endl << endl;
                cerr << "c trail: " ;
                for (int i = 0 ; i < trail.size(); ++ i) {
                        cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?";
                        if (!reason(var(trail[i])).isBinaryClause() && reason(var(trail[i])).getReasonC() == CRef_Undef) { cerr << "U"; }
                        else {
                            if (!reason(var(trail[i])).isBinaryClause()) { cerr << reason(var(trail[i])).getReasonC(); }
                            else { cerr << "L" << reason(var(trail[i])).getReasonL(); }
                        }
                    } cerr << endl;
                });
                resetRestrictedExtendedResolution(); // done with the current pattern
                maxRERclause = maxRERclause >= currentLearnedClause.size() ? maxRERclause : currentLearnedClause.size();
                totalRERlits += currentLearnedClause.size();

                assert(!hasComplementary(currentLearnedClause) && !hasDuplicates(currentLearnedClause) && "no duplicate literals!");

                return rerDontAttachAssertingLit;
            } else {
                DOUT(if (config.opt_rer_debug) cerr << "c add as [ " << rerLits.size() << " ] candidate [" << rerLearnedSizeCandidates << "] : " << currentLearnedClause << endl;);
                rerLearnedSizeCandidates ++;
                assert(!hasComplementary(currentLearnedClause) && !hasDuplicates(currentLearnedClause) && "no duplicate literals!");
                return rerMemorizeClause; // add the next learned clause to the database as well!
            }
        }
    }
    return rerUsualProcedure;
}

void Solver::resetRestrictedExtendedResolution()
{
    rerCommonLits.clear();
    rerCommonLitsSum = 0;
    rerLits.clear();
    rerFuseClauses.clear();
}

Solver::rerReturnType Solver::restrictedERITE(const Lit& previousFirst, const vec< Lit >& previousPartialClause, vec< Lit >& currentClause)
{
    // the first literal of currentClause cannot be in rerIteLits
    // however, its complement could be present
    // hence, check for the other literals whether there is a complementary pair, or whether there is another literal present

    if (currentClause.size() <= 2) { return rerAttemptFailed; }  // perform this check only with clauses that are larger than binary

    MethodClock mc(rerITEcputime); // measure the time spend in this method!
    rerITEtries ++;

    if (currentClause.size() != 1 + previousPartialClause.size()) { return rerAttemptFailed; }  // the two clauses do not match


    // first, scan for literal 's', hence mark all literals of the current learned clause
    lbd_marker.nextStep();
    for (int i = 0 ; i < currentClause.size(); ++ i) {
        lbd_marker.setCurrentStep(toInt(currentClause[i]));
    }

    Lit iteS = lit_Undef;
    if (lbd_marker.isCurrentStep(toInt(~previousFirst))) { iteS = ~previousFirst; }      // check first literal

    for (int i = 0 ; i < previousPartialClause.size(); ++ i) {
        if (lbd_marker.isCurrentStep(toInt(~previousPartialClause[i]))) {
            if (iteS == lit_Undef) { iteS = ~previousPartialClause[i]; }
            else { iteS = lit_Error; break; }
        }
    }
    if (iteS == lit_Error || iteS == lit_Undef) {
        rerITErejectS ++;   // stats
        return rerAttemptFailed; // there are more complementary literals, or there is no complementary literal
    }

    // scan for literal t
    lbd_marker.setCurrentStep(toInt(~iteS));   // add this literal to the set of literals that cannot be the literal ~t
    Lit iteT = lit_Undef;
    // TODO: this loop might be joined with the above loop?
    if (! lbd_marker.isCurrentStep(toInt(~previousFirst))) { iteT = ~previousFirst; }      // check first literal. if its not marked, than its a candidate for being literal t
    for (int i = 0 ; i < previousPartialClause.size(); ++ i) {
        if (! lbd_marker.isCurrentStep(toInt(~previousPartialClause[i]))) {
            if (iteT == lit_Undef) { iteT = ~previousPartialClause[i]; }
            else { iteT = lit_Error; break; }
        }
    }
    if (iteT == lit_Error || iteT == lit_Undef) {
        rerITErejectT ++;   // stats
        return rerAttemptFailed; // there are more literals that are not present in the other clause (and -s), or there is not enough literals present in the other clause
    }


    // scan for literal 'f', hence mark all literals of the previously learned clause, as well as the literal s
    lbd_marker.nextStep();
    lbd_marker.setCurrentStep(toInt(previousFirst));
    lbd_marker.setCurrentStep(toInt(iteS));
    for (int i = 0 ; i < previousPartialClause.size(); ++ i) {
        lbd_marker.setCurrentStep(toInt(previousPartialClause[i]));
    }

    Lit iteF = lit_Undef;
    for (int i = 0 ; i < currentClause.size(); ++ i) {
        if (! lbd_marker.isCurrentStep(toInt(~currentClause[i]))) {
            if (iteF == lit_Undef) { iteF = ~currentClause[i]; }
            else { iteF = lit_Error; break; }
        }
    }
    if (iteF == lit_Error || iteF == lit_Undef) {
        rerITErejectF ++;   // stats
        return rerAttemptFailed; // there are more literals that are not present in the other clause (and s), or there is not enough literals present in the other clause
    }


//   cerr << "c ITE(" << iteS << " , " <<  iteT << " , " <<  iteF << " ) " << endl;
    // rerFuseClauses;

    // perform RER step
    // add all the RER clauses with the fresh variable (and set up the new variable properly!
    Var usedVars [ 3]; usedVars[0] = var(iteS); usedVars[1] = var(~iteT); usedVars[2] = var(~iteF);
    const Var x = newVar(true, true, 'r'); // do not assign a value, because it will be undone anyways!

    // select level to jump to:
    int jumpLevel = level(usedVars[0]);
    jumpLevel = level(usedVars[1]) < jumpLevel ? level(usedVars[1]) : jumpLevel;
    jumpLevel = level(usedVars[2]) < jumpLevel ? level(usedVars[2]) : jumpLevel;
    if (jumpLevel == 0) { return rerAttemptFailed; }  // there is an assigned literal among the ITE literals, hence, do not use RER!
    jumpLevel --;

    // delete the current decision level as well, so that the order of the reason clauses can be set right!
    assert(decisionLevel() > 0 && "can undo a decision only, if it didnt occur at level 0");
    const Lit lastDecisoin = trail [ trail_lim[ jumpLevel ] ];
    DOUT(if (config.opt_rer_debug) cerr << "c undo decision level " << decisionLevel() << ", jump to " << jumpLevel << endl;);
    rerOverheadTrailLits += trail.size(); // store how many literals have been removed from the trail to set the order right!
    cancelUntil(jumpLevel);
    rerOverheadTrailLits -= trail.size();

    // detach all learned clauses from fused clauses
    {
        // its one clause here!
        assert(rerFuseClauses[0] != reason(var(ca[rerFuseClauses[0]][0])).getReasonC() && "from a RER-CDCL point of view, these clauses cannot be reason clause");
        assert(rerFuseClauses[0] != reason(var(ca[rerFuseClauses[0]][1])).getReasonC() && "from a RER-CDCL point of view, these clauses cannot be reason clause");
        // ca[rerFuseClauses[i]].mark(1); // mark to be deleted!
        DOUT(if (config.opt_rer_debug) cerr << "c remove clause (" << 0 << ")[" << rerFuseClauses[0] << "] " << ca[ rerFuseClauses[0] ] << endl;);
        removeClause(rerFuseClauses[0]); // drop this clause!
    }

    // we do not need a reason here, the new learned clause will do!
    // ITE(" << iteS << " , " <<  iteT << " , " <<  iteF << " ) " << endl;
    // clauses: x, -s, -t AND x,s,-f
    oc.clear(); oc.push(mkLit(x, false)); oc.push(~iteS); oc.push(~iteT);  // first clause
    for (int i = 0 ; i < 2; ++ i) {
        if (i == 1) { oc[0] = mkLit(x, false); oc[1] = iteS; oc[2] = ~iteF;  } // setup the second clause
        CRef icr = ca.alloc(oc, config.opt_rer_as_learned); // add clause as non-learned clause
        ca[icr].setLBD(1); // all literals are from the same level!
        DOUT(if (config.opt_rer_debug) cerr << "c add clause [" << icr << "]" << ca[icr] << endl;);
        nbDL2++; nbBin ++; // stats
        if (config.opt_rer_as_learned) {  // add clause
            learnts.push(icr);
        } else { clauses.push(icr); }
        attachClause(icr); // all literals should be unassigned
    }


    // set the activity of the new variable
    double newAct = 0;
    if (config.opt_rer_newAct == 0) {
        for (int i = 0; i < 3; ++ i) { newAct += activity[ usedVars[i] ]; }
        newAct /= (double)3;
    } else if (config.opt_rer_newAct == 1) {
        for (int i = 0; i < 3; ++ i) { // max
            newAct = newAct >= activity[ usedVars[i] ] ? newAct : activity[  usedVars[i] ];
        }
    } else if (config.opt_rer_newAct == 2) {
        newAct = activity[ usedVars[0]  ];
        for (int i = 1; i < 3; ++ i) { // min
            newAct = newAct > activity[ usedVars[i] ] ? activity[ usedVars[i] ] : newAct ;
        }
    } else if (config.opt_rer_newAct == 3) {
        for (int i = 0; i < 3; ++ i) { // sumcurrentLearnedClause
            newAct += activity[ usedVars[i] ];
        }
    } else if (config.opt_rer_newAct == 4) {
        for (int i = 0; i < 3; ++ i) { // geo mean
            newAct += activity[ usedVars[i] ];
        }
        newAct = pow(newAct, 1.0 / (double)3);
    }
    activity[x] = newAct;
    // from bump activity code - scale and insert/update
    if (newAct > 1e100) {
        for (int i = 0; i < nVars(); i++) { activity[i] *= 1e-100; }
        var_inc *= 1e-100;
    }
    // Update order_heap with respect to new activity:
    if (order_heap.inHeap(x)) { order_heap.decrease(x); }

//  // code from search method - enqueue the last decision again!
//  newDecisionLevel();
//  uncheckedEnqueue( lastDecisoin ); // this is the decision that has been done on this level before! search loop will propagate this next ...
//  if( config.opt_rer_debug ) {
//    cerr << "c new decision level " << decisionLevel() << endl;
//    for( int i = 0 ; i < decisionLevel() ; ++i ) cerr << "c dec [" << i << "] = " << trail[ trail_lim[i] ] << endl;
//  }

    // modify the current learned clause according to the ITE gate
    currentClause[0] = mkLit(x, false);
    for (int i = 1 ; i < currentClause.size(); ++i) {
        if (currentClause[i] == ~iteF || currentClause[i] == iteS) {  // delete the other literal from the clause!
            currentClause[i] = currentClause[ currentClause.size() - 1 ]; currentClause.pop(); // fast remove without keeping order
            break;
        }
    }
    // make sure that two unassigned literals are at the front
    assert(value(currentClause[0]) == l_Undef && "the first literal (new variable) has to be unassigned");
    if (value(currentClause[1])  != l_Undef) {
        for (int i = 2 ; i < currentClause.size(); ++i) {  // find another unassigned literal!
            if (value(currentClause[i]) == l_Undef) { const Lit tmp = currentClause[i]; currentClause[i] = currentClause[1]; currentClause[1] = tmp; break; }    // swap literals
            else { assert(value(currentClause[i]) == l_False && "there cannot be satisfied literals in the current learned clause"); }
        }
    }
    bool propagateAndAttach = false;
    if (value(currentClause[1]) != l_Undef) {
        // sort second highest level at second position, change return value to "attach and propagate"!
        int highest = 1;
        for (int i = 2; i < currentClause.size(); ++ i)
            if (level(var(currentClause[i])) > level(var(currentClause[highest]))) { highest = i; }
        const Lit tmp = currentClause[highest]; currentClause[highest] = currentClause[1]; currentClause[1] = tmp;
        propagateAndAttach = true;
    }

    // stats
    rerLearnedClause ++; rerLearnedSizeCandidates ++;

    resetRestrictedExtendedResolution(); // done with the current pattern
    maxRERclause = maxRERclause >= currentClause.size() ? maxRERclause : currentClause.size();
    totalRERlits += currentClause.size();

    rerITEsuccesses ++;

    if (propagateAndAttach) { return rerUsualProcedure; }
    else { return rerDontAttachAssertingLit; }
}


void Solver::disjunctionReplace(Lit p, Lit q, const Lit& x, const bool& inLearned, const bool& inBinary)
{

    for (int m = 0 ; m < (inLearned ? 2 : 1); ++ m) {
        const vec<CRef>& cls = (m == 0 ? clauses : learnts);

        for (int i = 0 ; i < cls.size(); ++ i) {  // check all clauses of the current set
            Clause& c = ca[cls[i]];
            if (c.mark() != 0) { continue; }  // do not handle clauses that are marked for being deleted!
            if (!inBinary && c.size() <= 2) { continue; }  // skip binary clauses, if doable -- TODO: for rer check whether it is relevant to check binary clauses! for ecl its not!
            int firstHit = 0;
            for (; firstHit < c.size(); ++ firstHit) {
                const Lit& l = c[firstHit];
                if (l == p || l == q) { break; }
                else if (l == ~p || l == ~q) { firstHit = -1; break;}
            }
            if (firstHit == -1 || firstHit == c.size()) { continue; }  // do not handle this clause - tautology found, or no hit

            if (c[firstHit] == q) { const Lit tmp = q; q = p; p = tmp; }
            int secondHit = firstHit + 1;
            for (; secondHit < c.size(); ++ secondHit) {
                const Lit& l = c[secondHit];
                if (l == q) { break; }
                else if (l == ~q) {secondHit = -1; break; }
            }
            if (secondHit == -1 || secondHit == c.size()) { continue; }  // second literal not found, or complement of other second literal found

            assert(firstHit < secondHit && "if the clause will be rewritten, then the first position has to be before the second");
            // found both literals in the clause ...
            DOUT(if (config.opt_rer_debug) {
            cerr << "c rewrite clause [" << cls[i] << "]@" << decisionLevel() << " : " << c << endl;
                cerr << "c hit1: " << c[firstHit] << " undef=" << (l_Undef == value(c[firstHit])) << "@" << level(var(c[firstHit])) << endl;
                cerr << "c hit2: " << c[secondHit] << " undef=" << (l_Undef == value(c[secondHit])) << "@" << level(var(c[secondHit])) << endl;
            });
            if (c.size() == 2) {
                assert(decisionLevel() == 0 && "can add a unit only permanently, if we are currently on level 0!");
                removeClause(cls[i]);
                uncheckedEnqueue(x);
                continue; // nothing more to be done!
            } else { // TODO: could be implemented better (less watch moving!)
                // rewrite clause
                // reattach clause if neccesary
                // assert( (leve(var(firstHit)) > decisionLevel() || decisionLevel () == 0 ) && "a reason clause should not be rewritten, such that the first literal is moved!" );
                if (firstHit < 2 || c.size() == 3) {
                    DOUT(if (config.opt_rer_debug) cerr << "c dettach clause [" << cls[i] << "] : " << ca[cls[i]] << endl;);
                    detachClause(cls[i], true);   // not always necessary to remove the watches!
                } else {
                    if (c.learnt()) { learnts_literals --; }
                    else { clauses_literals--; }
                }
                c[firstHit] = x;
                c[secondHit] = c[ c.size() - 1 ];
                c.shrink(1);
                DOUT(if (config.opt_rer_debug) cerr << "c rewrite clause into " << c << endl;);
                assert(c.size() > 1 && "do not produce unit clauses!");
                if (firstHit < 2 || c.size() == 2) {
                    DOUT(if (config.opt_rer_debug) cerr << "c attach clause [" << cls[i] << "] : " << ca[cls[i]] << endl;);
                    attachClause(cls[i]);   // attach the clause again with the corrected watcher
                }
            }

        }
    }
}

bool Solver::interleavedClauseStrengthening()
{
    // cerr << "c enter interleaved clause strengthening" << endl;
    // TODO: have dynamic updates of the limits, so that a good time/result ratio can be reached!
    icsCalls ++;
    MethodClock thisMethodTime(icsTime);   // clock that measures how long the procedure took
    // freeze current state
    vec<Lit> trailCopy;
    trail.copyTo(trailCopy);
    vec<int> trailLimCopy;
    trail_lim.copyTo(trailLimCopy);
    varFlags.copyTo(backupSolverState);

    const int oldVars = nVars();
    const int oldLearntsSize = learnts.size();

    // backtrack to level 0
    DOUT(if (config.opt_ics_debug) for (int i = 0 ; i < trailLimCopy.size(); ++i) cerr << "c decision " << trailCopy[ trailLimCopy[i] ]  << "@" << i + 1 << endl;);
    cancelUntil(0);

    // perform reducer algorithm for some of the last good learned clauses - also adding the newly learnt clauses
    int backtrack_level; unsigned int nblevels;   // helper variable (more or less borrowed from search method
    unsigned dependencyLevel;           // helper variable (more or less borrowed from search method
    vec<Lit> learnt_clause;       // helper variable (more or less borrowed from search method
    // do the loop
    const int end = learnts.size();
    int start = 0, count = 0;
    double lbdSum = 0, sizeSum = 0;
    // calculate avgs. to be able to  reject clauses -- drop clauses that are not "usual" (mark() != 0)
    for (int i = learnts.size() - 1; i >= 0; -- i) {
        const Clause& c = ca [ learnts[i] ];
        if (c.mark() != 0) { continue; }  // do not consider this clause (seems to be deleted already, must not be part of a watched list any more ... )
        lbdSum += c.lbd();
        sizeSum += c.size();
        if (++count > config.opt_ics_processLast) { start = i; break; }  // stop the scan here, and start ICS with this clause!
    }
    const double lbdCount = count;
    const double sizeLimit = (sizeSum / lbdCount) * config.opt_ics_SIZEpercent;  // clauses with a size smaller than this are dropped
    const double lbdLimit = (lbdSum / lbdCount) * config.opt_ics_LBDpercent;  // clauses with an LBD smaller than this are dropped

    for (int i = start; i < (config.opt_ics_shrinkNew ? learnts.size() : end) ; ++ i) {  // do not process the new clauses, nor keep them ...
        Clause& c = ca [ learnts[i] ];
        if (c.mark() != 0) { continue; }  // do not consider this clause (seems to be deleted already, must not be part of a watched list any more ... )
        if (c.size() > sizeLimit || c.lbd() > lbdLimit) { icsDroppedCandidates++; continue; } // do not consider this clause!
        icsCandidates ++; // stats - store how many learned clauses have been tested
        DOUT(if (config.opt_ics_debug) cerr << "c ICS on [ " << i << " / " << learnts.size() << " = " << learnts[i] << " / " << ca.size() << " ]: lits= " << c.size() << " : " << c << endl;);
        if (c.size() == 1 || satisfied(c)) { continue; }  // do not work on satisfied clauses, and not on unit clauses!
        detachClause(learnts[i], true);   // remove the clause from the solver, to be able to rewrite it
        for (int j = 0 ; j < c.size(); ++ j) {
            if (value(c[j]) == l_True) { c.mark(1); break; }    // do not use clauses that are satisfied!
        }
        if (c.mark() != 0) { continue; }  // this clause is removed from the solver!

        // TODO: could sort the literals somehow here ...
        int k = 0; // number of literals to keep in the clause
        int j = 0;
        bool droppedLit = false;
        int dropPosition = -1;
        if (outputsProof()) {        // copy original clause
            oc.clear();
            for (int j = 0 ; j < c.size(); ++ j) {
                oc.push(c[j]);
            }
        }
        for (; j < ca[learnts[i]].size(); ++ j) {    // check each literal and enqueue it negated -- do not use the reference, because lhbr in propagate can make it invalid
            DOUT(if (config.opt_ics_debug) cerr << "c check lit " << j << "/" << c.size() << ": " << ca[learnts[i]][j] << " with value " << (value(ca[learnts[i]][j])) << endl;);
            if (value(ca[learnts[i]][j]) == l_True) {    // just need to keep all previous and this literal
                DOUT(if (config.opt_ics_debug) {
                cerr << "c interrupt because of sat.lit, current trail " << endl;
                cerr << "c write to " << k << " / " << c.size() << " literal from " << j << " / " << c.size() << endl;
                });
                ca[learnts[i]][k++] = ca[learnts[i]][j];
                break; // this literals does not need to be kept! // TODO: what if the clause is already satisfied, could be stopped as well!
            } else if (value(ca[learnts[i]][j]) == l_False) {
                droppedLit = true;
                dropPosition = j; // dropped the literal here
                DOUT(if (config.opt_ics_debug) cerr << "c jump over false lit: " << ca[learnts[i]][j] << endl;);
                continue; // can drop this literal
            }
            ca[learnts[i]][k++] = ca[learnts[i]][j]; // keep the current literal!
            newDecisionLevel(); // we are not working on decision level 0, so nothing breaks
            uncheckedEnqueue(~ca[learnts[i]][j]);
            CRef confl = propagate();
            if (confl != CRef_Undef) {  // found a conflict, handle it (via usual, simple, conflict analysis)
                if (config.nanosleep != 0) { nanosleep(config.nanosleep); }    // sleep for a few nano seconds
                learnt_clause.clear(); // prepare for analysis
                printConflictTrail(confl);
                int ret = analyze(confl, learnt_clause, backtrack_level, nblevels, dependencyLevel);
                cancelUntil(0);
                if (ret == 0) {
                    addCommentToProof("learnt clause during ICS");
                    addToProof(learnt_clause);
                    if (learnt_clause.size() == 1) {
                        assert(decisionLevel() == 0 && "enequeue unit clause on decision level 0!");
                        topLevelsSinceLastLa ++;
                        #ifdef PCASSO
                        vardata[var(learnt_clause[0])].dependencyLevel = dependencyLevel;
                        #endif
                        if (value(learnt_clause[0]) == l_Undef) {  // propagate unit clause!
                            uncheckedEnqueue(learnt_clause[0]);
                            int proofTopLevels = trail.size();
                            bool propagationFailed = propagate() != CRef_Undef;
                            if (outputsProof()) {
                                for (; proofTopLevels < trail.size(); ++ proofTopLevels) { addUnitToProof(trail[ proofTopLevels ]); }
                            }
                            if (propagationFailed) {
                                DOUT(if (config.opt_ics_debug) cerr << "c ICS cannot propagate the unit of a learned unit clause " << learnt_clause[0] << endl;);
                                return false;
                            }
                            nbUn ++;
                        } else if (value(learnt_clause[0]) == l_False) {
                            DOUT(if (config.opt_ics_debug) cerr << "c ICS learned a falsified unit clause " << learnt_clause[0] << endl;);
                            return false; // otherwise, we have a top level conflict here!
                        }
                    } else {
                        const CRef cr = ca.alloc(learnt_clause, true);
                        ca[cr].setLBD(nblevels);
                        #ifdef PCASSO
                        ca[cr].setPTLevel(dependencyLevel);
                        #endif
                        learnts.push(cr); // this is the learned clause only, has nothing to do with the other clause!
                        attachClause(cr); // for now, we'll also use this clause!
                        DOUT(if (config.opt_ics_debug) cerr << "c ICS learn clause [" << cr << "] " << ca[cr] << endl;);
                    }
                } else {
                    if (l_False == handleMultipleUnits(learnt_clause)) {
                        // learn multiple units here!
                        DOUT(if (config.opt_ics_debug) cerr << "c learned UNSAT with multi units!" << endl;);
                        return false;
                    }
                    if (propagate() != CRef_Undef) { return false; }
                }
                break; // we're done for this clause for now. ... what happens if we added the current learned clause now and repeat the process for the clause? there might be more reduction! -> TODO: shuffe clause and have parameter!
            } // end if conflict during propagate
        } // have checked all literals of the current clause ...
        // refresh reference! there might have been a ca.alloc!
        Clause& d = ca[ learnts[i] ];
        cancelUntil(0);   // to be on the safe side, if there has not been a conflict before
        // shrink the clause and add it back again!
        DOUT(if (config.opt_ics_debug) cerr << "c ICS looked at " << j << " literals, and kept " << k << " with a size of " << d.size() << endl;);
        if (droppedLit || k < j) {  // actually, something has been done
            icsShrinks ++; icsShrinkedLits += (d.size() - k);  // stats -- store the success of shrinking
            d.shrink(d.size() - k);
            DOUT(if (config.opt_ics_debug) cerr << "c ICS changed clause to " << d << " from " << oc << endl;);
            addCommentToProof("shrinked by ICS");
            addToProof(d); addToProof(oc, true); // add shorter clause, remove longer clause
        }
        DOUT(if (config.opt_ics_debug) cerr << "c ICS return (modified) clause: " << d << endl;);

        if (d.size() > 1) {
            DOUT(if (config.opt_ics_debug) {
            for (int j = 0 ; j < d.size(); ++ j) {
                    for (int k = j + 1; k < d.size(); ++ k) { assert(d[j] != d[k] && "do not have clauses with a duplicate literal!"); }
                }
            });
            attachClause(learnts[i]);   // unit clauses do not need to be added!
        } else if (d.size() == 1) {   // if not already done, propagate this new clause!
            if (value(d[0]) == l_Undef) {
                uncheckedEnqueue(d[0]);
                int proofTopLevels = trail.size();
                bool propagationFailed = propagate() != CRef_Undef;
                if (outputsProof()) {
                    DOUT(cerr << "add all literals from the top level to the proof as well: " << proofTopLevels << " to " << trail.size() << endl;);
                    for (; proofTopLevels < trail.size(); ++ proofTopLevels) { addUnitToProof(trail[ proofTopLevels ]); }
                }
                if (propagationFailed) {
                    DOUT(if (config.opt_ics_debug) cerr << "c ICS return false, because unit " << d[0] << " cannot be propagated" << endl;);
                    return false;
                }
            } else if (value(d[0]) == l_False) {
                DOUT(if (config.opt_ics_debug) cerr << "c ICS learned falsified unit " << d[0] << endl;);
                return false; // should not happen!
            }
        } else if (d.size() == 0) {
            DOUT(if (config.opt_ics_debug) cerr << "c ICS learned an empty clause [" << learnts[i] << "]" << endl;);
            return false; // unsat, since c is empty
        }
    }

    // remove the newly learned clauses
    if (!config.opt_ics_keepLearnts) {
        for (int j = oldLearntsSize; j < learnts.size(); ++ j) {
            if (ca[learnts[j]].mark() == 0) { removeClause(learnts[j], true); }    // remvoes a clause "lazily"
        }
        learnts.shrink(learnts.size() - oldLearntsSize);
        assert(learnts.size() == oldLearntsSize && "remove exactly all the clauses that have been added before");
    }

    // role back solver state again
    assert(oldVars == nVars() && "no variables should have been added during executing this algorithm!");
    assert(decisionLevel() == 0 && "after ICS should be on level 0");

    for (int i = 0 ; i < trailLimCopy.size(); ++i) {
        newDecisionLevel();
        DOUT(if (config.opt_ics_debug) cerr << "c enqueue " << trailCopy[ trailLimCopy[i] ]  << "@" << i + 1 << endl;);
        if (value(trailCopy[ trailLimCopy[i] ]) == l_False) {
            cancelUntil(decisionLevel() - 1);
            assert(decisionLevel() <= i && "the last literal should not be added to the trail");
            break; // stop here, because the next decision has to be different! (and the search will take care of that!)
        } else if (value(trailCopy[ trailLimCopy[i] ]) == l_Undef) {
            DOUT(if (config.opt_ics_debug) cerr << "c enqueue next decision(" << i << ") idx=" << trailLimCopy[i] << " : "  << trailCopy[ trailLimCopy[i] ] << endl;);
            uncheckedEnqueue(trailCopy[ trailLimCopy[i] ]);
        } else { // the literal is already true, remove the currently added level, and proceed with the next level
            cancelUntil(decisionLevel() - 1);      // do not add the same literal multiple times on the decision vector!
            continue; // no need to propagate here!
        }
        assert(decisionLevel() > 0 && "a new literal has just been added");
        CRef confl = propagate();
        if (confl != CRef_Undef) {  // handle conflict. conflict at top-level -> return false!, else backjump, and continue with good state!
            cancelUntil(decisionLevel() - 1);
//       else {
//  cerr << "c during creating the trail again, an error has been found - cannot set level below the current value -- tried to enqueue decision literal " << trailCopy[ trailLimCopy[i] ] << " as " << i + 1 << "th decision " << endl;
//  return false;
//       }
            break; // interrupt re-building the trail
        }
    }



    for (int i = 0 ; i < backupSolverState.size(); ++i) { varFlags[i].polarity = backupSolverState[i].polarity; }
    DOUT(if (config.opt_ics_debug) {
    cerr << "c after ICS decision levels (" << decisionLevel() << ")" << endl;
        for (int i = 0 ; i < trail_lim.size(); ++i) {
            cerr << "c dec[" << i + 1 << "] : " << trail[ trail_lim[i] ] << endl;
        }
        cerr << endl;
    });
    lastICSconflicts = conflicts;
    // return as if nothing has happened
    DOUT(if (config.opt_ics_debug) cerr << "c finished ICS" << endl;);
    return true;
}

void Solver::setPreprocessor(Coprocessor::Preprocessor* cp)
{
    if (coprocessor == 0) { coprocessor = cp; }
}

void Solver::setPreprocessor(Coprocessor::CP3Config* _config)
{
    assert(coprocessor == 0 && "there should not exist a preprocessor when this method is called");
    coprocessor = new Coprocessor::Preprocessor(this, *_config);
}

Coprocessor::Preprocessor* Solver::swapPreprocessor(Coprocessor::Preprocessor* newPreprocessor)
{
    Coprocessor::Preprocessor* oldPreprocessor = coprocessor;
    coprocessor = newPreprocessor;
    return oldPreprocessor;
}

Preprocessor* Solver::getPreprocessor() const
{
    return coprocessor;
}

void Solver::extendModel(vec< lbool >& model)
{
    if (coprocessor != 0) { coprocessor->extendModel(model); }
}


void Solver::printFullSolverState()
{
    cerr << "c FULL SOLVER STATE" << endl;
    cerr << "c [SOLVER-STATE] trail: " << trail << endl;
    cerr << "c [SOLVER-STATE] decisionLevel: " << decisionLevel() << endl;
    cerr << "c [SOLVER-STATE] conflicts: " << conflicts << endl;
    for (int i = 0 ; i < clauses.size(); ++ i) {
        cerr << "c [SOLVER-STATE] clause(" << i << ")@" << clauses[i] << ": " << ca[clauses[i]] << endl;
    }
    for (int i = 0 ; i < learnts.size(); ++ i) {
        cerr << "c [SOLVER-STATE] learnt(" << i << ")@" << learnts[i] << ": " << ca[learnts[i]] << endl;
    }
    cerr << "c [SOLVER-STATE] activities: " << activity << endl;
    for (Var v = 0 ; v < nVars(); ++v) {
        for (int p = 0 ; p < 2; ++p) {
            const Lit l = mkLit(v, p == 1);
            cerr << "c [SOLVER-STATE] watch list(" << l << "): ";
            for (int i = 0 ; i < watches[l].size(); ++i) {
                cerr << " " << watches[l][i].cref();
                if (watches[l][i].isBinary()) { cerr << "b" ; }
            }
            cerr << endl;
        }
    }
    cerr << "c [SOLVER-STATE] decision heap: " << order_heap << endl;
}


void Solver::printHeader()
{
    if (verbosity >= 1) {
        printf("c ========================================[ MAGIC CONSTANTS ]==============================================\n");
        printf("c | Constants are supposed to work well together :-)                                                      |\n");
        printf("c | however, if you find better choices, please let us known...                                           |\n");
        printf("c |-------------------------------------------------------------------------------------------------------|\n");
        printf("c |                                |                                |                                     |\n");
        printf("c | - Restarts:                    | - Reduce Clause DB:            | - Minimize Asserting:               |\n");
        printf("c |   * LBD Queue    : %6d      |   * First     : %6d         |    * size < %3d                     |\n", lbdQueue.maxSize(), searchconfiguration.firstReduceDB, searchconfiguration.lbSizeMinimizingClause);
        printf("c |   * Trail  Queue : %6d      |   * Inc       : %6d         |    * lbd  < %3d                     |\n", trailQueue.maxSize(), searchconfiguration.incReduceDB, searchconfiguration.lbLBDMinimizingClause);
        printf("c |   * K            : %6.2f      |   * Special   : %6d         |                                     |\n", searchconfiguration.K, searchconfiguration.specialIncReduceDB);
        printf("c |   * R            : %6.2f      |   * Protected :  (lbd)< %2d     |                                     |\n", searchconfiguration.R, searchconfiguration.lbLBDFrozenClause);
        printf("c |                                |                                |                                     |\n");
        printf("c =========================================================================================================\n");
    }
}

void Solver::printSearchHeader()
{
    if (verbosity >= 1) {
        printf("c ==================================[ Search Statistics (every %6d conflicts) ]=========================\n", verbEveryConflicts);
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
    if (decisionLevel() != 0) { return status; }  // might jump back to L 0 once in a while

    if (coprocessor != 0 && useCoprocessorPP) {
        if (processOtfss(otfss)) { return l_False ; }    // make sure we work on the correct clauses still (collected before)
        preprocessCalls++;
        preprocessTime.start();
        status = coprocessor->preprocess();
        preprocessTime.stop();
        otfss.clearQueues(); // make sure there are no OTFSS pointers left over //FIXME process OTFSS in coprocessor

        // recompute values for LPD parameter
        if (config.opt_litPairDecisions > 0) {
            recomputeLPDdata();
        }
    }
    if (verbosity >= 1) { printf("c =========================================================================================================\n"); }
    return status;
}


void Solver::recomputeLPDdata()
{
    for (int index = 0 ; index < decisionLiteralPairs.size(); ++ index) { decisionLiteralPairs[index].reset(); }  // clear previous information
    for (int index = 0 ; index < clauses.size(); ++ index) {  // recompute based on the order of the clauses in the clauses vector
        const Clause& c = ca[clauses[index]];
        if (c.size() > 2) {   // collect literals only for larger clauses (not binary!)
            for (int i = 0 ; i < c.size(); ++i) {
                LitPairPair& lp = decisionLiteralPairs[ toInt(c [i]) ];
                if (lp.p.replaceWith != lit_Undef && lp.q.replaceWith != lit_Undef) { continue; }  // this literal already has enough literals stored
                if (lp.p.replaceWith == lit_Undef) {
                    lp.p.replaceWith = c [(i + 1) % c.size() ]; // store next two literals
                    lp.p.otherMatch  = c [(i + 2) % c.size() ]; // store next two literals
                } else {
                    assert(lp.q.replaceWith == lit_Undef && "this case is left over");
                    lp.q.replaceWith = c [(i + 1) % c.size() ]; // store next two literals
                    lp.q.otherMatch  = c [(i + 2) % c.size() ]; // store next two literals
                }
            }
        }
    }
}


lbool Solver::inprocess(lbool status)
{
    if (status == l_Undef) {
        // restart, triggered by the solver
        // if( coprocessor == 0 && useCoprocessor)  coprocessor = new Coprocessor::Preprocessor(this); // use number of threads from coprocessor
        if (coprocessor != 0 && useCoprocessorIP) {
            if (coprocessor->wantsToInprocess()) {
                if (decisionLevel() > 0) { cancelUntil(0); }  // make sure we operate on level 0!
                if (processOtfss(otfss)) { return l_False ; }    // make sure we work on the correct clauses still (collected before)
                communicationClient.receiveEE = true; // enable receive EE after first inprocessing (as there will be another one)
                inprocessCalls ++;
                inprocessTime.start();
                status = coprocessor->inprocess();
                inprocessTime.stop();

                otfss.clearQueues(); // make sure there are no OTFSS pointers left over //FIXME process OTFSS in coprocessor
                if (big != 0) {
                    big->recreate(ca, nVars(), clauses, learnts);   // build a new BIG that is valid on the "new" formula!
                    big->removeDuplicateEdges(nVars());
                    big->generateImplied(nVars(), add_tmp);
                    if (config.opt_uhdProbe > 2) { big->sort(nVars()); }     // sort all the lists once
                }

                // actually, only necessary if the variables got removed or anything like that ...
                if (config.opt_rer_extractGates || (config.opt_rer_rewriteNew &&  config.opt_rer_windowSize == 2)) {
                    erRewriteInfo.clear();
                    erRewriteInfo.growTo(2 * nVars(), LitPair());
                    rerInitRewriteInfo();
                }

                // recompute values for LPD parameter
                if (config.opt_litPairDecisions > 0) {
                    recomputeLPDdata();
                }
            }
        }
    }
    return status;
}


void Solver::printConflictTrail(CRef confl)
{
    DOUT(if (config.opt_printDecisions > 2) {
    printf("c conflict at level %d\n", decisionLevel());
        cerr << "c conflict clause: " << ca[confl] << endl;
        cerr << "c trail: " ;
        for (int i = 0 ; i < trail.size(); ++ i) {
            cerr << " " << trail[i] << "@" << level(var(trail[i])) << "?";
            if (!reason(var(trail[i])).isBinaryClause() && reason(var(trail[i])).getReasonC() == CRef_Undef) { cerr << "U"; }
            else {
                if (!reason(var(trail[i])).isBinaryClause()) { cerr << reason(var(trail[i])).getReasonC(); }
                else { cerr << "L" << reason(var(trail[i])).getReasonL(); }
            }
        } cerr << endl;
    });
}

void Solver::updateDecayAndVMTF()
{
    // as in glucose 2.3, increase decay after a certain amount of steps - but have parameters here!
    if (searchconfiguration.var_decay < config.opt_var_decay_stop && conflicts % config.opt_var_decay_dist == 0) {   // div is the more expensive operation!
        searchconfiguration.var_decay += config.opt_var_decay_inc;
        searchconfiguration.var_decay = searchconfiguration.var_decay >= config.opt_var_decay_stop ? config.opt_var_decay_stop : searchconfiguration.var_decay; // set upper boung
    }

    // update the mixture between VMTF and VSIDS dynamically, similarly to the decay
    if (useVSIDS != searchconfiguration.var_decay_end && conflicts % searchconfiguration.var_decay_distance == 0) {
        if (searchconfiguration.var_decay_end > searchconfiguration.var_decay_start) {
            useVSIDS += searchconfiguration.var_decay_inc;
            if (useVSIDS >= searchconfiguration.var_decay_end) { useVSIDS = searchconfiguration.var_decay_end; }
        } else if (searchconfiguration.var_decay_end < searchconfiguration.var_decay_start) {
            useVSIDS -= searchconfiguration.var_decay_inc;
            if (useVSIDS <= searchconfiguration.var_decay_end) { useVSIDS = searchconfiguration.var_decay_end; }
        } else {
            useVSIDS = searchconfiguration.var_decay_end;
        }
    }
}

lbool Solver::handleMultipleUnits(vec< Lit >& learnt_clause)
{
    assert(decisionLevel() == 0 && "found units, have to jump to level 0!");
    // update info for restart
    if (searchconfiguration.restarts_type == 0) {
        lbdQueue.push(1); // unit clause has one level
        slow_LBDs.update(1);
        recent_LBD.update(1);
    }
    sumLBD += 1;

    for (int i = 0 ; i < learnt_clause.size(); ++ i) {   // add all units to current state
        if (value(learnt_clause[i]) == l_Undef) { uncheckedEnqueue(learnt_clause[i]); }
        else if (value(learnt_clause[i]) == l_False) { return l_False; }  // otherwise, we have a top level conflict here!
        else {
            DOUT(if (config.opt_learn_debug) { cerr << "c tried to enqueue a unit clause that was already a unit clause ... " << endl; });
            DOUT(if (config.opt_printDecisions > 1) { cerr << "c enqueue multi-learned literal " << learnt_clause[i] << "(" << i << "/" << learnt_clause.size() << ") at level " <<  decisionLevel() << endl;});
        }
    }

    // write learned unit clauses to DRUP!
    for (int i = 0; i < learnt_clause.size(); i++) {
        addCommentToProof("learnt unit");
        addUnitToProof(learnt_clause[i]);
        IPASIR_shareUnit(learnt_clause[i]);
    }
    // store learning stats!
    totalLearnedClauses += learnt_clause.size(); sumLearnedClauseSize += learnt_clause.size(); sumLearnedClauseLBD += learnt_clause.size();
    maxLearnedClauseSize = 1 > maxLearnedClauseSize ? 1 : maxLearnedClauseSize;

    multiLearnt = (learnt_clause.size() > 1 ? multiLearnt + 1 : multiLearnt);   // stats
    topLevelsSinceLastLa ++;
    return l_Undef;
}

lbool Solver::handleLearntClause(vec< Lit >& learnt_clause, bool backtrackedBeyond, unsigned int nblevels, unsigned& dependencyLevel)
{
    // when this method is called, backjumping has been done already!
    rerReturnType rerClause = rerUsualProcedure;
    // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause" );
    if (doAddVariablesViaER && !isBiAsserting) {  // be able to block adding variables during search by the solver itself, do not apply rewriting to biasserting clauses!
        assert(!isBiAsserting && "does not work if isBiasserting is changing something afterwards!");
        extResTime.start();
        rerClause = restrictedExtendedResolution(learnt_clause, nblevels, dependencyLevel);
        // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause" );
        extResTime.stop();
    } else if (isBiAsserting) { resetRestrictedExtendedResolution(); }   // do not have two clauses in a row for rer, if one of them is bi-asserting!

    if (searchconfiguration.restarts_type == 0) {  // update only for dynamic restarts
        lbdQueue.push(nblevels);
        slow_LBDs.update(nblevels);
        recent_LBD.update(nblevels);
    }

    sumLBD += nblevels;
    // write learned clause to DRUP!
    addCommentToProof("learnt clause");
    // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause" );
    addToProof(learnt_clause);
    IPASIR_shareClause(learnt_clause);
    // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause" );
    // store learning stats!
    totalLearnedClauses ++ ; sumLearnedClauseSize += learnt_clause.size(); sumLearnedClauseLBD += nblevels;
    maxLearnedClauseSize = learnt_clause.size() > maxLearnedClauseSize ? learnt_clause.size() : maxLearnedClauseSize;

    // parallel portfolio: send the learned clause!
    if (sharingTimePoint == 0 || learnt_clause.size() < 3) {
        updateSleep(&learnt_clause, learnt_clause.size(), dependencyLevel);
    }  // shorter clauses are shared immediately!

    if (learnt_clause.size() == 1) {
        assert(decisionLevel() == 0 && "enequeue unit clause on decision level 0!");
        topLevelsSinceLastLa ++;
        #ifdef PCASSO
        vardata[var(learnt_clause[0])].dependencyLevel = dependencyLevel;
        #endif
        if (value(learnt_clause[0]) == l_Undef) {uncheckedEnqueue(learnt_clause[0]); nbUn++;}
        else if (value(learnt_clause[0]) == l_False) { return l_False; }  // otherwise, we have a top level conflict here!
        DOUT(if (config.opt_printDecisions > 1) cerr << "c enqueue learned unit literal " << learnt_clause[0] << " at level " <<  decisionLevel() << " from clause " << learnt_clause << endl;);
    } else {
        CRef cr = CRef_Undef;

        // assert( !hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause" );

        // is a core learnt clause, so we do not create a learned, but a "usual" clause
//  if (!activityBasedRemoval && nblevels < lbd_core_threshold + 1) {
        if (learnt_clause.size() <= config.opt_keep_permanent_size || nblevels <= lbd_core_threshold) {
            // no_LBD = false
            cr = ca.alloc(learnt_clause); // memorize that this is a learnt clause (in analyze method vsids activity is increased sometimes)
            if (rerClause == rerMemorizeClause) { resetRestrictedExtendedResolution(); } // do not memorize clause that is added to the formula
            ca[cr].setCoreClause(true);   // memorize that this clause is a core-learnt clause
            clauses.push(cr);
        } else {
            // 2 = normal(interesting) learnt clause
            // 3 = core learnt
            assert(!hasComplementary(learnt_clause) && !hasDuplicates(learnt_clause) && "do not have duplicate literals in the learned clause");
            cr = ca.alloc(learnt_clause, true);
            // ca[cr].mark(no_LBD ? 0 : nblevels < 6 ? 3 : 2);
            learnts.push(cr);
            if (rerClause == rerMemorizeClause) { rerFuseClauses.push(cr); }    // memorize this clause reference for RER

            if (config.opt_cls_act_bump_mode != 2) {
                claBumpActivity(ca[cr],                                                         // bump activity based on its
                                (config.opt_cls_act_bump_mode == 0 ? 1                          // constant
                                 : (config.opt_cls_act_bump_mode == 1) ? learnt_clause.size()  // size
                                 : nblevels              // LBD
                                ));
            } else {
                ca[cr].activity() = ca[cr].size() < config.opt_size_bounded_randomized ?       // if clause size is less than SBR
                                    ca[cr].size()                                              // use size as activity
                                    : config.opt_size_bounded_randomized + drand(random_seed);   // otherwise, use SBR
            }

        }
        ca[cr].setLBD(nblevels);
        if (nblevels <= 2) { nbDL2++; } // stats
        if (ca[cr].size() == 2) { nbBin++; } // stats
        if (learnt_clause.size() < 3) {ca[cr].setUsedInAnalyze(); ca[cr].setPropagated(); }  // this clause has been shared before, do not share it once more
        #ifdef CLS_EXTRA_INFO
        ca[cr].setExtraInformation(extraInfo);
        #endif
        attachClause(cr);

        // attach unit only, if  rer does allow it
        if (rerClause != rerDontAttachAssertingLit) {
            if (!isBiAsserting) {
                uncheckedEnqueue(learnt_clause[0], cr); // this clause is only unit, if OTFSS jumped to the same level!
                DOUT(if (config.opt_printDecisions > 1) cerr << "c enqueue literal " << learnt_clause[0] << " at level " <<  decisionLevel() << " from learned clause " << learnt_clause << endl;);
            } else {
                biAssertingPostCount++;
                lastBiAsserting = conflicts; // store number of conflicts for the last occurred bi-asserting clause so that the distance can be calculated
                isBiAsserting = false; // handled the current conflict clause, set this flag to false again
            }
        }
        DOUT(if (config.opt_printDecisions > 1) cerr << "c enqueue literal " << learnt_clause[0] << " at level " <<  decisionLevel() << " from learned clause " << learnt_clause << endl;);


        if (analyzeNewLearnedClause(cr)) {     // check whether this clause can be used to imply new backbone literals!
            ok = false; // found a contradtion
            return l_False;   // interupt search!
        }

    }
    return l_Undef;
}

void Solver::printSearchProgress()
{
    if (verbosity >= 1 && (verbEveryConflicts == 0 || conflicts % verbEveryConflicts == 0)) {
        printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% | \n",
               (int)starts, (int)nbstopsrestarts, (int)(conflicts / starts),
               (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals,
               (int)nbReduceDB, nLearnts(), (int)nbDL2, (int)nbRemovedClauses, progressEstimate() * 100);
    }
}

void Solver::clearOtfss(Solver::OTFSS& data)
{
    data.info.clear();
}


bool Solver::processOtfss(Solver::OTFSS& data)
{
    assert(decisionLevel() == 0 && "perform lazy otfss only on level 0");

    data.tmpPropagateLits.clear();
    if (data.info.size() > 0) { data.otfsss ++; }

    // mark all clauses that have been marked before
    for (int i = 0 ; i < data.info.size(); ++ i) {
        Clause& c = ca[ data.info[i].cr ];
        if (c.mark() != 0) { c.setLocked(); }
    }

    DOUT(if (config.debug_otfss) cerr << "c run OTFSS with trail: " << trail << endl;);

    // run over all clauses and delete the literal from the infomation block, if its still present (having a clause multiple times in the vector with different literals should be fine)
    for (int i = 0 ; i < data.info.size(); ++ i) {
        Clause& c = ca[ data.info[i].cr ];
        const Lit& removeLit = data.info[i].shrinkLit;

        if (c.size() < 2 || c.mark() != 0 || c.isLocked()) { continue; }   // ignore units and satified/marked clauses
        data.removedSat = (value(removeLit) == l_True) ? data.removedSat + 1 : data.removedSat ;

        DOUT(if (config.debug_otfss) cerr << "c OTFSS rewrite clause " << c << endl;);
        if (c.size() == 2) {
            if (c[0] == removeLit || c[1] == removeLit) {  // literal must not be in the clause anymore, because clause was in list multiple times
                const Lit other = toLit(toInt(c[0]) ^ toInt(c[1]) ^ toInt(removeLit));
                data.tmpPropagateLits.push(other);
                DOUT(if (config.debug_otfss) cerr << "c OTFSS-enqueue: " << other << endl;);
                data.otfssUnits ++; data.otfssClss++;
                // tell proof about reduced clause
                addCommentToProof("shrink to unit due to otfss");
                addUnitToProof(other);
                // clause will be removed later, when check for satisfied clauses is performed
            }
        } else if (c.size() == 3) {
            if (c[0] == removeLit || c[1] == removeLit || c[2] == removeLit) { // literal must not be in the clause anymore, because clause was in list multiple times
                detachClause(data.info[i].cr, true);   // is detached from all watch lists for longer clauses
                if (c[0] == removeLit) { c[0] = c[2]; c.pop(); }  // move last literal forward, shrink the clause
                else if (c[1] == removeLit) { c[1] = c[2]; c.pop(); }    // move last literal forward, shrink the clause
                else { c.pop(); }  // shrink the clause, remove the last literal
                assert(c.size() == 2 && "clause has to be binary now");
                attachClause(data.info[i].cr);   // added as binary watch again
                data.otfssBinaries ++; data.otfssClss++;
                // tell proof about reduced clause
                addCommentToProof("shrink ternary to binary due to otfss");
                addToProof(c);
                addToProof(c, true, removeLit);
                // remove clause, enqueue unit clause if necessary
                c.mark(1); // mark clause, so that its not processed twice, is undone afterwards again
                assert((value(c[0]) != l_False || value(c[1]) != l_False) && "otherwise the clause would have been found before");
                if (value(c[0]) == l_False) {
                    data.tmpPropagateLits.push(c[1]);
                    DOUT(if (config.debug_otfss) cerr << "c OTFSS-enqueue: " << c[1] << endl;);
                } else if (value(c[1]) == l_False) {
                    data.tmpPropagateLits.push(c[0]);
                    DOUT(if (config.debug_otfss) cerr << "c OTFSS-enqueue: " << c[0] << endl;);
                }
            }
        } else {

            for (int j = 0 ; j < c.size(); ++ j) {  // check the whole clause whether it contains the literal
                if (c[j] == removeLit) {  // we found the literal to be removed
                    data.otfssClss++;
                    c[j] = c[ c.size() - 1 ]; // move last literal forward
                    c.pop();                  // remove the literal
                    // tell proof about reduced clause
                    addCommentToProof("shrink clause due to otfss");
                    addToProof(c);
                    addToProof(c, true, removeLit);
                    // handle new clause
                    c.mark(1);                // mark clause, so that its not processed twice, is undone afterwards again
                    if (j < 2) {              // the literal was a watched literal

                        vec<Watcher>&  ws  = watches[ ~removeLit ];
                        for (int k = 0 ; k < ws.size(); ++ k) {
                            if (ws[k].cref() == data.info[i].cr) {
                                for (; k + 1 < ws.size(); ++k) { ws[k] = ws[k + 1]; } // copy all other elements forward TODO test fast remove
                                ws.pop(); // remove last element from list
                                break;
                            }
                        }

                        // check that we do not watch a literal that is false already
                        if (value(c[j]) == l_False) {
                            DOUT(if (config.debug_otfss) cerr << "c first watch was false:: " << c[ j ] << endl;);
                            int freePosition = 2;
                            for (; freePosition < c.size(); ++ freePosition) {
                                if (value(c[freePosition]) != l_False) { break; }
                            }
                            if (c.size() == freePosition) {  // unit
                                DOUT(if (config.debug_otfss) cerr << "c OTFSS-enqueue: " << c[ 1 - j ] << endl;);
                                data.tmpPropagateLits.push(c[ 1 - j ]);    // this clause became unit clause
                            } else { // move free literal forward before attaching clause again
                                const Lit tmpLit = c[j];  // swap literals
                                c[j] = c[freePosition];   // could remove the false literal here, but then the clause might become binary and would have to be placed in another watch list
                                c[freePosition] = tmpLit;
                            }
                        }

                        // attach to the list of the new literal
                        const Lit otherWatchedLit = c[ 1 - j ];                                 // the first two literals are the watched literals
                        DOUT(if (config.debug_otfss) cerr << "c add clause to watch list of literal " << ~c[j] << " with blocking lit " << otherWatchedLit << endl;);
                        watches[ ~ c[j] ].push(Watcher(data.info[i].cr, otherWatchedLit, 1));   // updates the blocking literal, add as long clause watch

                    }
                    break;
                }
            }

        }

        DOUT(if (config.debug_otfss) cerr << "c        into clause " << c << endl;);
    }

    // reset all marks of all the clauses that have not been marked before
    for (int i = 0 ; i < data.info.size(); ++ i) {
        Clause& c = ca[ data.info[i].cr ];
        if (!c.isLocked()) { c.mark(0); }  // remove mark flag again
        c.unlock();                        // unlock
    }

    // enqueue all found unit clauses, propagate afterwards (all new clauses are present already)
    for (int i = 0 ; i < data.tmpPropagateLits.size() ; ++ i) {
        const Lit& propLit = data.tmpPropagateLits[i];
        if (value(propLit) == l_False) {
            data.clearQueues(); // make sure that after processing otfss all data is removed.
            return true;
        } else if (value(propLit) == l_Undef) { uncheckedEnqueue(propLit); }
    }
    DOUT(if (config.debug_otfss) cerr << "c run OTFSS with trail after enqueue: " << trail << endl;);
    bool failed = propagate() != CRef_Undef;
    DOUT(if (config.debug_otfss) cerr << "c run OTFSS with trail after propagate: " << trail << endl;);
    if (failed) { ok = false; }

    data.clearQueues(); // make sure that after processing otfss all data is removed.
    return failed;
}

Solver::ConfigurationScheduler::ConfigurationScheduler()
    : lastConfigChangeConflict(0),
      currentConfig(0),
      growFactor(1)
{}


void Solver::ConfigurationScheduler::initConfigs(const Riss::Solver::SearchConfiguration& searchConfiguration, string schedule, float factor, int defaultC, int usualC)
{
    if (schedule.size() == 0) { return; }  // do not init anything

    growFactor = factor; // set factor

    // push default config with default conflict number
    searchConfigs.push(searchConfiguration);   // default object
    searchConfigConflicts.push(defaultC);

    SearchConfiguration sc; // temporary work object
    sc.var_decay = 0.95;  // only little update
    sc.var_decay_start = 0.95;
    sc.var_decay_end = 0.95;
    sc.restarts_type = 1; // luby
    sc.firstReduceDB = 30000;
    sc.incReduceDB   = 5000;
    sc.lbSizeMinimizingClause = 0;
    sc.use_reverse_minimization = false;

    searchConfigs.push(sc);    // add Minisat like configuration
    searchConfigConflicts.push(usualC);

    sc.var_decay = 0.8;
    sc.var_decay_start = 0.8;
    sc.var_decay_end = 0.85;
    sc.var_decay_inc = 0.005;
    sc.var_decay_distance = 1000;
    sc.restarts_type = 0;
    sc.firstReduceDB = 4000;
    sc.lbSizeMinimizingClause = 50;
    sc.use_reverse_minimization = false;
    sc.lbSizeReverseClause = 50;
    sc.lbLBDReverseClause = 20;

    searchConfigs.push(sc);    // add strong focus like configuration
    searchConfigConflicts.push(usualC);

//   cerr << "c schedule length: " << searchConfigs.size() << endl;
}

bool Solver::ConfigurationScheduler::checkAndChangeSearchConfig(int conflicts, Riss::Solver::SearchConfiguration& searchConfiguration)
{
    if (searchConfigs.size() == 0) { return false; }  // nothing to be done, no schedule

    const int diff = conflicts - lastConfigChangeConflict;

    // budget of this config is over?
    if (diff > searchConfigConflicts[ currentConfig ]) {

//     cerr << "c change config after " << conflicts << " conflicts" << endl;

        lastConfigChangeConflict = conflicts;

        currentConfig ++;
        if (currentConfig == searchConfigs.size()) {  // finished one round
            currentConfig = 0;  // set back to first (default)
            for (int i = 0 ; i < searchConfigConflicts.size(); ++ i) {   // increase all distances
                searchConfigConflicts[i] = (float) searchConfigConflicts[i] * growFactor;
            }
            searchConfiguration = searchConfigs[currentConfig]; // set current configuration
        }
        return true; // changed the configuration
    }

    return false;
}

void Solver::ConfigurationScheduler::reset(Riss::Solver::SearchConfiguration& searchConfiguration)
{
    if (searchConfigs.size() == 0) { return; }  // nothing to be done, no schedule
    lastConfigChangeConflict = 0;
    currentConfig = 0;
    searchConfiguration = searchConfigs[currentConfig];
}


} // namespace Riss
