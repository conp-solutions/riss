/*******************************************************************************************[sls.h]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/



#ifndef SLS_H
#define SLS_H

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Technique.h"

namespace Coprocessor {

/** implements a simple walksat solver that can be executed on the formula */
class Sls : public Technique
{

public:
  Sls(Coprocessor::CoprocessorData& _data, ClauseAllocator& _ca, ThreadController& _controller);
  ~Sls();

  /** run sls algorithm on formula 
  * @param model vector that can contain a model for the formula afterwards
  * @return true, if a model has been found
  */
  bool solve(  const vec< Minisat::CRef >& formula, uint32_t stepLimit  );

  /** if search succeeded, return polarity for variable v (1 = positive, -1 = negative) */
  char getModelPolarity( const Var v ) { return assignment[v]; }

  /** This method should be used to print the statistics of the technique that inherits from this class
  */
  void printStatistics( ostream& stream );

private:
  
  CoprocessorData& data;	// reference to coprocessor data object
    ClauseAllocator& ca;	// reference to clause allocator
    double solveTime;		// number of seconds for solving
  
	// work data
	vector< vector<CRef> > satClauses;   // clauses that are satisfied by a single literal (watch this literal)
	vector< vector<CRef> > unsatClauses; // clauses that are unsatisfied. all the literals of the clause are watched
	
	MarkArray unsatisfiedLiteralsContains;     // store which literals are in the unsatisfied literals vector
	vector< Lit > unsatisfiedLiterals;	    // heap that contains the literals, that have unsatisfied clauses
	
	vector< Lit > tmpSet;	// temporary set for single methods
	
	// formula data
	vector<char> assignment;
	vec<CRef>* formulaAdress;
	uint32_t varCnt;
	
	// work counter
	unsigned unsatisfiedClauses;	// number of clauses, that are unsatisfied at the moment
	uint64_t flips;	// number of variable flips
	
	vector<Var> safeVariables;	// vector to store safe literals
	float randomPropability;	// propability to not do a heuristic step, but a random one
	float walkPropability;	  	// propability to perform a usual walk step instead of a novelty step
	
	/** initialize the search
	* create random assignment, setup the clause watch lists
	*/
	void init(bool reinit = false);
	
	/** search until a satisfying assignment is found
	*	returns true, if a solution has been found
	*/
	bool search(uint32_t flipSteps);
	
	/** enqueue the clause into the list of the literal
	*/
	void watchSatClause( const CRef clause, const Lit satisfyingLiteral );
	
	/** enqueue the clause into the lists of all its literals
	*/
	void watchUnsatClause( const CRef clause );
	/** dequeue the clause
	*/
	void unwatchUnsatClause( const CRef clause );
	
	/** rearrange clauses
	*
	* s.t. new sat clauses become sat-watch
	* clauses that are sat by another literal are moved into the literals list
	* unsatisfied clauses are moved into unsat-lists
	*/
	void updateFlip(const Lit becameSat);
	
	/** heuristic implementations
	*/
	Var heuristic();
	
	/** fill the assignment with random values
	*/
	void createRandomAssignment();
	
	/** return number of clauses, that are satisfied (positive -> more clauses satisfied afterwards )
	*/
	int getFlipDiff( const Var v ) const;
	/** return number of clauses that become satisfied with flip of v (uses length of unsat list)*/
	unsigned int getFlipSat( const Var v ) const;
	/** return number of clauses, that become unsatisfied with flip of v (computes number) */
	int getFlipUnsat( const Var v ) const;
	
	
	/** check invariants
	* -satisfied clauses are watched by a satisfied literals
	* -unsatisfied clauses are watched by all their literals
	*/
	void debug();
	
	bool isSat( const vector<char>& assignment, const Lit l ) const { return (sign(l) && assignment[ var(l) ] == -1) || (!sign(l) && assignment[ var(l) ] == 1); }
	bool isUnsat( const vector<char>& assignment, const Lit l ) const { return (sign(l) && assignment[ var(l) ] == 1) || (!sign(l) && assignment[ var(l) ] == -1); }
	bool isUndef( const vector<char>& assignment, const Lit l ) const { return assignment[ var(l) ] == 0; }
	bool setPolarity( vector<char>& assignment, const Var v, const char pol ) { assignment[ v ] = pol; }
	bool invertPolarity( vector<char>& assignment, const Var v) { if(assignment[ v ] != 0) assignment[v] = -assignment[v]; }

};

}; // end namespace

#endif // SLS_H
