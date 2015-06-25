/*****************************************************************************************[LinearUS.cc]
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

#include "Alg_LinearUS.h"

#ifdef PBLIB
#include "../minisat_to_pblib.h"
using namespace PBLib;
#endif
using namespace NSPACE;

/************************************************************************************************
 //
 // LinearUS Search Algorithm
 //
 ************************************************************************************************/

void LinearUS::lbSearch_none()
{
  nbInitialVariables = nVars();
  lbool res = l_True;

  initRelaxation();
  solver = rebuildSolver();
  vec<Lit> assumptions;

#ifndef PBLIB
  encoder.setIncremental(_INCREMENTAL_NONE_);
#endif
  for (;;)
  {
    res = searchSATSolver(solver, assumptions);
    if (res == l_True)
    {
      nbSatisfiable++;
      uint64_t newCost = computeCostModel(solver->model);
      saveModel(solver->model);
      printf("o %" PRIu64 "\n", newCost);
      
      ubCost = newCost;

      if (nbSatisfiable == 1)
      {
#ifdef PBLIB
	pb2cnf->encode(PBConstraint(MinisatToPBLib::createCardinalityWeightedLiterals(objFunction), PBLib::LEQ, 0), *pbEncodingFormula, *auxvars);
#else
        // Stability fix.
        // Fix the same modulo operation for all encodings.
        if (encoding == _CARD_MTOTALIZER_)
          encoder.setModulo(ceil(sqrt(ubCost + 1)));

        encoder.encodeCardinality(solver, objFunction, 0);
#endif
      }
      else
      {
        printAnswer(_OPTIMUM_);
        exit(_OPTIMUM_);
      }
    }

    if (res == l_False)
    {
      lbCost++;
      if (verbosity > 0) printf("c LB : %-12" PRIu64 "\n", lbCost);

      // Assumes that the first SAT call is done with the soft clauses disabled.
      if (nbSatisfiable == 0)
      {
        printAnswer(_UNSATISFIABLE_);
        exit(_UNSATISFIABLE_);
      }

      if (lbCost == ubCost)
      {
        if (nbSatisfiable > 0)
        {
          if (verbosity > 0) printf("c LB = UB\n");
          printAnswer(_OPTIMUM_);
          exit(_OPTIMUM_);
        }
        else
        {
          printAnswer(_UNSATISFIABLE_);
          exit(_UNSATISFIABLE_);
        }
      }

      delete solver;
      solver = rebuildSolver();
#ifdef PBLIB
      pb2cnf->encode(PBConstraint(MinisatToPBLib::createCardinalityWeightedLiterals(objFunction), PBLib::LEQ, lbCost), *pbEncodingFormula, *auxvars);
#else 
      encoder.encodeCardinality(solver, objFunction, lbCost);
#endif
    }
  }
}

void LinearUS::lbSearch_blocking()
{
#ifndef PBLIB
  assert(encoding == _CARD_TOTALIZER_);
#endif 

  nbInitialVariables = nVars();
  lbool res = l_True;

  initRelaxation();
  solver = rebuildSolver();
  vec<Lit> assumptions;
  vec<Lit> join; // dummy empty vector.

#ifdef PBLIB
  bool firstUnsatResult = true;
#else
  encoder.setIncremental(_INCREMENTAL_BLOCKING_);
#endif
  for (;;)
  {

    res = searchSATSolver(solver, assumptions);
    if (res == l_True)
    {
      nbSatisfiable++;
      uint64_t newCost = computeCostModel(solver->model);
      saveModel(solver->model);
      printf("o %" PRIu64 "\n", newCost);
      
      ubCost = newCost;

      if (nbSatisfiable == 1)
      {
        for (int i = 0; i < objFunction.size(); i++)
          assumptions.push(~objFunction[i]);
      }
      else
      {
        printAnswer(_OPTIMUM_);
        exit(_OPTIMUM_);
      }
    }

    if (res == l_False)
    {
      nbCores++;
      lbCost++;
      if (verbosity > 0) printf("c LB : %-12" PRIu64 "\n", lbCost);

      if (lbCost == ubCost)
      {
        if (nbSatisfiable > 0)
        {
          if (verbosity > 0) printf("c LB = UB\n");
          printAnswer(_OPTIMUM_);
          exit(_OPTIMUM_);
        }
        else
        {
          printAnswer(_UNSATISFIABLE_);
          exit(_UNSATISFIABLE_);
        }
      }

#ifdef PBLIB
      if (firstUnsatResult)
	assumptions.clear();
      
      Lit conditionalLit = MinisatToPBLib::integerToLit(auxvars->getVariable());
      if (assumptions.size() > 0)
	assumptions[assumptions.size()-1] = ~assumptions[assumptions.size()-1];
      
      assumptions.push(conditionalLit);
      
      
      pbEncodingFormula->addConditional(MinisatToPBLib::litToInteger(conditionalLit));
      pb2cnf->encode(PBConstraint(MinisatToPBLib::createCardinalityWeightedLiterals(objFunction), PBLib::LEQ, lbCost), *pbEncodingFormula, *auxvars);
      pbEncodingFormula->getConditionals().clear();

#else
      // When using the blocking strategy the update cardinality builds a new
      // encoding for the current 'lbCost' and disables previous encodings.
      encoder.buildCardinality(solver, objFunction, lbCost);
      // Previous encodings are disabled by setting the
      // blocking literal to true in the 'incrementalUpdate' method.
      // NOTE: 'assumptions' are modified in 'incrementalUpdate'.
      encoder.incUpdateCardinality(solver, join, objFunction, lbCost,
                                   assumptions);
#endif
    }
  }
}

void LinearUS::lbSearch_weakening()
{

#ifndef PBLIB
  assert(encoding == _CARD_TOTALIZER_);
#endif

  nbInitialVariables = nVars();
  lbool res = l_True;

  initRelaxation();
  solver = rebuildSolver();
  vec<Lit> assumptions;
  vec<Lit> join; // dummy empty vector.

#ifdef PBLIB
  IncPBConstraint * objPBConstraint = nullptr;
#else
  encoder.setIncremental(_INCREMENTAL_WEAKENING_);
#endif
  for (;;)
  {

    res = searchSATSolver(solver, assumptions);
    if (res == l_True)
    {
      nbSatisfiable++;
      uint64_t newCost = computeCostModel(solver->model);
      saveModel(solver->model);
      printf("o %" PRIu64 "\n", newCost);
      
      

      ubCost = newCost;

      // Stability fix.
      // Fix the same modulo operation for all encodings.
      if (nbSatisfiable == 1)
      {
        for (int i = 0; i < objFunction.size(); i++)
          assumptions.push(~objFunction[i]);
      }
      else
      {
        assert(lbCost == ubCost);
        printAnswer(_OPTIMUM_);
        exit(_OPTIMUM_);
      }
    }

    if (res == l_False)
    {
      nbCores++;
      lbCost++;
      if (verbosity > 0) printf("c LB : %-12" PRIu64 "\n", lbCost);

      if (nbSatisfiable == 0)
      {
        printAnswer(_UNSATISFIABLE_);
        exit(_UNSATISFIABLE_);
      }

      if (lbCost == ubCost)
      {
        if (nbSatisfiable > 0)
        {
          if (verbosity > 0) printf("c LB = UB\n");
          printAnswer(_OPTIMUM_);
          exit(_OPTIMUM_);
        }
        else
        {
          printAnswer(_UNSATISFIABLE_);
          exit(_UNSATISFIABLE_);
        }
      }

#ifdef PBLIB
      if (objPBConstraint == nullptr)
      {
	objPBConstraint = new IncPBConstraint(MinisatToPBLib::createCardinalityWeightedLiterals(objFunction), PBLib::LEQ, ubCost - 1);
	pb2cnf->encodeIncInital(*objPBConstraint, *pbEncodingFormula, *auxvars);
	assumptions.clear();
      }
      
      Lit conditionalLit = MinisatToPBLib::integerToLit(auxvars->getVariable());
      if (assumptions.size() > 0)
	assumptions[assumptions.size()-1] = ~assumptions[assumptions.size()-1];
      
      assumptions.push(conditionalLit);
      
      
      pbEncodingFormula->addConditional(MinisatToPBLib::litToInteger(conditionalLit));
      objPBConstraint->encodeNewLeq(lbCost, *pbEncodingFormula, *auxvars);
      pbEncodingFormula->getConditionals().clear();
#else
      // The cardinality encoding is only build once.
      if (!encoder.hasCardEncoding())
        encoder.buildCardinality(solver, objFunction, ubCost);

      // At each iteration the right-hand side is updated using assumptions.
      // NOTE: 'assumptions' are modified in 'incrementalUpdate'.
      encoder.incUpdateCardinality(solver, join, objFunction, lbCost,
                                   assumptions);
#endif
    }
  }
}

void LinearUS::lbSearch_iterative()
{

#ifdef PBLIB
  printf("c lbSearch_iterative is not supported by pblib switching to lbSearch_weakening\n");
  lbSearch_weakening();
  return;
#else
  assert(encoding == _CARD_TOTALIZER_);
#endif

  nbInitialVariables = nVars();
  lbool res = l_True;

  initRelaxation();
  solver = rebuildSolver();
  vec<Lit> assumptions;
  vec<Lit> join; // dummy empty vector.

#ifndef PBLIB
  encoder.setIncremental(_INCREMENTAL_ITERATIVE_);
#endif
  for (;;)
  {

    res = searchSATSolver(solver, assumptions);
    if (res == l_True)
    {
      nbSatisfiable++;
      uint64_t newCost = computeCostModel(solver->model);
      saveModel(solver->model);
      printf("o %" PRIu64 "\n", newCost);
      
      ubCost = newCost;

      if (nbSatisfiable == 1)
      {
        for (int i = 0; i < objFunction.size(); i++)
          assumptions.push(~objFunction[i]);
      }
      else
      {
        assert(lbCost == ubCost);
        printAnswer(_OPTIMUM_);
        exit(_OPTIMUM_);
      }
    }

    if (res == l_False)
    {
      nbCores++;
      lbCost++;
      if (verbosity > 0) printf("c LB : %-12" PRIu64 "\n", lbCost);

      if (nbSatisfiable == 0)
      {
        printAnswer(_UNSATISFIABLE_);
        exit(_UNSATISFIABLE_);
      }

      if (lbCost == ubCost)
      {
        if (nbSatisfiable > 0)
        {
          if (verbosity > 0) printf("c LB = UB\n");
          printAnswer(_OPTIMUM_);
          exit(_OPTIMUM_);
        }
        else
        {
          printAnswer(_UNSATISFIABLE_);
          exit(_UNSATISFIABLE_);
        }
      }
#ifdef PBLIB
	//TODO add code here
  #warning does not compile yet
#else

      if (!encoder.hasCardEncoding())
        encoder.buildCardinality(solver, objFunction, lbCost);

      // The right-hand side is constrained using assumptions.
      // NOTE: 'assumptions' are modified in 'incrementalUpdate'.
      // When using iterative encoding the clauses that allow the encoding to
      // count up to 'k' are incrementally added.
      encoder.incUpdateCardinality(solver, join, objFunction, lbCost,
                                   assumptions);
#endif
    }
  }
}

// Public search method
void LinearUS::search()
{

  if (problemType == _WEIGHTED_)
  {
    //TODO add weighted MaxSAT support with PBLib
    printf("Error: Currently LinearUS does not support weighted MaxSAT "
           "instances.\n");
    printf("s UNKNOWN\n");
    exit(_ERROR_);
  }

  printConfiguration();

  switch (incremental_strategy)
  {
  case _INCREMENTAL_NONE_:
    lbSearch_none();
    break;
  case _INCREMENTAL_BLOCKING_:
#ifndef PBLIB
    if (encoder.getCardEncoding() != _CARD_TOTALIZER_)
    {
      printf("Error: Currently incremental blocking in LinearUS only supports "
             "the Totalizer encoding.\n");
      printf("s UNKNOWN\n");
      exit(_ERROR_);
    }
#endif
    lbSearch_blocking();
    break;

  case _INCREMENTAL_WEAKENING_:
#ifndef PBLIB
    if (encoder.getCardEncoding() != _CARD_TOTALIZER_)
    {
      printf("Error: Currently incremental weakening in LinearUS only supports "
             "the Totalizer encoding.\n");
      printf("s UNKNOWN\n");
      exit(_ERROR_);
    }
#endif
    lbSearch_weakening();
    break;

  case _INCREMENTAL_ITERATIVE_:
#ifndef PBLIB
    if (encoder.getCardEncoding() != _CARD_TOTALIZER_)
    {
      printf("Error: Currently iterative encoding in LinearUS only supports "
             "the Totalizer encoding.\n");
      printf("s UNKNOWN\n");
      exit(_ERROR_);
    }
#endif
    lbSearch_iterative();
    break;

  default:
    printf("Error: Invalid incremental strategy.\n");
    printf("s UNKNOWN\n");
    exit(_ERROR_);
  }
}

/************************************************************************************************
 //
 // Rebuild MaxSAT solver
 //
 ************************************************************************************************/

/*_________________________________________________________________________________________________
  |
  |  rebuildSolver : [void]  ->  [Solver *]
  |
  |  Description:
  |
  |    Rebuilds a SAT solver with the current MaxSAT formula.
  |
  |________________________________________________________________________________________________@*/
Solver *LinearUS::rebuildSolver()
{

  Solver *S = newSATSolver();

  for (int i = 0; i < nVars(); i++)
    newSATVariable(S);

  for (int i = 0; i < nHard(); i++)
    S->addClause(hardClauses[i].clause);

  vec<Lit> clause;
  for (int i = 0; i < nSoft(); i++)
  {
    clause.clear();
    softClauses[i].clause.copyTo(clause);
    for (int j = 0; j < softClauses[i].relaxationVars.size(); j++)
      clause.push(softClauses[i].relaxationVars[j]);

    S->addClause(clause);
  }
#ifdef PBLIB
  if (pbEncodingFormula != nullptr) delete pbEncodingFormula;
  if (auxvars != nullptr) delete auxvars;
  
  auxvars = new AuxVarManager(S->nVars() + 1);
  pbEncodingFormula = new SATSolverClauseDatabase(pbconfig, S);
#endif

  return S;
}

/************************************************************************************************
 //
 // Other protected methods
 //
 ************************************************************************************************/

/*_________________________________________________________________________________________________
  |
  |  initRelaxation : [void] ->  [void]
  |
  |  Description:
  |
  |    Initializes the relaxation variables by adding a fresh variable to the
  |    'relaxationVars' of each soft clause.
  |
  |  Post-conditions:
  |    * 'objFunction' contains all relaxation variables that were added to soft
  |      clauses.
  |
  |________________________________________________________________________________________________@*/
void LinearUS::initRelaxation()
{
  for (int i = 0; i < nbSoft; i++)
  {
    Lit l = newLiteral();
    softClauses[i].relaxationVars.push(l);
    softClauses[i].assumptionVar = l;
    objFunction.push(l);
  }
}
