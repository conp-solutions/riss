/*******************************************************************************************[sls.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/sls.h"

using namespace Coprocessor;

Sls::Sls(CoprocessorData& _data, ClauseAllocator& _ca, ThreadController& _controller)
: 
Technique(_ca, _controller)
, data(_data)
, ca ( _ca )
, solveTime(0)
{

}

Sls::~Sls()
{

}

void Sls::init(bool reinit)
{
	unsatisfiedClauses = 0;
	satClauses.resize( 2 *(varCnt) );
	unsatClauses.resize( 2 *(varCnt) );
	assignment.resize( varCnt );
	
	if( !reinit ){
		unsatisfiedLiteralsContains.resize( 2*varCnt );
		unsatisfiedLiteralsContains.reset();
	} else {
		// clear heap
		unsatisfiedLiteralsContains.create( varCnt * 2 );
		unsatisfiedLiterals.clear();
		for(int literal = 0; literal < 2 * varCnt; ++ literal )
		{
			satClauses[ literal ].clear();
			unsatClauses[ literal ].clear();
		}
	}

	unsatisfiedLiteralsContains.nextStep();

	createRandomAssignment();
	
	// fill sat and unsat clauses
	const vec<CRef>& formula = data.getClauses();
	
	for( unsigned i = 0 ; i < formula.size(); ++i )
	{
		Clause& clause = ca[ formula[i] ];
		// check for sat
		unsigned j = 0;
		for(  ; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j]) ){
				watchSatClause( formula[i], clause[j] );
				break;
			}
		}
		if( j == clause.size () ){
			watchUnsatClause( formula[i] );
			unsatisfiedClauses++;
		}
	}
}

void Sls::createRandomAssignment()
{
	// init random assignment
	for( Var v = 1 ; v <= varCnt; v++ )
	{
		setPolarity(assignment, v, ( (rand() & 1) == 1 ? 1 : -1) );
	}
}

void Sls::watchSatClause( const Minisat::CRef clause, const Lit satisfyingLiteral )
{
	satClauses[ toInt( satisfyingLiteral) ].push_back( clause );
}

void Sls::watchUnsatClause( const CRef clause )
{
	const Clause& cl = ca[clause];
	for( unsigned i = 0 ; i < cl.size(); ++i ){
		const unsigned index = toInt(cl[i]);
		const Lit literal = cl[i];
		unsatClauses[ index ].push_back( clause );
 		if( ! unsatisfiedLiteralsContains.isCurrentStep(index) ) { 
		  unsatisfiedLiteralsContains.setCurrentStep(index);
		  unsatisfiedLiterals.push_back( literal);
		}
	}
}

void Sls::unwatchUnsatClause( const CRef clause )
{
	const Clause& cl = ca[clause];
	for( unsigned i = 0 ; i < cl.size(); ++i ){
		vector<CRef>& list = unsatClauses[ toInt(cl[i]) ];
		const Lit literal = cl[i];
		// check for the clause in the list and remove it, by pushing the other clause into this list
		for( unsigned j = 0 ; j < list.size(); ++j ){
			if( list[j] == clause ){
				list[j] = list[ list.size() - 1];
				list.pop_back();
				break;
			}
		}
		
		// update heap
// 		if( ( --unsatClauseCounter[ toInt(literal) ]) == 0 )
// 		{
// 			if( unsatisfiedLiterals.contains( literal.data() ) ) unsatisfiedLiterals.remove_item( literal.data() );
// 		}
	}
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
	if( unsatisfiedLiterals.size() > 0 ){
		
		const int& limitL = unsatisfiedLiterals.size();
		Lit tmp = unsatisfiedLiterals[ rand() % limitL ];
		// get a random clause:
		const vector< CRef >& list = unsatClauses[ toInt(tmp) ];
		const int& limitC = list.size();
		const Clause& cl = ca[ list[ rand() %  limitC ] ];
		// do walksat on that clause
		
		assert( cl.size() > 0 && "cannot handle empty clauses!" );
		
		tmpSet.push_back( cl[0] );
		int smallestBreak = getFlipUnsat( var( cl[0] ) );
		
		for( int i = 1; i < cl.size(); ++ i ) {
		  int thisBreak =   getFlipUnsat( var( cl[i] ) );
		  
		  if( thisBreak > smallestBreak )  continue;
		  
		  if ( thisBreak == smallestBreak ) {
		    tmpSet.push_back( cl[i] );
		  } else {
		    tmpSet.clear();
		    tmpSet.push_back( cl[i] );
		  }
		  
		}
		
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
		
		flipMe = var(tmp);
		assert( flipMe != 0 && "sls pick heuristic has to be able to select at least one literal!" );
	}
	return flipMe;
}

void Sls::updateFlip(const Lit becameSat){
	// check whether new clauses became unsatisfiedClauses
	vector<CRef>& list = satClauses[ toInt(~becameSat) ];
	// cerr << "c process unsat clauses" << endl;
	for( unsigned i = 0 ; i < list.size(); ++i ){
		const Clause& clause = ca[ list[i] ];
		unsigned j = 0 ;
		for(; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j] ) )
			{
				// watch the satisfied clause
				watchSatClause( list[i], clause[j]);
				break;
			}
		}
		// handle the unsatisfied clause
		if( j == clause.size() ){
			watchUnsatClause( list[i] );
			++unsatisfiedClauses;
		}
	}
	
	// remove all clauses from this list (they have been either pushed to another sat-list or are in the unsat-lists)
	list.clear();

	// cerr << "c process sat clauses" << endl;
	// put newly satisfied clauses into sat clause structure
	while( unsatClauses[ toInt(becameSat) ].size() > 0 )
	{
		watchSatClause( unsatClauses[ toInt(becameSat) ][0], becameSat );
		unwatchUnsatClause( unsatClauses[ toInt(becameSat) ][0] );
		--unsatisfiedClauses;
	}
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
	init( true );
	restarts ++;
	cerr << "c start with " << unsatisfiedClauses << " unsatisfied clauses" << endl;
	// search
	solution = search(stepLimit);
	
	solveTime = cpuTime() - solveTime;
	
	// return satisfying assignment or 0, if no solution has been found so far
	return solution;
}

void Sls::printStatistics( ostream& stream )
{
	cerr << "c unsatisfied clauses:  " << unsatisfiedClauses << endl;
	cerr << "c unsatisfied literals: " << unsatisfiedLiterals.size() << endl;
	cerr << "c flips: " << flips << endl;
	cerr << "c fps: " << flips * 1000000 / solveTime << endl;
}


int Sls::getFlipDiff( const Var v ) const {
	const Lit currentSat = mkLit(v, assignment[v] == -1 );
	unsigned int newSat = unsatClauses[ toInt(~currentSat) ].size();
	int newUnsat = 0;
	const vector<CRef>& list = satClauses[ toInt(currentSat) ];
	for( unsigned i = 0 ; i < list.size(); ++i ){
		const Clause& clause = ca[list[i]];
		unsigned j = 0 ;
		for(; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j] ) ) break;
		}
		// handle the unsatisfied clause
		if( j == clause.size() ){
			newUnsat++;
		}
	}
	
	return newSat - newUnsat;
}

unsigned int Sls::getFlipSat( const Var v ) const {
	const Lit currentSat = mkLit(v, assignment[v] == 0 );
	return unsatClauses[ toInt(~currentSat) ].size();
}

int Sls::getFlipUnsat( const Var v ) const {
	const Lit currentSat = mkLit(v, assignment[v] == -1 );
	int newUnsat = 0;
	const vector<CRef>& list = satClauses[ toInt(currentSat) ];
	for( unsigned i = 0 ; i < list.size(); ++i ){
		const Clause& clause = ca[list[i]];
		unsigned j = 0 ;
		for(; j < clause.size(); ++j ){
			if( isSat( assignment, clause[j] ) ) break;
		}
		// handle the unsatisfied clause
		if( j == clause.size() ){
			newUnsat++;
		}
	}
	return newUnsat;
}


