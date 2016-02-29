/*
 * Graph.cpp
 *
 *  Created on: Oct 21, 2013
 *      Author: gardero
 */

#include "Graph.h"

#include <math.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include "dimension.h"
#include "community.h"
#include <algorithm>
#include "riss/mtl/Sort.h"
#include "riss/core/SolverTypes.h"

#include <iterator>



using namespace std;

SimpleGraph::Graph(int size, bool computingDerivative) :
    pagerank(size),
    node(size),
    nodeDeg(size, 0),
    degreeStatistics(computingDerivative),
    weightStatistics(computingDerivative, false), 
    exzentricityStatistics(computingDerivative),
    pagerankStatistics(computingDerivative),
    articulationpointsStatistics(computingDerivative) 
{
    // TODO Auto-generated constructor stub
    this->size = size;
    this->mergeAtTheEnd = true;
    addedDirectedEdge = false;
    addedUndirectedEdge = false;
    intermediateSort = true;
    intermediateSorts = 0;
    for (int i = 0 ; i < size; ++ i) { node[i].reserve(8); }   // get space for the first 8 elements
    sortSize.resize(size, 4);      // re-sort after 16 elements
    narity.resize(size, 0); 
}

SimpleGraph::Graph(int size, bool mergeAtTheEnd, bool computingDerivative) :
    pagerank(size),
    node(size),
    nodeDeg(size, 0),
    degreeStatistics(computingDerivative),
    weightStatistics(computingDerivative, false), 
    exzentricityStatistics(computingDerivative),
    pagerankStatistics(computingDerivative),
    articulationpointsStatistics(computingDerivative)
{
    // TODO Auto-generated constructor stub
    this->size = size;
    this->mergeAtTheEnd = mergeAtTheEnd;
    addedDirectedEdge = false;
    addedUndirectedEdge = false;
    intermediateSort = true;
    intermediateSorts = 0;
    sortSize.resize(size, 4);      // re-sort after 16 elements
    narity.resize(size, 0); 
    for (int i = 0 ; i < size; ++ i) { node[i].reserve(8); }   // get space for the first 8 elements
}

SimpleGraph::~Graph()
{
    // TODO Auto-generated destructor stub
//   cerr << "c intermediate sorts: " << intermediateSorts << endl;
}

void SimpleGraph::finalizeGraph() {
     for(int i = 0; i < node.size(); i++){
    sortAdjacencyList(node[i]);                    //finalizeGraph
    nodeDeg[i] = node[i].size();
   }
}

void SimpleGraph::setIntermediateSort(bool newValue)
{
    intermediateSort = newValue;
    if (intermediateSort == true) {
        sortSize.resize(size, 4);      // re-sort after 16 elements
    }
}

//void Graph::addEdge(int nodeA, int nodeB) {
//  addEdge(nodeA,nodeB,1);
//}
//
//void Graph::addEdge(int nodeA, int nodeB, double aweight) {
//  addUndirectedEdge(nodeA,nodeB,aweight);
//  addUndirectedEdge(nodeB,nodeA,aweight);
//}

int SimpleGraph::getDegree(int node)
{
    return this->nodeDeg[node];
}

double SimpleGraph::arity(int x) {     
		assert(x>=0 && x<= size-1); 
		return narity[x]; 
	}
	
double SimpleGraph::arity() {     
  double arit = 0;
		for(int i = 0; i<narity.size(); i++) arit+=narity[i];
		return arit;
	}

void SimpleGraph::addUndirectedEdge(int nodeA, int nodeB)
{
    addUndirectedEdge(nodeA, nodeB, 1);
}

void SimpleGraph::addUndirectedEdge(int nodeA, int nodeB, double aweight)
{
    assert( !addedDirectedEdge && "cannot mix edge types in implementation" );
    addedUndirectedEdge = true;
    if (nodeA >= node.size()) {
        stringstream sstm;
        sstm << "tying to put node " << nodeA << " in a " << node.size() << " nodes Graph" << endl;
        string s = sstm.str();
        cerr << s;
        assert(false);
    }
    if (nodeA > nodeB) { // always save the reference to the greater node.
        int t = nodeB;
        nodeB = nodeA;
        nodeA = t;
    }
    edge p(nodeB, aweight);
    node[nodeA].push_back(p); // add only in one list, do newer add directed and undirected edges
    nodeDeg[nodeA]++;
    nodeDeg[nodeB]++;
    narity[nodeA] += aweight;
    narity[nodeB] += aweight;
    if (intermediateSort && node[nodeA].size() > sortSize[ nodeA ]) {
        sortAdjacencyList(node[nodeA]);
        sortSize[ nodeA ] *= 2;
	nodeDeg[nodeA] = node[nodeA].size();
    }
}

void SimpleGraph::addDirectedEdge(int nodeA, int nodeB, double aweight)
{
    assert( !addedUndirectedEdge && "cannot mix edge types in implementation" );
    addedDirectedEdge = true;
    if (nodeA >= node.size()) {
        stringstream sstm;
        sstm << "tying to put node " << nodeA << " in a " << node.size() << " nodes Graph" << endl;
        string s = sstm.str();
        cerr << s;
        assert(false);
    }
    edge p(nodeB, aweight);
    node[nodeA].push_back(p);
    nodeDeg[nodeA]++;
    narity[nodeA] += aweight;
    narity[nodeB] += aweight;
    if (intermediateSort && node[nodeA].size() > sortSize[ nodeA ]) {
        sortAdjacencyList(node[nodeA]);
        sortSize[ nodeA ] *= 2;
	nodeDeg[nodeA] = node[nodeA].size();
    }
}

uint64_t SimpleGraph::addAndCountUndirectedEdge(int nodeA, int nodeB, double weight)
{
    assert( !addedDirectedEdge && "cannot mix edge types in implementation" );
    addedUndirectedEdge = true;
    uint64_t operations = 0;
    if (nodeA > nodeB) { // always save the reference to the greater node.
        int t = nodeB;
        nodeB = nodeA;
        nodeA = t;
    }
    if (mergeAtTheEnd) {
        addUndirectedEdge(nodeA, nodeB, weight);
        operations += 1;
    } else {
        int i;
        bool foundFlag = false;
        for (i = 0; i < node[nodeA].size(); ++i) {
            if (node[nodeA][i].first == nodeB) {
                node[nodeA][i].second += weight;
                foundFlag = true;
                operations += (i + 1);
                break;
            }
        }
        if (!foundFlag) {
            operations += node[nodeA].size();
            edge p(nodeB, weight);
            node[nodeA].push_back(p);
            nodeDeg[nodeA]++;
            nodeDeg[nodeB]++;
        }
    }
    return operations;
}

uint64_t SimpleGraph::computeStatistics(int quantilesCount)
{
    uint64_t operations = 0;
    if (!mergeAtTheEnd) {
        operations += computeOnlyStatistics(quantilesCount);
    } else {
        operations += computeNmergeStatistics(quantilesCount);
    }
    return operations;
}

uint64_t SimpleGraph::computeOnlyStatistics(int quantilesCount)
{
    uint64_t operations = 0;
    int i, nsize;
    // Statistics for the degrees
    for (i = 0; i < size; ++i) {
        nsize = getDegree(i);
        degreeStatistics.addValue(nsize);
    }
    operations += size;
    operations += degreeStatistics.compute(quantilesCount);
    //  degreeStatistics.tostdio();
    
     // Statistics for the exzentricity 
   for(int i=0; i< size; ++i) exzentricityStatistics.addValue(getExzentricity(i));
     operations += exzentricityStatistics.compute(quantilesCount);

   
   // Statistics for the pagerank 
   for(int i=0; i< size; ++i) pagerankStatistics.addValue(getPageRank(i));
     operations += pagerankStatistics.compute(quantilesCount);
   
   // Statistics for the articulationpoints
   for(int i=0; i< articulationpoints.size(); ++i){
     getArticulationPoints();
     articulationpointsStatistics.addValue(articulationpoints[i]);
   }
     operations += articulationpointsStatistics.compute(quantilesCount);
     operations += articulationpoints.size();
    // Statistics for the weights
    int j;
    for (i = 0; i < size; ++i) {
        for (j = 0; j < node[i].size(); ++j) {
            weightStatistics.addValue(node[i][j].second);
        }
        operations += node[i].size();
    }
    operations += weightStatistics.compute(quantilesCount);
    return operations;
}

bool nodesComparator(edge e1, edge e2)
{
    return (e1.first < e2.first);
}

uint64_t SimpleGraph::computeNmergeStatistics(int quantilesCount)
{
    uint64_t operations = 0;
    for (int i = 0; i < size; ++i) {
        if (node[i].size() > 0) {
            sort(node[i].begin(), node[i].end(), nodesComparator);
            operations += node[i].size() * log(node[i].size());
            int tnode = node[i][0].first;
            int cnode = 0; // counter for repeated values
            double wnode = node[i][0].second;
            for (int j = 1; j < node[i].size(); ++j) {
                if (tnode != node[i][j].first) {
                    // add it!!
                    weightStatistics.addValue(wnode);
                    nodeDeg[i] -= cnode;
                    nodeDeg[tnode] -= cnode;
                    // reset values!!
                    tnode = node[i][j].first;
                    cnode = 0;
                    wnode = 0;
                } else {
                    cnode++;
                }
                wnode += node[i][j].second;
            }
            operations += node[i].size();
            // add the last one!!
            nodeDeg[i] -= cnode;
            nodeDeg[tnode] -= cnode;
            weightStatistics.addValue(wnode);
        }
    }
    for (int i = 0; i < size; ++i) {
        degreeStatistics.addValue(nodeDeg[i]);
    }
    operations += size;
    operations += degreeStatistics.compute(quantilesCount);
    operations += weightStatistics.compute(quantilesCount);
    return operations;
}

uint64_t SimpleGraph::sortAdjacencyList(adjacencyList& aList)
{
    if( aList.size() == 0 ) return 0; // early abort, there is nothing to be done for this list!
    uint64_t operations = 0;
    
    sort(aList.begin(), aList.end(), nodesComparator);
    operations += aList.size() * log(aList.size());
    int kept = 0;
    for (int j = 1; j < aList.size(); ++j) { // start with second element, as the first element will be kept in all cases!
        if (aList[j].first != aList[kept].first) { aList[++kept] = aList[j]; }   // keep the next element
        else { aList[kept].second += aList[j].second; } // otherwise add the weights!
    }
    operations += aList.size();

    if( kept < aList.size() ) aList.resize(kept + 1);   // save space!

    intermediateSorts ++;
    
    return operations;
}

vector<int> SimpleGraph::getAdjacency(int adjnode)
{
 adjacencyList& adj = node[adjnode];
 vector<int> nodes;
 
 for(int i = 0; i < adj.size(); i++){
   edge edg = adj[i];
   
   nodes.push_back(edg.first);
 
  }
  
  return nodes;
}

void SimpleGraph::completeSingleVIG(){
     
   for(int j = size-2; j >= 0; j--){ //you dont have to check the last node
     adjacencyList& adj = node[j];
     for(int k = 0; k < adj.size(); k++) addDirectedEdge(adj[k].first, j, adj[k].second);
     }
     
     finalizeGraph(); //make sure that the graph is sorted and that there are no duplicates in adjacency lists
    
}

vector<double> SimpleGraph::getDistances(int nod){

  Riss::MarkArray visited;
  visited.create(size);
  visited.nextStep();
  vector<double> distance(size);
  vector<int> nodestovisit;
  adjacencyList& adj = node[nod];
  double tmpdistance;
  
  for(int x=0; x<size;x++)distance[x] = numeric_limits<double>::infinity(); //distance to all nodes = inf
  
  distance[nod] = 0;
   visited.setCurrentStep(nod);
  
  for(int i=0;i<adj.size();i++){ //setting distance of neighbors
    nodestovisit.push_back(adj[i].first);
    visited.setCurrentStep(adj[i].first);
    distance[adj[i].first] = adj[i].second;
  }
  
  for(int k=0; k<nodestovisit.size();++k){  //breadthfirstsearch algorithm to find the distance to all nodes
    
    tmpdistance = distance[nodestovisit[k]];
    adjacencyList& adjNeighbor = node[nodestovisit[k]]; 
     
     for(int j=0;j<adjNeighbor.size();++j){
       
     if(distance[adjNeighbor[j].first] > tmpdistance+adjNeighbor[j].second) distance[adjNeighbor[j].first] = tmpdistance+adjNeighbor[j].second;
     if(visited.isCurrentStep(adjNeighbor[j].first)) continue;
      visited.setCurrentStep(adjNeighbor[j].first);
        nodestovisit.push_back(adjNeighbor[j].first);   
    }
        
  }

return distance;
}

double SimpleGraph::getDiameter(){ //TODO: Elias: Maybe there are faster algorithms to find just the diameter/radius(without using the exzentricity of each node)

  double ex;
  double diameter = 0;
  
for(int i=0; i<size;++i){ //getting the biggest exzentricity (=diameter)
  ex = getExzentricity(i);
  if(ex > diameter) diameter = ex;
}  
return diameter;  
}

double SimpleGraph::getRadius(){

  double radius = numeric_limits<double>::infinity();
  double ex;
for(int i=0; i<size;++i){
  ex = getExzentricity(i);
  if(ex == 0) continue; //TODO: Elias: whats happening with nodes with no edges ? getExzentricity = 0 or inf ? 
  if(ex < radius) radius = ex;
}  
return radius;  
}

double SimpleGraph::getExzentricity(int nod){

 const vector<double>& distance = getDistances(nod);
 double max =0;
  
  for(int j=0; j<distance.size();++j){
    if(distance[j] == numeric_limits<double>::infinity()) continue;
    if(distance[j] > max) max = distance[j];
  }
   
  return max; 
  
}

vector<int> SimpleGraph::getArticulationPoints(){
  
  Riss::MarkArray visited;
  visited.create(size);
  visited.nextStep();
  vector<int> stack;
  int tmp = 0; //if root is cut_vertex tmp>1 (more then one child in DFS tree)
  
  int rootnode = 0; //root node
  
  const vector<int>& adj = getAdjacency(rootnode);
  visited.setCurrentStep(rootnode);
  stack.push_back(rootnode);

  for(int i =0; i< adj.size(); ++i){
  if(visited.isCurrentStep(adj[i])) continue;
    visited.setCurrentStep(adj[i]);
    stack.push_back(adj[i]);
    tmp++;

    DepthFirstSearch(stack, visited, articulationpoints);
  }
  
  if(tmp>1){
    articulationpoints.push_back(rootnode);
    
  }
  return articulationpoints;
 
}

void SimpleGraph::DepthFirstSearch(vector<int>& stack, Riss::MarkArray& visited, vector<int>& articulationspoints){

int nod = stack.back();
const vector<int>& adj = getAdjacency(nod);
int tmpstacksize = stack.size();
bool artic;

  for(int i=0; i<adj.size(); ++i){
   
    if(visited.isCurrentStep(adj[i])) continue;
    
    stack.push_back(adj[i]);
    visited.setCurrentStep(adj[i]);
    DepthFirstSearch(stack, visited, articulationspoints);
    
    artic = true;
    
    for(int j=stack.size()-1; j >= tmpstacksize; --j){
      
     for(int k =0; k < tmpstacksize-1;k++){
        if(thereisanedge(stack[k], stack[j])) artic = false;
    }
     if (artic){ 
       articulationspoints.push_back(stack[tmpstacksize-1]);
       }
     stack.pop_back();
    }
  }
  
}


bool SimpleGraph::thereisanedge(int nodeA, int nodeB){ //TODO: Maybe check if Graph is finalized (+backedges added) [also in other methods]

  
 const adjacencyList& adj = node[nodeA];
 
 if(adj.size() < 64){ //linear search, faster von size < 64(?) TODO: Find bordervalue(binarysearch faster then linearsearch)
  
  for(int i=0; i<adj.size(); ++i){
  
    if(adj[i].first == nodeB) return true;
    
  }
 }
 
 else{//binary search
 int left = 0;
 int right = adj.size()-1;
 int middle;
 
 while(left <= right){
 
   middle = (left+right)/2;
   
   if(adj[middle].first==nodeB) return true;
   
   if(adj[middle].first > nodeB) right = middle-1;
   else left = middle+1;
   
  }
 }
 
 
  return false;
  
}

int SimpleGraph::gettreewidth(){

  bool check = false;
  int k = 0;
  vector<int> empty_vec, nodes;
  
  for(int a=0; a<node.size(); ++a) nodes.push_back(a);
  
  //int tw = recursive_treewidth(empty_vec, nodes);
  
  while(!check){
 
    k++;
    check = improved_recursive_treewidth(k);
    
  }
  
  return k;
  
  
 // return tw;
}

bool SimpleGraph::improved_recursive_treewidth(int k){
  
  bool check, tbool;
  vector<int> nodes;
  vector<int> empty_vec;
  
  
  for(int a=0; a<node.size(); ++a) nodes.push_back(a);
  
   if(node.size() <= k+1) return true;
   
   
   if((k<= 0.25*node.size()) | (k>= 0.4203*node.size())){
     
     vector<vector<int>> sets  = getSets(nodes, k+1);
     for(int i=0; i<sets.size(); ++i){
       
     vector<vector<int>> components = getConnectedComponents(sets[i]);
     
     check = true;
     
       for(int j=0;j<components.size();++j){
		 
	 if(components[j].size() > ((node.size() - sets[i].size() +1)/2)) check = false;
	 
      }
      if(check){ 
      tbool = true;
      for(int n=0;n<components.size();++n){
    
	tbool = (tbool | (recursive_treewidth(empty_vec,components[n]) <= k));
	
      }
      if(tbool) return true;
      }
    }
  }
  else{
  
    vector<vector<int>> sets  = getSets(nodes, 0.4203*node.size());
    
    for(int i=0; i<sets.size(); ++i){
     vector<vector<int>> components = getConnectedComponents(sets[i]);
     check = true;
       for(int j=0;j<components.size();++j){
       
	 if(components[j].size() > (node.size() - sets[i].size() +1)/2) check = false;
	 
      }
      
      if(check){
      
	SimpleGraph *Graph_plus = new SimpleGraph(sets[i].size(), true);
	
	tbool = Graph_plus->improved_recursive_treewidth(k);
	
	vector<vector<int>> components = getConnectedComponents(sets[i]);
	
	for(int j=0; j<components.size(); ++j){
	
	 tbool = (tbool || (recursive_treewidth(empty_vec,components[j]) <= k));
	}
	
	if(tbool) return true;
	
      }
    
  }
 }
  
  return false;
}

int SimpleGraph::recursive_treewidth(const vector<int>& Lset,const vector<int>& component){

  vector<int> newcomponent, newLset;
  newLset = Lset;
  bool toadd, addtonewLset;
  int tmp;
  Riss::MarkArray visited;
  visited.create(node.size());
  visited.nextStep();
  
  if(component.size() == 1){
    
    int Q = 0;
    bool wbool, vbool;
    
     for(int i = 0 ; i< node.size(); ++i){
     toadd = true;
     
      if(i == component[0]) continue;
      for(int j =0; j< Lset.size(); ++j){
	if(i == Lset[j]) toadd = false; 
      }
      if(toadd) newcomponent.push_back(i);
       }
      
       vector<vector<int>> G = getConnectedComponents(newcomponent);
       
       for(int w=0; w<newcomponent.size();++w){
       
	 if(visited.isCurrentStep(newcomponent[w])) continue;
	 
	 const adjacencyList& adj = node[newcomponent[w]];
	 
	 for(int x =0; x < G.size(); ++x){
	   
	   if(visited.isCurrentStep(newcomponent[w])) break;
	   
	     wbool =false;
	     vbool =false;
	    for(int y =0; y< G[x].size(); ++y){
	    if(component[0] == G[x][y]) vbool = true; 
	    }
	    if(vbool){
	    for(int y =0; y< G[x].size(); ++y){
	      for(int k =0; k< adj.size(); ++k){
	    if(adj[k].first == G[x][y]) wbool = true;
	      }
	    }
	    if(wbool) Q++; visited.setCurrentStep(newcomponent[w]);
	   }
	   
	   
	}
	 
      }
      
      return Q;
      
   }
   
   int Opt = numeric_limits<int>::max();
   
   vector<vector<int>> sets  = getSets(component, component.size()/2);
   
   for(int k =0; k< sets.size(); ++k){
   
     int v1 = recursive_treewidth(Lset ,sets[k]);
     tmp = v1;
     
     for(int i = 0 ; i< component.size(); ++i){
     toadd = true;
      for(int j =0; j< sets[k].size(); ++j){
	addtonewLset = true;
	for(int m=0; m< newLset.size();++m){
	  if(newLset[m] == sets[k][j]) addtonewLset = false;   
	}
	if(addtonewLset) newLset.push_back(sets[k][j]);
	
	if(component[i] == sets[k][j]) toadd = false;  
        	
      }
      if(toadd) newcomponent.push_back(component[i]);
       }
     
     int v2 = recursive_treewidth(newLset, newcomponent);
     
     newLset = Lset;
     newcomponent.clear();
     
     if(v2 > v1) tmp = v2;
     if(tmp < Opt) Opt = tmp;
     
  }
  
  return Opt;
  
}

void SimpleGraph::compute_G_plus(SimpleGraph*& Graph_plus ,const vector<int>& set){

  vector<vector<int>> connected = getConnectedComponents(set);
	
	int node1, node2;
	bool nod1, nod2;
	
	
	for(int j = 0; j < set.size(); ++j){
	  node1 = set[j];
	  const adjacencyList& adj1 =node[node1];
	  for(int k = set.size() -1; k > j; ++k){
	    
	    node2 = set[k];
	    const adjacencyList& adj2 =node[node2];
	    
	    for(int l =0; l < connected.size(); ++l){
	      nod1 = false;
	      nod2 = false;
	     for(int m=0; m < connected[l].size(); ++m){
	       for(int n=0; n < adj1.size(); ++n) if(m == adj1[n].first) nod1 = true;
	       for(int n=0; n < adj2.size(); ++n) if(m == adj2[n].first) nod2 = true;
	       if(nod1 && nod2) Graph_plus->addUndirectedEdge(j,k);
	       
	    }
	      
	    }
	    
	  }
	  
	}
  
}

vector<vector<int>> SimpleGraph::getConnectedComponents(const vector<int>& set){ //connected components without set

Riss::MarkArray visited;
visited.create(node.size());
visited.nextStep();
vector<int> componentnodes;
vector<vector<int>> components;

for(int a=0; a<set.size(); a++) visited.setCurrentStep(set[a]); //dont look at nodes, that are in the set 

for(int k=0; k<node.size(); k++){

if(visited.isCurrentStep(k)) continue;  

componentnodes.push_back(k);
visited.setCurrentStep(k);

for(int i=0; i<componentnodes.size(); ++i){

 const adjacencyList& adj=node[componentnodes[i]];
  
  
  for(int j=0; j<adj.size();++j){
    
        if(visited.isCurrentStep(adj[j].first)) continue;    
        componentnodes.push_back(adj[j].first);
	visited.setCurrentStep(adj[j].first);
    
  }
  
}

components.push_back(componentnodes);
componentnodes.clear();

}

return components;

}

vector<vector<int>> SimpleGraph::getSets(const vector<int>& nodes, int k){
  vector<vector<int>> sets;
  vector<int> set;
  
  for(int i=0;i<nodes.size()-(k-1);i++){
  
    set.push_back(nodes[i]);
    
    if(k==1) sets.push_back(set);
    else{
    for(int j = i+1; j<=nodes.size()-(k-1);++j){
     
    getallcombinations(nodes ,set, sets, k-2, j);
    }
    }
    set.pop_back();
  }
  
  return sets;
}

void SimpleGraph::getallcombinations(const vector<int>& nodes,vector<int> set,vector<vector<int>>& sets, int k, int position){
  
    set.push_back(nodes[position]);
    
    if(k>0){
      
    for(int i=1;nodes.size()-position-k>=i;++i){
      
    getallcombinations(nodes, set, sets, k-1, position+i);
    }
   }
   else sets.push_back(set);
  
}

double SimpleGraph::getPageRank(int node){
  
double pgrnk;  

if(pagerank[node] == 0){

 for(int i=0; i<10; ++i) doPageRank(); //ten iterationsteps
  
}

pgrnk = pagerank[node];

return pgrnk;  
  
}

void SimpleGraph::doPageRank(){

  double d = 0.85;
  int neighbors_neighbor;
 
  double pgrnk, pgrnk_neighbor;
  
  for(int i =0; i < pagerank.size(); ++i) pagerank[i] = 1; //setting each pagerank to 1 (= startvalue)
  
  
  for(int i =0; i < pagerank.size(); ++i){
  
    pgrnk = 0;
    
    for(int j=0; j<node[i].size(); ++j){
      
     pgrnk_neighbor = pagerank[node[i][j].first];
     neighbors_neighbor = nodeDeg[node[i][j].first];
      pgrnk += (pgrnk_neighbor/neighbors_neighbor);
    }
    
     pagerank[i] = pgrnk*d + (1-d);
     
  }
  
}

void SimpleGraph::getCommunities(double precision){

  Community c(this);
    double modularity;
			cerr << "Computing COMMUNITY Structure (VIG)" << endl;
	
		
		modularity = c.compute_modularity_GFA(precision);
		c.compute_communities();

		
			cerr << "modularity = " << modularity << endl;
			cerr << "communities = " << (int)c.ncomm << endl;
			cerr << "largest size = " << (double)c.Comm[c.Comm_order[0].first].size()/getSize() << endl;
			cerr << "iterations = " << c.iterations << endl;
			cerr << "------------" << endl;
  
}

vector<vector<int>> SimpleGraph::getCommunityForEachNode(double prec){
 Community c(this);
 vector<vector<int>> comm;					
		c.compute_modularity_GFA(prec);
		c.compute_communities();
		
		comm = c.Comm;
		comm.resize(c.ncomm);
		
		return comm;	
  
}

void SimpleGraph::getDimension(){

   int minx = 0;
    int maxx2 = 6;
    vector<int> needed;
    vector <pair <double,double> > v1;
    vector <pair <double,double> > v2;
    pair <double,double> polreg = make_pair(-1,-1);
    pair <double,double> expreg = make_pair(-1,-1);
    
			cerr << "Computing SELF-SIMILAR Structure (VIG)" << endl;
			
                needed = computeNeeded(this);
		
		for(int i=1; i<needed.size(); i++){
			if(i>=minx && i<=maxx2){
				v1.push_back(pair<double,double>(log(i), log(needed[i])));
				v2.push_back(pair<double,double>((double)i, log(needed[i])));	
			}
		}
		
		polreg = regresion(v1);
		expreg = regresion(v2);
		
		
	    		cerr << "dimension = " << -polreg.first << endl;
			cerr << "decay = " << -expreg.first << endl;
			cerr << "------------" << endl;
  
}