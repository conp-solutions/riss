#include "pcasso-src/SolverPT.h"
// for clock_gettime()
#include <time.h>

// Davide> My includes
#include "pcasso-src/LevelPool.h"
#include <stdexcept>

using namespace Splitter;

// Davide> Options

static BoolOption    opt_learnt_unary_res       ("SPLITTER + SHARING", "learnt-unaryres", "If false, the learnt clause is NOT resolved with any \"unsafe\" unary clause, so that it can be shared at a higher level in the tree\n", false);
static IntOption     opt_addClause_FalseRemoval ("SPLITTER + SHARING", "addcl-falserem", "Controls the removal of the false literals in a clause that is being added to the solver. With 0, the literals are removed only if this does not worsen the PTLevel of the clause. (0=standard, 1=aggressive)", 1, IntRange(0,1));
static IntOption     opt_sharedClauseMaxSize    ("SPLITTER + SHARING", "shclause-size", "A clause is eligible to be shared if its size is less than or equal to ..", 1, IntRange(1,100));
static IntOption     opt_LBD_lt                 ("SPLITTER + SHARING", "lbd-lt", "A clause is eligible to be shared if its Literals Blocks Distance is less than or equal to ..", 0, IntRange(0,10));
static BoolOption    opt_learnt_worsening       ("SPLITTER + SHARING", "learnt-worsening", "During conflict clause minimization, the PTLevel of the learnt clause is increased in order to remove more literals from it\n", false);
static BoolOption    opt_pools_filling          ("SPLITTER + SHARING", "pools-filling", "Whenever a pool is filled, the shared clause is stored in ONE OF the pools of the children nodes\n", false);
static BoolOption    opt_dynamic_PTLevels       ("SPLITTER + SHARING", "dynamic-ptlevels", "A clause c of PTLevel pt is sent to the pool of level pt + floor(log2(|c|))\n", false);
static BoolOption    opt_random_sharing         ("SPLITTER + SHARING", "random-sharing", "A learnt clause is NOT eligible to be shared with probability given by the rnd-shprob option. All other learnts are eligible\n", false);
static IntOption     opt_random_sh_prob         ("SPLITTER + SHARING", "rnd-shprob", "If a learnt clause is eligible to be shared, it is NOT shared with probability rnd-shprob", 0, IntRange(0,100));
static BoolOption    opt_shconditions_relaxing  ("SPLITTER + SHARING", "shcnd-rel", "Conditions for eligibility of a learnt clause are relaxed of the logarithm of its PTLevel\n", false);
static BoolOption    opt_every_shpool           ("SPLITTER + SHARING", "every-shpool", "At every restart, every shpool accessible from the current node is accessed\n", false);
static BoolOption    opt_disable_stats          ("SPLITTER + SHARING", "disable-stats", "Disable statistics for the Solver\n", false);
static BoolOption    opt_disable_dupl_removal   ("SPLITTER + SHARING", "dis-duplrem", "Disable the removal of duplicate clauses into shared pools\n", false);
static BoolOption    opt_disable_dupl_check     ("SPLITTER + SHARING", "dis-duplcheck", "Disable the duplicate check completely\n", false);
static IntOption     opt_sharing_delay          ("SPLITTER + SHARING", "sh-delay", "A clause eligible to be shared is added to its pool every n conflicts, or whenever the learnts vector is changed\n", 16, IntRange(1,INT32_MAX));
static BoolOption    opt_flag_based             ("SPLITTER + SHARING", "flag-based", "Enable flag-based sharing\n", false);
static IntOption     opt_receiver_filter        ("SPLITTER + SHARING", "receiver-filter", "Filter on received clauses: 0 - NONE, 1 - SIZE, 2 - LDB_LT, 3 - PSM, 4 PSM_ACTIVITY\n", 0, IntRange(0,4));
static IntOption     opt_receiver_score         ("SPLITTER + SHARING", "receiver-score", "Score value of the receiver filter\n", 1, IntRange(0,100));
//>>>>>>>>>ahmed>>>>>>>>>
static BoolOption    opt_dyn_lbd_shr             ("SPLITTER + SHARING", "dyn-lbdsh", "Enable dynamic lbd sharing\n", false);
static DoubleOption    opt_dyn_lbd_shr_fac             ("SPLITTER + SHARING", "dyn-lbdshfac", "Enable dynamic lbd sharing\n",  1, DoubleRange(0, false, 1, true));
static BoolOption opt_sharing_var_bump("SPLITTER + SHARING", "shvar-bump", "Enable bumping activity of variables in shared clauses\n", false);
static BoolOption opt_unit_sharing("SPLITTER + SHARING", "unit-sharing", "Enable sharing decision level0 units\n", false);
//static IntOption opt_unit_sharing_ptlevel_limit("SPLITTER + SHARING", "unitptlvl-lim", "sharing greater or equal than pt level \n", 1, IntRange(0,64));
static IntOption opt_update_act_pol("SPLITTER + SHARING", "upd-actpol", "Update Activity and polarity in treenode: 0 - disable, 1 activity only, 2 polarity only, 3 activity and polarity \n", 3, IntRange(0,3));
static BoolOption opt_init_random_act_pol("SPLITTER + SHARING", "rnd-actpol", "Initialize random polarity and activity, except for the root\n", false);
static IntOption opt_pull_learnts_interval("SPLITTER + SHARING", "pull-int", "learnt pull interval - zero to check on restart only \n", 0, IntRange(0,INT32_MAX));
static BoolOption    diversification   ("SPLITTER + SHARING", "split-diver", "If only one child formula is unsolved, then stop the solver of that node.\n", false);
static IntOption opt_max_tree_height("SPLITTER + SHARING", "max-tree", "Max tree height for diversification option, such that it does not go beyond that\n", 512, IntRange(8,512));
static BoolOption    randomization  ("SPLITTER + SHARING", "node-rnd", "Randomization in the nodes increases with 1% per level, if split-diver is off.\n", false);
static IntOption opt_diversification_conflict_limit("SPLITTER + SHARING", "spdiver-lim", "conflict limit for the solver with diversification option \n", 0, IntRange(0,INT32_MAX));
static BoolOption    protect_root_node  ("SPLITTER + SHARING", "prot-root", "Do not interrupt root node.\n", true);
static BoolOption    opt_restart_strategy_satunsat   ("SPLITTER + SHARING", "satunsat-restart", "SAT restart strategy for parent and UNSAT restart strategy for child.\n", false);
static IntOption  cleaning_delay     ("SPLITTER", "clean-delay","Cleaning delay for leaf and parent nodes; O is off, 1 is decreasing order, 2 is increasing order\n", 0, IntRange(0,2));
static IntOption opt_shared_clean_delay("SPLITTER + SHARING", "shclean-delay", "Keep the shared clause for a certain number of cleanings\n", 1, IntRange(0,1));
static BoolOption    opt_lbd_minimization  ("SPLITTER + SHARING", "lbd-min", "Enable lbd minimization.\n", true);
static BoolOption opt_simulate_portfolio ("SPLITTER + SHARING", "sim-port", "Enable Simulation of Portfolio.\n", false);
//=================================================================================================

SolverPT::SolverPT() :
        								tOut( 0 )
// Davide> Options
, learnt_unary_res(opt_learnt_unary_res)
, addClause_FalseRemoval(opt_addClause_FalseRemoval)
, sharedClauseMaxSize(opt_sharedClauseMaxSize)
, LBD_lt(opt_LBD_lt)
, learnt_worsening(opt_learnt_worsening)
, pools_filling(opt_pools_filling)
, dynamic_PTLevels(opt_dynamic_PTLevels)
, random_sharing(opt_random_sharing)
, random_sh_prob(opt_random_sh_prob)
, shconditions_relaxing(opt_shconditions_relaxing)
, every_shpool(opt_every_shpool)
, disable_stats(opt_disable_stats)
, disable_dupl_check(opt_disable_dupl_check)
, disable_dupl_removal(opt_disable_dupl_removal)
, sharing_delay(opt_sharing_delay)
, flag_based(opt_flag_based)
, receiver_filter(opt_receiver_filter)
, receiver_score(opt_receiver_score)
, update_act_pol(opt_update_act_pol)
, PTLevel        (0) // Davide> Used in analyze, litRedundant & search
, max_bad_literal(0) // Davide> used in analyze, litRedundant & search
, rndDecLevel0(false)
, level0UnitsIndex(0)
, lastLevel(0)
{
	// Davide> Statistics
	if( !disable_stats ){
		diversification_stop_nodes_ID = localStat.registerI("diversification_stop_nodes");
		n_pool_duplicatesID = localStat.registerI("n_pool_duplicates");
		n_threads_blockedID = localStat.registerI("n_threads_blocked");
		n_import_shcl_unsatID = localStat.registerI("n_import_shcl_unsat");
		sum_clauses_pools_lv1ID = localStat.registerI("sum_clauses_pools_lv1");
		sum_clauses_pools_lv1_effID = localStat.registerI("sum_clauses_pools_lv1_eff");
		sum_clauses_pools_lv0ID = localStat.registerI("sum_clauses_pools_lv0");
		sum_clauses_pools_lv0_effID = localStat.registerI("sum_clauses_pools_lv0_eff");
		n_unary_shclausesID =  localStat.registerI("n_unary_shclauses");
		n_binary_shclausesID = localStat.registerI("n_binary_shclauses");
		n_lbd2_shclausesID = localStat.registerI("n_lbd2_shclauses");
		n_clsent_curr_lvID = localStat.registerI("n_clsent_curr_lv");
		n_clsent_prev_lvID = localStat.registerI("n_clsent_prev_lv");
		n_clcanbeccminID = localStat.registerI("n_clcanbeccmin");
		n_ccmin_worseningID = localStat.registerI("n_ccmin_worsening");
		n_tot_sharedID      = localStat.registerI("n_tot_shared");
		n_tot_shared_minus_delID = localStat.registerI("n_tot_shared_minus_del");
		n_acquired_clausesID = localStat.registerI("n_acquired_clauses");
		n_tot_forced_restarts_ID = localStat.registerI("n_tot_forced_restarts");
		n_tot_reduceDB_calls_ID = localStat.registerI("n_tot_reduceDB_calls");
		//sharing_time_ID = localStat.registerD("clause_sharing_time");
	}
	receiver_filter_enum = receiver_filter == 0 ? NONE :
			receiver_filter == 1 ? SIZE :
					receiver_filter == 2 ? LBD_LT :
							receiver_filter == 3 ? PSM : PSM_FALSE;
}

SolverPT::~SolverPT() {
}

// Davide> Done, maybe .. It does remove literals of level zero
bool SolverPT::addClause_(vec<Lit>& ps, unsigned int pt_level) // Davide> pt_level has 0 as dfault
{
	assert(decisionLevel() == 0);
	if (!ok) return false;

	// Check if clause is satisfied and remove false/duplicate literals:
	sort(ps);
	Lit p; int i, j;
	for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
		if (value(ps[i]) == l_True || ps[i] == ~p)
			return true;
		else if (value(ps[i]) != l_False && ps[i] != p)
			ps[j++] = p = ps[i];
		else if ( value(ps[i]) == l_False && ps[i] != p ) {
			if( false && addClause_FalseRemoval == 0 ){ // TODO reconsider this
				// Davide> In order to maximize sharing, the safety of a clauses
				// is not corrupted
				if ( getLiteralPTLevel(ps[i]) > pt_level ) {
					ps[j++] = p = ps[i];
				}
			}
			else{
				const unsigned int& tmp = getLiteralPTLevel(ps[i]);
				pt_level = pt_level >= tmp ? pt_level : tmp; // Davide> Aggressive removal can prevent clauses to be shared higher in the PartitionTree
			}
		}
	ps.shrink(i - j);

	if (ps.size() == 0){
		lastLevel = pt_level;
		return ok = false;
	}
	else if (ps.size() == 1){
		if( value(ps[0]) == l_False ){
			assert(false && "addClause_ Unuseful");
			Debug::PRINTLN_NOTE("NOTE: tried to add unary false clause");
			if( !disable_stats ){
				localStat.changeI(n_import_shcl_unsatID, 1);
			}
			lastLevel = pt_level;
			return ok = false; // Davide> Trying to add unsat clause
		}
		uncheckedEnqueue(ps[0], CRef_Undef, pt_level); // Davide> attach pt_level info
		return ok = (propagate() == CRef_Undef);
	}else{
		const CRef cr = ca.alloc(ps,false);
		ca[cr].setPTLevel(pt_level); // Davide> Setting the pt_level

		clauses.push(cr);

		// TODO Norbert: uncomment, so that gets active
		//       Clause& c = ca[cr];
		//       assert(c.size() > 1 && "clause cannot be a unit clause" );
		//       // get watched literals right!
		//       int j = 0;
		//       for( int i = 0 ; i < 2; i++ ) {
		// 	for(  ; j < c.size(); ++ j ) {
		// 	  if( value(c[j]) == l_Undef || value(c[j]) == l_True ) break;
		// 	}
		// 	if( j < c.size() ) { // swap "watch-able" literal to front!
		// 	  const Lit tmp = c[j]; c[j] = c[i]; c[i] = tmp;
		// 	} else {
		// 	  assert( j == c.size() && "No other case left" );
		// 	  if( i == 0 ) {
		// 	    // this clause is empty -> this formula is UNSAT
		// 	    return ok = false;
		// 	  } else {
		// 	    assert( i == 1 && "No other case possible");
		// 	    uncheckedEnqueue(c[0], CRef_Undef, pt_level); // Davide> attach pt_level info
		// 	    c.mark(1); // delete clause during next garbageCollect!
		// 	    return ok = (propagate() == CRef_Undef);
		// 	  }
		// 	}
		//
		//      }

		attachClause(cr);
	}

	return true;
}

Var SolverPT::newVar(bool sign, bool dvar)
{
	assert(varPT.size() == nVars());
	varPT.push(0);//adding PT level of the new variable
	int v = nVars();
	watches  .init(mkLit(v, false));
	watches  .init(mkLit(v, true ));
	watchesBin  .init(mkLit(v, false));
	watchesBin  .init(mkLit(v, true ));
	assigns  .push(l_Undef);
	vardata  .push(mkVarData(CRef_Undef, 0));
	//activity .push(0);
	if(opt_init_random_act_pol ){
		//double rseed = random_seed + tnode->id();
		if(opt_update_act_pol>0 && curPTLevel==1){
			//activity.push(drand(random_seed) * 0.00001);
			polarity .push(irand(random_seed,2));
		}else if(opt_update_act_pol==0 && curPTLevel >0){
			activity.push(drand(random_seed) * 0.00001);
			polarity .push(irand(random_seed,2));
		}
	}
	activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
	seen     .push(0);
	permDiff  .push(0);
	polarity .push(sign);
	decision .push();
	trail    .capacity(v+1);
	setDecisionVar(v, dvar);
}

void SolverPT::setLiteralPTLevel(const Lit& l, unsigned pt){
	varPT[var(l)] = pt;
}

unsigned SolverPT::getLiteralPTLevel(const Lit& l) const {
	assert(var(l) < varPT.size());
	return varPT[var(l)];
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
void SolverPT::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel,unsigned int &lbd)
{
	int pathC = 0;
	Lit p     = lit_Undef;

	// Generate conflict clause:
	//
	out_learnt.push();      // (leave room for the asserting literal)
	int index   = trail.size() - 1;

	do{
		assert(confl != CRef_Undef); // (otherwise should be UIP)
		Clause& c = ca[confl];
		PTLevel = PTLevel >= c.getPTLevel() ? PTLevel : c.getPTLevel();
		//		if(c.getPTLevel() > PTLevel){PTLevel = c.getPTLevel();}

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

			if (!seen[var(q)]){
				if(level(var(q)) > 0){
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
					}
				}
				// Davide> This is already an extension to the work of Antti :
				//         clauses that contain literals that could resolve with
				//         some unsafe literal of level zero ARE NOT ( or they are, learnt_unary_res .. ) resolved.
				//         Thus, we avoid marking these clauses as UNSAFE
				//         and we can share them ( if they have not been obtained
				//         by a resolution with an unsafe clause, already )
				//
				else // Davide> level(var(q)) == 0
					if( !learnt_unary_res && getLiteralPTLevel(q) > PTLevel ){ // Davide> This PTLevel is temporary, it could increase ( that is, we could simplify more the clause ) as we continue to analyze the relevant clauses
						seen[var(q)] = 1;
						out_learnt.push(q);

						max_bad_literal = max_bad_literal >= getLiteralPTLevel(q) ? max_bad_literal : getLiteralPTLevel(q);
						//						if( getLiteralPTLevel(q) > max_bad_literal )
						//							max_bad_literal = getLiteralPTLevel(q);
					}
					else
						if( learnt_unary_res ){
							PTLevel = PTLevel >= getLiteralPTLevel(q) ? PTLevel : getLiteralPTLevel(q);
							//							if( getLiteralPTLevel(q) > PTLevel )
							//								PTLevel = getLiteralPTLevel(q);
							max_bad_literal = max_bad_literal >= getLiteralPTLevel(q) ? max_bad_literal : getLiteralPTLevel(q);
							//							if( getLiteralPTLevel(q) > max_bad_literal )
							//								max_bad_literal = getLiteralPTLevel(q);
						}
			}
		}

		// Select next clause to look at:
		while (!seen[var(trail[index--])]);
		p     = trail[index+1];
		confl = reason(var(p));
		seen[var(p)] = 0;
		pathC--;

	}while (pathC > 0);
	out_learnt[0] = ~p;
	if(confl!=CRef_Undef){
		Clause& c = ca[confl];
		PTLevel = PTLevel >= c.getPTLevel() ? PTLevel : c.getPTLevel();
		//		if(c.getPTLevel()>PTLevel) PTLevel = c.getPTLevel();
	}
	// Simplify conflict clause:
	//
	int i, j;
	out_learnt.copyTo(analyze_toclear);
	unsigned int oldSize = out_learnt.size();

	unsigned int oldPTLevel = PTLevel; // Davide> for statistics

	if (ccmin_mode == 2){
		// Davide> Niklas Sorensson's words:
		//
		// 'abstract_levels' is just a simple approximate representation of the
		// set of levels present in the original clause.
		// The idea is that a literal that introduces a level not present in the
		// original learned clause can never be successfully removed.

		uint32_t abstract_level = 0;

		for (i = 1; i < out_learnt.size(); i++)
			abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)
		for (i = j = 1; i < out_learnt.size(); i++)
			if (reason(var(out_learnt[i])) == CRef_Undef || !litRedundant(out_learnt[i], abstract_level) )
				out_learnt[j++] = out_learnt[i];

		// Davide> statistics
		if( !disable_stats )

			if( out_learnt.size() != oldSize )
				statistics.changeI(n_clcanbeccminID, 1);


	}else if (ccmin_mode == 1){ // Davide> & let's enable this

		// Davide> Here I follow a much conservative approach: if the clause is safe,
		//         then perform only those resolutions that do not destroy the
		//         safety of the learnt clause. Otherwise, do whatever you want
		//         Consider of adding a parameter

		for (i = j = 1; i < out_learnt.size(); i++){
			Var x = var(out_learnt[i]);

			if ( reason(x) == CRef_Undef /*|| (vardata[x].pt_level > PTLevel)*/ ) // Davide> If not safe, the literal cannot be removed if its level is bigger than the clause to be learnt ( EDITED )
				out_learnt[j++] = out_learnt[i];
			else{
				Clause& c = ca[reason(x)];
				if( c.getPTLevel() <= PTLevel ){ // Davide> the clause is safe
					for (int k = 1; k < c.size(); k++)
						if ( !seen[var(c[k])] )
							if( level(var(c[k])) > 0 || (getLiteralPTLevel(c[k]) > PTLevel ) ){ // Davide> literals
								out_learnt[j++] = out_learnt[i];
								break;
							}
							else if( learnt_worsening && level(var(c[k])) == 0  ){
								PTLevel = getLiteralPTLevel(c[k]); // Davide> ccmin clause worsening
							}
				}
				else{
					out_learnt[j++] = out_learnt[i]; } // Davide> do not consider unsafe reason clauses
			}
		}
		// Davide> statistics
		if( !disable_stats )

			if( out_learnt.size() != oldSize )
				statistics.changeI(n_clcanbeccminID, 1);

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
	if(opt_lbd_minimization && out_learnt.size()<=lbSizeMinimizingClause) {
		Debug::PRINTLN_DEBUG("START MINIMIZING LEARNT CLAUSE");
		Debug::PRINTLN_DEBUG(out_learnt);
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

			// Davide>
			// The binary clauses watched by ~out_learnt[0]
			vec<Watcher>&  wbin  = watchesBin[p];//p is ~outlearnt[0]
			Debug::PRINTLN_DEBUG("WBIN IS: ");
			int nb = 0;
			for(int k = 0;k<wbin.size();k++) {
				Lit imp = wbin[k].blocker;
				//Debug::DEBUG_PRINTLN("imp is:");
				//Debug::DEBUG_PRINTLN(imp);
				if(permDiff[var(imp)]==MYFLAG && value(imp)==l_True) {
					// Davide> Similar to self-resolution, so I handle in a similar way
					Clause&  c         = ca[wbin[k].cref];
					PTLevel = PTLevel >= c.getPTLevel() ? PTLevel : c.getPTLevel();
					//					if( c.getPTLevel() > PTLevel ){
					//						//Debug::PRINTLN("WORSENING!!");
					//						PTLevel = c.getPTLevel();
					//					}
					if(level(var(c[0]))==0){
						const unsigned &tmp_lt_ptlevel = getLiteralPTLevel(c[0]);
						PTLevel = PTLevel > tmp_lt_ptlevel ? PTLevel : tmp_lt_ptlevel;
					}
					if(level(var(c[1]))==0){
						const unsigned &tmp_lt_ptlevel = getLiteralPTLevel(c[1]);
						PTLevel = PTLevel > tmp_lt_ptlevel ? PTLevel : tmp_lt_ptlevel;
					}
					nb++;
					permDiff[var(imp)]= MYFLAG-1;
				}
			}
			int l = out_learnt.size()-1;
			if(nb>0) {
				nbReducedClauses++;
				for(int i = 1;i<out_learnt.size()-nb;i++) {
					if(permDiff[var(out_learnt[i])]!=MYFLAG) {
						// PTLevel = PTLevel >= getLiteralPTLevel(out_learnt[i]) ? PTLevel : getLiteralPTLevel(out_learnt[i]);
						Lit p = out_learnt[l];
						out_learnt[l] = out_learnt[i];
						out_learnt[i] = p;
						l--;i--;
					}
				}

				//    printClause(out_learnt);
				//printf("\n");
				out_learnt.shrink(nb);

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

	if( tnode->lv_pool->max_size == 0 ) return; // Davide> Sharing disabled
	if( flag_based && PTLevel != 0 ) return;    // Davide> Only safe clauses can be shared
	if(getNodePTLevel() !=0 && getNodePTLevel()==PTLevel && tnode->childsCount()==0) return; //if no children of this node ... then dont add the clauses to shared pool of current node

	double startSharingTime = cpuTime_t();
	// Davide> Add a UNARY clause to the shared pool, if it is "safe enough"
	if( out_learnt.size() == 1 ){
		//if(opt_unit_sharing && PTLevel<=opt_unit_sharing_ptlevel_limit) return;//will be shared by unit_sharing option
		if( (rand() % 100) < random_sh_prob ) return;

		unsigned int tempPTLevel = PTLevel;

		if( dynamic_PTLevels ){
			tempPTLevel = tempPTLevel + floor(log2(out_learnt.size()));  // Davide> out_learnt is never empty ... is it ?
			tempPTLevel = tempPTLevel >= getNodePTLevel() ? PTLevel: tempPTLevel;
			//			if( tempPTLevel > getNodePTLevel() ) tempPTLevel = PTLevel;
		}
		if( pools_filling ){
			vector<TreeNode*> intermediate_nodes;
			for( unsigned int i = 0; i <= getNodePTLevel(); i++ )
				intermediate_nodes.push_back(0);

			TreeNode* temp_node = tnode;
			for( int i = static_cast<int>(getNodePTLevel()); i >= static_cast<int>(tempPTLevel); i-- ){
				intermediate_nodes[i] = temp_node;
				temp_node = temp_node->getFather();
			}
			davide::LevelPool* pool;
			while( tempPTLevel < getNodePTLevel() ){
				pool = intermediate_nodes[tempPTLevel]->lv_pool;
				if( pool->isFull() )
					tempPTLevel++;
				else break;
			}
		}
		if( !disable_stats ){
			// Davide> statistics
			if( tempPTLevel == 1 )
				localStat.changeI(sum_clauses_pools_lv1ID, 1);
			else if( tempPTLevel == 0 )
				localStat.changeI(sum_clauses_pools_lv0ID, 1);
			// if( out_learnt.size() == 1 )
			localStat.changeI(n_unary_shclausesID, 1);
			// if( out_learnt_lbd <= 2 )
			localStat.changeI(n_lbd2_shclausesID, 1);
			if( oldPTLevel != tempPTLevel )
				localStat.changeI(n_ccmin_worseningID,1);
			if( tempPTLevel == getNodePTLevel() )
				localStat.changeI(n_clsent_curr_lvID,1);
			if( tempPTLevel == getNodePTLevel() - 1 )
				localStat.changeI(n_clsent_prev_lvID,1);
		}

		int back_steps = getNodePTLevel() - tempPTLevel;

		TreeNode* back_steps_node = tnode;
		while( back_steps-- > 0 ){
			back_steps_node = back_steps_node->getFather();
		}

		davide::LevelPool* pool = back_steps_node->lv_pool;

		if(pool == 0) return;

		bool sharedSuccess = false;

		// ******************** CRITICAL SECTION ************************** //

		//		int old_cancel_state; // THREADS DO NOT GET CANCELED ANYMORE, COMMENT THIS
		//		pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &old_cancel_state);

		if(!pool->levelPoolLock.try_wLock()){
			if( !disable_stats )
				localStat.changeI(n_threads_blockedID, 1); // Davide> statistics
			pool->levelPoolLock.wLock();
		}
		sharedSuccess = pool->add_shared(out_learnt, tnode->id(), disable_dupl_removal, disable_dupl_check);
		bool fullPool = pool->isFull();

		pool->levelPoolLock.unlock();
		//		pthread_setcancelstate(old_cancel_state, NULL);

		// ******************** END OF CRITICAL SECTION ******************* //

		if ( !sharedSuccess )
			if( !disable_stats )
				localStat.changeI(n_pool_duplicatesID, 1); // Davide> statistics

		if( !disable_stats ){
			if( !fullPool )
				localStat.changeI(n_tot_shared_minus_delID, 1);

			localStat.changeI(n_tot_sharedID, 1);

			if( tempPTLevel == 0 )
				localStat.changeI(sum_clauses_pools_lv0_effID,1);
			else if( tempPTLevel == 1 )
				localStat.changeI(sum_clauses_pools_lv1_effID,1);
		}


	} // Davide> END OF ADDING A UNARY CLAUSE TO THE SHARED POOL
	else if( (out_learnt.size() <= sharedClauseMaxSize || lbd <= LBD_lt) || shconditions_relaxing || random_sharing || opt_dyn_lbd_shr ){

		if( (rand() % 100) < random_sh_prob ) return;

		unsigned int tempPTLevel = PTLevel;

		if( dynamic_PTLevels ){
			tempPTLevel = tempPTLevel + floor(log2(out_learnt.size())); // Davide> out_learnt.size() should be different from zero ..
			if( tempPTLevel > getNodePTLevel() ) tempPTLevel = PTLevel;
		}
		if( pools_filling ){
			vector<TreeNode*> intermediate_nodes;
			for( unsigned int i = 0; i <= getNodePTLevel(); i++ )
				intermediate_nodes.push_back(0);

			TreeNode* temp_node = tnode;
			for( int i = static_cast<int>(getNodePTLevel()); i >= static_cast<int>(tempPTLevel); i-- ){
				intermediate_nodes[i] = temp_node;
				temp_node = temp_node->getFather();
			}
			davide::LevelPool* pool;
			while( tempPTLevel < getNodePTLevel() ){
				pool = intermediate_nodes[tempPTLevel]->lv_pool;
				if( pool->isFull() )
					tempPTLevel++;
				else break;
			}
		}
		if( shconditions_relaxing ){
			if( out_learnt.size() > sharedClauseMaxSize + floor(tempPTLevel != 0 ? log2(tempPTLevel) : 0) && lbd > LBD_lt + floor(tempPTLevel != 0 ? log2(tempPTLevel) : 0) )
				return;
		}
		if (opt_dyn_lbd_shr) {
			if(lbd>ceil(opt_dyn_lbd_shr_fac*(sumLBD / conflicts))&& out_learnt.size() > sharedClauseMaxSize && lbd > LBD_lt)
				return;
		}
		// Davide> statistics
		if( !disable_stats ){
			if( tempPTLevel == 1 )
				localStat.changeI(sum_clauses_pools_lv1ID, 1);
			else if( tempPTLevel == 0 )
				localStat.changeI(sum_clauses_pools_lv0ID, 1);
			if( out_learnt.size() == 1 )
				localStat.changeI(n_unary_shclausesID, 1);
			else if( out_learnt.size() == 2 )
				localStat.changeI(n_binary_shclausesID,1);
			if( lbd <= 2 )
				localStat.changeI(n_lbd2_shclausesID, 1);
			if( oldPTLevel != tempPTLevel )
				localStat.changeI(n_ccmin_worseningID,1);
			if( tempPTLevel == getNodePTLevel() )
				localStat.changeI(n_clsent_curr_lvID,1);
			if( tempPTLevel == getNodePTLevel() - 1 )
				localStat.changeI(n_clsent_prev_lvID,1);
		}

		// Davide> Register the cr index of the clause
		learnts_indeces[tempPTLevel].push_back(learnts.size());
	}
	// localStat.changeD(sharing_time_ID, cpuTime_t()-startSharingTime);
}

// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool SolverPT::litRedundant(Lit p, uint32_t abstract_levels)
{
	analyze_stack.clear(); analyze_stack.push(p);
	int top = analyze_toclear.size();
	while (analyze_stack.size() > 0){
		assert(reason(var(analyze_stack.last())) != CRef_Undef);
		Clause& c = ca[reason(var(analyze_stack.last()))];
		//CRef cr = reason(var(analyze_stack.last()));
		analyze_stack.pop();
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
			// Davide> This literal will not be counted, hence
			//         I should make the pt_level worse
			//         TODO: is it possible to avoid this worsening ?
			else if( level(var(p)) == 0 && getLiteralPTLevel(p) > PTLevel ){
				PTLevel = getLiteralPTLevel(p);
			}
		}
		// Davide> if I am here, a resolution can be done
		if( /*cr == CRef_Undef ||*/ c.getPTLevel() > PTLevel )
			if(learnt_worsening){
				/*if(cr == CRef_Undef)
                        PTLevel = getLiteralPTLevel(analyze_stack.last());
                    else*/
				PTLevel = c.getPTLevel();
			}
			else{
				for (int j = top; j < analyze_toclear.size(); j++)
					seen[var(analyze_toclear[j])] = 0;
				analyze_toclear.shrink(analyze_toclear.size() - top);
				return false;
			}
	}

	return true;
}

void SolverPT::uncheckedEnqueue(Lit p, CRef from, unsigned int pt_level)
{
	Solver::uncheckedEnqueue(p, from);
	setLiteralPTLevel(p, pt_level); // Davide> EDIT Updating the pt_level of the literal
}

Lit SolverPT::pickBranchLit()
{
	Var next = var_Undef;

	// Random decision:
	if (((rndDecLevel0 && decisionLevel()==0) || drand(random_seed) < random_var_freq) && !order_heap.empty()){
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
|  propagate : [void]  ->  [Clause*]
|  
|  Description:
|    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
|    otherwise CRef_Undef.
|  
|    Post-conditions:
|      * the propagation queue is empty, even if there was a conflict.
|________________________________________________________________________________________________@*/
CRef SolverPT::propagate()
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
				return wbin[k].cref;
			}

			if(value(imp) == l_Undef) {
				if( decisionLevel() == 0 && tnode->id()!=0 ){
					// Davide> TOO SLOW !! Trying to "optimize
					// Davide> I want the "most constraining" literal to be
					// found out
					unsigned int max_bad_pt_level = 0;
					Clause& c = ca[wbin[k].cref];
					for( int i = 0; i < c.size(); i++ )
						max_bad_pt_level = max_bad_pt_level >= getLiteralPTLevel(c[i]) ? max_bad_pt_level : getLiteralPTLevel(c[i]);
					//						if( getLiteralPTLevel(c[i]) > max_bad_pt_level ){
					//							max_bad_pt_level = getLiteralPTLevel(c[i]);
					//							// break; Davide> Unfortunately, I cannot break now here
					//						}
					if( c.getPTLevel() > max_bad_pt_level )
						uncheckedEnqueue(imp, wbin[k].cref, c.getPTLevel());
					else
						uncheckedEnqueue(imp, wbin[k].cref, max_bad_pt_level);
				} // Davide> End of my Part
				else{
					//printLit(p);printf(" ");printClause(wbin[k].cref);printf("->  ");printLit(imp);printf("\n");
					uncheckedEnqueue(imp,wbin[k].cref);
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
				confl = cr;
				qhead = trail.size();
				// Copy the remaining watches:
				while (i < end)
					*j++ = *i++;
			}else {
				// Davide> A zero-level variable is unsafe if:
				//         1) it underlies a unary constraint
				//         2) it is propagated by an unsafe clause
				//         3) it is propagated by a clause containing literals
				//            underlied by unsafe variables
				if(decisionLevel() == 0 && tnode->id()!=0 ){
					// Davide> TOO SLOW !! Trying to "optimize
					// Davide> I want the "most constraining" literal to be
					// found out
					unsigned int max_bad_pt_level = 0;
					for( int i = 0; i < c.size(); i++ )
						max_bad_pt_level = max_bad_pt_level >= getLiteralPTLevel(c[i]) ? max_bad_pt_level : getLiteralPTLevel(c[i]);
					//						if( getLiteralPTLevel(c[i]) > max_bad_pt_level ){
					//							max_bad_pt_level = getLiteralPTLevel(c[i]);
					//							// break; Davide> Unfortunately, I cannot break now here
					//						}
					assert(value(first) == l_Undef);
					unsigned temp = max_bad_pt_level >= c.getPTLevel() ? max_bad_pt_level : c.getPTLevel(); 
					uncheckedEnqueue(first, cr, temp);
					//if( c.getPTLevel() > max_bad_pt_level )
					//uncheckedEnqueue(first, cr, c.getPTLevel());
					//else
					//uncheckedEnqueue(first, cr, max_bad_pt_level);
				} // Davide> End of my Part
				else{
					uncheckedEnqueue(first, cr);
				}

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
						/*if(tnode->childsCount()>0 && c.lbd()>opt_LBD_lt && nblevels < opt_LBD_lt)  {
                    fprintf(stderr,"-------------------Sharing Decreased LBD Clause------------------ \n");
                                        //decreasedLbdPush(cr);
                                    }*/
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
//static  long conf4stats = 0,cons = 0,curRestart=1;
lbool SolverPT::search(int nof_conflicts)
{
	assert(ok);
	int         backtrack_level;
	int         conflictC = 0;
	vec<Lit>    learnt_clause;
	unsigned int nblevels;
	bool blocked=false;
	starts++;
	bool pullClausesCheck=false; //for checking if the clauses have been pulled
	//ahmed> pulling the shared clauses
	pull_learnts(curRestart);
	if(!ok)  return l_False;
	//ahmed> updating activity and polarity in treenode
	if(update_act_pol>0){
                        tnode->updateActivityPolarity(activity, polarity,update_act_pol);
                  }
	if(opt_pull_learnts_interval==0) {
		pull_learnts(starts);
		if(!ok)  return l_False;
	}

	for (;;){
		if(asynch_interrupt) return l_Undef; // Davide
		PTLevel = 0; // Davide> if I learn an unsafe clause I have to tag it
		max_bad_literal = 0;
		//Ahmed> pull shared clauses
		if (opt_pull_learnts_interval != 0 && !pullClausesCheck && conflictC % opt_pull_learnts_interval == 0){
			pull_learnts(starts);
			if(!ok)  return l_False;
			pullClausesCheck=true;
		}
		CRef confl = propagate();
		if (confl != CRef_Undef){
			// CONFLICT
			conflicts++; conflictC++;

			/*if (verbosity >= 1 && conflicts%verbEveryConflicts==0){
	    printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% |\n", 
		   (int)starts,(int)nbstopsrestarts, (int)(conflicts/starts), 
		   (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
		   (int)nbReduceDB, nLearnts(), (int)nbDL2,(int)nbRemovedClauses, progressEstimate()*100);
	  }*/
			if (decisionLevel() == 0) {
				Clause& c = ca[confl];
				lastLevel = c.getPTLevel();
				for( int i = 0; i < c.size(); i++ ){
					lastLevel = lastLevel > getLiteralPTLevel(c[i]) ? lastLevel : getLiteralPTLevel(c[i]);
				}

				return l_False;
			}

			trailQueue.push(trail.size());
			if( conflicts>LOWER_BOUND_FOR_BLOCKING_RESTART && lbdQueue.isvalid()  && trail.size()>R*trailQueue.getavg()) {
				lbdQueue.fastclear();
				nbstopsrestarts++;
				if(!blocked) {lastblockatrestart=starts;nbstopsrestartssame++;blocked=true;}
			}

			learnt_clause.clear();
			analyze(confl, learnt_clause, backtrack_level,nblevels);

			lbdQueue.push(nblevels);
			sumLBD += nblevels;


			cancelUntil(backtrack_level);

			if (learnt_clause.size() == 1){
				uncheckedEnqueue(learnt_clause[0], CRef_Undef, PTLevel);nbUn++;
				level0UnitsIndex++;
			}else{
				CRef cr = ca.alloc(learnt_clause, true);
				ca[cr].setLBD(nblevels);
				if(nblevels<=2) nbDL2++; // stats
				if(ca[cr].size()==2) nbBin++; // stats

				ca[cr].setPTLevel(PTLevel); //ahmed> setting the PTLevel of the new learnt clause
				if( decisionLevel() == 0 ){
					PTLevel = PTLevel >= max_bad_literal ? PTLevel : max_bad_literal;
					//					if( max_bad_literal > PTLevel )
					//						PTLevel = max_bad_literal; // Davide> for the propagation
				}

				learnts.push(cr);
				attachClause(cr);

				claBumpActivity(ca[cr]);
				//				if( decisionLevel() == 0 ){
				//					for( int i = 1; i < learnt_clause.size(); i++ )
				//						assert(getLiteralPTLevel(learnt_clause[i]) <= PTLevel);
				//				}
				uncheckedEnqueue(learnt_clause[0], cr, PTLevel);
			}

			varDecayActivity();
			claDecayActivity();

			if ( conflicts % sharing_delay == 0 ){ // Davide> That thing every 100 conflicts
				push_learnts(); // Davide>
			}
			pullClausesCheck=false;
		}else{
			// Our dynamic restart, see the SAT09 competition compagnion paper
			if (( lbdQueue.isvalid() && ((lbdQueue.getavg()*K) > (sumLBD / conflicts)))) {
				lbdQueue.fastclear();
				progress_estimate = progressEstimate();
				cancelUntil(0);
				return l_Undef; }


			// Simplify the set of problem clauses:
			if (decisionLevel() == 0){
				if(opt_unit_sharing && starts>1){
					push_units();
				}
				level0UnitsIndex = getTopLevelUnits(); //skipping the trivial units as they are already progated by parents
				push_learnts(); // Davide> Share learnts clauses that can be shared
				pull_learnts(starts);
				if(!ok)  return l_False;
				if(!simplify()) {
					printf("c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
					return l_False;
				}
			}
			// Perform clause database reduction !
			if(conflicts>=curRestart* nbclausesbeforereduce) 
			{
				push_learnts();
				localStat.changeI(n_tot_reduceDB_calls_ID, 1);
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

			// Increase decision level and enqueue 'next'
			newDecisionLevel();
			uncheckedEnqueue(next);
		}
	}
}

lbool SolverPT::solve_() {
	// Davide> attempt
	vector<unsigned int> a;
	for( unsigned int i = 0; i <= getNodePTLevel(); i++ )
		learnts_indeces.push_back(a); // Davide> copy

	model.clear();
	conflict.clear();
	if (!ok) return l_False;

	lbdQueue.initSize(sizeLBDQueue);

	trailQueue.initSize(sizeTrailQueue);
	sumLBD = 0;

	solves++;


	lbool   status        = l_Undef;
	nbclausesbeforereduce = firstReduceDB;

	// Search:
	int curr_restarts = 0;
	while (status == l_Undef){
		if(diversification && curPTLevel < opt_max_tree_height - 8) {
			if(!protect_root_node || tnode->id()!=0) {
				if(tnode->isOnlyChildScenario() && conflict_budget<0 ){
					localStat.changeI(diversification_stop_nodes_ID,1);
					Debug::PRINTLN_DEBUG("NOTE: Diversification limited solver\n" );
					conflict_budget=conflicts+opt_diversification_conflict_limit*(curPTLevel+1);
				}
			}
		}else if(opt_simulate_portfolio && tnode->id()!=0 && tnode->getFather()->isOnlyChildScenario()){
			rndDecLevel0=true;
		}
		if(opt_restart_strategy_satunsat) {
			if(!protect_root_node || tnode->id()!=0) {
				if(tnode->childsCount()>0) {
					satRestartStrategy();
				} else {
					unsatRestartStrategy();
				}
			}
		}

		status = search(0); // the parameter is useless in glucose, kept to allow modifications

		if (!withinBudget()) break;
		curr_restarts++;
	}

	if (status == l_True){
		// Extend & copy model:
		model.growTo(nVars());
		for (int i = 0; i < nVars(); i++) model[i] = value(i);
	}else if (status == l_False && conflict.size() == 0)
		ok = false;

	cancelUntil(0);
	return status;
}

// ## begin splitter minisat modifications:
/// Get CPU time used by this thread
double SolverPT::cpuTime_t() const  {
	struct timespec ts;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
		perror("clock_gettime()");
		ts.tv_sec = -1;
		ts.tv_nsec = -1;
	}
	return (double) ts.tv_sec + ts.tv_nsec/1000000000.0;
}

/// return true, if the run time of the solver exceeds the specified limit
bool SolverPT::timedOut() const {
	return (tOut > 0) && cpuTime_t() > tOut;
}

/// specify a number of seconds that is allowed to be executed
void SolverPT::setTimeOut(double timeout) {
	tOut = cpuTime_t() + timeout;
	//  fprintf(stderr, "set exit time to %f, current: %f\n", tOut, cpuTime_t() );
}

unsigned int SolverPT::getTopLevelUnits() const { 
	return ( trail_lim.size() > 1 ) ? trail_lim[1] : trail.size();
};

/// return a specifit literal from the trail
Lit SolverPT::trailGet( const unsigned int index ){
	return trail[index];
}

// ahmed > sharing units
void SolverPT::push_units(){
	if( tnode->lv_pool->max_size == 0 ) return; // Davide> Sharing disabled
	double startSharingTime = cpuTime_t();
	for(unsigned int trailIndex = level0UnitsIndex; trailIndex < getTopLevelUnits(); trailIndex++){
		Lit l = trail[trailIndex];
		unsigned int tempPTLevel = getLiteralPTLevel(l);
		//if(tempPTLevel<opt_unit_sharing_ptlevel_limit)//skipping the units whose level is below limit
		if(tempPTLevel<curPTLevel)//skipping the units whose ptlevel is less than the ptlevel of the node
			continue;
		if( pools_filling ){
			vector<TreeNode*> intermediate_nodes;
			for( unsigned int i = 0; i <= getNodePTLevel(); i++ )
				intermediate_nodes.push_back(0);

			TreeNode* temp_node = tnode;
			for( int i = static_cast<int>(getNodePTLevel()); i >= static_cast<int>(tempPTLevel); i-- ){
				intermediate_nodes[i] = temp_node;
				temp_node = temp_node->getFather();
			}
			davide::LevelPool* pool;
			while( tempPTLevel < getNodePTLevel() ){
				pool = intermediate_nodes[tempPTLevel]->lv_pool;
				if( pool->isFull() )
					tempPTLevel++;
				else break;
			}
		}
		if( !disable_stats ){
			// Davide> statistics
			if( tempPTLevel == 1 )
				localStat.changeI(sum_clauses_pools_lv1ID, 1);
			else if( tempPTLevel == 0 )
				localStat.changeI(sum_clauses_pools_lv0ID, 1);
			// if( out_learnt.size() == 1 )
			localStat.changeI(n_unary_shclausesID, 1);
			// if( out_learnt_lbd <= 2 )
			localStat.changeI(n_lbd2_shclausesID, 1);
			if( tempPTLevel == getNodePTLevel() )
				localStat.changeI(n_clsent_curr_lvID,1);
			if( tempPTLevel == getNodePTLevel() - 1 )
				localStat.changeI(n_clsent_prev_lvID,1);
		}

		int back_steps = getNodePTLevel() - tempPTLevel;

		TreeNode* back_steps_node = tnode;
		while( back_steps-- > 0 ){
			back_steps_node = back_steps_node->getFather();
		}

		davide::LevelPool* pool = back_steps_node->lv_pool;
		if(pool == 0) continue;

		// Davide> PREPARING FOR CRITICAL SECTION

		bool sharedSuccess = false;
		bool fullPool = false;
		vec<Lit> c;
		c.push(l);

		// ******************** CRITICAL SECTION ************************** //

		if(!pool->levelPoolLock.try_wLock()){
			if( !disable_stats )
				localStat.changeI(n_threads_blockedID, 1);
			pool->levelPoolLock.wLock();
		}

		sharedSuccess =
				pool->add_shared(c, tnode->id() , disable_dupl_removal, disable_dupl_check);
		fullPool = pool->isFull();
		pool->levelPoolLock.unlock();

		// ******************** END OF CRITICAL SECTION ******************* //

		if ( !sharedSuccess )
			if( !disable_stats )
				localStat.changeI(n_pool_duplicatesID, 1); // Davide> statistics

		if( !disable_stats ){
			if( !fullPool )
				localStat.changeI(n_tot_shared_minus_delID, 1);

			localStat.changeI(n_tot_sharedID, 1);

			if( tempPTLevel == 0 )
				localStat.changeI(sum_clauses_pools_lv0_effID,1);
			else if( tempPTLevel == 1 )
				localStat.changeI(sum_clauses_pools_lv1_effID,1);
		}
	}
	level0UnitsIndex = getTopLevelUnits(); //storing the index of start of level1 literal
	//localStat.changeD(sharing_time_ID, cpuTime_t()-startSharingTime);
}

// Davide> Shares the learnts
void SolverPT::push_learnts(){
	if( tnode->lv_pool->max_size == 0 ) return; // Davide> Sharing disabled
	double startSharingTime = cpuTime_t();
	// Davide> Idea1 : put shared clauses into shared pools
	vector<davide::LevelPool*> previous_pools;

	for( unsigned int i = 0; i <= getNodePTLevel(); i++ )
		previous_pools.push_back(0);

	TreeNode* curNode = tnode;

	unsigned int l = getNodePTLevel();
	while ( curNode != 0 ){
		previous_pools[l] = curNode->lv_pool;
		curNode = curNode->getFather();
		l--;
	}

	bool sharedSuccess = false;
	bool fullPool = false;

	for( unsigned int i = 0; i < learnts_indeces.size(); i++ ){
		davide::LevelPool* pool = previous_pools[i];

		if(pool == 0) continue;


		// ******************** CRITICAL SECTION ************************** //

		if(!pool->levelPoolLock.try_wLock()){ // Attempt with spin-lock
			if( !disable_stats )
				localStat.changeI(n_threads_blockedID, 1); // Davide> statistics
			pool->levelPoolLock.wLock();
		}
		for( unsigned int j = 0; j < learnts_indeces[i].size(); j++ ){
			Clause& c = ca[learnts[learnts_indeces[i][j]]];
			c.setShared();

			vec<Lit> c_lits;
			for(unsigned j=0; j < c.size(); j++){//creating vector of literals present in the clause
				c_lits.push(c[j]);
			}


			sharedSuccess = pool->add_shared( c_lits, tnode->id(), disable_dupl_removal, disable_dupl_check );
			fullPool = pool->isFull();

			if ( !sharedSuccess )
				if( !disable_stats )
					localStat.changeI(n_pool_duplicatesID, 1); // Davide> statistics

			if( !disable_stats ){
				if( !fullPool )
					localStat.changeI(n_tot_shared_minus_delID, 1);

				localStat.changeI(n_tot_sharedID,1);

				if( i == 0 )
					localStat.changeI(sum_clauses_pools_lv0_effID,1);
				else if( i == 1 )
					localStat.changeI(sum_clauses_pools_lv1_effID,1);
			}
		}
		pool->levelPoolLock.unlock(); // Davide> End of critical

		// ******************** END OF CRITICAL SECTION ******************* //
	}
	// Davide> cleaning things
	for( unsigned int i = 0; i < learnts_indeces.size(); i++ )
		learnts_indeces[i].clear();

	//localStat.changeD(sharing_time_ID, cpuTime_t()-startSharingTime);
}

void SolverPT::pull_learnts(int curr_restarts){
	unsigned lbdAverage = (unsigned)(sumLBD / conflicts);
	if( tnode->lv_pool->max_size != 0 ){
		double startSharingTime = cpuTime_t();
		// Davide> Luby pool access approach
		// Every level gets assigned a certain luby sequence. Which one ?
		// I should put a parameter for deciding that
		int i = getNodePTLevel();
		int back_steps;
		TreeNode* back_steps_node;
		if(opt_simulate_portfolio && tnode->isOnlyChildScenario()){
			//Debug::PRINTLN("NOTE: Starting portfolio");
			i=tnode->getOnlyChildScenarioChildNode()->getPTLevel();
			if(shared_indeces.size()<=i){
				assert(1 + i - shared_indeces.size() >=0);
				unsigned j=1 + i - shared_indeces.size();
				while(j>0){
					shared_indeces.push_back(0);
					j--;
				}
			}
		}
		vec<Lit> chunk;
		for( ; i >= 0; i -- ){
			if( /*curr_restarts % ( (getNodePTLevel() - i) + 1 ) == 0 ||*/ every_shpool ){
				if(opt_simulate_portfolio && tnode->isOnlyChildScenario()){
					back_steps=tnode->getOnlyChildScenarioChildNode()->getPTLevel()-i;
					back_steps_node = tnode->getOnlyChildScenarioChildNode();
				}else{
					back_steps = getNodePTLevel() - i;
					back_steps_node = tnode;
				}

				while( back_steps-- > 0 ){
					back_steps_node = back_steps_node -> getFather();
				}

				chunk.clear();
				davide::LevelPool* pool = back_steps_node->lv_pool;
				if(pool==0)
					continue;
				bool check = true;
				// ******************** CRITICAL SECTION ************************** //

				if(!pool->levelPoolLock.try_rLock()){
					if( !disable_stats )
						localStat.changeI(n_threads_blockedID, 1); // Davide> statistics
						pool->levelPoolLock.rLock();                 // Davide> START CRITICAL
				}
				int readP = shared_indeces[i];

				pool->getChunk(readP, chunk);

				readP = pool->writeP;

				pool->levelPoolLock.unlock(); // Davide> End of critical

				// ******************** END OF CRITICAL SECTION ******************* //

				addChunkToLearnts(chunk, i>getNodePTLevel() ? getNodePTLevel() : i, shared_indeces[i], pool->writeP);

				shared_indeces[i] = readP; // Davide> update of the shared index
				//cout << "EXITING WHILE" << endl;

				//fprintf(stderr,"Pull Learnts: Read Unlock\n");
				//				pthread_setcancelstate(old_cancel_state, NULL);

				if(!ok)
					break;
			} // Davide> End of if
		} // Davide> End of for
		//localStat.changeD(sharing_time_ID, cpuTime_t()-startSharingTime);
	}// End of if(max_size !=0)
}

bool SolverPT::addSharedLearnt(vec<Lit>& ps, unsigned int pt_level){
	// Check if clause is satisfied and remove false/duplicate literals:
	//sort(ps);

	Lit p; int i, j;
	for (i = j = 0; i < ps.size(); i++)
	{
		if (value(ps[i]) == l_True && level ( var(ps[i]) ) == 0 ) // we do not use satisfied clauses on any level
			return true;
		else if (value(ps[i]) == l_Undef || level(var(ps[i]))!=0 ) // its undef, or not assigned at  level 0 => keep literal
			ps[j++] = ps[i];
		else if (level(var(ps[i]))==0 &&  value(ps[i]) == l_False ) //removing propagated literals at decision level zero  from the clause
		{
			if( false && addClause_FalseRemoval == 0 ){
				// Davide> In order to maximize sharing, the safety of a clauses
				// is not corrupted
				if ( getLiteralPTLevel(ps[i]) > pt_level ) {
					ps[j++] = ps[i];
				}
			}
			else {
				const unsigned int& tmp = getLiteralPTLevel(ps[i]);
				pt_level = pt_level >= tmp ? pt_level : tmp; // Davide> Aggressive removal can prevent clauses to be shared higher in the PartitionTree
			}
		}
	}
	// there can be satisfied literals inside the clause!
	ps.shrink(i - j);

	if (ps.size() == 0){
		if( !disable_stats )
			localStat.changeI(n_import_shcl_unsatID, 1);
		lastLevel = pt_level; // TODO Tests
		return ok = false;
	}
	// its a unit, and we do not have it yet as unit
	else if (ps.size() == 1 ){
		int prevDecLevel = decisionLevel();
		if (level( var(ps[0]) ) > 0 || value(ps[0]) != l_True ) {
			lbdQueue.fastclear();
			progress_estimate = progressEstimate();
			cancelUntil(0); // its a unit, you have to go to level 0 in any case!
			//starts++;
			if( value(ps[0]) == l_False ){
				Debug::PRINTLN_NOTE("NOTE: tried to add unary false clause");
				if( !disable_stats ){
					localStat.changeI(n_import_shcl_unsatID, 1);
				}
				lastLevel = pt_level;
				return ok = false;
			} else if ( value(ps[0]) == l_Undef ){
				uncheckedEnqueue(ps[0], CRef_Undef, pt_level); // Davide> attach pt_level info
				level0UnitsIndex++; //need not to share this unit as it is already coming from a shared pool, thats why we are increasing the index of level0 units to be shared
				if(!disable_stats && prevDecLevel>0) localStat.changeI(n_tot_forced_restarts_ID,1);
				return ok = (propagate() == CRef_Undef); // FIXME Davide> Changed
			}
		} // else, we already have that unit clause!
	} else {
		assert( ps.size() > 1 && "unit and empty are handled above!" );
		// order literals s.t. satisfied are in front, then undefined ones, then falsified ones by level (descending)
		/*int s = 0;
      int u = 0;
      // push sat and undef forward!
      for( int i = 0; i < ps.size(); ++i ) {
	const Lit&l = ps[i];
	if( value(l) == l_True ) {
	  const Lit tmp = ps[i]; ps[i] = ps[s]; ps[s] = tmp; s++; 
	}
      }
      if( s < 2 ) {
	// moved undefined literals forward
	u = s;
	for( int i = u; i < ps.size(); ++i ) {
	  const Lit&l = ps[i];
	  if( value(l) == l_Undef ) {
	    const Lit tmp = ps[i]; ps[i] = ps[u]; ps[u] = tmp; u++; 
	  }
	}
        if( u < 2 ) {
	  if( level(var(ps[u])) < level(var(ps[u+1])) ) { const Lit tmp = ps[u]; ps[u] = ps[u+1]; ps[u+1] = tmp; }
	  for( int i = u+2; i < ps.size(); ++ i ) {
	    const Lit &l = ps[i];
	    const int lev = level(var(l));
	    if( lev > level(var(ps[u+1])) ) { // you have to move this literal forward!
	      if( lev >= level( var(ps[u]) ) ) {
		const Lit tmp = ps[i];
		ps[i] = ps[u+1];
		ps[u+1] = ps[u];
		ps[u] = tmp;
	      } else {
		const Lit tmp = ps[i];
		ps[i] = ps[u+1];
		ps[u+1] = tmp;
	      }
	    }
	  }
	}
      }*/
		CRef cr;
		cr = ca.alloc(ps, true);

		if(opt_sharing_var_bump && pt_level<getNodePTLevel())//bumping the var activity present in the shared clause if the shared clauses level is lower than the level of this solver
			for(int i=0; i<ps.size(); i++)
				varBumpActivity(var(ps[i]),var_inc/double(pt_level+1));

		// Davide> If the clause comes from one sh-pool, treat it as a learnt clause
		learnts.push(cr);
		//claBumpActivity(ca[cr]);  //ahmed> used as last option if there is tie between the lbd scores of 2 clauses
		ca[cr].setLBD(ps.size());  //ahmed> giving the lbd score equal to size of the clause..... will be updated by during propagation... if lucky before deletion ;-)
		ca[cr].setPTLevel(pt_level); // Davide> Setting the pt_level

		Clause& c = ca[cr];
		//c.setCanBeDel(false);
		c.setShared();
		c.initShCleanDelay(opt_shared_clean_delay);
		/*if( s == 0 && u == 1 ) {
	assert( value(c[0] ) == l_Undef && "unit literal has to be the very first literal of the received clause!" );
	assert(( addClause_FalseRemoval > 0 || decisionLevel() > 0) && "otherwise received clause has to be unit!" );
        uncheckedEnqueue(c[0], cr, pt_level);
      }

      assert( (value( c[0] ) == l_True || value(c[0]) == l_Undef || level(var(c[0])) >= level( var(c[1]) ) ) && "condition for incorporated learned clause" );
      assert( (value( c[1] ) == l_True || value(c[1]) == l_Undef || (c.size() == 2 || level(var(c[1])) >= level( var(c[2]) )) ) && "condition for incorporated learned clause" );
      assert( !(value(c[1]) == l_True) || value(c[0]) == l_True );
      assert( !(value(c[1]) == l_Undef) || (value(c[0]) == l_True ||value(c[0]) == l_Undef));
	*/	 
		attachClause(cr);
		localStat.changeI(n_acquired_clausesID, 1);
		//cons++; //ahmed> used by cleaning policy to count the number of clauses in learnts.... i dont know why glucose is using learnts.size()
	}

	return true;
}

void SolverPT::satRestartStrategy() {
	K = 0.8;
	lbdQueue.growTo(75);
	if(!diversification && randomization)
		random_var_freq=0.01*(double)curPTLevel;
	if(cleaning_delay==1){
		incReduceDB=200;
	}else if(cleaning_delay==2){
		incReduceDB=600;
	}
}

void SolverPT::unsatRestartStrategy() {
	K = 0.8;
	lbdQueue.growTo(50);
	if(cleaning_delay==1){
		incReduceDB=100;
	}else if(cleaning_delay==2){
		incReduceDB=900;
	}
}

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

void SolverPT::reduceDB() {
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
		//assert(c.getPTLevel()<learnts_indeces.size());
		if (c.lbd()>2 && c.size() > 2 && c.canBeDel() &&  !locked(c) && (i < limit) && c.getShCleanDelay()==0) {
			removeClause(learnts[i]);
			nbRemovedClauses++;
		}
		else {
			if(!c.canBeDel()) limit++; //we keep c, so we can delete an other clause
			c.setCanBeDel(true);       // At the next step, c can be delete
			learnts[j++] = learnts[i];
			if(c.isShared())
				c.decShCleanDelay();
			else if(c.lbd()<=LBD_lt)
				learnts_indeces[c.getPTLevel()].push_back(j-1);
		}
	}
	//push_learnts();
	learnts.shrink(i - j);
	checkGarbage();
}

// Davide> Davide's methods
void
SolverPT::addChunkToLearnts(vec<Lit>& chunk, unsigned int pt_level, int readP, int writeP){
	vec<Lit> clause;
	for( int i = 0; i < chunk.size(); ++i ){

		while(chunk[i] == lit_Undef) ++i;

		if(!ok) return;

		clause.clear();

		assert(i < chunk.size());
		assert(i == 0 || chunk[i-1] == lit_Undef );

		if(tnode->id() == toInt(chunk[i])){
			while( chunk[++i]!=lit_Undef );
			continue;
		}
		assert(i < chunk.size());

		assert(toInt(chunk[i]) != tnode->id());

		while(++i < chunk.size() && chunk[i] != lit_Undef){
			clause.push(chunk[i]);
		}
		assert(chunk[i] == lit_Undef);
		assert(clause.size() != 0);

		addClause(clause, pt_level, true);

	}
}
