/*
GraphFeatSAT: Graph Features Computation for SAT instances.
Version 2.3
Authors:
  - Carlos Ansótegui (DIEI - UdL)
  - María Luisa Bonet (LSI - UPC)
  - Jesús Giráldez-Cru (IIIA-CSIC)
  - Jordi Levy (IIIA-CSIC)

Contact: jgiraldez@iiia.csic.es

Copyright (C) 2016  C. Ansótegui, M.L. Bonet, J. Giráldez-Cru, J. Levy

This file is part of GraphFeatSAT: Graph Features Computation for SAT instances, version 2.3.

GraphFeatSAT is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GraphFeatSAT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with GraphFeatSAT. If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include <stack>

#include <algorithm>
#include <stdio.h> 

#ifndef COMMUNITY_H
#define COMMUNITY_H

#include "riss/utils/SimpleGraph.h"
#include "riss/utils/Debug.h"

using namespace std;

//-------------------------------------------------------------------------------------------
inline void shuffle(vector <int> &x) {
//-------------------------------------------------------------------------------------------
// Randomly re-order elements of a vector
//-------------------------------------------------------------------------------------------
	for (int i=0 ; i<x.size()-1 ; i++) {
		int j = rand()%(x.size()-i)+i;
		int aux = x[i];
		x[i] = x[j];
		x[j] = aux;
	}
}
//-------------------------------------------------------------------------------------------
inline double abs2(double x){
//-------------------------------------------------------------------------------------------
	if(x<0)	return -x;
	else	return x;
}
//-------------------------------------------------------------------------------------------
inline bool my_bigger_communities(pair <int,double> x, pair <int,double> y) {
	return x.second > y.second;
}
//-------------------------------------------------------------------------------------------


class Community {
  vector <double> arity; // arity[i] = Sum of the arities of nodes belonging to community "i"

 public:

  SimpleGraph* g;
  vector <int> n2c;      // Assigns every node a community (community identifiers belong to 0..n-1)
  vector <vector <int> > Comm;
  vector <pair <int,int> > Comm_order;

  Community(SimpleGraph* g2) {
	  if(g2 != NULL){
		  g = g2;
		  iterations = 0;

		  arity.resize(g->getSize());
		  n2c.resize(g->getSize());

		  for (int i=0; i<g->getSize(); i++)
			  n2c[i] = i;       // Every node to one singleton community

		  for (int i=0; i<g->getSize(); i++)
			  arity[i] = g->arity(i); // Arity of the (singleton) community equals to arity of node
	  }
  }
  
  Community(SimpleGraph* g2, vector<int> &n2cb){
	  g = g2;
	  iterations = 0;
	  
	  arity.resize(g->getSize());
	  n2c = n2cb;
	  
	  for(int i=0; i<g->getSize(); i++)
		  arity[n2c[i]] += g->arity(i);
  }

  int ncomm;
  int iterations;


//-------------------------------------------------------------------------------------------
double modularity() {
//-------------------------------------------------------------------------------------------
// Given the graph "g" and the partition "n2c" computes the modularity
//-------------------------------------------------------------------------------------------
	double w = 0;
	double arity = 0;
	vector <double> aritym(g->getSize(), 0);

	
	for (SimpleGraph::EdgeIter it=g->begin(); it != g->end(); it++) {
		//assert(it->orig >= 0 && it->orig < n2c.size());
		//assert(it->dest >= 0 && it->dest < n2c.size());
		if (n2c[it->orig] == n2c[it->dest]) 
			w += it->weight;
	}
	
	
	for (int i=0; i<g->getSize(); i++)
		aritym[n2c[i]] += g->arity(i);  

	
	
	for (int i=0; i<g->getSize(); i++)
		arity += aritym[i] * aritym[i] / g->arity() / g->arity();


//  cerr <<" Modularity = "<<w / g->arity()<<" - "<<arity<<" = "<<w / g->arity() - arity<<endl;
	return 2*w / g->arity() - arity;
}

//-------------------------------------------------------------------------------------------
bool one_level() {
//-------------------------------------------------------------------------------------------
// Given a graf "g" and a partition "n2c", improves the partition by moving nodes from one 
// partition to another. Modifies "arity" and "n2c". Returns "true" if partition changed.
//-------------------------------------------------------------------------------------------
	
  bool improved = false, changed;
	vector<int> random_order(g->getSize());

	for (int i=0 ; i<random_order.size(); i++)
		random_order[i]=i;

	vector <double> wc(g->getSize(), -1); // wc[c] = sum_{j\in c} w(n,j) for c not conected wc[c]=-1
	vector <int> nc;                  // neigh communities
	do {
		DOUT( cerr << "c Q=" << modularity() << " at iteration " << iterations << endl; );
		iterations++;
		shuffle(random_order);
		changed = false;
		
		for (int naux=0; naux<g->getSize(); naux++) {
			int n = random_order[naux];

			arity[n2c[n]] -= g->arity(n);

			for (int i=0; i < nc.size(); i++)
				wc[nc[i]] = -1;
			nc.clear();
			nc.resize(0);
			for (SimpleGraph::NeighIter it=g->begin(n); it != g->end(n); ++it) {
				int c = n2c[it->dest];
				if (it->dest != n) {
					if (wc[c] == -1) {
						wc[c] = 0;
						nc.push_back(c);
					}
					wc[c] += (double)it->weight;
				}
			}

			int best_c=n2c[n];
			double best_inc = 0;

			for (int i=0; i < nc.size(); i++) {
				double inc = wc[nc[i]] - g->arity(n) * arity[nc[i]] / g->arity();
				if (inc > best_inc) {
					best_inc = inc;
					best_c = nc[i];
				}
			}
			if (best_c != n2c[n]) { 
	//cerr << "Node " << n << " goes " << n2c[n] << " -> " <<best_c << " inc="<<2*best_inc/g->arity()<<endl;
				changed = true;
				improved = true;
				n2c[n] = best_c;
			}

			arity[best_c] += g->arity(n);
		}
	}
	while (changed);
	
	return (improved);
}

//-------------------------------------------------------------------------------------------
SimpleGraph* community2graph() {
//-------------------------------------------------------------------------------------------
// Given a graph "g" and a partition "n2c" generates a new grapf "g2" where nodes
// are communities and edges are the sum of the edges between both communities.
//-------------------------------------------------------------------------------------------
	vector <int> ren(n2c.size(), -1);
	for (int i=0; i<n2c.size(); i++)
		ren[n2c[i]] = 0;
	int aux=0;
	for (int i=0; i<ren.size(); i++)
		if (ren[i] == 0) ren[i] = aux++;
	for (int i=0; i<n2c.size(); i++)
		n2c[i] = ren[n2c[i]];

	SimpleGraph* g2 = new SimpleGraph(aux,0);

	for (SimpleGraph::EdgeIter it=g->begin(); it != g->end(); ++it)
		g2->addDirectedEdge(n2c[it->orig], n2c[it->dest], (double)it->weight);

	g2->finalizeGraph(); // make sure the graph is ok
	return g2;
}

//-------------------------------------------------------------------------------------------
double compute_modularity_GFA(double precision) {
//-------------------------------------------------------------------------------------------
// Given a graph "g", computes a partition "n2c" by the GFA method, applying "one-level" while it is possible, 
// and collapsing communities into nodes applying "community2graph".
//-------------------------------------------------------------------------------------------
	bool improved;
	Community* c = new Community(g);

	do {
		double aux = c->modularity();
		
		DOUT( cerr <<"c\tpre improved: " << improved << " c->modularity(): " << c->modularity() << " aux: " << aux <<endl; );
		
		improved = c->one_level() && abs2(c->modularity()-aux) > precision;
		//improved = c->one_level();
		DOUT( cerr <<"c\tipos mproved: " << improved << " c->modularity(): " << c->modularity() << " aux: " << aux <<endl; );
		
		if (improved) {
			c->g = c->community2graph();
			improved = (ncomm != c->g->getSize());
			ncomm = c->g->getSize();
			for (int i = 0; i<n2c.size(); i++) 
				n2c[i] = c->n2c[n2c[i]];
			iterations += c->iterations;
			c = new Community(c->g);
		}
		
		DOUT( cerr <<"c\tQ = "<<modularity()<<" #comm = "<<ncomm<<endl; );
		//c.g.print();
	}
	while (improved);
	
	
	return modularity();
}

//-------------------------------------------------------------------------------------------
double compute_modularity_LPA(double precision){
//-------------------------------------------------------------------------------------------
//TODO
//-------------------------------------------------------------------------------------------
	return 0;
}

//-------------------------------------------------------------------------------------------
void connected() {
//-------------------------------------------------------------------------------------------
// Given a graph "g", computes "n2c" assignning every node a connected component.
//-------------------------------------------------------------------------------------------

	for(int i=0; i<n2c.size(); i++) 
		n2c[i] = -1;

	int c=0;

	stack <int> nb;
	for(int i=0; i<g->getSize(); i++) {
		if (n2c[i] == -1) {
			n2c[i] = c;
			nb.push(i);
			while (!nb.empty()) {
				int n = nb.top();
				nb.pop();
				for(SimpleGraph::NeighIter it=g->begin(n); it != g->end(n); it++)
				if (n2c[it->dest] == -1) {
					n2c[it->dest] = c;
					nb.push(it->dest);   
				} 
			}
			c++;
		}
	}
	ncomm = c;
}

//-------------------------------------------------------------------------------------------
void compute_communities() {
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------

	Comm.resize(g->getSize());
	for (int i=0; i<g->getSize(); i++)
		Comm[n2c[i]].push_back(i); //original Version push_back(i+1)

	for (int i=0; i< Comm.size(); i++)
		Comm_order.push_back(make_pair(i,Comm[i].size()));
	sort(Comm_order.begin(), Comm_order.end(), my_bigger_communities);

	int i;
	for (i=0; i< Comm.size() && Comm[Comm_order[i].first].size() >0; i++);
	ncomm=i;

/*
cerr<<"Community sizes:"<<endl;
for (int i=0; i<ncomm; i++)
cerr<<i<<" "<<Comm[Comm_order[i].first].size()<<endl;
*/
}

//-------------------------------------------------------------------------------------------
void print_communities(char *filename) {
//-------------------------------------------------------------------------------------------
// Print communities of nodes in a file
//-------------------------------------------------------------------------------------------

	FILE *file;

	if((file = fopen(filename,"w"))== NULL) {
		cerr<<"Unable to open file "<<filename<<" for writing\n";
		exit(-1);
	}
	fprintf(file,"%d\n",(int)Comm.size());
	for (int i=0; i<Comm.size(); i++) {
		fprintf(file,"%d ",i+1);
		for (int j=0; j<Comm[Comm_order[i].first].size(); j++)
			fprintf(file,"%d ", Comm[Comm_order[i].first][j]);
		fprintf(file,"0\n");
	}
	fclose(file);
	cerr<<"Written community file\n";
}

//-------------------------------------------------------------------------------------------
void print() {
//-------------------------------------------------------------------------------------------
	cerr <<"------- COMMUNITY ------\n";
	cerr <<"Q = "<<modularity()<<endl;
	cerr <<"#C = "<<ncomm<<endl;  
	for (int i=0; i<n2c.size(); i++) 
		cerr <<i<<" -> "<<n2c[i]<<endl;
//g.print();
}

//-------------------------------------------------------------------------------------------
void print_modules(char *filename) {
//-------------------------------------------------------------------------------------------
	FILE *file;

	if((file = fopen(filename,"w"))== NULL) {
		cerr<<"Unable to open file "<<filename<<" for writing\n";
		exit(-1);
	}
	
	for(int i=0; i<n2c.size(); i++)
		fprintf(file, "%d\n", n2c[i]);
	fclose(file);
}

};
#endif
