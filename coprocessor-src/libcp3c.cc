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

// #pragma GCC visibility push(hidden)
// #pragma GCC visibility push(default)
// #pragma GCC visibility pop // now we should have default!

//#pragma GCC visibility push(default)

extern "C" {
  
    void* 
  CPinit() {
    libcp3* cp3 = new libcp3;
    cp3->solverconfig = new Minisat::CoreConfig();
    cp3->solverconfig->hk = false; // do not use laHack during preprocessing! (might already infere that the output lit is false -> unroll forever)
    cp3->cp3config = new Coprocessor::CP3Config();
    cp3->solver = new Minisat::Solver (*(cp3->solverconfig));
    cp3->cp3 = new Coprocessor::Preprocessor ( cp3->solver, *(cp3->cp3config) );
    return cp3;
  }

  void  
  CPdestroy(void*& preprocessor)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    delete cp3->cp3;
    delete cp3->cp3config;
    delete cp3->solver;
    delete cp3->solverconfig;
    delete cp3;
    preprocessor = 0;
  }

  void 
  CPpreprocess(void* preprocessor) {
    libcp3* cp3 = (libcp3*) preprocessor;
    cp3->cp3->preprocess();
  }

  void  
  CPwriteFormula(void* preprocessor, std::vector< int >& formula)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    cp3->cp3->dumpFormula(formula);
  }

    void  
  CPpostprocessModel(void* preprocessor, std::vector< uint8_t >& model)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    // dangerous line, since the size of the elements inside the vector might be different
    vec<lbool> cp3model;
    for( int i = 0 ; i < model.size(); ++ i ) cp3model.push( Minisat::toLbool(model[i]) );
    cp3->cp3->extendModel(cp3model);
    model.clear();
    for( int i = 0 ; i < cp3model.size(); ++ i ) model.push_back( toInt(cp3model[i]) );
  }

    void  
  CPfreezeVariable(void* preprocessor, int variable)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    cp3->cp3->freezeExtern(variable);
  }

    int  
  CPgetReplaceLiteral(void* preprocessor, int oldLit)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    return cp3->cp3->giveNewLit(oldLit);
  }

    void  
  CPparseOptions(void* preprocessor, int& argc, char** argv, bool strict)
  {
    libcp3* cp3 = (libcp3*) preprocessor;
    cp3->cp3config->parseOptions(argc,argv,strict);
  }

   void 
  CPsetConfig (void* preprocessor, int configNr) {
    libcp3* cp3 = (libcp3*) preprocessor;
    if( configNr == 1 ) {
      cp3->cp3config->opt_enabled = true;
      cp3->cp3config->opt_up = true;
      cp3->cp3config->opt_subsimp = true;
      cp3->cp3config->opt_sub_allStrengthRes = 3;
      cp3->cp3config->opt_bva = true;
      cp3->cp3config->opt_bva_Alimit =120000;
      cp3->cp3config->opt_bve=true;
      cp3->cp3config->opt_bve_lits=1;
      cp3->cp3config->opt_bve_bc=false;
      cp3->cp3config->opt_cce=true;
      cp3->cp3config->opt_cceSteps=2000000;
      cp3->cp3config->opt_ccelevel=1;
      cp3->cp3config->opt_ccePercent=100;
      cp3->cp3config->opt_unhide=true;
      cp3->cp3config->opt_uhd_UHLE=false;
      cp3->cp3config->opt_uhd_Iters=5;
      cp3->cp3config->opt_dense=true;
    } else if ( configNr == 2 ) {
      cp3->cp3config->opt_enabled = true;
      cp3->cp3config->opt_bva = true;
    }
  }
  
    void  
  CPaddLit(void* preprocessor, int lit)
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


   float 
  CPversion(void* preprocessor)
  {
#ifdef TOOLVERSION
    return TOOLVERSION / 100; // the number in TOOLVERSION gives the current version times 100
#else
    return -1;
#endif
  }

  
    int 
  CPnVars(void* preprocessor) {
    libcp3* cp3 = (libcp3*) preprocessor;
    return cp3->solver->nVars();
  }

    bool  
  CPok(void* preprocessor) {
    libcp3* cp3 = (libcp3*) preprocessor;
    return cp3->solver->okay();
  }

    bool  
  CPlitFalsified(void* preprocessor, int lit) {
    libcp3* cp3 = (libcp3*) preprocessor;
    return cp3->solver->value( lit > 0 ? mkLit( lit-1, false ) : mkLit( -lit-1, true ) ) == l_False;
  }

}

// #pragma GCC visibility pop

//#pragma GCC visibility pop