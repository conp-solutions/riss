/******************************************************************************************[BCE.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/bce.h"

using namespace Coprocessor;

BlockedClauseElimination::BlockedClauseElimination( CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data )
: Technique(_config, _ca,_controller),
data(_data)
{
  
}

void BlockedClauseElimination::reset()
{
  
}
  
bool BlockedClauseElimination::process()
{
  return false;
}
    
void BlockedClauseElimination::printStatistics(ostream& stream)
{
  
}

void BlockedClauseElimination::giveMoreSteps()
{
  
}
  
void BlockedClauseElimination::destroy()
{
  
}