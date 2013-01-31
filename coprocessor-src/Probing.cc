/**************************************************************************************[Probing.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Probing.h"


using namespace Coprocessor;

Probing::Probing(ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::CoprocessorData& _data)
: Technique( _ca, _controller )
, data( _data )
{

}

bool Probing::probe()
{
  return true;
}

void Probing::printStatistics( ostream& stream )
{
  stream << "c [PROBE] " << endl;   
}