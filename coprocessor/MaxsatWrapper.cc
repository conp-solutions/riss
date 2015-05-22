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
, currentWeight(1)
, sumWeights(0)

, specVars(-1)
, specCls (-1)
, fullVariables(-1)
, debugLevel(0)
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
  S->reserveVars( specVars ); 			// use reserve function of Riss
  while( S->nVars() < specVars ) S->newVar();	// are there variables missing, add them
}

void Mprocessor::setProblemType(int type)
{
  problemType = type;
  if( debugLevel > 2 ) cerr << "c set problem type to " << type << endl;
}

int Mprocessor::getProblemType()
{
  return problemType;
}

Var Mprocessor::newVar(bool polarity, bool dvar, char type)
{
  Var v = S->newVar( polarity, dvar, type );
  fullVariables =  v > fullVariables ? v : fullVariables;  // keep track of highest variable
  if( debugLevel > 2 ) cerr << "c set highest var to " << v << endl;
}

int Mprocessor::nVars() const
{
  return S->nVars();
}

void Mprocessor::setHardWeight(int weight)
{
  if( debugLevel > 2 ) cerr << "c set hard weight to " << weight << endl;
  hardWeight = weight;
}

void Mprocessor::setCurrentWeight(int weight)
{
  if( debugLevel > 2 ) cerr << "c set current weight to " << weight << endl;
  currentWeight = weight;
}

int Mprocessor::getCurrentWeight()
{
  return currentWeight;
}

void Mprocessor::updateSumWeights(int weight)
{
  if( debugLevel > 2 ) cerr << "c update sum weight " << weight << endl;
  sumWeights = weight;
}

void Mprocessor::addHardClause(vec< Lit >& lits)
{
  if( debugLevel > 1 ) cerr << "c added hard clause to MSW: " << lits << endl;
  S->addClause_( lits );
}

void Mprocessor::addSoftClause(int weight, vec< Lit >& lits)
{
  if( debugLevel > 1 ) cerr << "c added soft clause to MSW: " << weight << " ; "  << lits << endl;
  Var relaxVariables = S->newVar(false,false,'r'); // add a relax variable
  fullVariables = fullVariables > relaxVariables + 1 ? fullVariables : relaxVariables + 1; // keep track of highest variable
  
  if( debugLevel > 2 ) cerr << "c crate relax variable " << relaxVariables + 1 << endl;
  
  literalWeights.push(0);literalWeights.push(0); 		// weights for the two new literals!
  literalWeights[ toInt( mkLit(relaxVariables) ) ] += weight;	// assign the weight of the current clause to the relaxation variable!
  
  if( debugLevel > 2 ) cerr << "c new weight for variable " << relaxVariables + 1 << " : " << literalWeights[ toInt( mkLit(relaxVariables) ) ] << endl;
  
  if( lits.size() == 0 ) { // found empty weighted clause
    // to ensure that this clause is falsified, add the compementary unit clause!
    if( debugLevel > 2 ) cerr << "c add as HARD clause to PREPROCESSOR: " << mkLit( relaxVariables )<< endl;
    S->addClause( mkLit( relaxVariables ) );
  }
  lits.push( ~ mkLit( relaxVariables ) ); // add a negative relax variable to the clause
  S->freezeVariable( relaxVariables, true ); // set this variable as frozen!
  if( debugLevel > 2 ) cerr << "c add as HARD clause to PREPROCESSOR: " << lits << endl;
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
