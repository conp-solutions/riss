#include <iostream>
#include <fstream>
#include "riss/core/Dimacs.h"
#include "CNFClassifier.h"
#include "riss/utils/SimpleGraph.h"
#include "riss/utils/community.h"

using namespace std;

class communityInformation
{
public:
vector<set<int>> neighbors;   
  
private:
Community* community;
SimpleGraph* VIG;
 

int stepcounter;
double prec;
  
SimpleGraph* buildSingleVIG(char** argv, int argc){

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
    
    SimpleGraph *vigGraph = nullptr;
    vigGraph = new SimpleGraph(S.nVars(), true); 
    
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
                            for (int k = j + 1; k < c.size(); ++k) {
                    vigGraph->addDirectedEdgeAndInvertedEdge(v,   
                                   var(c[k]), 1);//with undirected edges there would be problems finding features(e.g. diameter)
                }
	      
            assert(var(~l) == var(l) && "the two variables have to be the same");
        }

    }
 return vigGraph; 
}

public:

  
communityInformation(int argc, char** argv, int stepcounter, double prec): stepcounter(stepcounter), prec(prec){

  VIG = buildSingleVIG(argv, argc);
  VIG->finalizeGraph();
  community = nullptr;
   //VIG->completeSingleVIG();  
    
}

bool getCommunityNeighbors(){
int step = 0; 
vector<int> adj; 
  
  if(community == nullptr){
    getCommunities();
  }
  
    for(int i=0; i<community->Comm.size()-1; ++i){
     for(int j =0; j<community->Comm[i].size(); ++j){
      adj = VIG->getAdjacency(community->Comm[i][j]);
       for(int k=0; k<adj.size(); ++k){
       	 if (community->n2c[adj[k]] > i){
	   neighbors[i].insert(community->n2c[adj[k]]);
	   neighbors[community->n2c[adj[k]]].insert(i);
	   if(neighbors[i].size() > (community->Comm.size()-1)) break;
	   }
	   step++;
	if(step > stepcounter) return false;
      }   
  }
}
  
  return true;
  
}

vector<vector<int>> getCommunities(){
 community = new Community(VIG);
 community->compute_modularity_GFA(prec);
 community->compute_communities();
 neighbors.resize(community->ncomm);
 vector<vector<int>> comm = community->Comm;
 comm.resize(community->ncomm);
  return comm;
    
}

vector<int> getNodes(){
  int step = 0;
  
  if(community->Comm.size() == 0){
  
    getCommunities();
  }
  
 return community->n2c;
  
}

};
