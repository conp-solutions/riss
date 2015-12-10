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
#include <algorithm>
#include "riss/mtl/Sort.h"
#include "riss/core/SolverTypes.h"

using namespace std;

Graph::Graph(int size, bool computingDerivative) :
    node(size),
    nodeDeg(size, 0),
    degreeStatistics(computingDerivative),
    weightStatistics(computingDerivative, false), exzentricity(computingDerivative)
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
}

Graph::Graph(int size, bool mergeAtTheEnd, bool computingDerivative) :
    node(size),
    nodeDeg(size, 0),
    degreeStatistics(computingDerivative),
    weightStatistics(computingDerivative, false), exzentricity(computingDerivative)
{
    // TODO Auto-generated constructor stub
    this->size = size;
    this->mergeAtTheEnd = mergeAtTheEnd;
    addedDirectedEdge = false;
    addedUndirectedEdge = false;
    intermediateSort = true;
    intermediateSorts = 0;
    sortSize.resize(size, 4);      // re-sort after 16 elements
    for (int i = 0 ; i < size; ++ i) { node[i].reserve(8); }   // get space for the first 8 elements
}

Graph::~Graph()
{
    // TODO Auto-generated destructor stub
//   cerr << "c intermediate sorts: " << intermediateSorts << endl;
}

void Graph::finalizeGraph() {
     for(int i = 0; i < node.size(); i++){
    sortAdjacencyList(node[i]);                    //finalizeGraph
    nodeDeg[i] = node[i].size();
   }
}

void Graph::setIntermediateSort(bool newValue)
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

int Graph::getDegree(int node)
{
    return this->nodeDeg[node];
}

void Graph::addUndirectedEdge(int nodeA, int nodeB)
{
    addUndirectedEdge(nodeA, nodeB, 1);
}

void Graph::addUndirectedEdge(int nodeA, int nodeB, double aweight)
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
    if (intermediateSort && node[nodeA].size() > sortSize[ nodeA ]) {
        sortAdjacencyList(node[nodeA]);
        sortSize[ nodeA ] *= 2;
	nodeDeg[nodeA] = node[nodeA].size();
    }
}

void Graph::addDirectedEdge(int nodeA, int nodeB, double aweight)
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
    if (intermediateSort && node[nodeA].size() > sortSize[ nodeA ]) {
        sortAdjacencyList(node[nodeA]);
        sortSize[ nodeA ] *= 2;
	nodeDeg[nodeA] = node[nodeA].size();
    }
}

uint64_t Graph::addAndCountUndirectedEdge(int nodeA, int nodeB, double weight)
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

uint64_t Graph::computeStatistics(int quantilesCount)
{
    uint64_t operations = 0;
    if (!mergeAtTheEnd) {
        operations += computeOnlyStatistics(quantilesCount);
    } else {
        operations += computeNmergeStatistics(quantilesCount);
    }
    return operations;
}

uint64_t Graph::computeOnlyStatistics(int quantilesCount)
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

uint64_t Graph::computeNmergeStatistics(int quantilesCount)
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

uint64_t Graph::sortAdjacencyList(adjacencyList& aList)
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

vector<int> Graph::getAdjacency(int adjnode)
{
 adjacencyList& adj = node[adjnode];
 vector<int> nodes;
 
 for(int i = 0; i < adj.size(); i++){
   edge edg = adj[i];
   nodes.push_back(edg.first);
 
  }
  
  return nodes;
}

void Graph::completeSingleVIG(){
     
   for(int j = size-2; j >= 0; j--){ //you dont have to check the last node
     adjacencyList& adj = node[j];
     for(int k = 0; k < adj.size(); k++) addDirectedEdge(adj[k].first, j, adj[k].second);
     }
     
     finalizeGraph(); //make sure that the graph is sorted
    
}

vector<double> Graph::getDistances(int nod){

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

double Graph::getDiameter(){ //TODO: Elias: Maybe there are faster algorithms to find just the diameter/radius(without using the exzentricity of each node)

  double ex;
  double diameter = 0;
  
for(int i=0; i<size;++i){ //getting the biggest exzentricity (=diameter)
  ex = getExzentricity(i);
  if(ex > diameter) diameter = ex;
}  
return diameter;  
}

double Graph::getRadius(){

  double radius = numeric_limits<double>::infinity();
  double ex;
for(int i=0; i<size;++i){
  ex = getExzentricity(i);
  if(ex == 0) continue; //TODO: Elias: whats happening with nodes with no edges ? getExzentricity = 0 or inf ? 
  if(ex < radius) radius = ex;
}  
return radius;  
}

double Graph::getExzentricity(int nod){

 const vector<double>& distance = getDistances(nod);
 double max =0;
  
  for(int j=0; j<distance.size();++j){
    if(distance[j] == numeric_limits<double>::infinity()) continue;
    if(distance[j] > max) max = distance[j];
  }
   
  return max; 
  
}

SequenceStatistics Graph::getExzentricityStatistics(){
   
   for(int i=0; i< size; ++i) exzentricity.addValue(getExzentricity(i));
  
   return exzentricity;
}

vector<int> Graph::getArticulationPoints(){
  
  vector<int> articulationpoints;
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
  
  if(tmp>1) articulationpoints.push_back(rootnode);

  return articulationpoints;
 
}

void Graph::DepthFirstSearch(vector<int>& stack, Riss::MarkArray& visited, vector<int>& articulationspoints){

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
     if (artic) articulationspoints.push_back(stack[tmpstacksize-1]);
     stack.pop_back();
    }
  }
  
}


bool Graph::thereisanedge(int nodeA, int nodeB){ //TODO: Maybe check if Graph is finalized (+backedges added) [also in other methods]

  
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

int Graph::gettreewidth(){

  bool check = false;
  int k = 0;
  
  while(!check){
  
    k++;
    check = improved_recursive_treewidth(k);
    
  }
  
  return k;
  
}

bool Graph::improved_recursive_treewidth(int k){
   if(node.size() <= k+1) return true;
  else if(k<= 0.25*node.size() || k>= 0.4203*node.size()){
     vector<vector<int>> sets  = getSets(k+1);
     for(int i=0; i<sets.size(); ++i){
     
       
       
    }
  }
  
  return false;
}

vector<vector<int>> Graph::getConnectedComponents(const vector<int>& set){ //connected components without set

Riss::MarkArray visited;
visited.create(node.size());
visited.nextStep();
vector<int> componentnodes;
vector<vector<int>> components;

for(int a=0; a<set.size(); a++) visited.setCurrentStep(set[a]); //dont look at nodes, that are in the set

for(int k=0; k<visited.size(); k++){

if(visited.isCurrentStep(k)) continue;  

componentnodes.push_back(set[k]);
visited.setCurrentStep(set[k]);

for(int i=0; i<componentnodes.size(); ++i){

 const adjacencyList& adj=node[componentnodes[i]];
  
  
  for(int j=0; j<adj.size();++j){
    
        if(visited.isCurrentStep(adj[i].first)) continue;    
        componentnodes.push_back(adj[j].first);
	visited.setCurrentStep(adj[i].first);
    
  }
  
}

components.push_back(componentnodes);
componentnodes.clear();

}

return components;

}

bool Graph::containingNumberofVertices(vector<int>){
  
  return;
}

vector<vector<int>> Graph::getSets(int k){
  vector<vector<int>> sets;
  vector<int> set;
  
  for(int i=0;i<node.size()-k;i++){
    set.push_back(node[i].first);
    k--;
    for(int j=1; i+j<=node.size()-k;++j){
    getallcombinations(set, sets, k-1, i+j);
    }
  }
  
  return sets;
}

void Graph::getallcombinations(vector<int> set,vector<vector<int>>& sets, int k, int position){
  
    set.push_back(node[position].first);
    
    if(k>0){
    for(int i=1;node.size()-position-k>=i;++i){
    getallcombinations(set, sets, k-1, position+i);
    }
   }
   else sets.push_back(set);
  
}


/*
int Graph::gettreewidth(){ 
    
  vector< pair< vector<int>, vector<int> > > bags;
  Riss::MarkArray visited;
  visited.create(size);
  
  vector<int> bagsizes;
  int tmpbagsize;
  int treewidth = numeric_limits<int>::max();
  
  for(int i=0; i< node.size(); ++i){
    
    if(node[i].empty()) continue;
    
    visited.nextStep();
    visited.setCurrentStep(i);
    bags.clear();
    bags.push_back(pair<vector<int>,vector<int>>());
    bags.back().first.push_back(i);
    findtree(bags, visited);    
    
    
    tmpbagsize = 0;
    for(int k=0; k<bags.size(); k++){
      
      if(tmpbagsize < bags[k].first.size()) tmpbagsize = bags[k].first.size();
    
    }
    
    if(tmpbagsize <= 2) return 1;
    
    bagsizes.push_back(tmpbagsize);
    
  }
  
  for(int j=0; j<bagsizes.size(); ++j){
    
    if(treewidth > bagsizes[j]) treewidth = bagsizes[j];
    
  }
  
  return treewidth-1;
  
}

void Graph::findtree(vector< pair< vector<int>, vector<int> > >& bags, Riss::MarkArray& visited){
  
  int actualnode = bags.back().first.back();
  const adjacencyList& adj = node[actualnode];
  
  for(int i=0; i<adj.size(); ++i){
  
    controllbagsandadd(bags, visited);
    
    if(visited.isCurrentStep(adj[i].first)) continue;
    
    visited.setCurrentStep(adj[i].first);
    bags.back().first.push_back(adj[i].first);
    
    findtree(bags, visited);
    }
  
}

void Graph::controllbagsandadd(vector< pair< vector<int>, vector<int> > >& bags, Riss::MarkArray& visited){

  pair<vector<int>,vector<int>> bagtocontroll;
  
  bool stayinbag;
  
  for(int i = 0; i< bags.back().first.size(); ++i){
  
    stayinbag = false;
    const adjacencyList& adj = node[bags.back().first[i]];
    
    for(int j=0; j<adj.size(); ++j){
    
      if(!visited.isCurrentStep(adj[j].first)) stayinbag = true; continue; 
      
    }
    
    if(stayinbag) bagtocontroll.first.push_back(bags.back().first[i]);
    
  }
  
  bags.push_back(bagtocontroll);
  
   
}
*/