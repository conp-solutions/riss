/********************************************************************************[MaxsatWrapper.cc]
Copyright (c) 2015, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor/MaxsatWrapper.h"

using namespace Coprocessor;

Mprocessor::Mprocessor(const char* configname)
: cpconfig(configname)
, S(0)
, preprocessor(0)
, problemType(0)
, hardWeight(0) 
, currentWeight(0)
, sumWeights(0)

, specVars(-1)
, specCls (-1)
, fullVariables(-1)
{
  S = new Solver(0, configname);
  S->setPreprocessor(&cpconfig); // tell solver about preprocessor
}

Mprocessor::~Mprocessor()
{
  if( preprocessor != 0 ) delete preprocessor;
  preprocessor = 0;
  if( S != 0 ) delete S;
  S = 0;
}


void Mprocessor::setSpecs(int specifiedVars, int specifiedCls)
{
  specVars = specifiedVars;
  specCls  = specifiedCls;
  literalWeights.growTo( specVars * 2 );
}

void Mprocessor::setProblemType(int type)
{
  problemType = type;
}

int Mprocessor::getProblemType()
{
  return problemType;
}

Var Mprocessor::newVar(bool polarity, bool dvar, char type)
{
  Var v = S->newVar( polarity, dvar, type );
  fullVariables =  v > fullVariables ? v : fullVariables;  // keep track of highest variable
}

int Mprocessor::nVars() const
{
  return S->nVars();
}

void Mprocessor::setHardWeight(int weight)
{
  hardWeight = weight;
}

void Mprocessor::setCurrentWeight(int weight)
{
  currentWeight = weight;
}

int Mprocessor::getCurrentWeight()
{
  return currentWeight;
}

void Mprocessor::updateSumWeights(int weight)
{
  sumWeights = weight;
}

void Mprocessor::addHardClause(vec< Lit >& lits)
{
  S->addClause_( lits );
}

void Mprocessor::addSoftClause(int weight, vec< Lit >& lits)
{
  Var relaxVariables = S->newVar(false,false,'r'); // add a relax variable
  
  literalWeights.push(0);literalWeights.push(0); // weights for the two new literals!
  literalWeights[ toInt( mkLit(relaxVariables) ) ] += weight;	// assign the weight of the current clause to the relaxation variable!
  
  if( lits.size() == 0 ) { // found empty weighted clause
    // to ensure that this clause is falsified, add the compementary unit clause!
    S->addClause( mkLit( relaxVariables ) );
  }
  lits.push( ~ mkLit( relaxVariables ) ); // add a negative relax variable to the clause
  S->freezeVariable( relaxVariables, true ); // set this variable as frozen!
  S->addClause(lits);
}

bool Mprocessor::okay()
{
  return S->okay();
}

void Mprocessor::simplify()
{
    // if we are densing, the the units should be rewritten and kept, and, the soft clauses need to be rewritten!
  if( cpconfig.opt_dense ) {
    cpconfig.opt_dense_keep_assigned = true; // keep the units on the trail!
    cpconfig.opt_dense_store_forward = true; // store a forward mapping
  }
    
  preprocessor = new Preprocessor( S , cpconfig);
  preprocessor->preprocess();
}
