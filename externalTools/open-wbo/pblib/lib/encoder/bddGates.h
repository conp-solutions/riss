#ifndef BDD_Gates_H
#define BDD_Gates_H

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <utility>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include "../SimplePBConstraint.h"
#include "../PBConfig.h"
#include "../clausedatabase.h"
#include "../auxvarmanager.h"
#include "../weightedlit.h"

using namespace std;






class BDD_Gates_Encoder 
{  
private:
  struct build_data
  {
    int32_t index;
    int64_t maxsum;
    int64_t currentsum;
    Formula result;
    Formula true_node;
    build_data(int64_t index, int64_t maxsum, int64_t currentsum) : index(index), maxsum(maxsum), currentsum(currentsum), result(_undef_), true_node(_undef_) {};
    
  };
  vector<build_data> stack;
  int32_t stack_index;
  
  PBConfig config;
  
  map<pair<int32_t, int64_t>, Formula>  history;

  int64_t currentNumberOfClauses;
  bool stop;
  bool unlimited;
  bool useMonotonicITE;
  
  int64_t leq;
  int64_t geq;
  
  int64_t maxNumberOfClause = 0; 
  
  vector<PBLib::WeightedLit> inputVars; 
  vector<Formula> nodes;
  
  
  Formula buildBDD( int64_t maxsum, ClauseDatabase & formula, AuxVarManager & auxvars);
  Formula buildBDD_geq_leq ( int64_t maxsum, ClauseDatabase & formula, AuxVarManager & auxvars);

  void encodeBoth ( const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars, bool noLimit , int64_t maxClauses );
public:
  void encode(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars, bool noLimit = true, int64_t maxClauses = 0);
  
  BDD_Gates_Encoder(PBConfig & config) : config(config),   stop(false), unlimited(true), useMonotonicITE(true) {};
	
  void setMaxNumberOfClauseToUnlimited(bool unlimited) { this->unlimited = unlimited; }
	
  virtual ~BDD_Gates_Encoder() {}
	
  bool wasToBig() const 
  {
    if (unlimited)
      return false;
    else
      return stop;
  }
  
  void setUseMonotonicITE(bool on) { useMonotonicITE = on; }

};

#endif // BDD_Gates_H
