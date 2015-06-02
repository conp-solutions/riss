/*
 * File:   SolverPT.h
 * Author: ahmed
 *
 * Created on December 24, 2012, 4:04 PM
 */

#ifndef SOLVERPT_H
#define SOLVERPT_H

#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "pcasso/SplitterSolver.h"

// Davide> Beginning of my includes
#include "riss/utils/LockCollection.h"
#include "pcasso/LevelPool.h"
#include "pcasso/PartitionTree.h"

#include "riss/utils/Statistics-mt.h"

namespace Pcasso
{
class SolverPT : public SplitterSolver
{

    CoreConfig& coreConfig;

  public:
    SolverPT(CoreConfig& config);
    ~SolverPT();

    void dummy()
    {
    }
    std::vector<unsigned> shared_indeces;

    std::string position; /** Davide> Position of the solver in the Partition tree */
    unsigned curPTLevel; // Davide> Contains the pt_level of curNode

    int infiniteCnt;  /** Davide> Temporary solution */

    TreeNode* tnode; // Davide>

    unsigned int PTLevel; // Davide> Used by analyze and litRedundant, and search
    unsigned int max_bad_literal; // Davide> as above

    bool rndDecLevel0;//Ahmed> choose random variable at decision level zero

    std::vector< std::vector<unsigned> > learnts_indeces; // Davide> attempt
    unsigned int level0UnitsIndex; // ahmed> saving the last index of shared level0 units
    enum ScoreType {NONE, SIZE, LBD_LT, PSM, PSM_FALSE}; // Davide>
    ScoreType receiver_filter_enum;

    // Davide> Statistics
    unsigned diversification_stop_nodes_ID;
    unsigned n_pool_duplicatesID;
    unsigned n_threads_blockedID;
    unsigned n_import_shcl_unsatID;
    unsigned sum_clauses_pools_lv1ID;
    unsigned sum_clauses_pools_lv1_effID; // Davide> What effectively is in the pool
    unsigned sum_clauses_pools_lv0ID;
    unsigned sum_clauses_pools_lv0_effID; // Davide> What effectively is in the pool
    unsigned n_unary_shclausesID;
    unsigned n_binary_shclausesID;
    unsigned n_lbd2_shclausesID;
    unsigned n_clsent_curr_lvID;
    unsigned n_clsent_prev_lvID;
    unsigned n_clcanbeccminID;
    unsigned n_ccmin_worseningID;
    unsigned n_tot_sharedID;
    unsigned n_tot_shared_minus_delID;
    unsigned n_acquired_clausesID;
    unsigned n_tot_forced_restarts_ID;
    unsigned n_tot_reduceDB_calls_ID;
    //unsigned sharing_time_ID;

    Riss::Var newVar(bool polarity = true, bool dvar = true, char type = 'o');
    /** Davide> adds a clause, toghether with its pt_level, to the clauses
    of the current formula. False literals of level zero will be
    removed from the clause only if their pt_level is equal to zero,
               i.e. the clause is "safe"
           **/
    bool    addClause (const Riss::vec<Riss::Lit>& ps, unsigned int pt_level = 0, bool from_shpool = false);
    bool    addClause_( Riss::vec<Riss::Lit>& ps, unsigned int level = 0);
    void     analyze          (Riss::CRef confl, Riss::vec<Riss::Lit>& out_learnt, int& out_btlevel, unsigned int& nblevels);   // (bt = backtrack)
    bool     litRedundant     (Riss::Lit p, uint32_t abstract_levels);                       // (helper method for 'analyze()')
    void     uncheckedEnqueue (Riss::Lit p, Riss::CRef from = Riss::CRef_Undef, unsigned int pt_level = 0);                         // Enqueue a literal. Assumes value of literal is undefined.
    Riss::Lit      pickBranchLit    ();                                                      // Return the next decision variable.
    Riss::CRef    propagate();
    Riss::lbool    search           (int nof_conflicts);                                     // Search for a given number of conflicts.
    Riss::lbool    solve_           ();                                                      // Main solve method (assumptions given in 'assumptions').
    void reduceDB();
    /**
        Davide> Returns the level of the current node in the Partition Tree
    **/
    inline unsigned int getNodePTLevel(void) const
    {
        return curPTLevel;
    }

    // Davide> Returns the pt_level associated to the literal l
    unsigned getLiteralPTLevel(const Riss::Lit& l) const;//get PT level of literal
    void setLiteralPTLevel(const Riss::Lit& l, unsigned pt);

    // Davide> Shares all the learnts that have to be shared
    void push_units();
    void push_learnts(void);
    void pull_learnts(int curr_restarts);
    bool addSharedLearnt(Riss::vec<Riss::Lit>& ps, unsigned int pt_level);

    /** Transforms a chunk of shared pool into clauses that
     *  will be added to the learnts database **/
    void addChunkToLearnts(Riss::vec<Riss::Lit>& chunk, unsigned int pt_level, int, int);

    // Davide> Options related to clause sharing
    //
    bool learnt_unary_res;
    int addClause_FalseRemoval;
    bool opt_shareClauses; // option to disable sharing completely
    int sharedClauseMaxSize;
    int LBD_lt;
    bool learnt_worsening;
    bool pools_filling;
    bool dynamic_PTLevels;
    bool random_sharing;
    int random_sh_prob;
    bool shconditions_relaxing;
    bool every_shpool;
    bool disable_stats;
    bool disable_dupl_check;
    bool disable_dupl_removal;
    int sharing_delay;
    bool flag_based;
    int  receiver_filter;
    int receiver_score;
    int update_act_pol;
    unsigned int lastLevel;

    // Timeout in seconds, initialize to 0 (no timeout)
    double tOut;
    double cpuTime_t() const ; // CPU time used by this thread
    /** return true, if the run time of the solver exceeds the specified limit */
    bool timedOut() const;

    /** specify a number of seconds that is allowed to be executed */
    void setTimeOut(double timeout);

    /** local version of the statistics object */
    Statistics localStat; // Norbert> Local Statistics

    /** return the number of top level units from that are on the trail */
    unsigned int getTopLevelUnits() const;

    /** return a specifit literal from the trail */
    Riss::Lit trailGet( const unsigned int index );
  private:
    Riss::vec<unsigned> varPT; //storing the PT level of each variable
    //CMap<unsigned> clausePT; //storing the PT level of each clause
    //int lbd_lt(Riss::vec<Riss::Lit>&);
    //different restart strategy settings for sat and unsat
    void satRestartStrategy();
    void unsatRestartStrategy();
};

inline bool     SolverPT::addClause       (const Riss::vec<Riss::Lit>& ps, unsigned int pt_level, bool from_shpool)    { ps.copyTo(add_tmp); return from_shpool ? addSharedLearnt(add_tmp, pt_level) : addClause_(add_tmp, pt_level); } // Davide> Added pt_level info;
}
#endif  /* SOLVERPT_H */

