/**************************************************************************************************

Solver_C.C

C-wrapper for Solver.C

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


#include "Solver.h"
#include "CoreConfig.h"

struct SolverLibData {
  CoreConfig* config;
  Solver* solver;
};

extern "C" {
#include "Solver_C.h"
}

extern "C" MiniSat_ptr MiniSat_Create()
{
  SolverLibData* data = new SolverLibData();
  data->config = new CoreConfig();
  data->solver = new Solver( *data->config );
  return (MiniSat_ptr)data;
}

extern "C" void MiniSat_Delete(MiniSat_ptr ms)
{
  delete (CoreConfig *)ms->config;
  delete (Solver *)ms->solver;
  delete (SolverLibData* )ms;
}

extern "C" int MiniSat_Nof_Variables(MiniSat_ptr ms)
{
  return ((Solver *)ms)->nVars();
}

extern "C" int MiniSat_Nof_Clauses(MiniSat_ptr ms)
{
  return ((Solver *)ms->solver)->nClauses();
}

/* variables are in the range 1...N */
extern "C" int MiniSat_New_Variable(MiniSat_ptr ms)
{
  /* Actually, minisat used variable range 0 .. N-1,
     so in all function below there is a convertion between
     input variable (1..N) and internal variables (0..N-1)
  */	
  return ((Solver *)ms->solver)->newVar() + 1;
}


/*
 * Here clauses are in dimacs form, variable indexing is 1...N
 */
extern "C" int MiniSat_Add_Clause(MiniSat_ptr ms,
                                  int *clause_lits, int num_lits)
{
  int i;
  vec<Lit> cl;
  for(i = 0; i < num_lits; i++) {
    const int lit = clause_lits[i];
    assert(abs(lit) > 0);
    assert(abs(lit) <= MiniSat_Nof_Variables((Solver*)ms->solver));
    int var = abs(lit) - 1;
    cl.push((lit > 0) ? Lit(var) : ~Lit(var));
  }
  ((Solver *)ms->solver)->addClause(cl);
  if(((Solver *)ms->solver)->okay())
    return 1;
  return 0;
}

extern "C" int MiniSat_Solve(MiniSat_ptr ms)
{
  bool ret = ((Solver *)ms->solver)->solve();
  if(ret)
    return 1;
  return 0;
}


/*
 * Here the assumption is in "dimacs form", variable indexing is 1...N
 */
extern "C" int MiniSat_Solve_Assume(MiniSat_ptr ms,
                                    int nof_assumed_lits,
                                    int *assumed_lits)
{
  int i;
  vec<Lit> cl;
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);

  solver.simplify();
  if(solver.okay() == false)
    return 0;

  assert(nof_assumed_lits >= 0);
  for(i = 0; i < nof_assumed_lits; i++) {
    const int lit = assumed_lits[i];
    assert(abs(lit) > 0);
    assert(abs(lit) <= MiniSat_Nof_Variables((Solver*)ms->solver));
    int var = abs(lit) - 1;
    cl.push((lit > 0) ? Lit(var) : ~Lit(var));
  }

  if (solver.solve(cl))
    return 1;
  return 0;
}

extern "C" int MiniSat_simplifyDB(MiniSat_ptr ms)
{
  ((Solver *)ms->solver)->simplify();
  if(((Solver *)ms->solver)->okay())
    return 1;
  return 0;
}

/*
 * Here variables are numbered 1...N
 */
extern "C" int MiniSat_Get_Value(MiniSat_ptr ms, int var_num)
{
  assert(var_num > 0);
  if(var_num > MiniSat_Nof_Variables(ms->solver))
    return -1;
  /* minisat assigns all variables. just check */
  assert(((Solver *)ms->solver)->model[var_num-1] != l_Undef); 
  
  if(((Solver *)ms->solver)->model[var_num-1] == l_True)
    return 1;
  return 0;
}


extern "C" int MiniSat_Get_Nof_Conflict_Lits(MiniSat_ptr ms)
{
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);

  return solver.conflict.size();
}

extern "C" void MiniSat_Get_Conflict_Lits(MiniSat_ptr ms, int* conflict_lits)
{
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);

  vec<Lit>& cf = solver.conflict;

  for (int i = 0; i < cf.size(); i++) {
    int v = var(~cf[i]);
    int s = sign(~cf[i]);
    assert(v != var_Undef);
    conflict_lits[i] = (s == 0) ? (v+1) : -(v+1);
  }
}

/** mode can be  polarity_true, polarity_false, polarity_user, polarity_rnd */
extern "C" void MiniSat_Set_Polarity_Mode(MiniSat_ptr ms, int mode)
{
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);  
  solver.polarity_mode = mode;
}

extern "C" int MiniSat_Get_Polarity_Mode(MiniSat_ptr ms)
{
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);  
  return solver.polarity_mode;
}

extern "C" void MiniSat_Set_Random_Seed(MiniSat_ptr ms, double seed)
{
  assert(((Solver*)0) != ((Solver*)ms->solver)); 
  Solver& solver = *((Solver*)ms->solver);
  solver.setRandomSeed(seed);
}

