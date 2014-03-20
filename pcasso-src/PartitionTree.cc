/*****************************************************************************************[partitiontree.cc]
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

#include "PartitionTree.h"
#include "Master.h"

#include <iostream> // Davide> Debug

unsigned int TreeNode::runningID = 0;

Lock TreeNode::unitLock = Lock();

TreeNode::TreeNode()
: parent(0),
  _size(0),
  solvesMe(-1),
  myID(runningID++),
  childs(0),
  lv_pool(0),
  solveTime(0),
  level(0 ),
  eval_level(-1),
  expanded(false),
  s(unknown),
  pt_level(0), // Davide
  inheritedActPol(false),
  childrenActPolUpdCount(0),
  onlyChildScenario(false),
  onlyChildScenarioChildNode(0)    
{}

TreeNode::TreeNode(const vector< vector<Lit>* >& localConstraints, TreeNode* parent)
: parent(parent),
  _size(0),
  solvesMe(-1),
  myID( runningID++),
  childs(0),
  lv_pool(0),
  solveTime(0),
  level(0 ),
  eval_level(-1),
  expanded(false),

  s(unknown),
  pt_level(parent->pt_level), // Davide>
  inheritedActPol(false),
  childrenActPolUpdCount(0),
  onlyChildScenario(false),
  onlyChildScenarioChildNode(0)
{
	// unitLock.wait();
	additionalConstraints = localConstraints;
	// unitLock.unlock();
}

void
TreeNode::setup(const vector< vector<Lit>* >& localConstraints, TreeNode* parent)
{
	this->parent=parent;
	childs=0;
	lv_pool=0;
	_size = 0;
	solvesMe=-1;
	expanded=false;
	level = (parent != 0 ) ? parent->level+1 : 0;
	s=unknown;
	// unitLock.wait();
	additionalConstraints = localConstraints;
	// unitLock.unlock();
}

TreeNode::~TreeNode()
{
	if( childs != 0 ){
		delete [] childs;
		childs = 0;
	}

	// free clauses as well

	for( unsigned 	int i = 0 ; i < additionalConstraints.size(); i++ ){
		delete additionalConstraints[i];
	}

	for( unsigned int i = 0; i < additionalUnaryConstraints.size(); i++ ){
		delete additionalUnaryConstraints[i].first;
	}

	if( this->lv_pool != 0 ){
		delete this->lv_pool; // The destructor should be called
		this->lv_pool = 0;
	}

}

/*
  vector< vector<Lit>* > additionalConstraints;
  TreeNode* parent;
  TreeNode* childs;
  unsigned int size;

  bool expanded;
  enum state {sat = 1, unsat = 2, unknown = 3} s;
 */

// add children to the node
void
TreeNode::expand( const vector< vector< vector<Lit>* >* >& childCnfs){
	assert( expanded == false );
	assert( childs == 0 );
	
	if( this->s == unsat ) return; // Davide>

	childs = new TreeNode[ childCnfs.size() ];

	_size = childCnfs.size();
	for( unsigned int i = 0 ; i < _size; ++i ){
		// tell all the nodes about their extra clauses and their parent

		//TreeNode(const vector< vector<Lit>* >& localConstraints, TreeNode* parent = 0);
		childs[i].setup( *(childCnfs[i]), this );

		childs[i].setPosition(position + davide::to_string(i)); // Davide>
		childs[i].pt_level = pt_level + 1; // Davide> CHECK

	}

	expanded = true;

}

unsigned int
TreeNode::size() const
{
	return _size;
}


int
TreeNode::getLevel () const 
{
	return level;
}

int
TreeNode::getEvaLevel () const 
{
	return eval_level;
}

int TreeNode::getSubTreeHeight () const 
{
	if( ! expanded ) return 0;
	int l = 0;
	for( unsigned int i = 0; i < _size; ++i ){
		int c = childs[i].getSubTreeHeight();
		l = l > c ? l : c;
	}
	// return size of sub tree plus own level
	return l + 1;
}

TreeNode*
TreeNode::getChild(const unsigned int index)
{
	assert( childs != 0 );
	assert( index < _size );
	return &(childs[index]);
}

TreeNode::state
TreeNode::getState() const
{
	return s;
}


void
TreeNode::setState(const state _s, bool recursive)
{
	s = _s;
	eval_level = level;
	if( recursive ){
		if( !expanded ) return;
		for( unsigned int i = 0; i < _size; ++i ){
			childs[i].setState(_s,true);
		}
	}
}

void
TreeNode::fillConstraintPath( vector< vector<Lit>* >* toFill )
{
	assert(toFill != 0 );

	// printf("lock filling here\n");

	unitLock.wait();
	// fill the vector up to the top by simply calling the parent node to do the same
	toFill->insert(toFill->end(), additionalConstraints.begin(),additionalConstraints.end());
	// printf("unlock filling here\n");
	unitLock.unlock();

	if( parent != 0 ){
		parent->fillConstraintPath( toFill );
	}
}

void
TreeNode::fillConstraintPathWithLevels( vector< pair< vector<Lit>*, unsigned int > >* toFill )
{
	assert(toFill != 0 );

	unitLock.wait();

	// Davide> Fill the vector with the Top level units
	toFill->insert(toFill->end(), additionalUnaryConstraints.begin(), additionalUnaryConstraints.end());

	// fill the vector up to the top by simply calling the parent node to do the same
	// Davide> Edited in order to include pt_level information
	for( unsigned int i = 0; i < additionalConstraints.size(); i++ )
		toFill->push_back(make_pair( additionalConstraints[i], pt_level ));

	unitLock.unlock();

	if( parent != 0 ){
		parent->fillConstraintPathWithLevels( toFill );
	}
}

void
TreeNode::addNodeConstraint(vector< Lit >* clause)
{
	// change clauses only, if we are the only user
	unitLock.wait();
	additionalConstraints.push_back(clause);
	unitLock.unlock();
}

void 
TreeNode::addNodeUnaryConstraint( vector<Lit>* clause, unsigned int pt_level ){
	unitLock.wait();
	additionalUnaryConstraints.push_back(make_pair(clause, pt_level));
	unitLock.unlock();
}

void
TreeNode::evaluate (Master& m)
{
	assert( (getState() == unknown || eval_level != -1) && "if the node has been evaluated with some value, its evaluation level has to be different than -1" );

                  checkOnlyChildScenarioChild();
	// nothing to do, if the state of the node is already known
	if( getState() == sat || getState() == unsat ) return;

	// if there are no children yet, there is nothing more to evaluate
	if( !expanded ) return;

	// evaluate children
	for( unsigned int i = 0; i < _size; ++i ){
		childs[i].evaluate(m);
	}

	// check whether this node can be evaluated by the children
	unsigned int countUnsat = 0;
	int max_eval_level = -1;
	for( unsigned int i = 0; i < _size; ++i ){
		if( childs[i].getState() == sat ){ setState(sat); eval_level = childs[i].eval_level; break; }
		if( childs[i].getState() == unsat ) {
			countUnsat ++;
			max_eval_level = max_eval_level >= childs[i].eval_level ? max_eval_level : childs[i].eval_level;
		}
		if (childs[i].getState() == retry) {
			// add nodes again to the queue?!
			childs[i].setState(unknown);
			statistics.changeI( m.retryNodesID, 1 );
			m.lock();
			m.solveQueue.push_back(&childs[i]);
			m.unlock();
		}
	}
	// subtree is unsat? -> this node and all subnodes are unsat as well
	if( countUnsat == _size ){
		eval_level = max_eval_level;
		setState( unsat,true );
	}

	assert( (getState() == unknown || eval_level != -1) && "if the node has been evaluated with some value, its evaluation level has to be different than -1" );
}

void
TreeNode::print(){
	fprintf(stderr, "=== START TREE ===\n");
	fprintf(stderr, "root(%d)  s: %d c: %d l: %d t: %lld\n", myID, s, _size, level, solveTime);
	printClauses();
	fprintf(stderr, "\n");
	if( !expanded ){
		fprintf(stderr, "===  END TREE  ===\n");
		return;
	}

	for( unsigned int i = 0; i < _size; ++i ){
		childs[i].printSubTree(i, 1);
	}
	fprintf(stderr, "===  END TREE  ===\n");
}

void
TreeNode::printSubTree(int nodeNr, int debth){
	char push [2*debth + 1];
	for( int i = 0 ; i < debth; ++i ){ push[2*i] = ' '; push[2*i+1] = '|'; }
	push[2*debth] = 0;

	fprintf(stderr, "%s--= %d(%d):  s: %d c: %d l: %d t: %lld \n", push, nodeNr, myID, s, _size, level, solveTime);
	fprintf(stderr, "%s    ", push);
	printClauses();
	fprintf(stderr, "\n");
	if( !expanded ) return;

	for( unsigned int i = 0; i < _size; ++i ){
		childs[i].printSubTree(i, debth+1);
	}
}

void
TreeNode::printClauses(){
	for( unsigned int i = 0 ; i < additionalConstraints.size(); ++i ){
		vector<Lit>& clause = *(additionalConstraints[i]);
		fprintf(stderr, " ;");
		for( unsigned int j =0; j < clause.size(); j++ ){
			int lit = sign(clause[j]) ? var(clause[j]) + 1 : 0-var(clause[j]) - 1;
			fprintf(stderr, " %d", lit );
		}
	}
	fprintf(stderr, " ;");
}

unsigned int
TreeNode::id() const{
	return myID;
}

bool
TreeNode::isUnsat() {
	bool u = false;
	if( getState() == unsat ) u = true;
	if( !u ) {
		// check the path to the root, whether some node is unsat
		TreeNode* p = parent;
		while( p != 0 ) {
			if( p->getState() == unsat ) { u = true; break; }
			else if( p->getState() == sat ) break;
			// no need to stop at unknown, because a parent of unknown could be unsat as well
			p = p->parent;
		}
	}
	// if unsat, set all child nodes to unsat as well
	if( u ) {
		setState(unsat, true);
	}
	return u;
}
// Davide> Get the position of the node in the tree
string
TreeNode::getPosition(){
	return position;
}

// Davide> Set the position of the node in the tree
void
TreeNode::setPosition(string _position){
	position = _position;
}

// Davide> Returns the father
TreeNode*
TreeNode::getFather(void){
	return this->parent;
}

// Davide> deprecated
unsigned int
TreeNode::getPTLevel(void) const{
	return this->pt_level;
}
void
TreeNode::setSize(unsigned int size){
	_size = size;
}
void
TreeNode::setExpanded(bool value){
	expanded = value;
}
//ahmed
void TreeNode::activityCopyTo(vec<double>& act) {
	unitLock.lock();
	if(activity.size()>0)
		activity.copyTo(act);
	unitLock.unlock();
}

void TreeNode::phaseCopyTo(vec<char>& ph) {
	unitLock.lock();
	if(phase.size()>0)
		phase.copyTo(ph);
	unitLock.unlock();
}

void TreeNode::updateActivityPolarity(vec<double>& act, vec<char>& ph, int option) {
	if(!inheritedActPol && parent!=NULL && pt_level>1) {//level 1 nodes do not receive act & polarity from parents
		vec<double> parentActivity;
		vec<char> parentPhase;
		if(option>0) parent->activityCopyTo(parentActivity);
		if(option>1) parent->phaseCopyTo(parentPhase);
		if(parentActivity.size()!=0 && parentPhase.size()!=0){
			if(option>1) assert(ph.size() <= parentPhase.size());
			if(option>0) assert(act.size() <= parentActivity.size());
			if(option==3){
                                                        for(int i=0; i<ph.size() && i<act.size(); i++) {
                                                                ph[i] =parentPhase[i];
                                                                act[i] = parentActivity[i];
                                                        }
                                                    }
                                                    else if(option==1){
                                                        for(int i=0; i<ph.size() && i<act.size(); i++) {
                                                                act[i] = parentActivity[i];
                                                        }
                                                    }
                                                    else if(option==2){
                                                        for(int i=0; i<ph.size() && i<act.size(); i++) {
                                                                ph[i] =parentPhase[i];
                                                        }
                                                    }
                                        inheritedActPol=true;
                                        parent->incChildrenActPolUpdCount();
		}
	}
	if(pt_level>0 && size()>0) {
                        unitLock.lock();
                        if(childrenActPolUpdCount<size()) {
                            if(option>0) act.copyTo(activity);
                            if(option>1) ph.copyTo(phase);
                        }
                        unitLock.unlock();
                }
}

//incrementing the count of the children which has updated the activity and polarity
void TreeNode::incChildrenActPolUpdCount() {
    unitLock.lock();
    childrenActPolUpdCount++;
    unitLock.unlock();
}
 
// Remove the pool associated to the node
void
TreeNode::removePoolRecursive(bool recursive){
	if(this->lv_pool != 0) delete this->lv_pool;
	else{
		this->lv_pool = 0;
		return;
	}
	this->lv_pool = 0;
//	cerr << "M: DELETED POOL OF " << this->position << endl;
	if(recursive){
		for(int i = 0; i < this->childsCount(); i++){
			childs[i].removePoolRecursive(recursive);
		}
	}
}

void TreeNode::checkOnlyChildScenarioChild(){
    if(!expanded) return;
    if(!onlyChildScenario){
        unsigned solved=0;
        TreeNode *child;
        TreeNode *unsolvedChild;
        for(unsigned j=0; j< childsCount(); j++) {
                child = getChild(j);
                if (child->getState()==unsat)
                        solved++;
                else
                    unsolvedChild=child;
        }
        if(solved == childsCount()-1){
            onlyChildScenario=true;
            onlyChildScenarioChildNode=unsolvedChild;
        }
    }else{
        TreeNode *child=onlyChildScenarioChildNode;
        TreeNode *unsolvedChild=onlyChildScenarioChildNode;
        while(child->isOnlyChildScenario()){
            child=child->getOnlyChildScenarioChildNode();
            unsolvedChild=child;
        }
        onlyChildScenarioChildNode=unsolvedChild;
    }
}

bool TreeNode::isOnlyChildScenario(){
    return onlyChildScenario;
}

TreeNode* TreeNode::getOnlyChildScenarioChildNode(){
    return onlyChildScenarioChildNode;
}
