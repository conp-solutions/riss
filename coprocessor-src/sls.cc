/*******************************************************************************************[sls.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/sls.h"

using namespace Coprocessor;

static const char* _cat = "SLS";
static BoolOption opt_debug (_cat, "sls-debug", "Print SLS debug output", false);

Sls::Sls(CoprocessorData& _data, ClauseAllocator& _ca, ThreadController& _controller)
: 
Technique(_ca, _controller)
, data(_data)
, ca ( _ca )
, solveTime(0)
, unsatisfiedClauses(0)
, flips ( 0 )
{

}

Sls::~Sls()
{

}

void Sls::init()
{
	unsatisfiedClauses = 0;
	assignment.resize( varCnt );
	breakCount.resize( varCnt );
	
	watchSatClauses.resize( varCnt * 2 );
	
	createRandomAssignment();
	
	if( opt_debug ) {
	cerr << "SLS randomly created assignment: ";
	  for( Var v = 0 ; v < data.nVars(); ++ v ) cerr << " " << ((int)assignment[v]) * (v+1);
	  cerr << endl;
	}
	
	// fill sat and unsat clauses
	const vec<CRef>& formula = data.getClauses();
	
	for( unsigned i = 0 ; i < formula.size(); ++i )
	{
		const Clause& clause = ca[ formula[i] ];
		if( clause.can_be_deleted() ) continue; // do not work with redundant clauses!
		Lit satLit = lit_Undef;
		// check for sat
		unsigned j = 0;
		for(  ; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j]) ){
				satLit = clause[j];
				watchSatClause( formula[i], clause[j] );
				break;
			}
		}
		if( j == clause.size () ){
		  if( opt_debug) {
		    cerr << "clause " << ca[ formula[i] ] << " is considered unsatisfied" << endl;
		    for( int k = 0 ; k < clause.size(); ++ k ) {
		      cerr << " " << clause[k] << " isUnsat: " << isUnsat( assignment, clause[k] ) << "; "; 
		    }
		    cerr << endl;
		  }
		  unsatisfiedClauses++;
		  unsatClauses.push_back( formula[i] );
		} else {
		  assert( satLit != lit_Undef && "if clause is satisfied, there needs to be a satisfied literal" );
		  // if there is only this literal, which satisfies the clause, increase the break count!
		  ++j; // next literal!
		  for(  ; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j]) ) break;
		  }
		  if( j == clause.size () ) {
		     breakCount [ var( satLit ) ] ++;
		  }
		}
	}
}

void Sls::createRandomAssignment()
{
	// init random assignment
	for( Var v = 0 ; v < varCnt; v++ )
	{
		setPolarity(assignment, v, ( (rand() & 1) == 1 ? 1 : -1) );
	}
}

void Sls::watchSatClause( const Minisat::CRef clause, const Lit satisfyingLiteral )
{
	watchSatClauses[ toInt( satisfyingLiteral) ].push_back( clause );
}

bool Sls::search(uint32_t flipSteps)
{
	const vec<CRef>& formula = *formulaAdress;
	// check all clauses whether they are satisfied, if not, flip a random variable
	bool satisfied = false;
	
	walkPropability = 0;
	
	// printHeap();
	while( unsatisfiedClauses > 0 )
	{
		// kill from threads
// 		pthread_testcancel();
		// do a restart
// 		if( flips > restart ){
// 			return false;
// 		}
		// cerr << "c flips " << flips << endl;
		// cerr << "c assignment: "; printAssignment(); cerr << endl; 
		// debug();
		flips++;
		
		if( flips > flipSteps ) return false;

		// get the flip variable
		const Var flipMe = heuristic();
		

		// flip the variable and propagate the flip
		if( flipMe != 0 ){
			/*
			cerr << "c flip " << flipMe << endl;
			cerr << "c newSAT:   " << this->getFlipSat(   flipMe ) << endl;
			cerr << "c newUNSAT: " << this->getFlipUnsat( flipMe ) << endl;
			cerr << "c diff: " << this->getFlipDiff( flipMe ) << endl;
			cerr << "c unsatisfied clauses: " << unsatisfiedClauses << endl;
			*/
			invertPolarity(assignment,flipMe);
			const Lit becameSat = mkLit( flipMe, assignment[ flipMe ] == -1);
			
			updateFlip( becameSat );
			
		} else {
			// assert( unsatisfiedClauses == 0 );
			break;
		}

	}
	// printHeap();
	return true;
}

Var Sls::heuristic(){
	Var flipMe = 0;
	
	if( false &&  opt_debug ) {
	cerr << endl << "=======================" << endl;
	cerr << "SLS model before pick: ";
	  for( Var v = 0 ; v < data.nVars(); ++ v ) cerr << " " << ((int)assignment[v]) * (v+1);
	  cerr << endl;
	
	cerr << "sat clauses: " << endl;
	for( Var v = 0 ; v < data.nVars(); ++ v ) {
	  cerr << "Lit " <<  mkLit(v,false) << ":";
	  for( int i = 0 ; i < watchSatClauses[toInt(mkLit(v,false))].size(); ++ i ) cerr << ca[ watchSatClauses[toInt(mkLit(v,false))][i] ] << ", ";
	  cerr << endl;
	  cerr << "Lit " <<  mkLit(v,true) << ":";
	  for( int i = 0 ; i < watchSatClauses[toInt(mkLit(v, true))].size(); ++ i ) cerr << ca[ watchSatClauses[toInt(mkLit(v, true))][i] ] << ", ";
	  cerr << endl;
	  cerr << "BreakCount[ " << v+1 << " ]: " << breakCount[v] << endl;
	}
	cerr << "unsat clauses[ " << unsatisfiedClauses << " ]: " << endl;
	for( int i = 0 ; i < unsatClauses.size(); ++ i) {
	  cerr << ca[unsatClauses[i]] << endl;
	}
	}
	
	if( unsatisfiedClauses > 0 ){
		// get a random clause:
		const int& limitC = unsatClauses.size();
		const Clause& cl = ca[ unsatClauses[ rand() %  limitC ] ];
		// do walksat on that clause
		
	
		assert( cl.size() > 0 && "cannot handle empty clauses!" );
		
		tmpSet.clear();
		tmpSet.push_back( cl[0] );
		int smallestBreak = breakCount[ var( cl[0] ) ];
		
		for( int i = 1; i < cl.size(); ++ i ) {
		  int thisBreak =   breakCount[ var( cl[i] ) ];
		  
		  if( thisBreak > smallestBreak )  continue;
		  
		  if ( thisBreak == smallestBreak ) {
		    tmpSet.push_back( cl[i] );
		  } else {
		    tmpSet.clear();
		    tmpSet.push_back( cl[i] );
		  }
		  
		}
		
		if( opt_debug ) cerr << "smallest break: " << smallestBreak << " candidates: " << tmpSet.size() << endl;
		
		Lit tmp = lit_Undef;
		// if literal without break, select smallest such literal!
		if( smallestBreak == 0 ) {
		  tmp = tmpSet[ rand() % tmpSet.size() ]; 
		} else {
		  
		  // with 20 percent, select a random variable!
		  if( rand() % 10000 < 2000 ) {
		    tmp = cl[ rand() % cl.size() ]; 
		  } else {
		    // select one of the variables with the smallest breack count!
		    tmp = tmpSet[ rand() % tmpSet.size() ];   
		  }
		}
		
		assert( tmp != lit_Undef && "sls pick heuristic has to be able to select at least one literal!" );
		flipMe = var(tmp);
		if( opt_debug ) cerr << "heuristic picks " << flipMe+1 << endl;
	}
	return flipMe;
}

void Sls::updateFlip(const Lit becameSat){
  
	if( opt_debug ) cerr << "update flip with " << becameSat << endl;
  
	// check all unsat clauses whether the have been satisfied now
	for( unsigned i = 0 ; i < unsatClauses.size(); ++i )
	{
		const Clause& clause = ca[ unsatClauses[i] ];
		if(false &&  opt_debug ) cerr << "check whether " << clause << " contains " << becameSat << endl;
		for( int j = 0 ; j < clause.size(); ++ j ) {
		  if( clause[j] == becameSat ) {
		    // this clause is satisfied now by exactly this literal!
		    watchSatClause(unsatClauses[i], becameSat);
		    breakCount[ var(becameSat) ] ++;
		    unsatisfiedClauses --;
		    if( opt_debug ) cerr << "c satisfied clause " << ca[ unsatClauses[i] ] << " with literal " << becameSat << endl;
		    // remove this clause from the unsatisfied clauses vector!
		    unsatClauses[i] = unsatClauses[ unsatClauses.size() - 1 ];
		    unsatClauses.pop_back();
		    --i;
		    break;
		  }
		}
	}
  
  
	
	// check whether new clauses became unsatisfiedClauses
	vector<CRef>& list = watchSatClauses[ toInt(~becameSat) ];
	breakCount[var(becameSat)] -= list.size();
	for( unsigned i = 0 ; i < list.size(); ++i )
	{
		const Clause& clause = ca[ list[i] ];
		Lit satLit = lit_Undef;
		// check for sat
		unsigned j = 0;
		for(  ; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j]) ){
				satLit = clause[j];
				watchSatClause( list[i], clause[j] );
				break;
			}
		}
		if( j == clause.size () ){
		  if( opt_debug) {
		    cerr << "clause " << ca[ list[i] ] << " is considered unsatisfied" << endl;
		    for( int k = 0 ; k < clause.size(); ++ k ) {
		      cerr << " " << clause[k] << " isUnsat: " << isUnsat( assignment, clause[k] ) << "; "; 
		    }
		    cerr << endl;
		  }
		  unsatisfiedClauses++;
		  unsatClauses.push_back( list[i] );
		} else {
		  assert( satLit != lit_Undef && "if clause is satisfied, there needs to be a satisfied literal" );
		  // if there is only this literal, which satisfies the clause, increase the break count!
		  ++j; // next literal!
		  for(  ; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j]) ) break;
		  }
		  if( j == clause.size () ) {
		     breakCount [ var( satLit ) ] ++;
		  }
		}
	}
	list.clear(); // clear watch list of this literal!
	
	
	
	// assert( false && "implement this method!" );
}

bool Sls::solve( const vec<CRef>& formula, uint32_t stepLimit )
{
        solveTime = cpuTime() - solveTime;
	// setup main variables
	assignment.resize(data.nVars());
	this->varCnt = data.nVars();
	this->formulaAdress = (vec<CRef>*)&formula;
	
	// for algorithm
	bool solution = false;
	unsigned restarts = 0;

	// init search
	init();
	
	if( opt_debug ) cerr << "c start with " << unsatisfiedClauses << " unsatisfied clauses" << endl;
	// search
	solution = search(stepLimit);
	
	solveTime = cpuTime() - solveTime;
	
	if( opt_debug ) {
	  if( solution) {
	    bool foundUnsatisfiedClause = false;;
	    for( int i = 0 ; i < formula.size()  ; ++ i ) {
	      bool thisClauseIsSatisfied = false;
	      const Clause& cl = ca[ formula[i] ];
	      if( cl.can_be_deleted() ) continue;
	      for( int j = 0 ; j < cl.size(); ++ j ) {
		if ( isSat( assignment, cl[j] ) ) { thisClauseIsSatisfied = true; break; }
	      }
	      if( ! thisClauseIsSatisfied ) { 
		foundUnsatisfiedClause = true;
		if( opt_debug ) cerr << "the clause " << ca[formula[i]] << " is not satisfied by the SLS model" << endl;
		break;
	      }
	    }
	    if( !foundUnsatisfiedClause ) {
	      cerr << "SLS model was successfully checked" << endl; 
	    }
	  }
	}
	
	if( opt_debug ) {
	  cerr << "SLS model: ";
	  for( Var v = 0 ; v < data.nVars(); ++ v ) cerr << " " << ((int)assignment[v]) * (v+1);
	  cerr << endl;
	}
	
	
	
	// return satisfying assignment or 0, if no solution has been found so far
	return solution;
}

void Sls::printStatistics( ostream& stream )
{
  stream << "c [STAT] SLS " << solveTime << " s, " << flips << " flips, " << flips / solveTime << " fps" << endl;
  stream << "c [STAT] SLS-info " <<  unsatisfiedClauses << " unsatClauses" << endl;
}


