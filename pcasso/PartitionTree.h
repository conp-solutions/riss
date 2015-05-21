/*****************************************************************************************[partitiontree.h]
Copyright (c) 2012,      Norbert Manthey, Antti Hyvärinen

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **************************************************************************************************/

#ifndef PARTITIONTREE_H
#define PARTITIONTREE_H

// libc
#include <vector>
#include <cstdio>

// Minisat
#include "riss/mtl/Vec.h"
#include "riss/core/SolverTypes.h"
#include "riss/core/Solver.h"

// read and write constraints
#include "riss/utils/LockCollection.h"

// Davide>
#include <string>
#include "pcasso/ToString.h"
#include "pcasso/LevelPool.h"

// using namespace Pcasso;
// using namespace std;

namespace Pcasso {

// to avoid circular dependencies with an extra #include, use a forward declaration
class Master;

class TreeNode
{
public:

	// can be solved by solving the child, because it has only a single child
	enum state {sat = 1, unsat = 2, unknown = 3, retry = 4};

	// Davide> The pool
	PcassoDavide::LevelPool* lv_pool;
	TreeNode* childs;

private:
	static unsigned int runningID;
	vector< vector<Lit>* > additionalConstraints;
	vector< pair< vector<Lit>*, unsigned int> > additionalUnaryConstraints; // Davide>

	TreeNode* parent;
	unsigned int _size;
	int solvesMe;
	unsigned int myID;

	vec<double> activity; //Ahmed> for storing the activity of the node after certain number of conflicts
	vec<char> phase;      //Ahmed> for storing the phase of the node after certain number of conflicts
	bool inheritedActPol;
                  unsigned childrenActPolUpdCount;
                  
                  bool onlyChildScenario;
                  TreeNode* onlyChildScenarioChildNode;
                  
public:
	long long int solveTime;
private:
	int level;	// level that the node has in the tree
	int eval_level; // level that has been used to evaluate that node finally

	bool expanded;
	state s;

	// Davide> Position in the tree
	string position; // TODO CURRENTLY, IT ONLY WORKS WITH A BRANCHING OF CARDINALITY UP TO 9 !! ( base 10 encoding )
	                 // TODO IF YOU WANT TO BRANCH MORE, FIXME OTHERWISE BUG IN KILLUNSATCHILDREN (Master)
	// Davide> Level in the tree
	unsigned int pt_level;

	/// lock to prevent threads to read on data that is update at the moment
	static ComplexLock unitLock;

	void setup(const vector< vector<Lit>* >& localConstraints, TreeNode* parent = 0);


	void printSubTree(int nodeNr, int debth);

	void printClauses();

public:

	// setup the node
	TreeNode();
	TreeNode(const vector< vector<Lit>* >& localConstraints, TreeNode* parent = 0);
	~TreeNode();

	// add children to the node
	void expand( const vector< vector< vector<Lit>* >* >& childCnfs);

	unsigned int size() const;

	int getLevel () const ;
	int getSubTreeHeight () const ;
	int getEvaLevel () const ;

	TreeNode* getChild(const unsigned int index);

	TreeNode* getFather(void); // Davide> I need this in "search()"

	state getState() const;

	// can set a state recusively, such that all sub nodes will be set immediately as well
	void setState(const state _s, bool recursive = false);

	void fillConstraintPath( vector< vector<Lit>* >* toFill );

	void fillConstraintPathWithLevels( vector< pair< vector<Lit>*, unsigned int > >* toFill ); // Davide> A slightly different version able to store levels for every clause

	/// add more constraints to the node, for example found unit clauses
	void addNodeConstraint( vector<Lit>* clause );

	// Davide> Add unary constraints & specify their pt_level
	void addNodeUnaryConstraint( vector<Lit>* clause, unsigned int pt_level );

	/** tries to set the state of the node by using the sub tree below the node
	 * NOTE: writes only to the state value, if the state is not unknown any more
	 */
	void evaluate(Master& m);

	// prints the tree below (including) this node
	void print();

	unsigned int id() const;

	/** check whether this node is unsatisfiable (take knowledge about parent into account)
	 */
	bool isUnsat();

	// Davide> Get the position of the node in the tree
	string getPosition(void);

	// Davide> Set the position of the node in the tree
	void setPosition(string _position);

	// Davide> useful
	void setSize(unsigned int size);

	// Davide> Return the level of the current node
	unsigned int getPTLevel(void) const;

	void setExpanded(bool value);

	bool isAncestorOf(TreeNode& t){
//		if( t.size() < this->position.size() )
//			return false;
//		int i = 0;
//		while( i < this->position.size() && this->position[i] == t.position[i] )
//			i++;
//		if(i == this->position.size()) return true;
//		return false;
		TreeNode* curNode = &t;
		while(curNode != 0){
			if(curNode->id() == this->id())
				return true;
			curNode = curNode->getFather();
		}
		return false;
	}
	unsigned int childsCount(){
		return _size;
	}
	void activityCopyTo(vec<double>& act);
	void phaseCopyTo(vec<char>& ph);
	void updateActivityPolarity(vec<double>& act, vec<Riss::Solver::VarFlags>& pol, int option);
                  void incChildrenActPolUpdCount();
	// Remove the pool associated to the node
	void removePoolRecursive(bool recursive);
                  void checkOnlyChildScenarioChild();
                  bool isOnlyChildScenario();
                  TreeNode* getOnlyChildScenarioChildNode();
                  
};

} // namespace Pcasso

#endif // PARTITIONTREE_H
