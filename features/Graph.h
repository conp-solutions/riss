/*
 * Graph.h
 *
 *  Created on: Oct 21, 2013
 *      Author: gardero
 */
#ifndef RISS_GRAPH_H_
#define RISS_GRAPH_H_
#include <iterator>
#include <iostream>
#include <vector>
#include "classifier/SequenceStatistics.h"
#include "riss/core/SolverTypes.h"
#include <set>

//using namespace std;

typedef std::pair<int, double> edge;
typedef std::vector<edge> adjacencyList;

/**
 * undirected weighted graph.
 */
class Graph
{
  private:
    void doPageRank();
    int recursive_treewidth(const std::vector<int>& Lset, const std::vector<int>& component);
    void compute_G_plus(Graph*& Graph_plus, const std::vector<int>& set);
    std::vector<std::vector<int>> getConnectedComponents(const std::vector<int>& set);
    void getallcombinations(const std::vector<int>& nodes,std::vector<int> set,std::vector<std::vector<int>>& sets, int k, int position);
    bool improved_recursive_treewidth(int k);
    void findtree(std::vector<std::pair<std::vector<int>, std::vector<int>>>& bags, Riss::MarkArray& visited);
    std::vector<std::vector<int>> getSets(const std::vector<int>& nodes, int k);
    void controllbagsandadd(std::vector<std::pair<std::vector<int>, std::vector<int>>>& bags, Riss::MarkArray& visited);
    int size;
    std::vector<adjacencyList> node;
    std::vector<int> nodeDeg;
    // statistics variables
    SequenceStatistics articulationpointsStatistics;
    SequenceStatistics pagerankStatistics;
    SequenceStatistics exzentricityStatistics;
    SequenceStatistics degreeStatistics;
    SequenceStatistics weightStatistics;
    bool mergeAtTheEnd; // do not detect duplicate entries during the creation of the graph
    bool intermediateSort;  // remove duplicates in adjacency lists already during the algorithm execution
    int intermediateSorts; // statistics
    bool addedDirectedEdge, addedUndirectedEdge; // bools to make sure in one graph not two different edge types have been added
    std::vector<double> getDistances(int nod);
    /** sort the adjacencyList and remove duplicate entries */
    uint64_t sortAdjacencyList(adjacencyList& aList);
   
    std::vector<double> pagerank;
    std::vector<int> articulationpoints;
    std::vector <double> narity;     

  public:
    double getWeight(int nodeA, int nodeB);
    double arity(int x);
    double arity();
    double getPageRank(int node);
    int gettreewidth();
    void DepthFirstSearch(std::vector<int>& stack, Riss::MarkArray& visited, std::vector<int>& articulationspoints);
    bool thereisanedge(int nodeA, int nodeB);
    std::vector<int> getArticulationPoints();
    double getExzentricity(int nod);
    double getRadius();
    double getDiameter();
    void completeSingleVIG();
    std::vector<int> getAdjacency(int adjnode); 
    Graph(int nodes, bool computingDerivative);
    Graph(int nodes, bool merge, bool computingDerivative);
    virtual ~Graph();
    void addUndirectedEdge(int nodeA, int nodeB);
    void addUndirectedEdge(int nodeA, int nodeB, double weight);
    void addDirectedEdge(int nodeA, int nodeB, double weight);
    uint64_t addAndCountUndirectedEdge(int nodeA, int nodeB, double weight);
    int getDegree(int node);

    std::vector<int> sortSize;  // for each adjacencyList store the size when its re-sorted
    void setIntermediateSort(bool newValue);

    uint64_t computeStatistics(int quantilesCount);
    uint64_t computeNmergeStatistics(int quantilesCount);
    uint64_t computeOnlyStatistics(int quantilesCount);

    /** finalize adjacency lists after all nodes have been added to the graph */
    void finalizeGraph();

    int getSize(){return size;}
    
    const SequenceStatistics& getDegreeStatistics() const
    {
        return degreeStatistics;
    }

    const SequenceStatistics& getWeightStatistics() const
    {
        return weightStatistics;
    }
    
    const SequenceStatistics& getArticulationpointsStatistics() const
    {
        return articulationpointsStatistics;
    }
    
    const SequenceStatistics& getPagerankStatistics() const
    {
        return pagerankStatistics;
    }
    
    const SequenceStatistics& getExzentricityStatistics() const
    {
       return exzentricityStatistics;
    }
    
    
   //--------------- ITERATOR ON EDGES --------------------------------------------------

	typedef struct {int orig; int dest; double weight;} edgeNewDef; 
    
  class EdgeIter;                          // Iterator to traverse all EDGES of the Graph
  friend class EdgeIter;
	
  class EdgeIter : public std::iterator<std::input_iterator_tag, edgeNewDef> {
		Graph &g;
	        std::vector<int>::iterator it;
		int node;
		Graph::edgeNewDef e;

	public:
                
		EdgeIter(Graph &x) : g(x){}
		EdgeIter(Graph &x, std::vector<int>::iterator y, int n): g(x), it(y), node(n) {
		  };

		EdgeIter (const EdgeIter &x) : g(x.g), it(x.it), node(x.node) {}

		EdgeIter& operator++() {
			assert(node >= 0 && node < g.size);
			assert(node < g.size-1 || it != g.getAdjacency(node).end()); 
			do {
				if (++it == g.getAdjacency(node).end() && node < g.size-1) {
					do {
						node++; 
						it = g.getAdjacency(node).begin(); 
						
					}
					while (it == g.getAdjacency(node).end() && node < g.size-1); //skip nodes without neighs
				}
			}
			while (!(it == g.getAdjacency(node).end() && node == g.size-1)   //skip when orig > dest
				&& node > *it);
			return *this;
		}
		EdgeIter& operator++(int) {
			EdgeIter clone(*this);
			assert(node >= 0 && node < g.size);
			assert(node < g.size-1 || it != g.getAdjacency(node).end()); 
			do {
				if (++it == g.getAdjacency(node).end() && node < g.size-1) {
					do {
						node++; 
						it = g.getAdjacency(node).begin(); 
					}
					while (it == g.getAdjacency(node).end() && node < g.size-1); //skip nodes without neighs
				}
			}
			while (!(it == g.getAdjacency(node).end() && node == g.size-1)   //skip when orig > dest
				&& node > *it);
			return *this;
		}

		bool operator==(const EdgeIter &rhs) {return it==rhs.it;}
		bool operator!=(const EdgeIter &rhs) {return it!=rhs.it;}

		Graph::edgeNewDef& operator*() {
			std::cerr<<"ENTER *\n";
			e.orig = node; 
			e.dest = *it; 
			e.weight = g.getWeight(node, *it); 
			return e;
		}
		Graph::edgeNewDef *operator->() {
		 
			e.orig = node; 
			e.dest = *it; 
			std::cerr <<*it<<std::endl;
			std::cerr <<e.orig<<std::endl;
			e.weight = g.getWeight(node, *it); 
			 std::cerr <<e.weight<<std::endl;
			return &e;
		}
	};

	EdgeIter begin() {
		int node = 0;
		while (getAdjacency(node).size()==0 && node < size) node++;
		
		return EdgeIter(*this, getAdjacency(node).begin(), node);  
	// First neight of first node
	}

	EdgeIter end() {
		return EdgeIter(*this, getAdjacency(size-1).end(), size);  
	// Last neight of last node
	}
    
    //--------------- ITERATOR ON NEIGHBORS --------------------------------------------------

	class NeighIter : public std::vector<int>::iterator {
	        Graph &g;
		std::vector<int>::iterator it;
		Graph::edgeNewDef e;
		int nod;
		
	public:
               
	    	NeighIter(std::vector<int>::iterator x, int node, Graph &y) : g(y), it(x){
		  nod = node;
		}
		
		Graph::edgeNewDef *operator->() {
			e.dest = *it; 
			e.weight = g.getWeight(nod, *it); 
			return &e;
		}
		NeighIter& operator++() {
			++it;
			return *this;
		}
		NeighIter& operator++(int) {
			it++;
			return *this;
		}
		bool operator==(const NeighIter& x) {return it==x.it;}
		bool operator!=(const NeighIter& x) {return it!=x.it;}
	};

	NeighIter begin(int x) { 
	        
		assert(x>=0 && x<= size-1); 
		return NeighIter(getAdjacency(x).begin(), x, *this); 
	}

	NeighIter end(int x) {
	  
		assert(x>=0 && x<= size-1); 
		return NeighIter(getAdjacency(x).end(), x, *this); 
	}

    
};

#endif /* GRAPH_H_ */
