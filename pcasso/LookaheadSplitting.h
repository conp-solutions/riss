/* 
 * File:   Splitting.h
 * Author: ahmed
 *
 * Created on December 23, 2012, 6:07 PM
 */

#ifndef LOOKAHEADSPLITTING_H
#define	LOOKAHEADSPLITTING_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"
#include "utils/LockCollection.h"
#include "core/Solver.h"
#include "utils/Statistics-mt.h"
#include "pcasso-src/SplitterSolver.h"
#include "utils/Debug.h"

namespace Pcasso {
    class LookaheadSplitting : public SplitterSolver {
      
    CoreConfig& coreConfig;
      
    // more global data structures
    vec<Lit> learnt_clause;
    vec<CRef> otfssClauses;
    uint64_t extraInfo;
      
    public:
        LookaheadSplitting(CoreConfig& config);
        ~LookaheadSplitting();
        void dummy(){
        }
        lbool produceSplitting(vec<vec<vec<Lit>* >* > **splits, vec<vec<Lit>* > **valid);
        double cpuTime_t() const ; // CPU time used by this thread
        void setTimeOut(double to);
        struct VarScore{
            int var;
            double score;

            VarScore() {}
            VarScore(int v, double s)  : var(v),  score(s) {}

            bool operator < (const VarScore& vs) const
            {
                return (score < vs.score);
            }
        };
//        CRef     propagate        (); 
        void     removeSatisfied  (vec<CRef>& cs);                                         // Shrink 'cs' to contain only non-satisfied clauses.
        /// local version of the statistics object
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
        vec<Lit> pureLiterals;
        
        //vec<int>& sortedVar,vec<double>& negHScore, vec<double>& posHScore, int watcherPosLitSize[], int watcherNegLitSize[], int numPosLitTerClause[], int numNegLitTerClause[],int& maxClauseSize

        //data
        vec<vec<Lit>* > *splitting; //splittings produced
        vec<vec<vec<Lit>* >* > *validAtLevel; //valid for each splittings
        double timeOut;
        vec<bool> tabuList;
        vec<int> bestKList;//subset of variables for lookahead
        vec<int> sortedVar;
        vec<unsigned> numNegLitTerClause;
        vec<unsigned> numPosLitTerClause;
        vec<double> negHScore;
        vec<double> posHScore;
        vec<unsigned> watcherNegLitSize;
        vec<unsigned> watcherPosLitSize;
	vec<VarFlags> phaseSaving; // to not change the solver
        int maxClauseSize;
        MarkArray markArray;
        vec<unsigned> learntsLimit;
        
        ComplexLock communicationLock;
        //helper methods
        void shrinkClauses(); //shrink the constraint clauses in solver
        bool checkSolution();
        void clearLearnts();
        void preselectionHeuristic();
        void preselectVar(vec<int>& sv, vec<int>& bkl);
        bool lookahead(Lit p, vec<Lit>& lookaheadTrail, vec<Lit>& units);
        bool doubleLookahead(bool& sol, vec<Lit>& binClauses, vec<Lit>& unitLearnts, Lit lastDecision);
        Lit pickBranchLiteral(vec<vec<Lit>* > **valid);
        void constraintResolvent(const vec<Lit>& t);
        void learntsLimitPush();
        void learntsLimitPop();
    };
    
}
#endif	/* LOOKAHEADSPLITTING_H */

