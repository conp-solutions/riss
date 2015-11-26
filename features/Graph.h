/*
 * Graph.h
 *
 *  Created on: Oct 21, 2013
 *      Author: gardero
 */
#ifndef RISS_GRAPH_H_
#define RISS_GRAPH_H_

#include <vector>
#include "classifier/SequenceStatistics.h"
#include "riss/core/SolverTypes.h"

// using namespace std;

typedef std::pair<int, double> edge;
typedef std::vector<edge> adjacencyList;

/**
 * undirected weighted graph.
 */
class Graph
{
  private:
    int size;
    std::vector<adjacencyList> node;
    std::vector<int> nodeDeg;
    // statistics variables
    SequenceStatistics exzentricity;
    SequenceStatistics degreeStatistics;
    SequenceStatistics weightStatistics;
    bool mergeAtTheEnd; // do not detect duplicate entries during the creation of the graph
    bool intermediateSort;  // remove duplicates in adjacency lists already during the algorithm execution
    int intermediateSorts; // statistics
    bool addedDirectedEdge, addedUndirectedEdge; // bools to make sure in one graph not two different edge types have been added
    std::vector<double> getDistances(int nod);
    /** sort the adjacencyList and remove duplicate entries */
    uint64_t sortAdjacencyList(adjacencyList& aList);

  public:
    void DepthFirstSearch(std::vector<int>& stack, Riss::MarkArray& visited, std::vector<int>& articulationspoints);
    bool thereisanedge(int nodeA, int nodeB);
    std::vector<int> getArticulationPoints();
    SequenceStatistics getExzentricityStatistics();
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

    const SequenceStatistics& getDegreeStatistics() const
    {
        return degreeStatistics;
    }

    const SequenceStatistics& getWeightStatistics() const
    {
        return weightStatistics;
    }
};

#endif /* GRAPH_H_ */
