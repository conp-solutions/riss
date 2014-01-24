/********************************************************************************[CNFClassifier.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/


#include "CNFClassifier.h"

#include "utils/System.h" // for cpuTime

CNFClassifier::CNFClassifier( ClauseAllocator& _ca, const vec<CRef>& _clauses, int _nVars )
: ca(_ca), 
clauses(_clauses), 
nVars(_nVars)
{
}

CNFClassifier::~CNFClassifier()
{

}

std::vector<std::string> CNFClassifier::featureNames()
{
  // TODO: should return the names of the features in the order returned by the extractFeatures method
  vector<string> ret;
  return ret;
}


std::vector< double > CNFClassifier::extractFeatures()
{
  vector<double> ret;
  // TODO should return the vector of features. If possible, the features should range between 0 and 1 - all the graph features could be scaled down by number of variables, number of clauses or some other measure

  
  // some useful code snippets:
  
  /// measure time in seconds
  double  time1 = cpuTime(); // start timer
  // do some work here to measure the time for
  time1 = cpuTime() - time1; // stop timer
  
  // iterate over all clauses and parse all the literals
  for( int i = 0 ; i < clauses.size(); ++ i ) {
    
    //You should know the difference between Java References, C++ Pointers, C++ References and C++ copies. 
    // Especially if you write your own methods and pass large objects that you want to modify, this makes a difference!
    
    const Clause& c = ca[clauses[i]];
    for( int j = 0; j < c.size(); ++ j ) {
      const Lit& l = c[j]; // for read access only, you could use a read-only reference of the type literal. 
      const Lit cpl = ~l;  // build the complement of the literal
      const Var v = var(l); // calcluate a variable from the literal
      const bool isNegative = sign(l); // check the polarity of the literal
      // some check:
      assert( var(~l) == var(l) && "the two variables have to be the same" );
      // print some information
      cerr << "c checked the " << j << "th literal of the " << i << "th clause with size " << c.size() << ": " << l << ", var " << v << ", complement " << cpl << ", is negative: " << isNegative << endl;
    }
  }
  
  
  return ret;
}
