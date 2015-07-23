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

#ifndef RISS_SOLVER_C_h
#define RISS_SOLVER_C_h

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

//=================================================================================================
#endif

