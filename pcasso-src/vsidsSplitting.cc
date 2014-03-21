#include "vsidsSplitting.h"
#include "mtl/Vec.h"
#include "core/SolverTypes.h"
#include "pthread.h"
#include <time.h>

using namespace Pcasso;

static const char* _cat = "VSIDSSCATTERING";
static IntOption     opt_scatter_fact  (_cat, "sc-fact",             "The number of derived instances produced in a split", 8, IntRange(0, INT_MAX));
static BoolOption    opt_scatter_time  (_cat, "sc-timeLimit",        "Limit the time during two scatters", false);
static BoolOption    opt_scatter_con   (_cat, "sc-conLimit",         "Limit the number of conflicts during two scatters", true);
static IntOption     opt_scatter_conBw (_cat, "sc-betweenConflicts", "The number of derived instances produced in a split", 8096, IntRange(0, INT_MAX));
static IntOption     opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
static DoubleOption  opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));

VSIDSSplitting::VSIDSSplitting():
  restart_first    (opt_restart_first)
, restart_inc      (opt_restart_inc)


// Parameters (the rest):
//
, learntsize_factor((double)1/(double)3), learntsize_inc(1.1)

// Parameters (experimental):
//
, learntsize_adjust_start_confl (100)
, learntsize_adjust_inc         (1.5)
{
    
}

VSIDSSplitting::~VSIDSSplitting(){
    
}

/** produce a split that is based on scattering, variables that have the highest VSIDS-score are picked first
 */
lbool VSIDSSplitting::vsidsScatteringSplit(vec<vec<vec<Lit>* >* > **splits, vec<vec<Lit>* > **valid, double to) {
    vsidsScatterData data;
    
    // set all the data
    assert((opt_scatter_con || opt_scatter_time) && "one of the two limits has to be set");
    data.cnfl_cond = opt_scatter_con;
    data.time_cond = opt_scatter_time;
    data.startscatter = true;
    data.isscattering = false;
    data.scatterfactor = opt_scatter_fact;

    // divide time equally so that the time per produced scatter is distirbuted equally
    data.sc_min_time = to / (double)(data.scatterfactor+1);
    data.sc_bw_time =  to / (double)(data.scatterfactor+1);
    
    data.sc_min_cnfl = (uint64_t)opt_scatter_conBw;
    data.sc_bw_cnfl  = (uint64_t)opt_scatter_conBw;
    data.cnfl_bw = (uint64_t)opt_scatter_conBw;
    
    data.starttime = cpuTime_t();
    data.tout = -1;

    // create a new scatters vector and point to it
    *splits = new vec<vec<vec<Lit>* >* >;
    data.out_scatters = *splits;
    
    // run the search to get VSIDS scores and print the scatters
    lbool res = scatterSolve(&data); //solve_();
    
    fprintf( stderr, "return in splitting from solve with %s\n", res == l_False ? "false" : ( res == l_Undef ? "undef" : "true") );

		//fprintf ( stderr, "stop splitting with %d sub formulas\n", (*splits)->size());
		// if res is true, than there is a solution
	  if (res == l_True) {
			  // fprintf( stderr, "the result is true and we return that\n");
	      return l_True;
	  // if the result has been false, it has to be unsat
    } else if (res == l_False){
    		// fprintf( stderr, "the result is false and we return that\n");
    		// and there cannot be any children	
    		assert((*splits)->size() == 0 && "if the problem is unsatisfiable there should not be any child nodes");
        return l_False;
    } else if ( res == l_Undef) {
		    // fprintf( stderr, "the result is undef and we return that\n");
		    // there should be at least one split, in case no solution has been found (no way to timeout before the first split)
		    
		    // fprintf( stderr, "\n\n\nEither there has been a problem during splitting, or an external interrupt\n\n\n");
		    
		    assert( ((*splits)->size() > 0 || asynch_interrupt) && "splitting has to return with a result" );

        return l_Undef;
    }
   
    assert(false && "this code should never be reached" );
    return l_False;
}

/// implementation of the luby series
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

/** note: this method is very similar to Minisats usual ::solve_() method. 
 * The only difference is that a different search-method is called and that the pointer to data is passed
 */
lbool VSIDSSplitting::scatterSolve(void* data)
{
   model.clear();
    conflict.clear();
    if (!ok) return l_False;

    solves++;

    max_learnts               = nClauses() * learntsize_factor;
    learntsize_adjust_confl   = learntsize_adjust_start_confl;
    learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
    lbool   status            = l_Undef;

//    if (verbosity >= 1){
//        fprintf( stderr,"============================[ Search Statistics ]==============================\n");
//        fprintf( stderr,"| Conflicts |          ORIGINAL         |          LEARNT          | Progress |\n");
//        fprintf( stderr,"|           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |          |\n");
//        fprintf( stderr,"===============================================================================\n");
//    }

    //struct timeval tmp_t;
    //gettimeofday(&tmp_t, NULL);
    //double start_t = tmp_t.tv_sec + tmp_t.tv_usec/1000000;
    //double tcput = cpuTime_t();
    // Search:
    int curr_restarts = 0;
    while (status == l_Undef){
        double rest_base = luby(restart_inc, curr_restarts);
        status = scatterSeach(rest_base * restart_first,data);
        if (!withinBudget()) break;
        curr_restarts++;
    }

//    if (verbosity >= 1)
//        fprintf( stderr,"===============================================================================\n");


    if (status == l_True){
		    // fprintf( stderr, "found model (in solve method)\n");
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
    }else if (status == l_False && conflict.size() == 0)
        ok = false;

    cancelUntil(0);

    //gettimeofday(&tmp_t, NULL);
    //double rt = tmp_t.tv_sec + tmp_t.tv_usec/1000000 -  start_t;
    //printf("I was running for wall time %f and per thread cpu time %f\n",
      //  rt, cpuTime_t() - tcput);
    
    return status;
}

/** note: this method is very similar to Minisats usual ::search() method. 
* The only difference is that a different search-method is called and that the pointer to data is passed
*/
lbool VSIDSSplitting::scatterSeach(int nof_conflicts, void* data)
{
    vsidsScatterData& d = *( (vsidsScatterData*)data );
    
    
    assert(ok);
    int         backtrack_level;
    int         conflictC = 0;
    starts++;

    // Change this to scatter mode
    bool timed_out = false;
    while (d.scatters.size() < d.scatterfactor - 1)
    {
        CRef confl = propagate();
	
	// fprintf( stderr, "c [VSIDS SCATTERING] conflicts %d, %d\n", (int)conflicts, d.sc_bw_cnfl );
	
        if (confl != CRef_Undef){

            // check for timing out
            if (d.tout != -1 && cpuTime_t() > d.tout) { timed_out = true; break; }
            // check whether the instance should be canceled
            pthread_testcancel();

            // CONFLICT
            conflicts++; conflictC++;
            if (decisionLevel() == 0) {
                if (d.scatters.size() == 0) return l_False;
                else { interrupt(); return l_Undef; }
            }

            learnt_clause.clear();otfssClauses.clear(); extraInfo=0;
            unsigned lbd=0;
            int ret = analyze(confl, learnt_clause, backtrack_level,lbd, otfssClauses, extraInfo); // Davide> scatt !! my invention
	    
	    assert( ret == 0 && "can handle only usually learnt clauses" );
	    if( ret != 0 ) _exit(1); // abort, if learning is set up wrong

            cancelUntil(backtrack_level);

            if (learnt_clause.size() == 1){
                uncheckedEnqueue(learnt_clause[0]);
            }else{
                CRef cr = ca.alloc(learnt_clause, true);
                learnts.push(cr);
                attachClause(cr);
                claBumpActivity(ca[cr]);
                ca[cr].setLBD(lbd);
                uncheckedEnqueue(learnt_clause[0], cr);
            }

            varDecayActivity();
            claDecayActivity();

            if (--learntsize_adjust_cnt == 0){
                learntsize_adjust_confl *= learntsize_adjust_inc;
                learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
                max_learnts             *= learntsize_inc;
            }

        } else {
            // NO CONFLICT
            if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()){
                // Reached bound on number of conflicts:
                progress_estimate = progressEstimate();
                cancelUntil(0);
                return l_Undef;
	    }

	    // minimum time / confl before starting the scatter
	    bool scatterCond = ((d.time_cond) && (cpuTime_t() - d.starttime >= d.sc_min_time)) ||  ((d.cnfl_cond) && (conflicts >= d.sc_min_cnfl));
	    // minimum time / confl between each scatter
	    bool scatterCond_bw = ((d.time_cond) && (cpuTime_t() - d.prevscatter >= d.sc_bw_time)) || ((d.cnfl_cond) && (conflicts >= d.sc_bw_cnfl));

            if (d.startscatter) {
                if ((!d.isscattering && scatterCond) ||
                    (d.isscattering && scatterCond_bw)) {
                    // First round of scattering starts now
                    cancelUntil(assumptions.size());
                    d.startscatter = false;
                    d.isscattering = true;
                    if (d.time_cond)
                        d.prevscatter = cpuTime_t();
                }
            }

            // Simplify the set of problem clauses:

            if (decisionLevel() == 0 && !simplify()) {

                if (d.scatters.size() == 0){
                		// fprintf( stderr, "fail after simplification\n");
                    return l_False;
                } else {
                    interrupt();
                    return l_Undef;
                }
            }

            if (learnts.size()-nAssigns() >= max_learnts){
                // Reduce the set of learnt clauses:
                reduceDB();
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
                    if (d.scatters.size() == 0){
                    		// fprintf( stderr, "fail after assumption failure\n");
                        return l_False;
                    } else {
                        interrupt();
                        return l_Undef;
                    }
                }else{
                    next = p;
                    break;
                }
            }

	    //if( (int)conflicts % 128 == 0 )
	      
            
            // Are we yet in the scatter level?
            bool scatterLevel = false;
	    if( !d.startscatter && d.isscattering ) { // only necessary to calculate in this case, because otherwise the below if won't trigger anyways
		int dl;
		if (!d.isscattering) scatterLevel = false;
		else {
		  // Current scattered instance number i = scatters.size()+1
		  float r = 1/(float)(d.scatterfactor-d.scatters.size());
		  for (int i = 0;;i++) {
		      // 2 << i == 2^(i+1)
		      if ((2 << (i-1) <= d.scatterfactor - d.scatters.size()) &&
			  (2 << i >= d.scatterfactor - d.scatters.size())) {
			  // r-1/(2^i) < 0 and we want absolute
			  dl = -(r-1/(float)(2<<(i-1))) > r-1/(float)(2<<i) ? i+1 : i;
			  break;
		      }
		  }
		  scatterLevel = (dl == decisionLevel());
		}
	    }
            
            if (!d.startscatter && d.isscattering && scatterLevel)
	    {
                fprintf(stderr, "Scatter %d, conflicts %d, this limit %d: interval %d :", d.scatters.size(), (int)conflicts, d.sc_bw_cnfl, d.cnfl_bw);
                vec<Lit>* declits = new vec<Lit>;
                for(int i = assumptions.size(); i < trail_lim.size(); i++) {
                    Lit decL = trail[trail_lim[i]];
                    Lit l = mkLit(var(decL), sign(decL));
                    declits->push(l);
                }
                d.scatters.push(declits);
                cancelUntil(assumptions.size());
                
		/*
		* The scatters consist of the scattering assumptions and the scattering
		* constraints.  We could possibly add also some learned clauses.  The
		* output should be a vec<vec<Lit>* >, that is added to the data structure d.
		*/
		
		    int n_sc = d.scatters.size();
		    vec<vec<Lit> *> *tmp_cnf = new vec<vec<Lit> *>;

		    {
			// Push n unit clauses to tmp_cnf
			for (int i = 0; i < d.scatters[n_sc - 1]->size(); i++) {
			    // The new clause
			    vec<Lit>* tmp_cls = new vec<Lit>;
			    printf("%s%d ", sign((*d.scatters[n_sc-1])[i]) ? "-" : "",
					  var((*d.scatters[n_sc-1])[i]) + 1);
			    // Push the literal to the clause
			    (*tmp_cls).push((*d.scatters[n_sc-1])[i]);
			    // Push the clause to the cnf
			    (*tmp_cnf).push(tmp_cls);
			}
			printf("\n");
		    }

		    // The scattering constraints
		    for (int i = 0; i < d.scatters.size() - 1; i++) {
			vec<Lit>* tmp_cls = new vec<Lit>;
			for (int j = 0; j < d.scatters[i]->size(); j++) {
			    Lit tmp_lit = mkLit(var(~((*d.scatters[i])[j])), 
						sign(~((*d.scatters[i])[j])));
			    (*tmp_cls).push(tmp_lit);
			}
			(*tmp_cnf).push(tmp_cls);
			//delete tmp_cls;
		    }
		    d.out_scatters->push(tmp_cnf);
		    // free memory
		    //delete tmp_cnf;
		
		
		
		// Insert the negation of the literals in declits as a clause to the
		// clause database. Never call from decision level > 0.
		
		bool excludeAssumptions = false;
		{
		  vec<Lit>* cl = new vec<Lit>;
		  for (int i = 0; i < declits->size(); i++)
		  cl->push(~ (*(declits))[i]);
		  addClause_(*cl);
		  delete cl;
		  reduceDB();
		  if (ok)  excludeAssumptions = true;
		}
		
                // Include the negation to the instance
                if (! excludeAssumptions) {
                    printf("Unsat after adding scatter clause\n");
                    interrupt();
                    return l_Undef;
                }
                d.startscatter = true;
                d.isscattering = true;
                if (d.cnfl_cond)
                    d.sc_bw_cnfl = conflicts + d.cnfl_bw;
                else
                    d.prevscatter = cpuTime_t();
            }
            else {

                if (next == lit_Undef){
                    // New variable decision:
                    decisions++;
                    next = pickBranchLit();

                    if (next == lit_Undef){
                        // Model found:
                        return l_True;
                    }
                }

                // Increase decision level and enqueue 'next'
                newDecisionLevel();
                uncheckedEnqueue(next);
            }
        }
    } // Last scatter
    
    if (d.tout == -1) {
    		//fprintf( stderr, "finished without a timeout set\n");
        vec<Lit>* declits = new vec<Lit>;
        d.scatters.push(declits);
        /*
	* The scatters consist of the scattering assumptions and the scattering
	* constraints.  We could possibly add also some learned clauses.  The
	* output should be a vec<vec<Lit>* >.
	*/
	{
	    int n_sc = d.scatters.size();
	    vec<vec<Lit> *> *tmp_cnf = new vec<vec<Lit> *>();

	    // The scattering constraints
	    for (int i = 0; i < d.scatters.size() - 1; i++) {
		vec<Lit>* tmp_cls = new vec<Lit>;
		for (int j = 0; j < d.scatters[i]->size(); j++) {
		    Lit tmp_lit = mkLit(var(~((*d.scatters[i])[j])), 
					sign(~((*d.scatters[i])[j])));
		    (*tmp_cls).push(tmp_lit);
		}
		(*tmp_cnf).push(tmp_cls);
		// delete tmp_cls;
	    }
	    d.out_scatters->push(tmp_cnf);
	    // delete tmp_cnf;
	}
	
        interrupt();
        return l_Undef;
    }
    else if (timed_out) {
		    //fprintf( stderr, "timed out ... (during work)\n");
        interrupt();
        return l_Undef;
    }
    else {
    		//fprintf( stderr, "report unsat (worker)\n");
        return l_False;
    }
}

/// Get CPU time used by this thread
double VSIDSSplitting::cpuTime_t() const  {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        perror("clock_gettime()");
        ts.tv_sec = -1;
        ts.tv_nsec = -1;
    }
    return (double) ts.tv_sec + ts.tv_nsec/1000000000.0;
}