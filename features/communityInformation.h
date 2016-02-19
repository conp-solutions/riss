#include <iostream>
#include <fstream>
#include "riss/core/Dimacs.h"
#include "CNFClassifier.h"
#include "Graph.h"
using namespace std;

class communityInformation
{
private:

Graph* VIG;
vector<vector<int>> communities;
vector<int> nodes;
vector<vector<int>> neighbors;  

int stepcounter;
  
Graph* buildSingleVIG(char** argv, int argc, bool deri){

   string line = "";
    ofstream* fileout = new ofstream();
   
   const char* cnffile = argv[1];

    CoreConfig coreConfig;
    Solver S(&coreConfig);

    double initial_time = cpuTime();
    S.verbosity = 0;

        printf("c trying %s\n", cnffile);
    

    gzFile in = gzopen(cnffile, "rb");
    if (in == nullptr) {
        printf("c ERROR! Could not open file: %s\n",
               argc == 1 ? "<stdin>" : cnffile);
    }

    parse_DIMACS(in, S);
    gzclose(in);

    if (S.verbosity > 0) {
        printf(
            "c |  Number of variables:  %12d                                                                  |\n",
            S.nVars());
        printf(
            "c |  Number of clauses:    %12d                                                                   |\n",
            S.nClauses());
    }

    double parsed_time = cpuTime();
    if (S.verbosity > 0)
        printf(
            "c |  Parse time:           %12.2f s                                                                 |\n",
            parsed_time - initial_time);

    // here convert the found unit clauses of the solver back as real clauses!
    if (S.trail.size() > 0) {
        
            cerr << "c found " << S.trail.size()
                 << " unit clauses after propagation" << endl;
        
        // build the reduct of the formula wrt its unit clauses
        S.buildReduct();

        vec<Lit> ps;
        for (int j = 0; j < S.trail.size(); ++j) {
            const Lit l = S.trail[j];
            ps.clear();
            ps.push(l);
            CRef cr = S.ca.alloc(ps, false);
            S.clauses.push(cr);
        }
    }
      
     vector<int> clsSizes(10, 0);
    
    Graph *vigGraph = nullptr;
    vigGraph = new Graph(S.nVars(), deri); 
    
     for (int i = 0; i < S.clauses.size(); ++i) {
      
        //You should know the difference between Java References, C++ Pointers, C++ References and C++ copies.
        // Especially if you write your own methods and pass large objects that you want to modify, this makes a difference!
        const Clause& c = S.ca[S.clauses[i]];
        clsSizes[ c.size() + 1 < clsSizes.size() ? c.size() : clsSizes.size() - 1 ] ++; // cumulate number of occurrences of certain clause sizes

        double wvariable = pow(2, -c.size());
        for (int j = 0; j < c.size(); ++j) {
            const Lit& l = c[j]; // for read access only, you could use a read-only reference of the type literal.
            const Lit cpl = ~l;  // build the complement of the literal
           
            const Var v = var(l); // calculate a variable from the literal

            // Adding edges for the variable graph
            if (deri) {
                for (int k = j + 1; k < c.size(); ++k) {
                    vigGraph->addDirectedEdge(v,   
                                   var(c[k]), 1);//with undirected edges there would be problems finding features(e.g. diameter)
                }
            }

            assert(var(~l) == var(l) && "the two variables have to be the same");
        }

    }
 return vigGraph; 
}

public:

  
communityInformation(int argc, char** argv, bool derivative, int stepcounter): stepcounter(stepcounter){
   VIG = buildSingleVIG(argv, argc, derivative);
   VIG->finalizeGraph();
   VIG->completeSingleVIG();  
   nodes.resize(VIG->getSize());
  
}

vector<vector<int>> getCommunityNeighbors(){
int step = 0; 
  
  if(communities.size() == 0){
  
    getCommunities();
  }
  
   vector<int> adj; 
   for(int i=0; i<communities.size()-1; ++i){
     for(int j =0; j<communities[i].size(); ++j){
      adj = VIG->getAdjacency(communities[i][j]-1);
       for(int k=0; k<adj.size(); ++k){
       	 if (nodes[adj[k]] > i){
	   neighbors[i].push_back(nodes[adj[k]]);
	   neighbors[nodes[adj[k]]].push_back(i);
	   }
	   step++;
	if(step > stepcounter) return neighbors;
      }   
  }
}
  
  return neighbors;
  
}

vector<vector<int>> getCommunities(){
  
  communities = VIG->getCommunityForEachNode(0.000001);
  neighbors.resize(communities.size());
  
  return communities;
    
}

vector<int> getNodes(){
  int step = 0;
  
  if(communities.size() == 0){
  
    getCommunities();
  }
  
   for(int i=0; i<communities.size(); ++i) {
    for(int j=0; j<communities[i].size();j++){ nodes[communities[i][j]-1] = i;
      step++;
      if(step > stepcounter) return nodes;
    }
    }
    
  return nodes;
  
}

};
