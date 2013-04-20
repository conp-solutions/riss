/***************************************************************************************[TwoSAT.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/TwoSAT.h"

static const char* _cat = "COPROCESSOR 3 - REWRITE";

#if defined CP3VERSION 
static const int debug_out = 0;
#else
static IntOption debug_out                 (_cat, "2sat-debug",  "Debug Output of 2sat", 0, IntRange(0, 4));
static BoolOption useUnits                 (_cat, "2sat-units",  "If 2SAT finds units, use them!", false);
#endif

Coprocessor::TwoSatSolver::TwoSatSolver(ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::CoprocessorData& _data)
: Technique( _ca, _controller)
, data( _data )
{
  solveTime = 0;
  touchedLiterals = 0;
  permLiterals = 0;
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
  stream << "c [STAT] 2SAT " << solveTime << " s, " << touchedLiterals << " lits, " << permLiterals << " permanents" << endl;
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

    if( debug_out > 2 && tmpUnitQueue.size() % 100 == 0 ) cerr << "queue.size() = " << tmpUnitQueue.size() << endl;
    
    touchedLiterals ++;
    
    // if found a conflict, propagate the other polarity for real!
    if (tempVal[toInt(x)] == -1)
    {
      unitQueue.push_back(x);
      if (! tmpUnitQueue.empty()) tmpUnitQueue.clear();
      return unitPropagate();
    }

    assert( var(x) < data.nVars() && "do handle variables only that are part of the formula" );
    tempVal[toInt(x)] = 1; tempVal[toInt(~x)] = -1;
    const Lit* impliedLiterals = big.getArray(x);
    const uint32_t impliedLiteralsSize = big.getSize(x);  
    for (int i = 0 ; i < impliedLiteralsSize; ++ i )
    {
      const Lit l = impliedLiterals[i];
      assert( var(l) != var(x) && "a variable should/cannot imply iteself" );
      if (permVal[ toInt(l) ] != 0 || tempVal[toInt(l)] == 1)
        continue;
      else {
	tmpUnitQueue.push_back(l);
	if (tempVal[toInt(l)] == -1) {// conflict!!
	  unitQueue.push_back(~x); // we cannot set x like we do it now, otherwise, we would have to set l and -l
	  if (! tmpUnitQueue.empty()) tmpUnitQueue.clear();
	  return unitPropagate();
	} else { tempVal[toInt(l)] = 1; tempVal[toInt(~l)] = -1; }
      }
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
    
    if(useUnits) data.enqueue(x); // if allowed, use the found unit!
    
    if (permVal[toInt(x)] == -1)
    {
      if( debug_out > 1 ) cerr << "Prop Unit Conflict " << x  << endl;
      return false;
    }
    
    if (permVal[toInt(x)] == 1) {
      assert( tempVal[ toInt(x)] == 1 && "when unit propagated, temporary value should also be fixed!");
      continue;
    }
    
 //   if (Debug_Print2SATAssignments.IsSet())
 //     std::cout << "PERM: Assign " << toNumber(x) << " ";
    permVal[toInt(x)] = 1; permVal[toInt(~x)] = -1; // actually, this should be the case already
    tempVal[toInt(x)] = 1; tempVal[toInt(~x)] = -1;
    permLiterals ++;
    
    const Lit* impliedLiterals = big.getArray(x);
    const uint32_t impliedLiteralsSize = big.getSize(x);  
    
    for (int i = 0 ; i < impliedLiteralsSize; ++ i)
    {      
      if( permVal[ toInt(impliedLiterals[i]) ] == 0 ) {
	permVal[ toInt(impliedLiterals[i]) ] = 1; permVal[ toInt(~impliedLiterals[i]) ] = -1;
	tempVal[ toInt(impliedLiterals[i]) ] = 1; tempVal[ toInt(~impliedLiterals[i])] = -1;
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
  
  tempVal.assign(data.nVars()* 2,0);
  permVal.assign(data.nVars()* 2,0);
  unitQueue.clear();
  tmpUnitQueue.clear();
  lastSeenIndex = -1;
  
  bool Conflict = !unitPropagate();
  
  int decs = 0;
  
  while (!Conflict && hasDecisionVariable())
  {
    Lit DL = getDecisionVariable();
    decs++;
    if( debug_out > 2 && decs % 100 == 0 ) cerr << "c dec " << decs << "/" << data.nVars() << " mem: " << memUsedPeak() << endl;
    //if (Debug_Print2SATAssignments.IsSet()) std::cout << "DECIDE: " << toNumber(DL) << " ";
    tmpUnitQueue.push_back(DL);
    Conflict = !tmpUnitPropagate();
  }
    
  //if (Debug_Print2SATAssignments.IsSet())
  if( debug_out > 2 ) cerr << "2SAT result: " << (Conflict ? "UNSAT " : "SAT ") << endl ;
  
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
