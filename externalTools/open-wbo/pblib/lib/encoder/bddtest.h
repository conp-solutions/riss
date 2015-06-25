#ifndef BDD_TEST_H
#define BDD_TEST_H

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
#include <tuple>


namespace std
{

  
//   template <>
//   struct hash<pair<int32_t, int64_t> >
//   {
//     std::size_t operator()(const pair<int32_t, int64_t>& k) const
//     {
//       size_t hash = k.first xor k.second;
// 
//       return hash;
//     }
//   };
  
//   template <>
//   struct hash<tuple<int32_t, int32_t, int32_t> >
//   {
//     std::size_t operator()(const tuple<int32_t, int32_t, int32_t>& k) const
//     {
//       size_t hash = get<0>(k) xor get<1>(k) xor get<2>(k);
// 
//       return hash;
//     }
//   };
}



class BDD_Test_Encoder 
{  
private:
  struct build_data
  {
    int64_t maxsum = -1;
    int64_t currentsum = -1;
    int32_t result = 0;
    int32_t low = 0;
    int32_t high = 0;
//     build_data(int64_t index, int64_t maxsum, int64_t currentsum) : index(index), maxsum(maxsum), currentsum(currentsum), result(0) {};    
  };
  
  int32_t current_node_id = 3;
  
  vector<build_data> stack;
  
  PBConfig config;
  
  map<pair<int32_t, int64_t>, int32_t> sumHistory;
  map<std::tuple<int32_t, int32_t, int32_t>, int32_t>  nodeHistory;

  int64_t k;
  int32_t true_lit;
  
  int32_t test_counter = 0;
  
  vector<PBLib::WeightedLit>  inputVars; 

  int32_t buildBDD(int index, int64_t currentsum, int64_t maxsum, ClauseDatabase& formula, AuxVarManager& auxvars);
  void recusiveEncoding(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars, bool noLimit = true, int64_t maxClauses = 0);
  void iterativeEncoding(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars, bool noLimit = true, int64_t maxClauses = 0);
  
public:
  void encode(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars, bool noLimit = true, int64_t maxClauses = 0);
  
  BDD_Test_Encoder(PBConfig & config) : config(config) { };
	
	
  virtual ~BDD_Test_Encoder() {}

};

#endif // BDD_TEST_H
