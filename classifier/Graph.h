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
    SequenceStatistics degreeStatistics;
    SequenceStatistics weightStatistics;
    bool mergeAtTheEnd; // do not detect duplicate entries during the creation of the graph
    bool intermediateSort;  // remove duplicates in adjacency lists already during the algorithm execution
    int intermediateSorts; // statistics
    bool addedDirectedEdge, addedUndirectedEdge; // bools to make sure in one graph not two different edge types have been added

    /** sort the adjacencyList and remove duplicate entries */
    uint64_t sortAdjacencyList(adjacencyList& aList);

  public:
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
