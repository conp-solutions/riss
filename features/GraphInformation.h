//GraphFeatures to compute 

#include "Graph.h"
#include <vector>

class GraphInformation{

private:
bool radius, diameter, treewidth, articulationpoints, exzentricity, pagerank, communitysize, communityneighbors, communitybridges, dimensions, degree, weight;  
Graph* graph;
int quantilesCount;
std::vector<std::string> featuresNames;


public:

GraphInformation(Graph* g, int q){
  
  graph=g;
  quantilesCount = q;
  radius = false;
  diameter = false;
  treewidth = false;
  articulationpoints = false;
  exzentricity = false;
  pagerank = false;
  communitybridges = false;
  communityneighbors = false;
  communitysize = false;
  dimensions = false;
  degree = false;
  weight = false;
};  

std::vector<double> getFeatures(){

  std::vector<double> ret;
  featuresNames.clear();
  
 if(exzentricity){
  
    graph->computeExzentricityStatistics(quantilesCount);
    graph->getExzentricityStatistics().infoToVector("variables exzentricity", featuresNames, ret);
    }
    if(pagerank){
     
    graph->computePagerankStatistics(quantilesCount);
    graph->getPagerankStatistics().infoToVector("variables pagerank", featuresNames, ret);
    }
    if(articulationpoints){
      
    graph->computeArticulationpointsStatistics(quantilesCount);
    graph->getArticulationpointsStatistics().infoToVector("graph articulationpoints", featuresNames, ret);
    }
     if(communitysize || communitybridges || communityneighbors){
    graph->computeCommunityStatistics(quantilesCount);
   if(communitysize) graph->getCommunitySizeStatistics().infoToVector("graph communities size", featuresNames, ret);
   if(communityneighbors) graph->getCommunityNeighborStatistics().infoToVector("graph communities neighbors", featuresNames, ret);
   if(communitybridges) graph->getCommunityBridgeStatistics().infoToVector("graph communities bridgevariables", featuresNames, ret);
     }
    if(radius){ 
    ret.push_back(graph->getRadius());
    featuresNames.push_back("graphradius");
    }
    if(diameter){
    ret.push_back(graph->getDiameter());
    featuresNames.push_back("graphdiameter");
    }
    if(treewidth){
    ret.push_back(graph->gettreewidth());
    featuresNames.push_back("graphtreewidth");
    }
    if(degree){
    graph->getDegreeStatistics().infoToVector("variables degree", featuresNames, ret);
    }
    if(weight){
    graph->getWeightStatistics().infoToVector("variables weight", featuresNames, ret);
    }
   /* if(dimensions){
    vigGraph->getWeightStatistics().infoToVector("variables weight", featuresNames, ret);
    }  
  */
   
   return ret;
}

std::vector<std::string> getFeaturesNames(){

  return this->featuresNames;
  
}

void computeradius(bool b){
  
  radius=b;
  
}

void computediameter(bool b){
  
  diameter=b;
  
}

void computetreewidth(bool b){
  
  treewidth=b;
  
}

void computearticulationpoints(bool b){
  
  articulationpoints=b;
  
}

void computeexzentricity(bool b){
  
  exzentricity=b;
  
}

void computepagerank(bool b){
  
  pagerank=b;
  
}

void computecommunitysize(bool b){
  
  communitysize=b;
  
}

void computecommunityneighbors(bool b){
  
  communityneighbors=b;
  
}

void computecommunitybridges(bool b){
  
  communitybridges=b;
  
}

void computedimensions(bool b){
  
  dimensions=b;
  
}

void computedegree(bool b){
  
  degree=b;
  
}

void computeweight(bool b){
  
  weight=b;
  
}

  
};
