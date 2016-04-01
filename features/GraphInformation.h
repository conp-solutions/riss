/*
 * GraphFeatures to compute 
*/
#include "Graph.h"
#include "CNFClassifier.h"
#include <vector>

class GraphInformation{

private:
bool radius, diameter, treewidth, articulationpoints, exzentricity, pagerank, communitystructure, dimensions, degree, weight;  
Graph* graph;
int quantilesCount;
std::vector<std::string> featuresNames;
CNFClassifier* classifier;
Riss::ClauseAllocator& ca;
Riss::vec<Riss::CRef>& clauses;

public:

GraphInformation(CNFClassifier* c): classifier(c), ca(c->getCa()), clauses(c->getClauses()){ 
  quantilesCount = c->getQuantilesCount();
  radius = false;
  diameter = false;
  treewidth = false;
  articulationpoints = false;
  exzentricity = false;
  pagerank = false;
  communitystructure = false;
  dimensions = false;
  degree = false;
  weight = false;
  
  vector<int> clsSizes(10, 0);
 if(classifier->isComputingVarGraph()) graph = new Graph(classifier->getnVars(), classifier->isComputingDerivative());
  
   for (int i = 0; i < clauses.size(); ++i) {
      
        //You should know the difference between Java References, C++ Pointers, C++ References and C++ copies.
        // Especially if you write your own methods and pass large objects that you want to modify, this makes a difference!
        const Clause& c = ca[clauses[i]];
        clsSizes[ c.size() + 1 < clsSizes.size() ? c.size() : clsSizes.size() - 1 ] ++; // cumulate number of occurrences of certain clause sizes

        double wvariable = pow(2, -c.size());
        for (int j = 0; j < c.size(); ++j) {
            const Lit& l = c[j]; // for read access only, you could use a read-only reference of the type literal.
            const Lit cpl = ~l;  // build the complement of the literal
           
            const Var v = var(l); // calculate a variable from the literal

            // Adding edges for the variable graph
            if (classifier->isComputingVarGraph()) {
                for (int k = j + 1; k < c.size(); ++k) {
		                      graph->addDirectedEdgeAndInvertedEdge(v,   
                                   var(c[k]), 1);//with undirected edges there would be problems finding features(e.g. diameter)
                }
            }

            assert(var(~l) == var(l) && "the two variables have to be the same");
        }

    }
    
    graph->finalizeGraph(); //finalize
}  

std::vector<double> getFeatures(){

  std::vector<double> ret;
  featuresNames.clear();
  
  classifier->extractFeatures(ret);
  featuresNames = classifier->getFeaturesNames();
  
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
     if(communitystructure){
    graph->computeCommunityStatistics(quantilesCount);
      graph->getCommunitySizeStatistics().infoToVector("graph communities size", featuresNames, ret);
      graph->getCommunityNeighborStatistics().infoToVector("graph communities neighbors", featuresNames, ret);
      graph->getCommunityBridgeStatistics().infoToVector("graph communities bridgevariables", featuresNames, ret);
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

void computecommunitystructure(bool b){
  
  communitystructure=b;
  
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
