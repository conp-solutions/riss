/**************************************************************************************[ModPrep.cc]
Copyright (c) 2016, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "ModPrep.h"

#include "riss/utils/community.h"

using namespace Riss;
using namespace std;

namespace Coprocessor
{

ModPrep::ModPrep(CP3Config& _config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Solver& _solver)
    : Technique(_config, _ca, _controller)
    , data(_data)
    , solver(_solver)
    , processTime(0)
{

}

void ModPrep::reset()
{

}

bool ModPrep::process()
{
    // parameters
    double precision = 0.000001;
    int randomCommunities =0; // 20;
    int stepLimit = 1000000;  // TODO: make it a parameter
    int singleConflicts = 1000; // conflict per community pair
    int totalConflictLimit = 1000000; // number of conflicts that can be performed in total

    // usual code
    DOUT(if (config.modprep_debug > 0) cerr << "c run MODPREP process" << endl;);
    MethodTimer mt(&processTime);

    if (! performSimplification()) { return false; }   // do not do anything?!
    modifiedFormula = false;
    // setup structures
    data.ma.resize(2 * data.nVars());
    data.lits.clear();
    data.clss.clear();
    
    if( data.nVars() == 0 || data.getClauses().size() + data.getLEarnts().size() == 0 ) return false;
    
    // get communities here!
    
    vector< int > communityPerVariable ( data.nVars(), -1 );
    SimpleGraph *communities = nullptr;
    SimpleGraph *communityNeighbors = nullptr;
    bool reachedLimit = false;
    
    if( randomCommunities != 0 ) {
      DOUT( cerr << "c create random communities: " << randomCommunities << endl; );
      reachedLimit = getRandomCommunities( randomCommunities, communityPerVariable, communities, communityNeighbors );
    } else {
      DOUT( cerr << "c create communities " << endl; );
      reachedLimit = getCommunities( communityPerVariable, communities, communityNeighbors );
    }
    
    if( reachedLimit ) {
      DOUT( cerr << "c stop as we reached the limit during creating communities" << endl; );
      return modifiedFormula;
    }


#if 0
#error iterate over pairs of communities
#error attach all clauses that contain variables with these two communities
#error solve the formula
#error clear all watch lists of all literals that have been used
#error continue with next pair
#error handle step counter appropriately (have extra methods)
#error clear graph class
#endif

    // get all watch lists into the solver
    data.reSetupSolver();
  
    // setup structures
    data.ma.resize( data.nVars() );
    MarkArray watchArray;
    watchArray.resize( data.nVars() ); // .resize( data.nVars() );
    
    // clear all watches!
    for (int v = 0; v < solver.nVars(); v++) {
        for (int s = 0; s < 2; s++) {
            solver.watches[ mkLit(v, s) ].clear();
        }
    }
    // clear all watches!
    solver.watches.cleanAll();
    
    int totalConflicts = 0;
    lbool status = l_Undef;
    vector<int> community, neigborCommunities, neighborCommunity;
    vector<Var> usedVariables;
    vec<Lit> dummy;
    DOUT( cerr << "c iterate over communities: " << communities->getSize() << endl; );
    
    for(int i=0; i  < communities->getSize(); ++i){
      communityNeighbors->getAdjacency( i, neigborCommunities );
      DOUT( cerr << "c go for community " << i << " with " << neigborCommunities.size() << " communities" << endl; );
      // iterate over all neighboring communities
      for(int n=0; n  < neigborCommunities.size(); ++n){
	
	if( neigborCommunities[n] < i ) continue; // solver each pair only once!
	
	DOUT( cerr << "c check pair: " << i << " with " << neigborCommunities[n] << endl; );
	
	data.ma.nextStep();    // memorize variables we already processed
	watchArray.nextStep(); // memorize all variables that have been added to the solver
	usedVariables.clear(); 

	Solver checkSolver; // get a usual solver
	checkSolver.reserveVars( data.nVars() );
	
	communities->getAdjacency( neigborCommunities[n], neighborCommunity );
	
	// get all variables of this community, and attach all clauses to the solver again!
	communities->getAdjacency( i, community );      
	
	// add all clauses with variables of the two communities!
	for( int selector = 0; selector < 2; ++ selector ) {
	  vector<int>& currentCommunity = (selector == 0) ? community : neighborCommunity; // depending on iteration, select community
	  
	  for( int j = 0 ; j < currentCommunity.size(); ++ j ) { // for the given community, collect all clauses with a variable of this community!
	    Var v = currentCommunity[j];
	    if( data.ma.isCurrentStep( v ) ) continue; // saw variable already
	    data.ma.setCurrentStep( v );
	    
	    for( int p = 0 ; p < 2; ++ p ) {
	      const Lit l = mkLit( v, p == 0 );
	      for ( int k = 0 ; k < data.list(l).size(); ++ k ) {
		const Clause& c = ca[ data.list(l)[k] ];
		if( c.can_be_deleted() ) continue; // jump over ignored clauses
		assert( c.size() > 1 && "there should not be unit clauses in the formula" );
		if( l != c[0] ) continue; 
		checkSolver.addClause( ca[ data.list(l)[k] ] ); // only attach clauses where the current variable is the first element of the clause!
		
		for( int m = 0 ; m < c.size(); ++ m ) { // collect all variables that are added to the solver to clean a subset of the watch lists afterwards
		  if( ! watchArray.isCurrentStep( var(c[m]) ) ) {
		    watchArray.setCurrentStep( var(c[m]) );
		    usedVariables.push_back( var(c[m]) );
		  }
		}
	      }
	    }
	  }
	
	}
	
	// solve the current formula with the given conflict limit
	checkSolver.setConfBudget( singleConflicts );
#warning garbageCollection might kill the idea of using the same solver again and again!
	status = checkSolver.solveLimited(dummy);
	totalConflicts += solver.conflicts - totalConflicts;
	
	
	// found UNSAT? or reached limits?
	if( status == l_False || totalConflictLimit < totalConflicts ) break; 
	
	vec<Lit>& trail = checkSolver.trail;
	for( int t = 0 ; t < trail.size(); ++ t ) {
	  if( data.value( trail[t] ) == l_False ) data.setFailed();
	  if( data.value( trail[t] ) == l_Undef ) data.enqueue( trail[t] );
	}
	vec<CRef>& learnts = checkSolver.learnts;
	for( int t = 0 ; t < learnts.size(); ++ t ) {
	  const Clause& c = checkSolver.ca[ learnts[t] ];
	  CRef newClause = ca.alloc( c,false );
	  data.addClause( newClause );
	  data.getClauses().push( newClause );
	  data.addSubStrengthClause( newClause );
	}
	
	// checkSolver will be destroyed automatically
	
      } // end iterating over neigbors
      if( status == l_False || totalConflictLimit < totalConflicts ) break; // found UNSAT? or reached limits?
      
    } // end iterating over all communities
    
    
    cleanSolver();

    return modifiedFormula;
}

bool ModPrep::getRandomCommunities(int randomCommunities, vector< int >& communityPerVariable, SimpleGraph*& communities, SimpleGraph*& communityNeighbors)
{
  int stepLimit = 2000000000;  // TODO: make it a parameter
  // code
  int step = 0; // make it a member variable

  SimpleGraph* vigGraph = getVIG( step, stepLimit);
  if( vigGraph == nullptr ) {
    DOUT( cerr << "c does not create VIG" << endl; );
    return true; // hit step limit!
  }
  
  // assign random communities to variables
  for( int i = 0; i < communityPerVariable.size(); ++ i ) {
    communityPerVariable[i] = rand() % randomCommunities;
  }
  
  // build adjacency lists for communities
  communities = new SimpleGraph ( randomCommunities, false );
  for( int i = 0; i < communityPerVariable.size(); ++ i ) {
    //DOUT( cerr << "c add edge: " << communityPerVariable[i] << " - " << i << endl; );
    communities->addDirectedEdge( communityPerVariable[i], i , 1); // add variable to its community
  }
  communities->finalizeGraph();
  // no need for finalizeGraph, as we added each element (i) exactly once!
  
  communityNeighbors = new SimpleGraph(communities->getSize(), false);
   // create neighbor information per community (over all communities, collect the communities of all neighbors of its variables)
   vector<int> adj, community;
   for(int i=0; i + 1 < communities->getSize(); ++i) {
     communities->getAdjacency( i, community );
     DOUT( cerr << "c process community " << i << " with " << community.size() << " elements" << endl; );
     
     for(int j =0; j< community.size(); ++j){       // list with all variables of community i
      vigGraph->getAdjacency( community[j], adj );  // create list with all neightbors of the variable 
      for(int k=0; k<adj.size(); ++k) {
	if (communityPerVariable[adj[k]] > i){ // neighbors in one directions!
	  DOUT( cerr << "c add neighbors " << i << "  --  " << communityPerVariable[adj[k]] << endl; );
	   communityNeighbors->addUndirectedEdge(i, communityPerVariable[adj[k]]); // community i and communityPerVariable[adj[k]] are neighbors
	}
	step++;
	if(step > stepLimit) {
	  delete communityNeighbors ; communityNeighbors = nullptr;
	  delete communities ; communities = nullptr;
	  DOUT( cerr << "c reached limit during creating communities" << endl; );
	  return true;
	}
      }
     }
  }
  
  // from a space point of view, we do not need the VIG any more!
  delete vigGraph; vigGraph = nullptr;
  
  cerr << "c graph pointers: " << std::hex << " community: " << communities << " neighbors: " << communityNeighbors << std::dec << endl;
  
  communityNeighbors->finalizeGraph(); // make sure all adjacency lists are there only once
  return false;
}


SimpleGraph* ModPrep::getVIG(int& step, int steplimit)
{
    // feed formula into VIG 
    SimpleGraph *vigGraph = new SimpleGraph(data.nVars(), false);
     for (int i = 0; i < solver.clauses.size(); ++i) {
        const Clause& c = solver.ca[solver.clauses[i]];
	if( c.can_be_deleted() ) continue;
        double wvariable = pow(2, -c.size());
        for (int j = 0; j < c.size(); ++j) {
            const Lit& l = c[j]; // for read access only, you could use a read-only reference of the type literal.
            const Lit cpl = ~l;  // build the complement of the literal
            const Var v = var(l); // calculate a variable from the literal
                for (int k = j + 1; k < c.size(); ++k) {
                    vigGraph->addDirectedEdgeAndInvertedEdge(v, var(c[k]), 1); //with undirected edges there would be problems finding features(e.g. diameter)
                }
            step ++;
	    if( step > steplimit ) { // when reaching the limit, we do not return a graph at all!
	      delete vigGraph; vigGraph = nullptr;
	      return vigGraph;
	    }
        }
    }
    vigGraph->finalizeGraph();     // get adjacency lists right
    vigGraph->completeSingleVIG(); // make complete graph
    return vigGraph;
}


bool ModPrep::getCommunities(vector< int >& communityPerVariable, SimpleGraph *& communities, SimpleGraph *& communityNeighbors)
{
    // parameters
    double precision = 0.00001;
    int stepLimit = 1000000;  // TODO: make it a parameter

    // code
    int step = 0;

    SimpleGraph* vigGraph = getVIG( step, stepLimit);
    if( vigGraph == nullptr ) return true; // hit step limit!
    
    
    // get communities
    Community c( vigGraph );
    double modularity = c.compute_modularity_GFA( precision ); // TODO make this a parameter
    c.compute_communities();
  
    cerr << "c MODPREP: found communities: " << c.Comm.size() << " modularity: " << modularity << endl;
    
    // store community ID per variable
   for(int i=0; i<c.Comm.size(); ++i) {
     for(int j=0; j< c.Comm[i].size(); j++){ 
	communityPerVariable[c.Comm[i][j]] = i;
	step++;
	if(step > stepLimit) break;
     }
     if(step > stepLimit) break;
   }
   
   // TODO: have a better handler!
   if(step > stepLimit) return false;
   
   communityNeighbors = new SimpleGraph(c.Comm.size(), false);
    
   // create neighbor information per community (over all communities, collect the communities of all neighbors of its variables)
   vector<int> adj;
   for(int i=0; i< c.Comm.size()-1; ++i){
     for(int j =0; j< c.Comm[i].size(); ++j){ // list with all variables of community i
      vigGraph->getAdjacency( c.Comm[i][j], adj );  // create list with all neightbors of the variable 
      for(int k=0; k<adj.size(); ++k) {
	if (communityPerVariable[adj[k]] > i){ // neighbors in one directions!
	   communityNeighbors->addUndirectedEdge(i, communityPerVariable[adj[k]]); // community i and communityPerVariable[adj[k]] are neighbors
	}
	step++;
	if(step > stepLimit) {
	  delete communityNeighbors ; communityNeighbors = nullptr;
	  return false;
	}
      }
     }
  }
  communityNeighbors->finalizeGraph(); // make sure all adjacency lists are there only once
  
  // from a space point of view, we do not need the VIG any more!
  delete vigGraph; vigGraph = nullptr;
  
  DOUT(if (config.modprep_debug > 0) cerr << "c run MODPREP process" << endl;);
  
   // copy communities to be used outside this method in a generic manner
   if( step < stepLimit ) {
    communities = new SimpleGraph( c.Comm.size(), false );
    for(int i=0; i<c.Comm.size(); ++i) {
      for(int j=0; j< c.Comm[i].size(); j++){ 
	communities->addDirectedEdge( i,j,1 );
      }
    }
   }
  
  return false;
}


void ModPrep::printStatistics(ostream& stream)
{
    stream << "c [STAT] MODPREP " << processTime << endl;
}

void ModPrep::giveMoreSteps()
{

}

void ModPrep::destroy()
{

}

void ModPrep::cleanSolver()
{
    // clear all watches!
    solver.watches.cleanAll();

    // clear all watches!
    for (int v = 0; v < solver.nVars(); v++)
        for (int s = 0; s < 2; s++) {
            solver.watches[ mkLit(v, s) ].clear();
        }

    solver.learnts_literals = 0;
    solver.clauses_literals = 0;
    solver.watches.cleanAll();

    for (int i = 0 ; i < solver.learnts.size(); ++ i) {
        ca[ solver.learnts[i] ].sort();
    }
    for (int i = 0 ; i < solver.clauses.size(); ++ i) {
        ca[ solver.clauses[i] ].sort();
    }
}

} // namespace Coprocessor
