/**************************************************************************************************

Solver_C.h

C-wrapper for Solver.h

  This file is part of NuSMV version 2.
  Copyright (C) 2007 by FBK-irst.
  Author: Roberto Cavada <cavada@fbk.eu>

  NuSMV version 2 is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  NuSMV version 2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA.

  For more information on NuSMV see <http://nusmv.fbk.eu>
  or email to <nusmv-users@fbk.eu>.
  Please report bugs to <nusmv-users@fbk.eu>.

  To contact the NuSMV development board, email to <nusmv@fbk.eu>. ]

**************************************************************************************************/

#ifndef RISS_NUSVM_SOLVER_C_h
#define RISS_NUSVM_SOLVER_C_h

//=================================================================================================
// Solver -- the main class:

#define MiniSat_ptr void *

enum { polarity_true = 0, polarity_false = 1,
       polarity_user = 2, polarity_rnd = 3
     };

MiniSat_ptr MiniSat_Create();
void MiniSat_Delete(MiniSat_ptr);
int MiniSat_Nof_Variables(MiniSat_ptr);
int MiniSat_Nof_Clauses(MiniSat_ptr);
int MiniSat_New_Variable(MiniSat_ptr);
int MiniSat_Add_Clause(MiniSat_ptr, int *clause_lits, int num_lits);
int MiniSat_Solve(MiniSat_ptr);
int MiniSat_Solve_Assume(MiniSat_ptr, int nof_assumed_lits, int *assumed_lits);
int MiniSat_simplifyDB(MiniSat_ptr);
int MiniSat_Get_Value(MiniSat_ptr, int var_num);
int MiniSat_Get_Nof_Conflict_Lits(MiniSat_ptr ms);
void MiniSat_Get_Conflict_Lits(MiniSat_ptr ms, int* conflict_lits);

void MiniSat_Set_Polarity_Mode(MiniSat_ptr ms, int mode);
int MiniSat_Get_Polarity_Mode(MiniSat_ptr ms);
void MiniSat_Set_Random_Seed(MiniSat_ptr ms, double seed);

// NuSMV: PREF MOD
void MiniSat_Set_Preferred_Variable(MiniSat_ptr, int);
void MiniSat_Clear_Preferred_Variables(MiniSat_ptr);
// NuSMV: PREF MOD END

//=================================================================================================
//
// actual implementation as wrapper of C-interface of riss
//
// ================================================================================================

#include "riss/librissc.h"  // include the actual library interface of riss
#include "assert.h"
#include "math.h"
#include <cstdlib>


inline MiniSat_ptr MiniSat_Create()
{
    // return the usual Riss Binary
    return (MiniSat_ptr) riss_init();
}

inline void MiniSat_Delete(MiniSat_ptr ms)
{
    // destroy the riss solver object
    riss_destroy(&ms);
}

inline int MiniSat_Nof_Variables(MiniSat_ptr ms)
{
    // return the number of variables of the solver
    return riss_variables(ms);
}

inline int MiniSat_Nof_Clauses(MiniSat_ptr ms)
{
    return riss_clauses(ms);
}

/* variables are in the range 1...N */
inline int MiniSat_New_Variable(MiniSat_ptr ms)
{
    /* Actually, minisat used variable range 0 .. N-1,
       so in all function below there is a convertion between
       input variable (1..N) and internal variables (0..N-1)
    */
    return riss_new_variable(ms) ;
}


/*
 * Here clauses are in dimacs form, variable indexing is 1...N
 */
inline int MiniSat_Add_Clause(MiniSat_ptr ms,
                              int *clause_lits, int num_lits)
{
    int i, ret = 0;

    for (i = 0; i < num_lits; i++) {
        const int lit = clause_lits[i];
        assert(lit != 0);
        assert(abs(lit) <= MiniSat_Nof_Variables(ms)); // NuSVM wants to make sure this check itself - riss_add would create variable data structures itself
        ret = riss_add(ms, lit) || ret;     // collect all return codes disjunctively
    }
    ret = riss_add(ms, 0) || ret;    // add a terminating 0
    return ret;
}

inline int MiniSat_Solve(MiniSat_ptr ms)
{
    // return 1, if the current formula is satisfiable
    assert(riss_assumptions(ms) == 0);
    return (10 == riss_sat(ms)) ? 1 : 0;
}


/*
 * Here the assumption is in "dimacs form", variable indexing is 1...N
 */
inline int MiniSat_Solve_Assume(MiniSat_ptr ms,
                                int nof_assumed_lits,
                                int *assumed_lits)
{
    int i;
    assert(ms != 0);

    if (0 == riss_simplify(ms)) { return 0; }     // unit propagation failed

    assert(nof_assumed_lits >= 0);
    assert(riss_assumptions(ms) == 0 && "there should not be old assumption variables left");

    for (i = 0; i < nof_assumed_lits; i++) {
        const int lit = assumed_lits[i];
        assert(abs(lit) > 0);
        assert(abs(lit) <= MiniSat_Nof_Variables(ms));
        riss_assume(ms, lit);
    }

    return (10 == riss_sat(ms)) ? 1 : 0;    // return 1, if the SAT call was successful
}

inline int MiniSat_simplifyDB(MiniSat_ptr ms)
{
    return 0 == riss_simplify(ms) ? 0 : 1;   // return 0, if simplification failes
}

/*
 * Here variables are numbered 1...N
 */
inline int MiniSat_Get_Value(MiniSat_ptr ms, int var_num)
{
    assert(var_num > 0);
    if (var_num > MiniSat_Nof_Variables(ms)) {
        return -1;
    }
    /* minisat assigns all variables. just check */
    assert(riss_deref(ms, var_num) != 0);

    return riss_deref(ms, var_num) == 1 ? 1 : 0;
}


inline int MiniSat_Get_Nof_Conflict_Lits(MiniSat_ptr ms)
{
    assert(0 != ms);
    return riss_conflict_size(ms);
}

inline void MiniSat_Get_Conflict_Lits(MiniSat_ptr ms, int* conflict_lits)
{
    assert(0 != ms);

    const int conflictSize = riss_conflict_size(ms);
    for (int i = 0; i < conflictSize; i++) {
        conflict_lits[i] = riss_conflict_lit(ms, i);
    }
}

/** not implemented, Riss uses phase_saving heuristic as default */
inline void MiniSat_Set_Polarity_Mode(MiniSat_ptr ms, int mode)
{
}

/** not implemented, Riss uses phase_saving heuristic as default */
inline int MiniSat_Get_Polarity_Mode(MiniSat_ptr ms)
{
    return polarity_false;
}

inline void MiniSat_Set_Random_Seed(MiniSat_ptr ms, double seed)
{
    assert(0 != ms);
    assert(seed >= 0 && seed <= 1);
    riss_set_randomseed(ms, seed);
}


// NuSMV: PREF MOD
/* variables are in the range 1...N */
inline void MiniSat_Set_Preferred_Variable(MiniSat_ptr ms, int x)
{
    /* Actually, minisat used variable range 0 .. N-1,
       so in all function below there is a convertion between
       input variable (1..N) and internal variables (0..N-1)
    */
    riss_add_prefered_decision(ms, x);
}

inline void MiniSat_Clear_Preferred_Variables(MiniSat_ptr ms)
{
    riss_clear_prefered_decisions(ms);
}
// NuSMV: PREF MOD END



#endif

