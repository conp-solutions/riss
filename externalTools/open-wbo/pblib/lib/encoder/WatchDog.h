#ifndef WATCHDOG_H
#define WATCHDOG_H
#include "../IncSimplePBConstraint.h"
#include "../SimplePBConstraint.h"
#include "../PBConfig.h"
#include "../clausedatabase.h"
#include "../auxvarmanager.h"
#include "../weightedlit.h"
#include <unordered_map>


class WatchDog
{
private:
  PBConfig config;
  int true_lit;
  
  int32_t polynomial_watchdog(SimplePBConstraint const & constraint, ClauseDatabase & formula, AuxVarManager & auxvars);
  void unary_adder(vector< int32_t > const & u, vector< int32_t > const & v, vector< int32_t >& w, ClauseDatabase& formula, AuxVarManager& auxvars);
  void totalizer(vector<int32_t> const & x, vector<int32_t> & u_x, ClauseDatabase & formula, AuxVarManager & auxvars);
  
public:
  void encode(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars);
    
  WatchDog(PBConfig & config);
};

#endif // WATCHDOG_H
