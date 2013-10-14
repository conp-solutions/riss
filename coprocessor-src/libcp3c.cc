/**************************************************************************************[libcp3c.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Coprocessor.h"
using namespace Minisat;

/** struct that stores the necessary data for a preprocessor */
struct libcp3 {
  Minisat::vec<Minisat::Lit> currentClause;
  Minisat::CoreConfig* solverconfig;
  Minisat::Solver* solver;
  Coprocessor::CP3Config* cp3config;
  Coprocessor::Preprocessor* cp3;
};

extern "C" void* __attribute__ ((visibility ("default"))) 
cp3initPreprocessor() {
  libcp3* cp3 = new libcp3;
  cp3->solverconfig = new Minisat::CoreConfig();
  cp3->solverconfig->hk = false; // do not use laHack during preprocessing! (might already infere that the output lit is false -> unroll forever)
  cp3->cp3config = new Coprocessor::CP3Config();
  cp3->solver = new Minisat::Solver (*(cp3->solverconfig));
  cp3->cp3 = new Coprocessor::Preprocessor ( cp3->solver, *(cp3->cp3config) );
  return cp3;
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3destroyPreprocessor(void*& preprocessor)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  delete cp3->cp3;
  delete cp3->cp3config;
  delete cp3->solver;
  delete cp3->solverconfig;
  delete preprocessor;
  preprocessor = 0;
}

extern "C" void* __attribute__ ((visibility ("default"))) 
cp3preprocess(void* preprocessor) {
  libcp3* cp3 = (libcp3*) preprocessor;
  cp3->cp3->preprocess();
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3dumpFormula(void* preprocessor, std::vector< int >& formula)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  cp3->cp3->dumpFormula(formula);
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3extendModel(void* preprocessor, std::vector< uint8_t >& model)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  // dangerous line, since the size of the elements inside the vector might be different
  vec<lbool> cp3model;
  for( int i = 0 ; i < model.size(); ++ i ) cp3model.push( Minisat::toLbool(model[i]) );
  cp3->cp3->extendModel(cp3model);
  model.clear();
  for( int i = 0 ; i < cp3model.size(); ++ i ) model.push_back( toInt(cp3model[i]) );
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3freezeExtern(void* preprocessor, int variable)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  cp3->cp3->freezeExtern(variable);
}

extern "C" int  __attribute__ ((visibility ("default"))) 
cp3giveNewLit(void* preprocessor, int oldLit)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  cp3->cp3->giveNewLit(oldLit);
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3parseOptions(void* preprocessor, int& argc, char** argv, bool strict)
{
  libcp3* cp3 = (libcp3*) preprocessor;
  cp3->cp3config->parseOptions(argc,argv,strict);
}

extern "C" void  __attribute__ ((visibility ("default"))) 
cp3add(void* preprocessor, int lit)
{
  libcp3* cp3 = (libcp3*) preprocessor;
    if( lit != 0 ) cp3->currentClause.push( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) );
    else { // add the current clause, and clear the vector
      // reserve variables in the solver
      for( int i = 0 ; i < cp3->currentClause.size(); ++i ) {
	const Lit l2 = cp3->currentClause[i];
	const Var v = var(l2);
	while ( cp3->solver->nVars() <= v ) cp3->solver->newVar();
      }
      cp3->solver->addClause( cp3->currentClause );
      cp3->currentClause.clear();
    }
}

extern "C" int __attribute__ ((visibility ("default"))) 
cp3nVars(void* preprocessor) {
  libcp3* cp3 = (libcp3*) preprocessor;
  return cp3->solver->nVars();
}

extern "C" bool  __attribute__ ((visibility ("default"))) 
cp3ok(void* preprocessor) {
  libcp3* cp3 = (libcp3*) preprocessor;
  return cp3->solver->okay();
}

extern "C" bool  __attribute__ ((visibility ("default"))) 
cp3isUnsat(void* preprocessor, int lit) {
  libcp3* cp3 = (libcp3*) preprocessor;
  return cp3->solver->value( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) ) == l_False;
}
