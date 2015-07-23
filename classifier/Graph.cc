/*
 * Graph.cpp
 *
 *  Created on: Oct 21, 2013
 *      Author: gardero
 */

#include "classifier/Graph.h"
#include <math.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "riss/mtl/Sort.h"

using namespace std;

Graph::Graph(int size, bool computingDerivative) :
    node(size),
    nodeDeg(size, 0),
    degreeStatistics(computingDerivative),
    weightStatistics(computingDerivative, false)
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
    weightStatistics(computingDerivative, false)
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
    uint64_t operations = 0;
    sort(aList.begin(), aList.end(), nodesComparator);
    operations += aList.size() * log(aList.size());
    int kept = 0;
    for (int j = 0; j < aList.size(); ++j) {
        if (aList[j].first != aList[kept].first) { aList[kept++] = aList[j]; }   // keep the next element
        else { aList[kept].second += aList[j].second; } // otherwise add the weights!
    }
    operations += aList.size();
    aList.resize(kept);   // save space!

    intermediateSorts ++;
    
    return operations;
}
