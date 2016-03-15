/*
 * BipartiteGraph.h
 *
 *  Created on: Oct 22, 2013
 *      Author: gardero
 */
#ifndef RISS_BIPARTITEGRAPH_H_
#define RISS_BIPARTITEGRAPH_H_

#include <vector>
#include "classifier/SequenceStatistics.h"
#include "Graph.h"

/**
 * class representing a directed bipartite (Black and White nodes) graph.
 * Black nodes keep a counter to how many and to which white nodes they are related.
 * White nodes keep the list of black nodes to which they are related.
 */
class BipartiteGraph
{
  private:
    int sizeB, sizeW;
    std::vector<int> nodeB;
    std::vector< std::vector<int> > nodeW, nodeBAdj;


  public:
    BipartiteGraph(int sizeB, int sizeW, bool computingDerivative);
    virtual ~BipartiteGraph();
    void addEdgeFromB(int nodeB, int nodeW);
    void addEdgeFromW(int nodeW, int nodeB);
    void addEdge(int nodeW, int nodeB);

    /**
     * 100*positiveCount/degreeOfNode
     */
    int getPositiveDegreeFromB(int nodeB);
    int getPositiveDegreeFromW(int nodeB);
    uint64_t computeStatistics(int quantilesCount);

    int getBCount(int indexB) const
    {
        return nodeB[indexB];
    }

    const std::vector<int>& getAjacencyW(int indexW) const
    {
        return nodeW[indexW];
    }
    
    
    const std::vector<int>& getAjacencyB(int indexW) const
    {
        return nodeBAdj[indexW];
    }


};

/* BIPARTITEGRAPH_H_ */
#endif
