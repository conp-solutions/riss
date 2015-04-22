/* 
 * File:   vsidsSplitting.h
 * Author: ahmed
 *
 * Created on April 22, 2013, 1:53 PM
 */

#ifndef VSIDSSPLITTING_H
#define	VSIDSSPLITTING_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"
#include "utils/LockCollection.h"
#include "core/Solver.h"
#include "pcasso-src/SplitterSolver.h"

namespace Pcasso {
    class VSIDSSplitting : public SplitterSolver {
      
    CoreConfig& coreConfig;
      
      // more global data structure
      vec<Lit>    learnt_clause; 
      vec<CRef> otfssClauses; 
      uint64_t extraInfo;
      
    public:
        VSIDSSplitting(CoreConfig& config);
        ~VSIDSSplitting();
        void dummy(){
        }
        lbool vsidsScatteringSplit(vec<vec<vec<Lit>* >* > **splits, vec<vec<Lit>* > **valid, double to);
        lbool scatterSolve(void* data);
        lbool scatterSeach(int nof_conflicts, void* data);
        double cpuTime_t() const ; // CPU time used by this thread
        struct vsidsScatterData {
            vec<vec<Lit> *>     scatters;
            vec<vec<vec<Lit> *> *> *out_scatters;
            bool     startscatter;
            bool     isscattering;
            int      scatterfactor;
            bool     time_cond;		// use time to limit interval between two scatters
            double   sc_min_time;
            double   sc_bw_time;
            double   starttime;
            bool     cnfl_cond;		// use conflicts to limit interval between two scatters
            uint64_t sc_min_cnfl;	// minimum number of conflicts before any scatter is produced
            uint64_t sc_bw_cnfl;	// conflicts before next scatter is produced
            double   prevscatter;	// time of prevous scatter
            double   tout; 
            // Scattering-related parameters
            uint64_t cnfl_bw;		// conflicts to be reached before next scatter output
            uint64_t time_bw;		// number conflicts between two scatters
        };
        double              max_learnts;
        double              learntsize_adjust_confl;
        int                 learntsize_adjust_cnt;
        
        int       restart_first;      // The initial restart limit.                                                                (default 100)
        double    restart_inc;        // The factor with which the restart limit is multiplied in each restart.                    (default 1.5)
        double    learntsize_factor;  // The intitial limit for learnt clauses is a factor of the original clauses.                (default 1 / 3)
        double    learntsize_inc;     // The limit for learnt clauses is multiplied with this factor each restart.                 (default 1.1)

        int       learntsize_adjust_start_confl;
        double    learntsize_adjust_inc;

    };
}

#endif	/* VSIDSSPLITTING_H */

