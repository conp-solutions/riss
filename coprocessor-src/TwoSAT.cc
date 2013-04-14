/***************************************************************************************[TwoSAT.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/TwoSAT.h"

static const char* _cat = "COPROCESSOR 3 - 2SAT";

static IntOption  opt_iterations (_cat, "2sat-times", "run another time with inverse polarities", 1, IntRange(1, INT32_MAX));


Coprocessor::TwoSatSolver::TwoSatSolver(ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::CoprocessorData& _data)
: Technique( _ca, _controller)
, data( _data )
{
  solveTime = 0;
  touchedLiterals = 0;
  permLiterals = 0;
  flips = 0;
}

Coprocessor::TwoSatSolver::~TwoSatSolver()
{
}

bool Coprocessor::TwoSatSolver::isSat(const Lit& l) const
{
  assert( toInt(l) < permVal.size() && "literal has to be in bounds" );
  return ( permVal[ toInt( l ) ]  == 1 
    || ( permVal[ toInt( l ) ]  == 0 && tempVal[ toInt( l ) ]  == 1  ) );
}

bool Coprocessor::TwoSatSolver::isPermanent(const Var v) const
{
assert( v < permVal.size() && "literal has to be in bounds" );
  return ( permVal[ toInt( mkLit(v,false) ) ]  != 0 );
}

void Coprocessor::TwoSatSolver::printStatistics(ostream& stream)
{
  stream << "c [STAT] 2SAT " << solveTime << " s, " << touchedLiterals << " lits, " << permLiterals << " permanents, " << flips << " flips, " << endl;
}


char Coprocessor::TwoSatSolver::getPolarity(const Var v) const
{
  assert( toInt(mkLit(v,false)) < permVal.size() && "variable has to have an assignment" );
  if( permVal[toInt(mkLit(v,false))] != 0 ) return permVal[toInt(mkLit(v,false))];
  else return tempVal[toInt(mkLit(v,false))];
}

bool Coprocessor::TwoSatSolver::tmpUnitPropagate()
{
  while (! tmpUnitQueue.empty())
  {
    Lit x = tmpUnitQueue.front();
    tmpUnitQueue.pop_front();

    touchedLiterals ++;
    
    // if found a conflict, propagate the other polarity for real!
    if (tempVal[toInt(x)] == -1)
    {
      unitQueue.push_back(x);
      if (! tmpUnitQueue.empty()) tmpUnitQueue.clear();
      return unitPropagate();
    }

    tempVal[toInt(x)] = 1; tempVal[toInt(~x)] = -1;
    const Lit* impliedLiterals = big.getArray(x);
    const uint32_t impliedLiteralsSize = big.getSize(x);  
    
    for (int i = 0 ; i < impliedLiteralsSize; ++ i )
    {
      const Lit l = impliedLiterals[i];
      if (permVal[ toInt(l) ] != 0 || tempVal[toInt(l)] == 1)
        continue;
      else
	tmpUnitQueue.push_back(l);
    }
  }
  
  return true;
}

bool Coprocessor::TwoSatSolver::unitPropagate()
{
  while (! unitQueue.empty())
  {
    Lit x = unitQueue.front();
    unitQueue.pop_front();
    
    if (permVal[toInt(x)] == -1)
      return false;

    // do not propagate this unit twice!
    if (permVal[toInt(x)] == 1) continue;
    
    // TODO: can this information used from outside the 2sat search? always binary clauses as reason -> binary learned clause, or even unit?
    permVal[toInt(x)] = 1; permVal[toInt(~x)] = -1;
    permLiterals ++;
    
    const Lit* impliedLiterals = big.getArray(x);
    const uint32_t impliedLiteralsSize = big.getSize(x);  
    
    for (int i = 0 ; i < impliedLiteralsSize; ++ i)
    {      
      if( permVal[ toInt(impliedLiterals[i]) ] == 0 ) {
	permVal[ toInt(impliedLiterals[i]) ]  = 1;
	permVal[ toInt(~impliedLiterals[i]) ] = -1;
        unitQueue.push_back( impliedLiterals[i] );
      } else if ( permVal[ toInt(impliedLiterals[i]) ] == -1 )
	return false;
    }
  }
  return true;
}


bool Coprocessor::TwoSatSolver::hasDecisionVariable()
{
  
  for (unsigned int i = lastSeenIndex+1, end = tempVal.size(); i < end; ++i)
  {
    if (permVal[i] == 0 && tempVal[i] == 0)
    {
      lastSeenIndex = i-1;
      return true;
    }
  }
  
  return false;
}

Lit Coprocessor::TwoSatSolver::getDecisionVariable()
{
  
  for (unsigned int i = lastSeenIndex+1, end = tempVal.size(); i < end; ++i)
  {
    if (permVal[i] == 0 && tempVal[i] == 0)
    {
      lastSeenIndex = i;
      return toLit(i);
    }
  }
  
  assert(false && "This must not happen");
  return lit_Error;
}

bool Coprocessor::TwoSatSolver::solve()
{
  solveTime = cpuTime() - solveTime;
  big.create(ca, data, data.getClauses() );
  
  tempVal.resize(data.nVars()* 2,0);
  permVal.resize(data.nVars()* 2,0);
  unitQueue.clear();
  tmpUnitQueue.clear();
  lastSeenIndex = -1;
  
  vector<char> pol;
  
  bool Conflict = !unitPropagate();
  
  int iter = 0;
  
  for ( int iter = 0 ; iter < opt_iterations; ++ iter )
  {
    
    // at the beginning of the second loop, reset everything, and setup a polarity vector
    if( iter > 0 ) {
      pol = tempVal; // store old polarities!
      tempVal.assign(data.nVars()* 2,0);
      permVal.assign(data.nVars()* 2,0);
      unitQueue.clear();
      tmpUnitQueue.clear();
      lastSeenIndex = -1;
    }
    
    while (!Conflict && hasDecisionVariable())
    {
      Lit DL = getDecisionVariable();
      if( iter != 0 ) { // only in later iterations
	if(  sign(DL) && -1 == pol [ var(DL) ]
	  || !sign(DL) && 1 == pol [ var(DL) ]
	) { DL = ~DL; flips ++; }  // choose inverse polarity!
      }
      tmpUnitQueue.push_back(DL);
      Conflict = !tmpUnitPropagate();
    }
  }
  
  solveTime = cpuTime() - solveTime;
  return !Conflict;
  
}

void Coprocessor::TwoSatSolver::destroy()
{
  vector<char>().swap( tempVal );
  vector<char>().swap( permVal );
  deque<Lit>().swap(  unitQueue);
  deque<Lit>().swap( tmpUnitQueue);
}
