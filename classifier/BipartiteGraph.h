/*
 * BipartiteGraph.h
 *
 *  Created on: Oct 22, 2013
 *      Author: gardero
 */
#include <vector>
#include "SequenceStatistics.h"
#include "Graph.h"
#ifndef BIPARTITEGRAPH_H_
#define BIPARTITEGRAPH_H_

/**
 * class representing a directed bipartite (Black and White nodes) graph.
 * Black nodes keep a counter to how many white nodes they are related.
 * White nodes keep the list of black nodes to which they are related.
 */
class BipartiteGraph {
private:
	int sizeB, sizeW;
	std::vector<int> nodeB;
	std::vector< vector<int> > nodeW;


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

	int getBCount(int indexB) const{
		return nodeB[indexB];
	}

	const vector<int>& getAjacencyW(int indexW) const {
		return nodeW[indexW];
	}


};

#endif /* BIPARTITEGRAPH_H_ */
