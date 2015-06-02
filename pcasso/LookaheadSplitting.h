/*
 * File:   Splitting.h
 * Author: ahmed
 *
 * Created on December 23, 2012, 6:07 PM
 */

#ifndef LOOKAHEADSPLITTING_H
#define LOOKAHEADSPLITTING_H

#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/utils/LockCollection.h"
#include "riss/core/Solver.h"
#include "riss/utils/Statistics-mt.h"
#include "pcasso/SplitterSolver.h"
#include "riss/utils/Debug.h"

namespace Pcasso
{

class LookaheadSplitting : public SplitterSolver
{

    Riss::CoreConfig& coreConfig;

    // more global data structures
    Riss::vec<Riss::Lit> learnt_clause;
    Riss::vec<Riss::CRef> otfssClauses;
    uint64_t extraInfo;

  public:
    LookaheadSplitting(Riss::CoreConfig& config);
    ~LookaheadSplitting();
    void dummy()
    {
    }
    Riss::lbool produceSplitting(Riss::vec<Riss::vec<Riss::vec<Riss::Lit>* >* > **splits, Riss::vec<Riss::vec<Riss::Lit>* > **valid);
    double cpuTime_t() const ; // CPU time used by this thread
    void setTimeOut(double to);
    struct VarScore {
        int var;
        double score;

        VarScore() {}
        VarScore(int v, double s)  : var(v),  score(s) {}

        bool operator < (const VarScore& vs) const
        {
            return (score < vs.score);
        }
    };
    // Riss::CRef     propagate        ();
    /** Shrink 'cs' to contain only non-satisfied clauses. */
    void     removeSatisfied  (Riss::vec<Riss::CRef>& cs); 
    /** local version of the statistics object */
    Statistics localStat; // Norbert> Local Statistics

    unsigned splitterMaxPreselectedVariablesID;
    unsigned splitterTotalTimeID;
    unsigned splitterTotalDecisionsID;
    unsigned splitterPenaltyID;
    unsigned splitterTotalPureLiteralsID;
    unsigned splitterTotalFailedLiteralsID;
    unsigned splitterTotalNecessaryAssignmentsID;
    unsigned splitterTotalClauseLearntUnitsID;
    unsigned splitterTotalLocalLearningsID;
    unsigned splitterTotalLiteralEquivalencesID;
    unsigned splitterTotalDoubleLookaheadCallsID;
    unsigned splitterTotalDoubleLookaheadSuccessID;

  protected:
    void lock();//communicationLock locked
    void unlock();//communicationLock unlocked
  private:
    //options
    unsigned optNumChild;
    unsigned optBranchDepth;
    //static variables
    static double triggerDoubleLookahead;
    static double branchThreshold;
    static unsigned constraintResolventClSize;
    static unsigned dSeq[];
    //for adaptive preselection heuristic
    static unsigned countFailedLiterals;
    static unsigned countLookaheadDecisions;
    //pure literals
    Riss::vec<Riss::Lit> pureLiterals;

    // Riss::vec<int>& sortedVar;
    // Riss::vec<double>& negHScore;
    // Riss::vec<double>& posHScore;
    // int watcherPosLitSize[];
    // int watcherNegLitSize[];
    // int numPosLitTerClause[];
    // int numNegLitTerClause[];
    // int& maxClauseSize;

    //data
    Riss::vec<Riss::vec<Riss::Lit>* > *splitting; //splittings produced
    Riss::vec<Riss::vec<Riss::vec<Riss::Lit>* >* > *validAtLevel; //valid for each splittings
    double timeOut;
    Riss::vec<bool> tabuList;
    Riss::vec<int> bestKList;//subset of variables for lookahead
    Riss::vec<int> sortedVar;
    Riss::vec<unsigned> numNegLitTerClause;
    Riss::vec<unsigned> numPosLitTerClause;
    Riss::vec<double> negHScore;
    Riss::vec<double> posHScore;
    Riss::vec<unsigned> watcherNegLitSize;
    Riss::vec<unsigned> watcherPosLitSize;
    Riss::vec<VarFlags> phaseSaving; // to not change the solver
    int maxClauseSize;
    Riss::MarkArray markArray;
    Riss::vec<unsigned> learntsLimit;

    ComplexLock communicationLock;
    //helper methods
    void shrinkClauses(); //shrink the constraint clauses in solver
    bool checkSolution();
    void clearLearnts();
    void preselectionHeuristic();
    void preselectVar(Riss::vec<int>& sv, Riss::vec<int>& bkl);
    bool lookahead(Riss::Lit p, Riss::vec<Riss::Lit>& lookaheadTrail, Riss::vec<Riss::Lit>& units);
    bool doubleLookahead(bool& sol, Riss::vec<Riss::Lit>& binClauses, Riss::vec<Riss::Lit>& unitLearnts, Riss::Lit lastDecision);
    Riss::Lit pickBranchLiteral(Riss::vec<Riss::vec<Riss::Lit>* > **valid);
    void constraintResolvent(const Riss::vec<Riss::Lit>& t);
    void learntsLimitPush();
    void learntsLimitPop();
};

} // namespace Pcasso

#endif  /* LOOKAHEADSPLITTING_H */

