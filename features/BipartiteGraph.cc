/*
 * BipartiteGraph.cc
 *
 *  Created on: Oct 22, 2013
 *      Author: gardero
 */

#include "classifier/BipartiteGraph.h"

BipartiteGraph::BipartiteGraph(int sizeB, int sizeW, bool computingDerivative):
    nodeB(sizeB, 0), nodeW(sizeW)
{
    this->sizeB = sizeB;
    this->sizeW = sizeW;
}

BipartiteGraph::~BipartiteGraph()
{
    // TODO Auto-generated destructor stub
}

void BipartiteGraph::addEdgeFromB(int nodeBidx, int nodeWidx)
{
    nodeB[nodeBidx]++;
}

void BipartiteGraph::addEdgeFromW(int nodeWidx, int nodeBidx)
{
    nodeW[nodeWidx].push_back(nodeBidx);
}

void BipartiteGraph::addEdge(int nodeWidx, int nodeBidx)
{
    addEdgeFromW(nodeWidx, nodeBidx);
    addEdgeFromB(nodeBidx, nodeWidx);
}

