/*
 * File:   Splitting.cc
 * Author: ahmed
 *
 * Created on December 23, 2012, 6:07 PM
 */
#include <math.h>

#include "pcasso/LookaheadSplitting.h"

#include "riss/mtl/Sort.h"
#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"

using namespace Riss;

namespace Pcasso
{

static const char* _cat = "LOOKAHEADSPLITTING";
static DoubleOption     opt_preselection_factor(_cat, "presel-fac",             "Factor for Preselection variables", 0.15, DoubleRange(0, false, 1, true));
static IntOption    opt_preselection_minimum(_cat, "presel-min",        "minimum number of preselection variables", 64, IntRange(1, 128));
static IntOption    opt_preselection_maximum(_cat, "presel-max",        "maximum number of preselection variables", 512, IntRange(128, 4096));
static IntOption    opt_failed_literals(_cat, "fail-lit",        "Use failed literals in splitting", 2, IntRange(0, 2));
static IntOption    opt_nec_assign(_cat, "nec-assign",        "Use necessary assignment (local learning) in splitting", 2, IntRange(0, 2));
static IntOption     opt_num_iterat(_cat, "num-iterat",         "Number of iterations to find more necessary assignments and failed literals", 3, IntRange(1, INT32_MAX));
static BoolOption    opt_double_lookahead(_cat, "double-la",        "Use double lookahead", true);
static DoubleOption     opt_double_lookahead_trigger_decay(_cat, "double-decay",             "The factor of total variables to trigger double lookahead", 0.95, DoubleRange(0, false, 1, true));
static IntOption    opt_constraint_resolvent(_cat, "con-resolv",        "Use of constraint resolvent in lookahead", 1, IntRange(0, 1));
static BoolOption    opt_tabu(_cat, "tabu",        "Tabu List for decision variables for scattering option", false);
static BoolOption    opt_bin_constraints(_cat, "bin-const",        "Sharing of binary constraints ", false);
static IntOption     opt_lookahead_heuristic(_cat, "la-heur",         "Lookahead heuristic to use", 4, IntRange(0, 7));
static IntOption     opt_preselection_heuristic(_cat, "presel-heur",         "Variable Preselection heuristic to use", 2, IntRange(0, 2));
static BoolOption    opt_adp_preselection_ranking(_cat, "adp-presel",        "Adaptive number of preselection variables", true);
static IntOption     opt_adp_preselection_L(_cat, "adp-preselL",         "Adaptive Preselection Constant L, minimum number of variables", 50, IntRange(1, INT32_MAX));
static IntOption     opt_adp_preselection_S(_cat, "adp-preselS",         "Adaptive Preselection Constant S, importance of failed literals", 7, IntRange(1, INT32_MAX));
static IntOption    opt_clause_learning(_cat, "clause-learn",        "Learn clause from failed Literal", 2, IntRange(0, 2));
static IntOption     opt_child_direction_priority(_cat, "dir-prior",         "0=Positive,1=Negative,2=higher reduction,3=lower reduction,4=random,5=phasefromsolver,6=adaptive", 3, IntRange(0, 6));
static DoubleOption     opt_child_direction_adaptive_factor(_cat, "dir-adp-fac",             "Adaptive Direction heuristic factor", 0.1, DoubleRange(0, false, 1, false));
static IntOption     opt_children_count(_cat, "child-count",         "Number of children", 8, IntRange(2, 16));
static BoolOption    opt_shrink_clauses(_cat, "shrk-clause",        "Shrink clauses in the starting of splitting ", true);
static IntOption    opt_pure_lit(_cat, "pure-lit",        "check for pure literals during clause shrinking; 0=no, 1=local,2=pass to splitting", 0, IntRange(0, 2));
static IntOption    opt_var_eq(_cat, "var-eq",        "check for variable equivalence 0=no, 1=check,2=local, 3=pass to splitting", 2, IntRange(0, 3));
static IntOption    opt_splitting_method(_cat, "split-method",        "Splitting method to use 0=simple, 1=scattering", 1, IntRange(0, 1));
static IntOption    opt_branch_depth(_cat, "split-depth",        "Number of decision per splitting", 0, IntRange(0, INT32_MAX));
static BoolOption    opt_dSeq(_cat, "dseq",        "Use d sequence proposed by Anntti; to use this feature fix -split-depth=0", true);
static DoubleOption    opt_branch_threshold(_cat, "split-thres",        "Split Depth threshold", 1, DoubleRange(0, true, INT32_MAX, true));
static DoubleOption     opt_branch_threshold_penalty(_cat, "split-penal",             "The penality factor if selected literal turns out to be failed literal", 0.7, DoubleRange(0, false, 1, true));
static DoubleOption     opt_branch_threshold_increment(_cat, "thres-inc",             "Increment threshold by factor", 0.05, DoubleRange(0, true, 1, true));
static IntOption    opt_hscore_accuracy(_cat, "h-acc",        "hScore accuracy; number of iterations", 3, IntRange(1, 32));
static IntOption    opt_hscore_maxclause(_cat, "h-maxcl",        "hScore max clause size", 7, IntRange(1, 32));
static IntOption    opt_hscore_clause_weight(_cat, "h-cl-wg",        "hScore clause weight", 5, IntRange(1, 32));
static DoubleOption     opt_hscore_clause_upperbound(_cat, "h-upper",             "Upper bound for hscore of a literal", 10900, DoubleRange(0, false, INT32_MAX, true));
static DoubleOption     opt_hscore_clause_lowerbound(_cat, "h-lower",             "lower bound for hscore of a literal", 0.1, DoubleRange(0, false, INT32_MAX, true));
static BoolOption    opt_sort_split(_cat, "sort-split",        "Sort the splits according to their trail score", false);

double LookaheadSplitting::triggerDoubleLookahead = 0;
double LookaheadSplitting::branchThreshold = 0;
unsigned LookaheadSplitting::constraintResolventClSize = 3;
unsigned LookaheadSplitting::dSeq[] = {4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 2, 2, 2, 1, 0};
unsigned LookaheadSplitting::countFailedLiterals = 0;
unsigned LookaheadSplitting::countLookaheadDecisions = 0;

LookaheadSplitting::LookaheadSplitting(CoreConfig& config):

    SplitterSolver(config) ,
    coreConfig(config),

    timeOut(-1),
    maxClauseSize(0),
    optNumChild(opt_children_count),
    optBranchDepth(opt_branch_depth)
{
    splitting = new vec<vec<Lit>* >();
    validAtLevel = new vec<vec<vec<Lit>* >* >();

    //statistics
    splitterMaxPreselectedVariablesID = statistics.reregisterI("splitterMaxPreselectedVariables");
    splitterTotalTimeID = statistics.reregisterD("splitterTotalTime");
    splitterTotalDecisionsID = statistics.reregisterI("splitterTotalDecisions");
    splitterPenaltyID = statistics.reregisterI("splitterPenalty");
    splitterTotalPureLiteralsID = statistics.reregisterI("splitterTotalPureLiterals");;
    splitterTotalFailedLiteralsID = statistics.reregisterI("splitterTotalFailedLiterals");
    splitterTotalNecessaryAssignmentsID = statistics.reregisterI("splitterTotalNecessaryAssignments");
    splitterTotalClauseLearntUnitsID = statistics.reregisterI("splitterTotalClauseLearntUnits");
    splitterTotalLocalLearningsID = statistics.reregisterI("splitterTotalLocalLearnings");
    splitterTotalLiteralEquivalencesID = statistics.reregisterI("splitterTotalLiteralEquivalences");
    splitterTotalDoubleLookaheadCallsID = statistics.reregisterI("splitterTotalDoubleLookaheadCalls");
    splitterTotalDoubleLookaheadSuccessID = statistics.reregisterI("splitterTotalDoubleLookaheadSuccess");
}

LookaheadSplitting::~LookaheadSplitting()
{
}

void LookaheadSplitting::setTimeOut(double to)
{
    timeOut = to;
}

lbool LookaheadSplitting::produceSplitting(vec<vec<vec<Lit>* >* > **splits, vec<vec<Lit>* > **valid)
{
    varFlags.copyTo(phaseSaving);

    tabuList.growTo(nVars(), false);
    numNegLitTerClause.growTo(nVars(), 0);
    numPosLitTerClause.growTo(nVars(), 0);
    negHScore.growTo(nVars(), 0);
    posHScore.growTo(nVars(), 0);
    watcherNegLitSize.growTo(nVars(), 0);
    watcherPosLitSize.growTo(nVars(), 0);
    markArray.resize(2 * nVars());

    lock();
    if (branchThreshold == 0)
    { branchThreshold = opt_branch_threshold; }
    unlock();
    //lock();
    //if(triggerDoubleLookahead <= 0)
    //  triggerDoubleLookahead = 0.17*nVars();
    //unlock();
    // create vector
    *splits = new vec<vec<vec<Lit>* >* >();
    *valid = new vec<vec<Lit>* >();

    // Simplify the set of problem clauses:
    if (decisionLevel() == 0 && !simplify()) {
        //fprintf(stderr, "splitter: Unsat node 1\n");
        return l_False;
    }

    // propagate top level
    CRef confl = propagate();
    if (confl != CRef_Undef) { // if there is a conflict on the top level, there cannot be a split
        // fprintf( stderr, "splitting the instance failed with a conflict during propagation\n" );
        // fprintf(stderr, "splitter: Unsat node 2\n");
        return l_False;
    }

    // chck if solution found.... extend the model if found
    if (checkSolution())
    { return l_True; }

    //int bestK = 0; //size of the subset of the variables on which lookahead will be performed
    vec<Lit> *decList = 0;
    vec<vec<Lit>*> *localValid = 0;
    Lit next = lit_Undef;
    int initTrailSize = trail.size();
    vec<VarScore> splittingScore; //storing the number of assigned literals as score for each index of splitting

    if (opt_splitting_method == 0) {
        optNumChild = 1;
        decList = new vec<Lit>();
        //vec<Lit> comDec; //common decisionss
        vec<bool> triedVar;
        triedVar.growTo(nVars(), 0);
        //int flip=0;
        //int depth=0;
        vec<vec<vec<Lit>*>*> *validList = new vec<vec<vec<Lit>*>*>();;
        localValid = new vec<vec<Lit>* >();

        shrinkClauses();
        preselectionHeuristic();

        do {
            next = lit_Undef;
            // Simplify the set of problem clauses:
            //branchThreshold=opt_branchThreshold;
            if (decisionLevel() == 0 && !simplify()) {
                if (splitting->size() == 0 && decList->size() == 0) {
                    //fprintf(stderr, "splitter: Unsat node 3\n");
                    return l_False;
                } else
                { break; }
            }

            // propagate top level
            CRef confl = propagate();
            if (confl != CRef_Undef) { // if there is a conflict on the top level, there cannot be a split
                if (splitting->size() == 0 && decList->size() == 0) {
                    //fprintf(stderr, "splitter: Unsat node 4\n");
                    return l_False;
                } else
                { break; }
            }
            int prevLocalValidSize = 0;
            // flip=(flip+1)%2;
            // chck if solution found.... extend the model if found
            if (checkSolution())
            { return l_True; }

            vec<Lit> *tempDecList = decList;
            decList = new vec<Lit>();
            tempDecList->copyTo(*decList);
            //tempDecList->clear(true);                 //do not clear ..... as it is used the list of decision for the previous
            while (decList->size() > 0 && triedVar[var(decList->last())]) {
                cancelUntil(decisionLevel() - 1);
                triedVar[var(decList->last())] = false;
                decList->pop();
                validList->pop();
                //fprintf(stderr, "next = %d\n", sign(next) ? 0-var(next) : var(next));
                if (decList->size() == 0)
                { goto jump; }
            }

            learntsLimitPop();

            if (decList->size() > 0) {
                cancelUntil(decisionLevel() - 1);
                next = ~(decList->last());
                //fprintf(stderr, "flip = %d \n ", sign(next) ? 0-var(next) : var(next));
                decList->pop();
                decisions++;
                newDecisionLevel();
                uncheckedEnqueue(next);
                decList->push(next);
                learntsLimitPush();
                triedVar[var(next)] = true;
                localValid = new vec<vec<Lit>* >();
                //validList->pop();
                if (validList->size() > 1)
                { (*validList)[validList->size() - 1]->copyTo(*localValid); }
                //validList->push(localValid);
            }
            int level = decisionLevel();
            bool check1 = true;
            while (check1) {
                if (asynch_interrupt) { return l_Undef; } // Davide
                //fprintf(stderr, "decList size = %d\n",decList->size());
                //fprintf(stderr, "validList size = %d\n",validList->size());
                // Simplify the set of problem clauses:
                if (decisionLevel() == 0 && !simplify()) {
                    if (splitting->size() == 0 && decList->size() == 0) {
                        //fprintf(stderr, "splitter: Unsat node 6\n");
                        return l_False;
                    }
                    goto jump;
                }
                next = lit_Undef;
                prevLocalValidSize = localValid->size();
                CRef confl = propagate();
                if (confl != CRef_Undef) { // failed literal
                    if (decisionLevel() == 0) {
                        //fprintf( stderr, "splitter: Unsat node\n");
                        if (splitting->size() == 0 && decList->size() == 0) {
                            //fprintf(stderr, "splitter: Unsat node 5\n");
                            return l_False;
                        } else
                        { goto jump; }
                    } else {
                        cancelUntil(decisionLevel() - 1);
                        //depth--;
                        next = ~decList->last();
                        uncheckedEnqueue(next);
                        //  enqueue(next);
                        vec<Lit> *c = new vec<Lit>();
                        c->push(next);
                        decList->pop();
                        validList->pop();
                        //comDec.pop();
                        localValid->push(c);
                        learntsLimitPop();
                        lock();
                        branchThreshold *= opt_branch_threshold_penalty;
                        countFailedLiterals++;
                        countLookaheadDecisions--;
                        unlock();
                        statistics.changeI(splitterPenaltyID, 1);
                        //fprintf(stderr, "splitter: Penalty\n");
                    }
                    continue;
                }

                if (checkSolution())
                { return l_True; }
                if (optBranchDepth == 0) {
                    // branchThreshold+=(localValid->size()-prevLocalValidSize)/nVars();
                    //if((trail.size()-initTrailSize)*decisionLevel()*decisionLevel()*opt_children_count>branchThreshold*nVars() || decisionLevel()>=maxClauseSize)
                    if ((trail.size() - initTrailSize)*decisionLevel()*decisionLevel()*log(512000) > branchThreshold * (nVars() - initTrailSize) || decisionLevel() >= maxClauseSize - 1) {
                        check1 = false;
                        if (decisionLevel() >= maxClauseSize - 1) {
                            lock();
                            branchThreshold *= opt_branch_threshold_penalty;
                            unlock();
                            statistics.changeI(splitterPenaltyID, 1);
                        }
                        continue;
                    }

                } else {
                    if (decisionLevel() >= optBranchDepth) {
                        check1 = false;
                        continue;
                    }
                }
                next = pickBranchLiteral(&localValid);
                if (next == lit_Error) {
                    if (decisionLevel() == 0) {
                        //fprintf( stderr, "splitter: Unsat node\n");
                        if (splitting->size() == 0 && decList->size() == 0) {
                            //fprintf(stderr, "splitter: Unsat node 7\n");
                            return l_False;
                        } else
                        { goto jump; }
                    } else {
                        cancelUntil(decisionLevel() - 1);
                        //depth--;
                        next = ~(*decList)[decisionLevel()];
                        uncheckedEnqueue(next);
                        //enqueue(next);
                        decList->pop();
                        validList->pop();
                        //comDec.pop();
                        vec<Lit> *c = new vec<Lit>();
                        c->push(next);
                        localValid->push(c);
                        learntsLimitPop();
                        lock();
                        branchThreshold *= opt_branch_threshold_penalty;
                        countFailedLiterals++;
                        countLookaheadDecisions--;
                        unlock();
                        statistics.changeI(splitterPenaltyID, 1);
                        // fprintf(stderr, "splitter: Penalty\n");
                        continue;
                    }
                } else if (next == lit_Undef) {
                    if (checkSolution())
                    { return l_True; }
                }

                fprintf(stderr, "%d, ", sign(next) ? 0 - var(next) : var(next));
                //depth++;
                decisions++;
                newDecisionLevel();
                uncheckedEnqueue(next);
                decList->push(next);
                learntsLimitPush();
                //comDec.push(next);
                validList->push(localValid);
                vec<vec<Lit>*> *tempValid = localValid;
                localValid = new vec<vec<Lit>* >();
                tempValid->copyTo(*localValid);
                if (cpuTime_t() > timeOut) {
                    lock();
                    branchThreshold *= opt_branch_threshold_penalty;
                    unlock();
                    statistics.changeI(splitterPenaltyID, 1);
                    break;
                }
            }

            if (decList->size() > 0 && decisionLevel() - level >= 0) {
                if (splitting->size() > 0) {
                    vec<Lit> *alphaList = (*splitting)[0];
                    Lit alpha = (*alphaList)[0];
                    if (var((*decList)[0]) != var(alpha))
                    { goto jump; }
                }
                //pushing pure literals
                if (opt_pure_lit > 1) {
                    for (int i = 0; i < pureLiterals.size(); i++) {
                        vec<Lit> *c = new vec<Lit>();
                        c->push(pureLiterals[i]);
                        localValid->push(c);
                    }
                }
                validAtLevel->push(localValid);
                splitting->push(decList);
                splittingScore.push(VarScore(splitting->size() - 1, trail.size()));
                //  next=comDec.last();
                //comDec.pop();
                optNumChild++;
            }
            //branchThreshold *=(1.0f+opt_branchThreshold_increment);
            //if(cpuTime_t()>to)      break;
        } while (decisionLevel() > 0);
    }

    if (opt_splitting_method == 1) {
        shrinkClauses();
        while (optNumChild - 1 > 0) {
            // Simplify the set of problem clauses:
            //branchThreshold=opt_branchThreshold;
            if (decisionLevel() == 0 && !simplify()) {
                if (splitting->size() == 0 && decList->size() == 0) {
                    //fprintf(stderr, "splitter: Unsat node 3\n");
                    return l_False;
                } else
                { break; }
            }

            // propagate top level
            CRef confl = propagate();
            if (confl != CRef_Undef) { // if there is a conflict on the top level, there cannot be a split
                if (splitting->size() == 0 && decList->size() == 0) {
                    //fprintf(stderr, "splitter: Unsat node 4\n");
                    return l_False;
                } else
                { break; }
            }

            // chck if solution found.... extend the model if found
            if (checkSolution())
            { return l_True; }
            bool check1 = true;

            shrinkClauses();
            preselectionHeuristic();
            localValid = new vec<vec<Lit>* >();
            decList = new vec<Lit>();
            clearLearnts();
            while (check1) {
                if (asynch_interrupt) { return l_Undef; } // Davide
                next = lit_Undef;
                CRef confl = propagate();
                if (confl != CRef_Undef) { // failed literal
                    if (decisionLevel() == 0) {
                        //fprintf( stderr, "splitter: Unsat node\n");
                        if (splitting->size() == 0 && decList->size() == 0) {
                            //fprintf(stderr, "splitter: Unsat node 5\n");
                            return l_False;
                        } else
                        { goto jump; }
                    } else {
                        cancelUntil(decisionLevel() - 1);
                        next = ~(*decList)[decisionLevel()];
                        uncheckedEnqueue(next);
                        //  enqueue(next);
                        vec<Lit> *c = new vec<Lit>();
                        if (opt_tabu)
                        { tabuList[var(next)] = false; }
                        c->push(next);
                        decList->pop();
                        localValid->push(c);
                        learntsLimitPop();
                        lock();
                        branchThreshold *= opt_branch_threshold_penalty;
                        countFailedLiterals++;
                        countLookaheadDecisions--;
                        unlock();
                        statistics.changeI(splitterPenaltyID, 1);
                        //fprintf(stderr, "splitter: Penalty\n");
                    }
                    continue;
                }
                // Simplify the set of problem clauses:
                if (decisionLevel() == 0 && !simplify()) {
                    if (splitting->size() == 0 && decList->size() == 0) {
                        //fprintf(stderr, "splitter: Unsat node 6\n");
                        return l_False;
                    }
                    goto jump;
                }

                if (checkSolution())
                { return l_True; }
                next = pickBranchLiteral(&localValid);
                if (next == lit_Error) {
                    if (decisionLevel() == 0) {
                        //fprintf( stderr, "splitter: Unsat node\n");
                        if (splitting->size() == 0 && decList->size() == 0) {
                            //fprintf(stderr, "splitter: Unsat node 7\n");
                            return l_False;
                        } else
                        { goto jump; }
                    } else {
                        cancelUntil(decisionLevel() - 1);
                        next = ~(*decList)[decisionLevel()];
                        uncheckedEnqueue(next);
                        //enqueue(next);
                        decList->pop();
                        vec<Lit> *c = new vec<Lit>();
                        if (opt_tabu)
                        { tabuList[var(next)] = false; }
                        c->push(next);
                        localValid->push(c);
                        learntsLimitPop();
                        lock();
                        branchThreshold *= opt_branch_threshold_penalty;
                        countFailedLiterals++;
                        countLookaheadDecisions--;
                        unlock();
                        statistics.changeI(splitterPenaltyID, 1);
                        // fprintf(stderr, "splitter: Penalty\n");
                        continue;
                    }
                } else {
                    if (next == lit_Undef) {
                        if (checkSolution())
                        { return l_True; }
                    }
                    if (decisionLevel() == 0 && splitting->size() == 0) {
                        //localValid->copyTo(**valid);
                    }
                }

                // fprintf(stderr, "next%d = %d\n", optNumChild, sign(next) ? 0-var(next) : var(next));
                decisions++;
                newDecisionLevel();
                uncheckedEnqueue(next);
                decList->push(next);
                learntsLimitPush();

                if (optBranchDepth == 0) {
                    if (opt_dSeq) {
                        if (decisionLevel() >= dSeq[16 - opt_children_count + splitting->size()])
                        { check1 = false; }
                    } else { // branchThreshold+=(localValid->size()-prevLocalValidSize)/nVars(); //if((trail.size()-initTrailSize)*decisionLevel()*decisionLevel()*opt_children_count>branchThreshold*nVars() || decisionLevel()>=maxClauseSize)
                        if ((trail.size() - initTrailSize)*decisionLevel()*decisionLevel()*log(512000) > branchThreshold * (nVars() - initTrailSize)*log(optNumChild) || decisionLevel() >= maxClauseSize - 1)
                        { check1 = false; }
                        if (decisionLevel() >= maxClauseSize - 1) {
                            lock();
                            branchThreshold *= opt_branch_threshold_penalty;
                            unlock();
                            statistics.changeI(splitterPenaltyID, 1);
                        }
                    }
                } else {
                    if (decisionLevel() >= optBranchDepth)
                    { check1 = false; }
                }
                if (cpuTime_t() > timeOut)      { break; }
            }

            validAtLevel->push(localValid);
            splitting->push(decList);
            splittingScore.push(VarScore(splitting->size() - 1, trail.size()));
            optNumChild--;
            cancelUntil(0);
            vec<Lit> negCube;
            for (int i = 0; i < decList->size(); i++) {
                negCube.push(~((*decList)[i]));
            }
            addClause_(negCube);
            if (cpuTime_t() > timeOut)      { break; }
        }
    }

    if (opt_splitting_method == 1) {
        splittingScore.push(VarScore(splitting->size(), trail.size()));
    }
jump:
    vec<Lit> *clause;
    vec< vec<Lit>* >* childClauses;
    if (opt_sort_split) {
        sort(splittingScore);
    }
    for (int sortedIndex = 0; sortedIndex < splittingScore.size(); sortedIndex++) {
        int i = splittingScore[sortedIndex].var;
        assert(i <= splitting->size());
        childClauses = new vec< vec<Lit>* >();

        if (opt_splitting_method == 1) {
            for (int j = 0; j < i; j++) {
                decList = new vec<Lit>();
                (*splitting)[j]->copyTo(*decList);
                clause = new vec<Lit>();
                for (int k = 0; k < decList->size(); k++) {
                    clause->push(~((*decList)[k]));
                }
                childClauses->push(clause);
            }
        }
        if (opt_splitting_method == 1 && i == splitting->size()) {
            (*splits)->push(childClauses);
            Debug::PRINT_NOTE("Child-");
            Debug::PRINT_NOTE(i + 1);
            Debug::PRINTLN_NOTE(":");
            continue;
        }
        decList = new vec<Lit>();
        (*splitting)[i]->copyTo(*decList);
        Debug::PRINT_NOTE("Child-");
        Debug::PRINT_NOTE(i + 1);
        Debug::PRINTLN_NOTE(":");
        Debug::PRINTLN_NOTE(*decList);
        for (int j = 0; j < decList->size(); j++) {
            clause = new vec<Lit>();
            clause->push((*decList)[j]);
            childClauses->push(clause);
//            fprintf(stderr, "%s%d, ", sign((*decList)[j]) ? "-": "",var((*decList)[j])+1);
        }

        localValid = new vec<vec<Lit>* >();
        (*validAtLevel)[i]->copyTo(*localValid);
        Debug::PRINT_NOTE(" Valids count: ");
        Debug::PRINTLN_NOTE(localValid->size());
        for (int k = 0; k < localValid->size(); k++) {
            clause = new vec<Lit>();
            (*localValid)[k]->copyTo(*clause);
            childClauses->push(clause);
        }
        //fprintf(stderr, "splitter: Variable Selection for Child#%d \t\t\t = %d + %d\n", i+1, decList->size(), localValid->size());
        statistics.changeI(splitterTotalDecisionsID, decList->size());

        (*splits)->push(childClauses);
    }

    /*if(opt_splitting_method==1){
        childClauses= new vec< vec<Lit>* >();
        for(int j=0;j<splitting->size();j++){
                decList = new vec<Lit>();
                (*splitting)[j]->copyTo(*decList);
                clause = new vec<Lit>();
                for(int k=0;k<decList->size();k++){
                    clause->push(~(*decList)[k]);
                }
                childClauses->push(clause);
            }

        (*splits)->push(childClauses);
    }*/
    Debug::PRINT_NOTE("Max Clause Size = ");
    Debug::PRINTLN_NOTE(maxClauseSize);
    /* if(opt_failed_literals)
        //fprintf(stderr, "splitter: Failed Literal \t\t\t = %d\n", failedLiterals.size());
     if(opt_nec_assign)
        //fprintf(stderr, "splitter: Necessay Assignments \t\t\t = %d\n", necAssign.size());
     if(opt_double_lookahead)
        //fprintf(stderr, "splitter: DoubleLookahead Binary Clauses \t = %d \\ %d\n", temp, binaryForcedClauses.size()/2);
     if(opt_clause_learning)
        //fprintf(stderr, "splitter: Clauses Learnt \t\t\t = %d \n", learnts.size());

     //fprintf(stderr, "splitter: Time taken \t\t\t = %f \n", cpuTime_t());
     */
    decList->clear(true);
    localValid->clear(true);
    clearLearnts();
    lock();
    if (opt_splitting_method == 1) {
        if (cpuTime_t() > timeOut && opt_children_count - (*splits)->size() != 0)
        { branchThreshold *= opt_branch_threshold_penalty / (opt_children_count - (*splits)->size()); }
    }
    branchThreshold *= (1.0f + opt_branch_threshold_increment);
    unlock();
    statistics.changeD(splitterTotalTimeID, cpuTime_t());
    return l_Undef;
}

bool LookaheadSplitting::checkSolution()
{
    if (order_heap.empty()) {
        // Model found
        // extend the model (copied from solve method)
        //fprintf(stderr, "splitter: Solution Found\n");
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) { model[i] = value(i); }
        return true;
    }
    return false;
}

void LookaheadSplitting::shrinkClauses()
{
    if (opt_shrink_clauses) {
        assert(decisionLevel() == 0);

        removeSatisfied(learnts);
        if (remove_satisfied)
        { removeSatisfied(clauses); }
        vec<unsigned> pureLiteralCheck;
        pureLiteralCheck.growTo(nVars(), 0);
        Var v;
        Lit l;

        for (int i = 0, j = 0, k = 0; i < clauses.size(); i++) {
            Clause& c = ca[clauses[i]];
            for (j = 0, k = 0; j < c.size(); j++) {
                if (value(c[j]) != l_False) {
                    c[k] = c[j];
                    k++;
                    if (opt_pure_lit > 0) {
                        if (sign(c[j]) && value(c[j]) == l_Undef) {
                            v = var(c[j]);
                            if (pureLiteralCheck[v] != 1 && pureLiteralCheck[v] != 3)
                            { pureLiteralCheck[v] += 1; }
                        } else if (!sign(c[j]) && value(c[j]) == l_Undef) {
                            v = var(c[j]);
                            if (pureLiteralCheck[v] != 2 && pureLiteralCheck[v] != 3)
                            { pureLiteralCheck[v] += 2; }
                        }
                    }
                }
            }
            c.shrink(j - k);
        }

        if (opt_pure_lit > 0) {
            for (int i = 0; i < pureLiteralCheck.size(); i++) {
                if (pureLiteralCheck[i] == 1) {
                    l = mkLit(i, true);
                    pureLiterals.push(l);
                    uncheckedEnqueue(l);
                    statistics.changeI(splitterTotalPureLiteralsID, 1);
                } else if (pureLiteralCheck[i] == 2) {
                    l = mkLit(i, false);
                    pureLiterals.push(l);
                    uncheckedEnqueue(l);
                    statistics.changeI(splitterTotalPureLiteralsID, 1);
                }
            }
            //fprintf(stderr,"splitter: Found pure literals : %d\n",pureLiterals.size());
        }

        checkGarbage();
        rebuildOrderHeap();
    }
}

void LookaheadSplitting::clearLearnts()
{
    //fprintf(stderr, "Learnts Size: %d\n", learnts.size());
    for (int i = 0; i < learnts.size(); i++) {
        removeClause(learnts[i]);
    }
    learnts.clear();
    learntsLimit.clear();
    checkGarbage();
}

void LookaheadSplitting::preselectionHeuristic()
{
    int numBinClauses = 0;
    int numTerClauses = 0;

    double *negLitScore = new double[nVars()];
    double *posLitScore = new double[nVars()];
    int *numNegLitClause = new int[nVars()];
    int *numPosLitClause = new int[nVars()];
    vec<VarScore> varScore;
    sortedVar.clear(); // clear the vector with sorted variables
    assert( sortedVar.size() == 0 && "yet there should not be any variables in the preselection" );
    for (int i = 0; i < nVars(); i++) {
        posLitScore[i] = 0;
        negLitScore[i] = 0;
        numPosLitTerClause[i] = 0;
        numNegLitTerClause[i] = 0;
        numPosLitClause[i] = 0;
        numNegLitClause[i] = 0;
        Lit q = mkLit(i, false);
        watcherPosLitSize[i] = watches[q].size();
        watcherNegLitSize[i] = watches[~q].size();
    }

    for (int i = 0; i < clauses.size(); i++) {
        Clause& c = ca[clauses[i]];
        if (c.size() > maxClauseSize)
        { maxClauseSize = c.size(); }
        if (opt_preselection_heuristic == 0 && c.size() == 2 && !satisfied(c)) {
            numBinClauses++;
            sign(c[0]) ? negLitScore[var(c[0])]++ : posLitScore[var(c[0])]++;
            sign(c[1]) ? negLitScore[var(c[1])]++ : posLitScore[var(c[1])]++;
        } else if (c.size() == 3 && !satisfied(c)) {
            numTerClauses++;
            sign(c[0]) ? numNegLitTerClause[var(c[0])]++ : numPosLitTerClause[var(c[0])]++;
            sign(c[1]) ? numNegLitTerClause[var(c[1])]++ : numPosLitTerClause[var(c[1])]++;
            sign(c[2]) ? numNegLitTerClause[var(c[2])]++ : numPosLitTerClause[var(c[2])]++;
        }
    }

    if (opt_preselection_heuristic == 0) { //propz preselection heuristic
        for (int i = 0; i < nVars(); i++) {
            if (posLitScore[i] > 0 && negLitScore[i] > 0) {
                VarScore *vs = new VarScore(i, posLitScore[i]*negLitScore[i]);
                vs->var = i;
                vs->score = posLitScore[i] * negLitScore[i] + posLitScore[i] + negLitScore[i];
                varScore.push(*vs);
            }
        }
        sort(varScore);
	assert( varScore.size() <= nVars() && "cannot add variables" );
        for (int i = varScore.size() - 1; i >= 0; i--) {
            sortedVar.push(varScore[i].var);
        }
        //fprintf( stderr, "Number of binary clauses \t\t\t = %d\n", numBinClauses);
        //fprintf( stderr, "Number of preselected variables \t\t\t = %d / %d\n", bestK, nVars());
        //fprintf( stderr, "First Item score = %f Last Item score =%f\n", varScore[0].score, varScore[bestK-1].score);

    } else if (opt_preselection_heuristic == 1) { //Clause Reduction Approximation
        for (int i = 0; i < clauses.size(); i++) {
            Clause& c = ca[clauses[i]];
            if (c.size() > 2) {
                for (int j = 0; j < c.size(); j++) {
                    sign(c[j]) ? numNegLitClause[var(c[j])]++ : numPosLitClause[var(c[j])]++;
                }
            }
        }
        for (int i = 0; i < clauses.size(); i++) {
            Clause& c = ca[clauses[i]];
            if (c.size() == 2 && !satisfied(c)) {
                numBinClauses++;
                if (sign(c[0]))
                { sign(c[1]) ? negLitScore[var(c[0])] += numPosLitClause[var(c[1])] : negLitScore[var(c[0])] += numNegLitClause[var(c[1])]; }
                else
                { sign(c[1]) ? posLitScore[var(c[0])] += numPosLitClause[var(c[1])] : posLitScore[var(c[0])] += numNegLitClause[var(c[1])]; }

                if (sign(c[1]))
                { sign(c[0]) ? negLitScore[var(c[1])] += numPosLitClause[var(c[0])] : negLitScore[var(c[1])] += numNegLitClause[var(c[0])]; }
                else
                { sign(c[0]) ? posLitScore[var(c[1])] += numPosLitClause[var(c[0])] : posLitScore[var(c[1])] += numNegLitClause[var(c[0])]; }
            }
        }

        for (int i = 0; i < nVars(); i++) {
            if (posLitScore[i] > 0 && negLitScore[i] > 0) {
                VarScore *vs = new VarScore(i, posLitScore[i]*negLitScore[i]);
                vs->var = i;
                vs->score = posLitScore[i] * negLitScore[i] + posLitScore[i] + negLitScore[i];
                varScore.push(*vs);
            }
        }
        sort(varScore);
        for (int i = varScore.size() - 1; i >= 0; i--) {
            //   //fprintf( stderr, "%f, ", varScore[i].score);
            sortedVar.push(varScore[i].var);
        }
        //fprintf( stderr, "Number of binary clauses \t\t\t = %d\n", numBinClauses);
        //fprintf( stderr, "Number of preselected variables \t\t\t = %d / %d\n", bestK, nVars());
        //fprintf( stderr, "First Item score = %f Last Item score =%f\n", varScore[varScore.size()-1].score, varScore[varScore.size()-1-bestK].score);
    } else if (opt_preselection_heuristic == 2) { //Recursive Weight Heuristic
        vec<double> prevNegHScore;
        vec<double> prevPosHScore;

        prevNegHScore.growTo(nVars(), 1.0f);
        prevPosHScore.growTo(nVars(), 1.0f);

        double sum = 2 * nVars();

        for (int i = 1; i <= opt_hscore_accuracy; i++) {
            negHScore.clear();
            posHScore.clear();
            negHScore.growTo(nVars(), 0.0f);
            posHScore.growTo(nVars(), 0.0f);
            double avg = sum / (2 * nVars());
            vec<double> constant;
            constant.growTo(opt_hscore_maxclause + 1, 0.0f);
            for (int j = 2; j <= opt_hscore_maxclause; j++) {
                constant[j] = pow((int)opt_hscore_clause_weight, (int)opt_hscore_maxclause - j) / (pow(avg, (double)j - 1));
            }
            for (int k = 0; k < clauses.size(); k++) {
                Clause& c = ca[clauses[k]];
                if (c.size() <= opt_hscore_maxclause && !satisfied(c)) {
                    double product = constant[c.size()];
                    for (int l = 0; l < c.size(); l++) {
                        product = product * (sign(c[l]) ? prevPosHScore[var(c[l])] : prevNegHScore[var(c[l])]);
                    }
                    for (int l = 0; l < c.size(); l++) {
                        if (sign(c[l]))
                        { negHScore[var(c[l])] += (product / prevNegHScore[var(c[l])]); }
                        else
                        { posHScore[var(c[l])] += (product / prevPosHScore[var(c[l])]); }
                    }
                }
            }
            if (i == opt_hscore_accuracy) //to skip the normalization in the last iteration
            { break; }
            sum = 0;
            for (int k = 1; k < nVars(); k++) {
                if (negHScore[k] < opt_hscore_clause_lowerbound)
                { negHScore[k] = opt_hscore_clause_lowerbound; }
                if (negHScore[k] > opt_hscore_clause_upperbound)
                { negHScore[k] = opt_hscore_clause_upperbound; }
                if (posHScore[k] < opt_hscore_clause_lowerbound)
                { posHScore[k] = opt_hscore_clause_lowerbound; }
                if (posHScore[k] > opt_hscore_clause_upperbound)
                { posHScore[k] = opt_hscore_clause_upperbound; }
                sum += negHScore[k] + posHScore[k];
            }
            negHScore.copyTo(prevNegHScore);
            posHScore.copyTo(prevPosHScore);
        }
        for (int i = 1; i < nVars(); i++) {
            if (negHScore[i] > 0 && posHScore[i] > 0) {
                VarScore *vs = new VarScore(i, negHScore[i]*posHScore[i]);
                vs->var = i;
                vs->score = log(negHScore[i]) + log(posHScore[i]);
                varScore.push(*vs);
            }
        }
        sort(varScore);
        for (int i = varScore.size() - 1; i >= 0; i--) {
            //   //fprintf( stderr, "%f, ", varScore[i].score);
            sortedVar.push(varScore[i].var);
        }
    }

    if (opt_preselection_heuristic == 3) {
        while (!order_heap.empty()) {
            int v = order_heap.removeMin();
            if (value(v) == l_Undef)
            { sortedVar.push(v); }
        }
        rebuildOrderHeap();
    }

    delete[] negLitScore;
    delete[] posLitScore;
    delete[] numNegLitClause;
    delete[] numPosLitClause;
    varScore.clear(true);

}

void LookaheadSplitting::preselectVar(vec<int>& sv, vec<int>& bkl)
{
    assert( sv.size() < nVars() && "cannot work on more variables than occuring in the formula" );
    int bestK;
    if (opt_adp_preselection_ranking) {
        bestK = opt_adp_preselection_L + opt_adp_preselection_S * (countFailedLiterals / countLookaheadDecisions);
    } else {
        bestK = (int)(sv.size() * opt_preselection_factor);
    }

    bkl.clear();
    if (!opt_adp_preselection_ranking && bestK < opt_preselection_minimum)
    { bestK = opt_preselection_minimum; }
    if (bestK > opt_preselection_maximum)
    { bestK = opt_preselection_maximum; }
    if (bestK > sv.size())
    { bestK = sv.size(); }
    for (int i = 0; i < sv.size() && bestK > 0; i++) {
        //   //fprintf( stderr, "%f, ", varScore[i].score);
        if (value(sv[i]) == l_Undef) {
            if (opt_tabu && opt_splitting_method == 1) {
                if (!tabuList[sv[i]]) {
                    bkl.push(sv[i]);
                    bestK--;
                }
            } else {
                bkl.push(sv[i]);
                bestK--;
            }
        }
    }
    
    fprintf( stderr, "Number of preselected variables( objectID: %ld ) \t\t\t = %d / %d\n", (uint64_t)this, bkl.size(), nVars());
    assert( bkl.size() < nVars() && "cannot select more variables than present in the formula" );
    
    if (bkl.size() > statistics.getI(splitterMaxPreselectedVariablesID))
    { statistics.changeI(splitterMaxPreselectedVariablesID, bkl.size() - statistics.getI(splitterMaxPreselectedVariablesID)); }
}

bool LookaheadSplitting::lookahead(Lit p, vec<Lit>& lookaheadTrail, vec<Lit>& units)
{
    int trailStart = trail.size();
    decisions++;
    newDecisionLevel();
    uncheckedEnqueue(p);
    CRef confl = propagate();

    if (confl != CRef_Undef) { // failed literal
        if (opt_clause_learning > 0) {
            int backtrack_level;
            unsigned nbscore;
            learnt_clause.clear(); otfssClauses.clear(); extraInfo = 0; // reset global structures

            int ret = analyze(confl, learnt_clause, backtrack_level, nbscore,  extraInfo);
            assert(ret == 0 && "can handle only usually learnt clauses");
            if (ret != 0) { _exit(1); }   // abort, if learning is set up wrong

            cancelUntil(decisionLevel() - 1);
            if (learnt_clause[0] != ~p && value(learnt_clause[0]) == l_Undef) {
                if (learnt_clause.size() == 1) {
                    //fprintf(stderr, "splitter: Unit Clause Learnt at level %d\n", decisionLevel());
                    uncheckedEnqueue(learnt_clause[0]);
                    //CRef cr = ca.alloc(learnt_clause, true);
                    units.push(learnt_clause[0]);          //saving unit learnt clauses
                } else { /*if(learnt_clause.size() <= 2)*/
                    //fprintf(stderr, "splitter: Clause Learnt at level %d\n", decisionLevel());
                    CRef cr = ca.alloc(learnt_clause, true);
                    learnts.push(cr);
                    attachClause(cr);
                    ca[cr].setLBD(nbscore);
                    //claBumpActivity(ca[cr]);
                }
                //printf("splitter: Clause 1 End\n");
            }
        }
        return true;                    //lookahead successful
    }

    //copying the trail
    for (int j = trailStart; j < trail.size(); j++) {
        lookaheadTrail.push(trail[j]);
    }
    return false;                       //lookahead unsuccessful
}

/** Get CPU time used by this thread */
double LookaheadSplitting::cpuTime_t() const
{
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        perror("clock_gettime()");
        ts.tv_sec = -1;
        ts.tv_nsec = -1;
    }
    return (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

void LookaheadSplitting::constraintResolvent(const vec<Lit>& t)
{
    lock();
    int clauseLimit = constraintResolventClSize;
    unlock();
    vec<Lit> clause;
    for (int i = 1; i < t.size(); i++) {
        //if(binVarScore[var(t[i])]==0){
        if (value(var(t[i])) != l_Undef    // only check for the reason clause if it exists
                && ca[reason(var(t[i]))].size() >= clauseLimit) { // check if value is not already in trail //reason clause for propagated literal..... if its size is greater than 2 ... then we add a binary learnt clause
            clause.clear();
            clause.push(~t[0]);
            clause.push(t[i]);
            CRef cr = ca.alloc(clause, true);
            learnts.push(cr);
            attachClause(cr);
            //claBumpActivity(ca[cr]);
        }
    }
}

Lit LookaheadSplitting::pickBranchLiteral(vec<vec<Lit>* > **valid)
{
    //MarkArray markArray;
    //fprintf(stderr,"markArray Size = %d\n",markArray.size());
    //markArray.create( nVars() * 2 );
    // *valid=new vec<vec<Lit>* >();
    lock();
    countLookaheadDecisions++;
    triggerDoubleLookahead = opt_double_lookahead_trigger_decay * triggerDoubleLookahead; //trigger the double lookahead
    unlock();
    int temp = 0;
    int initTrailSize = trail.size(); //checking the trail size before performing looking ahead
    vec<Lit> failedLiterals;//failed literals found by lookahead
    vec<Lit> necAssign;//necessary assignments literals
    vec<Lit> unitLearnts;
    vec<Lit> varEq;
    vec<bool> varEqCheck;        varEqCheck.growTo(nVars(), false);
    vec<Lit> positiveTrail;
    vec<Lit> negativeTrail;
    vec<double> negScore;
    vec<double> posScore;
    vec<double> score;
    int numVarEq = 0;
    int numLearnts = learnts.size();
    double bestVarScore;//best variable score for splitting
    int bestVarIndex;//best variable index in the bestKList
    vec<Lit> binaryForcedClauses;//all the binary clauses locally learnt by double lookahead; saved in pair: first literal is negated first decision and second is forced literal
    bool progress;
    int numIterations;//number of iterations for finding for necessary assignments in lookahead
    int decLev = decisionLevel();

decLitNotFound:
    preselectVar(sortedVar, bestKList);
    score.growTo(bestKList.size(), 0);
    numIterations = opt_num_iterat;
    progress = true;
    bestVarIndex = var_Undef;
    bestVarScore = 0;

    while (progress && numIterations > 0) {
        progress = false;
        numIterations--;
        numVarEq = 0;

        int heuristic = opt_lookahead_heuristic;
        switch (heuristic) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
            negScore.clear();
            posScore.clear();
            negScore.growTo(nVars(), 0.0f);
            posScore.growTo(nVars(), 0.0f);
            break;
        case 5:
            negHScore.copyTo(negScore);
            posHScore.copyTo(posScore);
            break;
        }

        //removeSatisfied(learnts);//removing satisfied learnt clauses
        //checkGarbage();

        assert( bestKList.size() <= nVars() * 2 && "has to have information ready to be selected" );
        
        for (int i = 0; i < bestKList.size(); i++) {
            bool positiveLookahead = false;
            bool negativeLookahead = false;
            double sizePositiveLookahead = 0;
            double sizeNegativeLookahead = 0;
            double binClausePositiveLookahead = 0; //num of binary clauses created during lookahead
            double binClauseNegativeLookahead = 0;//num of binary clauses created during lookahead
            double sizeWatcherPositiveLookahead = 0; //size of watcher list during lookahead
            double sizeWatcherNegativeLookahead = 0;//size of watcher list during lookahead
            int binaryForcedClausesIndex = binaryForcedClauses.size();
            Lit p = lit_Undef;
            positiveTrail.clear(true);
            negativeTrail.clear(true);
            cancelUntil(decLev);

            CRef confl = propagate();
            if (confl != CRef_Undef) {
                //fprintf(stderr, "splitter: Failed literal at level %d\n", decisionLevel());
                //cancelUntil(decLev);
                return lit_Error;
            }

            //check if solution found
            if (checkSolution()) {
                return lit_Undef;
            }

            if (opt_tabu && opt_splitting_method == 1 && tabuList[bestKList[i]])
            { continue; }

            p = mkLit(bestKList[i], false);

            if (value(p) != l_Undef)
            { continue; }
            if (varEqCheck[bestKList[i]]) {
                //fprintf(stderr, "splitter: Var EQ Check\n");
                continue;
            }
            /*if (learnts.size() >= nClauses()){// Reduce the set of learnt clauses:
                //reduceDB();
                lock();
                constraintResolventClSize++;
                unlock();
                fprintf(stderr, "splitter: Local Learning Limit !!!!\n");
                statistics.changeI(splitterTotalLocalLearningsID,learnts.size()-numLearnts);
                clearLearnts();
                numLearnts=0;
            }*/

            //fprintf( stderr, "splitter: lookahead %d\n", sign(p) ? var(p) +1: 0-var(p)-1 );
            // Increase decision level and enqueue 'p'
            positiveLookahead = lookahead(p, positiveTrail, unitLearnts);

            if (positiveLookahead) {
                cancelUntil(decLev);
                score[i] = 0;
                if (opt_failed_literals > 0) {
                    uncheckedEnqueue(~p);
                    // enqueue(~p);
                    failedLiterals.push(p);
                }
                progress = true;

                continue;
            }
            sizePositiveLookahead = trail.size() - initTrailSize;
	    assert( trail.size() < nVars() && initTrailSize <= trail.size() && "there cannot be more elements in the trail than there are variables in the solver" );
            for (int j = initTrailSize; j < trail.size(); j++) {
                //fprintf(stderr, "Watcher size = %d\n", watches[trail[j]].size());
                sizeWatcherPositiveLookahead += sign(trail[j]) ? watcherNegLitSize[i] : watcherPosLitSize[i];
                binClausePositiveLookahead += sign(trail[j]) ? numPosLitTerClause[var(trail[j])] : numNegLitTerClause[var(trail[j])];
            }
            //check if solution found
            if (checkSolution()) {
                return lit_Undef;
            }

            //check if to perform double lookahead
            if (opt_double_lookahead) {
                //if(positiveTrail.size()>opt_double_lookahead_factor*nVars()){
                if (binClausePositiveLookahead > triggerDoubleLookahead) {
                    //printf("splitter: Doublelookahead +++++++++\n");
                    bool solutionFound = false;
                    //doubleLookahead(bool& sol, int& bestVarIndex, int& bestVarScore, vec<Lit>& binClauses, vec<int> bestKList, Lit lastDecision, double to)
                    if (doubleLookahead(solutionFound, binaryForcedClauses, unitLearnts, p)) {
                        cancelUntil(decLev);
                        score[i] = 0;
                        if (opt_failed_literals > 0) {
                            uncheckedEnqueue(~p);
//                            enqueue(~p);
                            failedLiterals.push(p);
                        }
                        progress = true;
                        // fprintf(stderr, "splitter: Doublelookahead +++++++++ success\n");
                        continue;
                    } else {
                        if (solutionFound) {
                            return lit_Undef;
                        }
                        //fprintf(stderr, "splitter: Doublelookahead +++++++++ failed\n");
                        lock();
                        triggerDoubleLookahead = binClausePositiveLookahead;
                        unlock();
                        cancelUntil(decLev);
                    }
                }
            }

            if (opt_constraint_resolvent > 0) {
                if (opt_var_eq < 2 || numIterations < opt_num_iterat - 1) { //skipping constraint resolvents in the first interation......
                    constraintResolvent(positiveTrail);
                }
            }
            cancelUntil(decLev);

            confl = propagate();
            if (confl != CRef_Undef) {
                //fprintf(stderr, "splitter: Failed literal at level %d\n", decisionLevel());
                cancelUntil(decLev);
                return lit_Error;
            }

            // perform lookahead on negative branch
            if (value(~p) == l_Undef) { //checking if the literal has already been assigned
                negativeLookahead = lookahead(~p, negativeTrail, unitLearnts);

                if (negativeLookahead) {
                    cancelUntil(decLev);
                    score[i] = 0;
                    if (opt_failed_literals > 0) {
                        failedLiterals.push(~p);
                        uncheckedEnqueue(p);
                        //enqueue(p);
                    }
                    progress = true;
                    continue;
                }

                //check if solution found
                if (checkSolution()) {
                    return lit_Undef;
                }
                sizeNegativeLookahead = trail.size() - initTrailSize;
                for (int j = initTrailSize; j < trail.size(); j++) {
                    sizeWatcherNegativeLookahead += sign(trail[j]) ? watcherNegLitSize[i] : watcherPosLitSize[i];
                    binClauseNegativeLookahead += sign(trail[j]) ? numPosLitTerClause[var(trail[j])] : numNegLitTerClause[var(trail[j])];
                }
                //check if to perform double lookahead
                if (opt_double_lookahead) {
                    if (binClauseNegativeLookahead > triggerDoubleLookahead) {
                        // printf("splitter: Doublelookahead ---------\n");
                        bool solutionFound = false;
                        //doubleLookahead(bool& sol, int& bestVarIndex, int& bestVarScore, vec<Lit>& binClauses, vec<int> bestKList, Lit lastDecision, double to)
                        if (doubleLookahead(solutionFound, binaryForcedClauses, unitLearnts, ~p)) {
                            cancelUntil(decLev);
                            score[i] = 0;
                            if (opt_failed_literals > 0) {
                                uncheckedEnqueue(p);
                                // enqueue(p);
                                failedLiterals.push(~p);
                            }
                            progress = true;
                            // fprintf(stderr, "splitter: Doublelookahead --------- success\n");
                            continue;
                        } else {
                            if (solutionFound) {
                                return lit_Undef;
                            }

                            //fprintf(stderr, "splitter: Doublelookahead --------- failed\n");
                            lock();
                            triggerDoubleLookahead = binClauseNegativeLookahead;
                            unlock();
                            cancelUntil(decLev);
                        }
                    }
                }
                //if(positiveLookahead && negativeLookahead) //unsat node       //don't need it... checked in start of the loop
                //      return l_False;
            }

            if (opt_constraint_resolvent > 0) {
                if (opt_var_eq < 2 || numIterations < opt_num_iterat - 1) { //skipping constraint resolvents in the first interation......
                    constraintResolvent(negativeTrail);
                }
            }
            cancelUntil(decLev);

            if (opt_double_lookahead) {
                vec<Lit> c;
                for (int j = binaryForcedClausesIndex; j < binaryForcedClauses.size() - 1; j = j + 2) {
                    if (value(binaryForcedClauses[j]) == l_Undef && value(binaryForcedClauses[j + 1]) == l_Undef) {
                        c.clear();
                        c.push(binaryForcedClauses[j]);
                        c.push(binaryForcedClauses[j + 1]);
                        CRef cr = ca.alloc(c, true);
                        learnts.push(cr);
                        attachClause(cr);
                        //claBumpActivity(ca[cr]);
                    }
                }
            }

            if (opt_nec_assign > 0 || opt_var_eq > 0) {

                markArray.nextStep();
                for (int j = 1; j < positiveTrail.size(); j++) {
                    markArray.setCurrentStep(toInt(positiveTrail[j]));
                }
                vec<Lit> clause1;
//                for(int j=1; j<positiveTrail.size(); j++){
                for (int k = 1; k < negativeTrail.size(); k++) {
                    if (value(negativeTrail[k]) != l_Undef)
                    { continue; }
                    const bool posHit = markArray.isCurrentStep(toInt(negativeTrail[k]));
                    const bool negHit = markArray.isCurrentStep(toInt(~(negativeTrail[k])));
                    if (posHit && opt_nec_assign > 0) {
                        necAssign.push(negativeTrail[k]);
                        uncheckedEnqueue(negativeTrail[k]);
                        progress = true;
                    } else if (negHit && opt_var_eq > 0 && numIterations == opt_num_iterat - 1) { //only performin eq check in first iteration..... as i am not checking if the eq is already found or not
                        varEqCheck[var(negativeTrail[k])] = true;
                        numVarEq++;
                        if (opt_var_eq > 1) {
                            clause1.clear();
                            clause1.push(negativeTrail[0]);
                            clause1.push(~negativeTrail[k]);
                            CRef cr = ca.alloc(clause1, true);
                            learnts.push(cr);
                            attachClause(cr);

                            clause1.clear();
                            clause1.push(negativeTrail[k]);
                            clause1.push(~negativeTrail[0]);
                            cr = ca.alloc(clause1, true);
                            learnts.push(cr);
                            attachClause(cr);
                            // fprintf(stderr, "splitter: Var Equivalence found\n");
                        }
                        if (opt_var_eq > 2 && numIterations == opt_num_iterat - 1) {
                            varEq.push(~negativeTrail[0]);
                            varEq.push(~negativeTrail[k]);
                        }
                    }
                }
                //              }
            }

            if (!positiveLookahead && !negativeLookahead) {
                switch (heuristic) {
                case 0:
                    negScore[bestKList[i]] = sizeNegativeLookahead;
                    posScore[bestKList[i]] = sizePositiveLookahead;
                    score[i] = 1024 * sizePositiveLookahead * sizeNegativeLookahead + sizePositiveLookahead + sizeNegativeLookahead;
                    break;
                case 1:
                    negScore[bestKList[i]] = binClauseNegativeLookahead;
                    posScore[bestKList[i]] = binClausePositiveLookahead;
                    score[i] = 1024 * binClausePositiveLookahead * binClauseNegativeLookahead + binClausePositiveLookahead + binClauseNegativeLookahead;
                    break;
                case 2:
                    negScore[bestKList[i]] = sizeWatcherNegativeLookahead + 1;
                    posScore[bestKList[i]] = sizeWatcherPositiveLookahead + 1;
                    score[i] = 1024 * sizeWatcherPositiveLookahead * sizeWatcherNegativeLookahead + sizeWatcherPositiveLookahead + sizeWatcherNegativeLookahead;
                    break;
                case 3:
                    for (int j = 0; j < negativeTrail.size(); j++) {
                        negScore[i] += sign(negativeTrail[j]) ? negHScore[var(negativeTrail[j])] : posHScore[var(negativeTrail[j])];
                    }
                    for (int j = 0; j < positiveTrail.size(); j++) {
                        posScore[i] += sign(positiveTrail[j]) ? negHScore[var(positiveTrail[j])] : posHScore[var(positiveTrail[j])];
                    }
                    score[i] = log(negScore[i] + 1) + log(posScore[i] + 1);
                    break;
                case 4:
                    negScore[bestKList[i]] = 0.3 * (sizeNegativeLookahead) + 0.7 * (binClauseNegativeLookahead);
                    posScore[bestKList[i]] = 0.3 * (sizePositiveLookahead) + 0.7 * (binClausePositiveLookahead);
                    score[i] = 1024 * posScore[bestKList[i]] * negScore[bestKList[i]] + posScore[bestKList[i]] + negScore[bestKList[i]];
                    break;
                case 5:
                    score[i] = negScore[bestKList[i]] * posScore[bestKList[i]];
                    goto jump;
                    break;
                case 6:
                    negScore[bestKList[i]] = 0.7 * (sizeNegativeLookahead) + 0.3 * (binClauseNegativeLookahead);
                    posScore[bestKList[i]] = 0.7 * (sizePositiveLookahead) + 0.3 * (binClausePositiveLookahead);
                    score[i] = 1024 * posScore[bestKList[i]] * negScore[bestKList[i]] + posScore[bestKList[i]] + negScore[bestKList[i]];
                    break;
                case 7:
                    negScore[bestKList[i]] = pow((int)opt_hscore_clause_weight, opt_hscore_maxclause - 1) * (sizeNegativeLookahead) + (negScore[bestKList[i]]);
                    posScore[bestKList[i]] = pow((int)opt_hscore_clause_weight, opt_hscore_maxclause - 1) * (sizePositiveLookahead) + (posScore[bestKList[i]]);
                    score[i] = 1024 * posScore[bestKList[i]] * negScore[bestKList[i]] + posScore[bestKList[i]] + negScore[bestKList[i]];
                    break;
                }
            }
        }
    }

jump:
    cancelUntil(decLev);

    for (int i = 0; i < score.size(); i++) {
        if (score[i] > 0 && score[i] > bestVarScore && value(bestKList[i]) == l_Undef) {
            bestVarScore = score[i];
            bestVarIndex = i;
        }
    }

    if (bestVarIndex == var_Undef) {
        vec<VarScore> varScore;
        for (int j = 0; j < bestKList.size(); j++) {
            if (value(bestKList[j]) == l_Undef && !tabuList[bestKList[j]]) {
                VarScore *vs = new VarScore(j, score[j]);
                varScore.push(*vs);
            }
        }
        if (varScore.size() > 0) {
            sort(varScore);
            bestVarIndex = varScore.last().var;
        }
    }

    Lit next;

    if (bestVarIndex != var_Undef) {
        bool pol;
        int direction = opt_child_direction_priority;
        switch (direction) {
        case 0:
            pol = false;      break;
        case 1:
            pol = true;       break;
        case 2:
            pol = negScore[bestKList[bestVarIndex]] > posScore[bestKList[bestVarIndex]];          break;
        case 3:
            pol = negScore[bestKList[bestVarIndex]] < posScore[bestKList[bestVarIndex]];         break;
        case 4:
            pol = drand(random_seed) < 0.5;                   break;
        case 5:
            pol = phaseSaving[bestKList[bestVarIndex]].polarity;                   break;
        case 6:
            pol = negScore[bestKList[bestVarIndex]] / posScore[bestKList[bestVarIndex]] <= 1 / opt_child_direction_adaptive_factor &&
                  negScore[bestKList[bestVarIndex]] / posScore[bestKList[bestVarIndex]] >= opt_child_direction_adaptive_factor ?
                  negScore[bestKList[bestVarIndex]]<posScore[bestKList[bestVarIndex]] :
                  negScore[bestKList[bestVarIndex]]>posScore[bestKList[bestVarIndex]];
            break;
        }
        next = mkLit(bestKList[bestVarIndex], pol);
    } else {
        // fprintf(stderr, "splitter: no variable picked by lookahead; redo with new preselection\n");
        if (bestKList.size() == 0) {
            next = pickBranchLit();
        } else {
            goto decLitNotFound;
        }
        // fprintf(stderr,"splitter: no variable picked by lookahead; calling pickBranchLit()\n");
        /*do{
            next=pickBranchLit();
        }while(opt_tabu && tabuList[var(next)]);

        lock();
        branchThreshold*=opt_branch_threshold_penalty;
        unlock();
        statistics.changeI(splitterPenaltyID,1);*/
    }
    //using to see if a literal has been already put as partition constraint
    permDiff.nextStep();
    vec<Lit>* clause;
    if (opt_failed_literals == 2) {
        for (int i = 0; i < failedLiterals.size(); i++) {
            permDiff.setCurrentStep(var(failedLiterals[i]));
            clause = new vec<Lit>();
            clause->push(~failedLiterals[i]);
            (*valid)->push(clause);
            //fprintf( stderr, "splitter: failed literals %d ==========\n", sign(failedLiterals[i]) ? var(failedLiterals[i]) +1: 0-var(failedLiterals[i])-1 );
        }
    }
    if (opt_nec_assign == 2) {
        for (int i = 0; i < necAssign.size(); i++) {
            permDiff.setCurrentStep(var(necAssign[i]));
            clause = new vec<Lit>();
            clause->push(necAssign[i]);
            (*valid)->push(clause);
            //fprintf( stderr, "splitter: Necessary Assignment %d +++++++++++\n", sign(necAssign[i]) ? var(necAssign[i]) +1: 0-var(necAssign[i])-1 );
        }
    }
    if (opt_clause_learning == 2) {
        for (int i = 0; i < unitLearnts.size(); i++) {
            permDiff.setCurrentStep(var(unitLearnts[i]));
            clause = new vec<Lit>();
            clause->push(unitLearnts[i]);
            (*valid)->push(clause);
            //fprintf( stderr, "splitter: unit clause learnt %d +++++++++++\n", sign(unitLearnts[i]) ? var(unitLearnts[i]) +1: 0-var(unitLearnts[i])-1 );
        }
    }
    if (opt_var_eq == 3) {
        Lit eqLit;
        for (int i = 0; i < varEq.size() - 1; i = i + 2) {
            eqLit = lit_Undef;
            if (value(varEq[i]) == l_True && permDiff.isCurrentStep(var(varEq[i])))
            { eqLit = varEq[i + 1]; }
            else if (value(~varEq[i]) == l_True && permDiff.isCurrentStep(var(varEq[i])))
            { eqLit = ~varEq[i + 1]; }
            if (value(varEq[i + 1]) == l_True && permDiff.isCurrentStep(var(varEq[i + 1])))
            { eqLit = varEq[i]; }
            else if (value(~varEq[i + 1]) == l_True && permDiff.isCurrentStep(var(varEq[i + 1])))
            { eqLit = ~varEq[i]; }

            if (eqLit != lit_Undef &&  ! permDiff.isCurrentStep(var(eqLit))) {
                permDiff.setCurrentStep(var(eqLit));
                clause = new vec<Lit>();
                clause->push(eqLit);
                (*valid)->push(clause);
            }
        }
    }
    if (opt_double_lookahead && opt_bin_constraints) {
        for (int i = 0, j = 0; i < binaryForcedClauses.size() - 1; i = i + 2) {
            if ((value(binaryForcedClauses[i]) == l_True) || value(binaryForcedClauses[i + 1]) == l_True)
            { continue; }
            if (permDiff.isCurrentStep(var(binaryForcedClauses[i])) && value(binaryForcedClauses[i]) == l_False && value(binaryForcedClauses[i + 1]) == l_Undef)
            { j = i + 1; }
            else if (permDiff.isCurrentStep(var(binaryForcedClauses[i + 1])) && value(binaryForcedClauses[i + 1]) == l_False && value(binaryForcedClauses[i]) == l_Undef)
            { j = i; }
            else
            { continue; }
            if (! permDiff.isCurrentStep(var(binaryForcedClauses[j]))) {
                permDiff.setCurrentStep(var(binaryForcedClauses[j]));
                clause = new vec<Lit>();
                clause->push(binaryForcedClauses[j]);
                (*valid)->push(clause);
                temp++;
            }
        }
    }

    // if(opt_failed_literals>0) { fprintf(stderr, "splitter: Failed Literal \t\t\t = %d\n", failedLiterals.size());                                }
    // if(opt_nec_assign>0) {      fprintf(stderr, "splitter: Necessay Assignments \t\t\t = %d\n", necAssign.size());                               }
    // if(opt_double_lookahead) {  fprintf(stderr, "splitter: DoubleLookahead Binary Clauses \t = %d \\ %d\n", temp, binaryForcedClauses.size()/2); }
    // if(opt_clause_learning>0) { fprintf(stderr, "splitter: Local Clauses Learnt \t\t\t = %d \n", learnts.size());                                }
    // if(opt_var_eq>0) {          fprintf(stderr, "splitter: Var Equivalence \t\t\t = %d \n", varEq.size()/2);                                     }
    // fprintf(stderr, "splitter: Best Var Index = %d\n",bestVarIndex);

    if (opt_tabu) {
        tabuList[var(next)] = true;
    }

    lock();
    countFailedLiterals += failedLiterals.size();
    unlock();

    statistics.changeI(splitterTotalFailedLiteralsID, failedLiterals.size());
    statistics.changeI(splitterTotalNecessaryAssignmentsID, necAssign.size());
    statistics.changeI(splitterTotalClauseLearntUnitsID, unitLearnts.size());
    statistics.changeI(splitterTotalLiteralEquivalencesID, numVarEq);
    statistics.changeI(splitterTotalLocalLearningsID, learnts.size() - numLearnts);

    failedLiterals.clear(true);
    necAssign.clear(true);
    unitLearnts.clear(true);
    positiveTrail.clear(true);
    negativeTrail.clear(true);
    negScore.clear(true);
    posScore.clear(true);
    score.clear(true);
    binaryForcedClauses.clear(true);
    varEq.clear(true);
    varEqCheck.clear(true);
    return next;
}

bool LookaheadSplitting::doubleLookahead(bool& sol, vec<Lit>& binClauses, vec<Lit>& unitLearnts, Lit lastDecision)
{
    //////////stats/////////
    statistics.changeI(splitterTotalDoubleLookaheadCallsID, 1);
    /////////////////////////
    vec<int> negLitScore;       negLitScore.growTo(nVars(), 0);
    vec<int> posLitScore;       posLitScore.growTo(nVars(), 0);
    vec<VarScore> varScore;
    vec<int> sortedVarDLA;
    vec<int> bestKListDLA;
    for (int i = 0; i < clauses.size(); i++) {
        Clause& c = ca[clauses[i]];
        /*if(c.size()==2 && !satisfied(c)){
            sign(c[0]) ? negLitScore[var(c[0])]++ : posLitScore[var(c[0])]++;
            sign(c[1]) ? negLitScore[var(c[1])]++ : posLitScore[var(c[1])]++;
        }else*/  if (c.size() == 3 && !satisfied(c)) {
            if (value(c[0]) != l_Undef || value(c[1]) != l_Undef || value(c[2]) != l_Undef) {
                for (int j = 0; j < 3; j++) {
                    if (value(c[j]) == l_Undef)
                    { sign(c[j]) ? negLitScore[var(c[j])]++ : posLitScore[var(c[j])]++; }
                }
            }
        }
    }

    for (int i = 0; i < nVars(); i++) {
        if (posLitScore[i] > 0 && negLitScore[i] > 0) {
            VarScore vs;
            vs.var = i;
            vs.score = posLitScore[i] * negLitScore[i] + posLitScore[i] + negLitScore[i];
            varScore.push(vs);
        }
    }
    sort(varScore);
    for (int i = varScore.size() - 1; i > 0; i--) {
        sortedVarDLA.push(varScore[i].var);
    }
    preselectVar(sortedVarDLA, bestKListDLA);

    Debug::PRINT_NOTE("splitter: Double Lookahead Preselection size = ");
    Debug::PRINT_NOTE(bestKListDLA.size());
    Debug::PRINT_NOTE(" out of ");
    Debug::PRINTLN_NOTE(nVars());

    vec<Lit> positiveTrail;
    vec<Lit> negativeTrail;
    bool progress = true;
    int numIterations = opt_num_iterat;//number of iterations for finding for necessary assignments in lookahead
    int decLev = decisionLevel();
    vec<bool> varEqCheck;        varEqCheck.growTo(nVars(), false);

    while (progress && numIterations > 0) {
        numIterations--;
        progress = false;
        for (int i = 0; i < bestKListDLA.size(); i++) {
            bool positiveLookahead = false;
            bool negativeLookahead = false;
            positiveTrail.clear(true);
            negativeTrail.clear(true);

            CRef confl = propagate();
            if (confl != CRef_Undef) {      //unsat node
                statistics.changeI(splitterTotalDoubleLookaheadSuccessID, 1);
                return true;                    //lookahead successful
            }

            Lit p = mkLit(bestKListDLA[i], false);
            if (value(p) != l_Undef)
            { continue; }      //skip the variables which are already propagated
            if (varEqCheck[var(p)])
            { continue; }

            //check the positive branch
            positiveLookahead = lookahead(p, positiveTrail, unitLearnts);

            if (positiveLookahead) { // failed literal
                //saving lastDecision -> ~p
                if (opt_failed_literals > 0) {
                    binClauses.push(~lastDecision);
                    binClauses.push(~p);
                }

                cancelUntil(decLev);
                uncheckedEnqueue(~p);
                progress = true;
                continue;
            }
            //check if solution found
            if (checkSolution()) {
                sol = true;
                return false;                               //lookahead failed
            }

            cancelUntil(decLev);
            confl = propagate();

            //lookahead negative branch
            if (value(~p) == l_Undef) {
                negativeLookahead = lookahead(~p, negativeTrail, unitLearnts);

                if (negativeLookahead) { // failed literal
                    //saving lastDecision -> p
                    if (opt_failed_literals > 0) {
                        binClauses.push(~lastDecision);
                        binClauses.push(p);
                    }
                    cancelUntil(decLev);
                    uncheckedEnqueue(p);
                    progress = true;
                    continue;
                }

                //check if solution found
                if (checkSolution()) {
                    sol = true;
                    return false;                               //lookahead failed
                }
            } else {
                cancelUntil(decLev);
                continue;
            }

            cancelUntil(decLev);

            if (opt_nec_assign > 0) {
                markArray.nextStep();
                for (int j = 0; j < positiveTrail.size(); j++) {
                    markArray.setCurrentStep(toInt(positiveTrail[j]));
                }
                for (int k = 0; k < negativeTrail.size(); k++) {
                    const bool posHit = markArray.isCurrentStep(toInt(negativeTrail[k]));
                    const bool negHit = markArray.isCurrentStep(toInt(~(negativeTrail[k])));
                    if (posHit) {
                        binClauses.push(~lastDecision);
                        binClauses.push(negativeTrail[k]);
                        uncheckedEnqueue(negativeTrail[k]);
                        progress = true;
                    } else if (negHit && opt_var_eq > 0) {
                        varEqCheck[var(negativeTrail[k])] = true;
                    }
                }
            }
        }
    }

    positiveTrail.clear(true);
    negativeTrail.clear(true);
    varEqCheck.clear(true);
    negLitScore.clear(true);
    posLitScore.clear(true);
    varScore.clear(true);
    sortedVarDLA.clear(true);
    bestKListDLA.clear(true);
    Debug::PRINT_NOTE("splitter: Lookahead Learning Size = ");
    Debug::PRINTLN_NOTE(binClauses.size());
    return false;
}

void LookaheadSplitting::lock()
{
    communicationLock.wait();
}

void LookaheadSplitting::unlock()
{
    communicationLock.unlock();
}

void LookaheadSplitting::learntsLimitPush()
{
    learntsLimit.push(learnts.size());
}

void LookaheadSplitting::learntsLimitPop()
{
    if (learntsLimit.size() == 0)
    { return; }
    unsigned limit = learntsLimit.last();
    learntsLimit.pop();
    for (int i = limit; i < learnts.size(); i++) {
        removeClause(learnts[i]);
    }
    checkGarbage();
}


#if 0
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
CRef LookaheadSplitting::propagate()
{
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll();
    watchesBin.cleanAll();
    while (qhead < trail.size()) {
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;


        // First, Propagate binary clauses
        vec<Watcher>&  wbin  = watchesBin[p];

        for (int k = 0; k < wbin.size(); k++) {

            Lit imp = wbin[k].blocker;

            if (value(imp) == l_False) {
                return wbin[k].cref;
            }

            if (value(imp) == l_Undef) {
                //printLit(p);printf(" ");printClause(wbin[k].cref);printf("->  ");printLit(imp);printf("\n");
                decisionLevel() == 0 ? uncheckedEnqueue(imp) : uncheckedEnqueue(imp, wbin[k].cref);
            }
        }



        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;) {
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True) {
                *j++ = *i++; continue;
            }

            // Make sure the false literal is data[1]:
            CRef     cr        = i->cref;
            Clause&  c         = ca[cr];
            Lit      false_lit = ~p;
            if (c[0] == false_lit)
            { c[0] = c[1], c[1] = false_lit; }
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True) {

                *j++ = w; continue;
            }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False) {
                    c[1] = c[k]; c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause;
                }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False) {
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                { *j++ = *i++; }
            } else {
                decisionLevel() == 0 ? uncheckedEnqueue(first) : uncheckedEnqueue(first, cr);

                #ifdef DYNAMICNBLEVEL
                // DYNAMIC NBLEVEL trick (see competition'09 companion paper)
                if (c.learnt()  && c.lbd() > 2) {
                    MYFLAG++;
                    unsigned  int nblevels = 0;
                    for (int i = 0; i < c.size(); i++) {
                        int l = level(var(c[i]));
                        if (permDiff[l] != MYFLAG) {
                            permDiff[l] = MYFLAG;
                            nblevels++;
                        }


                    }
                    if (nblevels + 1 < c.lbd()) {  // improve the LBD
                        if (c.lbd() <= lbLBDFrozenClause) {
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
#endif


void LookaheadSplitting::removeSatisfied(vec<CRef>& cs)
{

    int i, j;
    for (i = j = 0; i < cs.size(); i++) {
        Clause& c = ca[cs[i]];


        if (satisfied(c)) // A bug if we remove size ==2, We need to correct it, but later.
        { removeClause(cs[i]); }
        else
        { cs[j++] = cs[i]; }
    }
    cs.shrink_(i - j);
}

} // namespace Pcasso
