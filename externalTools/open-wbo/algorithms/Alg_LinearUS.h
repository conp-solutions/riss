/*****************************************************************************************[Alg_LinearUS.h]
Open-WBO -- Copyright (c) 2013-2015, Ruben Martins, Vasco Manquinho, Ines Lynce

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
************************************************************************************************/

#ifndef Alg_LinearUS_h
#define Alg_LinearUS_h

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../MaxSAT.h"
#ifdef PBLIB
  #include "../pblib/lib/pb2cnf.h"
  #include "../pblib/SATSolverClauseDatabase.h"
#else
#include "../Encoder.h"
#endif
#include <map>
#include <set>
#include <algorithm>

namespace NSPACE
{

//=================================================================================================
class LinearUS : public MaxSAT
{

public:
  LinearUS(int verb = _VERBOSITY_MINIMAL_, int incremental = _INCREMENTAL_NONE_,
           int enc = _CARD_TOTALIZER_)
  {
    solver = NULL;
    verbosity = verb;
    incremental_strategy = incremental;
#ifdef PBLIB
    pbconfig = std::make_shared<PBLib::PBConfigClass>();
    
    pb2cnf = new PBLib::PB2CNF(pbconfig);
    #warning TODO init pbconfig here
#else
    encoding = enc;
    encoder.setCardEncoding(enc);
#endif
  }
  ~LinearUS()
  {
    if (solver != NULL) delete solver;
#ifdef PBLIB    
  if (pbEncodingFormula != nullptr) delete pbEncodingFormula;
  if (auxvars != nullptr) delete auxvars;
  if (pb2cnf != nullptr) delete pb2cnf;
    
#endif
  }

  void search(); // LinearUS search.

  // Print solver configuration.
  void printConfiguration()
  {
    printf("c ==========================================[ Solver Settings "
           "]============================================\n");
    printf("c |                                                                "
           "                                       |\n");
    printf("c |  Algorithm: %23s                                             "
           "                      |\n",
           "LinearUS");
#ifdef PBLIB
    #warning TODO print pb config here
#else
    print_Card_configuration(encoder.getCardEncoding());
#endif
    print_Incremental_configuration(incremental_strategy);
    printf("c |                                                                "
           "                                       |\n");
  }

protected:
  // Rebuild MaxSAT solver
  //
  Solver *rebuildSolver(); // Rebuild MaxSAT solver.

  // LinearUS search algorithms.
  //
  void lbSearch_none();
  void lbSearch_blocking();
  void lbSearch_weakening();
  void lbSearch_iterative();

  // Other
  void initRelaxation(); // Relaxes soft clauses.

  Solver *solver;  // SAT Solver used as a black box.

  int incremental_strategy;
#ifdef PBLIB
  PBLib::SATSolverClauseDatabase * pbEncodingFormula = nullptr;
  PBLib::AuxVarManager * auxvars = nullptr;
  PBLib::PB2CNF * pb2cnf = nullptr;
  PBLib::PBConfig pbconfig;
#else  
  Encoder encoder; // Interface for the encoder of constraints to CNF.
  int encoding;
#endif

  // Literals to be used in the constraint that excludes models.
  vec<Lit> objFunction;
  vec<int> coeffs; // Coefficients of the literals that are used in the
                   // constraint that excludes models.
};
}

#endif
