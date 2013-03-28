/***************************************************************************************[TwoSAT.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/TwoSAT.h"

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
    {
//      if (Debug_Print2SATAssignments.IsSet())
//        std::cout << "Prop Unit Conflict " << toNumber(x)  << "\n";
      return false;
    }
    
    if (permVal[toInt(x)] == 1) continue;
    
 //   if (Debug_Print2SATAssignments.IsSet())
 //     std::cout << "PERM: Assign " << toNumber(x) << " ";
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
  
  bool Conflict = !unitPropagate();
  
  while (!Conflict && hasDecisionVariable())
  {
    Lit DL = getDecisionVariable();
    //if (Debug_Print2SATAssignments.IsSet()) std::cout << "DECIDE: " << toNumber(DL) << " ";
    tmpUnitQueue.push_back(DL);
    Conflict = !tmpUnitPropagate();
  }
    
  //if (Debug_Print2SATAssignments.IsSet())
  //  std::cout << "RESULT " << (Conflict ? "UNSAT " : "SAT ") << Model << "\n" ;
  
  solveTime = cpuTime() - solveTime;
  return !Conflict;
  
}



#if 0

#include <sys/types.h>
#include <stdint.h>

#include <sys/stat.h> // Get information about file size

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <cassert>
#include <cmath>
#include <cstdio>

#include <iostream>

#include <algorithm>

#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>

typedef long Literal;



class BinSatSolver
{
public:
  typedef uint64_t lit_t;
  typedef uint64_t var_t;
  
private:
  typedef std::vector<std::list<lit_t> > ImplGraph_t;
  
  std::vector<std::list<lit_t> > ImplGraph;
  std::vector<Literal> Model;
  
  Literal NV;
  
  enum VarStat
  {
    -1,
    1,
    0
  };
  
  std::queue<lit_t> tmpUnitQueue, unitQueue;
  std::vector<VarStat> tempVal, permVal;
  
  typedef std::list<lit_t> IntFacts_t;
  typedef std::list<lit_t> Assumptions_t;
  std::list<lit_t> InitFacts;
  std::list<lit_t> Assumptions;
  
  //===----------------------------------------------------------------------===//
  // Utilities
  //===----------------------------------------------------------------------===//

  /// var extracts the variable out of a literal
  inline var_t var(lit_t literal) const { return literal / 2; }

  /// lit creates a literal out of a variable and a polarity
  lit_t lit(var_t variable, int polarity) const {
    unsigned int literal = variable * 2;
    return (polarity == -1 ? literal+1 : literal);
  }
  
  /// pol returns the polarity of a literal
  int pol(lit_t literal) const {
    return (literal & 1) == 1 ? -1 : 1;
  }
  
  /// toNumber converts a literal back into a human readable number
  Literal toNumber(lit_t literal) const {
    return (pol(literal) == 1 ? var(literal) : 0-var(literal));
  }
  
  
  
  /// invert inverts the polarity of a literal
  inline lit_t invert(lit_t literal) const { return literal xor 1; } 

  //===----------------------------------------------------------------------===//
  // 
  //===----------------------------------------------------------------------===//
  bool unitPropagate();
  bool TempunitPropagate();

  void Init(Literal numvar);
  bool HasDecisionVariable();
  lit_t GetDecisionVariable();
  
  bool HaveunitPropagate;
  bool HavePropTempUnits;
  bool HaveDecided;
  
  unsigned int lastSeenIndex;
  
public:
  /// Returns the computed consequences: If Lit is in the result, it is known that it is a consequent. If Lit is not in the result, it is not known whether it is a consequent
  inline std::vector<Literal> GetComputedConsequences() const {
    return Model;
  }
  
  inline lit_t toLit(Literal number) const {
    return (number > 0 ? lit(number, 1) : lit(number * (-1), -1));
  }
  
  BinSatSolver(Literal NumberVariables);
  BinSatSolver();
  
  void Setup(Literal NumberVariables);
  
  inline void AddClause(Literal L1, Literal L2) {
    if (L1 == L2) return AddFact(L1);
    lit_t l1 = toLit(L1);
    lit_t l2 = toLit(L2);
    
    ImplGraph[invert(l1)].push_back(l2);
    ImplGraph[invert(l2)].push_back(l1);
  }
    

  bool Solve();
  
  inline void AddAssumption(Literal Lit)
  {
    Assumptions.push_back(toLit(Lit));
  }
  
  /// Removes a clause from the instance
  inline void RemoveClause(Literal Lit1, Literal Lit2)
  {
    if (Lit1 == Lit2) return RemoveFact(Lit1);
    
    lit_t l1 = toLit(Lit1);
    lit_t l2 = toLit(Lit2);
    
    ImplGraph[invert(l1)].remove(l2);
    ImplGraph[invert(l2)].remove(l1);
  }
  
    /// Adds unit clause [Lit]
  inline void AddFact(Literal Lit) {
    InitFacts.push_back(toLit(Lit));
  }

  /// Removes the unit clause [Lit]
  inline void RemoveFact(Literal Lit) {
    //std::cout << "REMOVE FACT: " << Lit << "\n";
    //InitFacts.sort();
    //InitFacts.unique();
    std::size_t size = InitFacts.size();
    InitFacts.remove(toLit(Lit));
    assert(InitFacts.size() < size);
  }
  
  void WriteInstance(const char* fn);
};



bool BinSatSolver::unitPropagate()
{
  while (! unitQueue.empty())
  {
    lit_t x = unitQueue.front();
    unitQueue.pop();
    
    if (permVal[x] == -1)
    {
//      if (Debug_Print2SATAssignments.IsSet())
//        std::cout << "Prop Unit Conflict " << toNumber(x)  << "\n";
      return false;
    }
    
    if (permVal[x] == 1)
      continue;
    
 //   if (Debug_Print2SATAssignments.IsSet())
 //     std::cout << "PERM: Assign " << toNumber(x) << " ";
    Model.push_back(toNumber(x));
    permVal[x] = 1; permVal[invert(x)] = -1;
    
    for (std::list<lit_t>::iterator  it = ImplGraph[x].begin(), end = ImplGraph[x].end(); it != end; it++)
    {      
      if (*it == 1)
        continue;
      
      HaveunitPropagate = true;

      unitQueue.push(*it);
    }
  }
  
  return true;
}

bool BinSatSolver::TempunitPropagate()
{
  while (! tmpUnitQueue.empty())
  {
    lit_t x = tmpUnitQueue.front();
    tmpUnitQueue.pop();
    
    if (tempVal[x] == -1)
    {
      unitQueue.push(x);
      while (! tmpUnitQueue.empty())
        tmpUnitQueue.pop();
      return unitPropagate();
    }

//    if (Debug_Print2SATAssignments.IsSet()) std::cout << "TEMP: Assign " << toNumber(x) << " ";
    
    tempVal[x] = 1; tempVal[invert(x)] = -1;
    for (std::list<lit_t>::const_iterator it = ImplGraph[x].begin(), end = ImplGraph[x].end(); it != end; it++)
    {      
      if (HaveunitPropagate) HavePropTempUnits = true;
      if (permVal[*it] != 0 || tempVal[*it] == 1)
        continue;
      
      tmpUnitQueue.push(*it);
    }

  }
  
  return true;
}




BinSatSolver::BinSatSolver(Literal numvar)
{
  Setup(numvar);
}

BinSatSolver::BinSatSolver()
{
}
  
void BinSatSolver::Setup(Literal numvar)
{
    NV = numvar;
  
  std::list<lit_t> EmptyLitList;
  
  while (numvar != 0)
  {
    tempVal.push_back(0); tempVal.push_back(0);
    permVal.push_back(0); permVal.push_back(0);
    ImplGraph.push_back(EmptyLitList); ImplGraph.push_back(EmptyLitList);
    --numvar;
  }
}

bool BinSatSolver::HasDecisionVariable()
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

BinSatSolver::lit_t BinSatSolver::GetDecisionVariable()
{
  
  for (unsigned int i = lastSeenIndex+1, end = tempVal.size(); i < end; ++i)
  {
    if (permVal[i] == 0 && tempVal[i] == 0)
    {
      lastSeenIndex = i;
      return i;
    }
  }
  
  assert(false && "This must not happen");
  return 0;
}

template<typename Ty>
struct abs_smaller_or_equal : public std::binary_function<Ty, Ty, Ty>
{
  inline Ty operator()(Ty lhs, Ty rhs) {
    return std::abs(lhs) <= std::abs(rhs);
  }
};

bool BinSatSolver::Solve()
{
  //Support::ScopeTimeGuard STG("BinSatSolver");  
  lastSeenIndex = -1;
  HaveunitPropagate = false;
  HavePropTempUnits = false;
  HaveDecided = false;
  
  // Clear everything
  for (unsigned int i = 0, end = tempVal.size(); i < end; ++i)
  {
    tempVal[i] = 0;
    permVal[i] = 0;
  }
  Model.clear();
  
  while (! unitQueue.empty())
    unitQueue.pop();
  
  while (! tmpUnitQueue.empty())
    unitQueue.pop();
  
  // Enqeue standard facts
  //std::for_each(Assumptions.begin(), Assumptions.end(), [&](lit_t lit) {unitQueue.push(lit);} );
  for (Assumptions_t::iterator it = Assumptions.begin(); it != Assumptions.end(); ++it)
    unitQueue.push(*it);
  
  Assumptions.clear();
  //std::for_each(InitFacts.begin(), InitFacts.end(), [&](lit_t lit) {unitQueue.push(lit);} );
  
  for (IntFacts_t::iterator it = InitFacts.begin(); it != InitFacts.end(); ++it)
    unitQueue.push(*it);
  // Do not delete init facts
  
  bool Conflict = !unitPropagate();
  
  HaveunitPropagate = false;
  
  while (!Conflict && HasDecisionVariable())
  {
    HaveDecided = true;
    lit_t DL = GetDecisionVariable();
    //if (Debug_Print2SATAssignments.IsSet()) std::cout << "DECIDE: " << toNumber(DL) << " ";
    tmpUnitQueue.push(DL);
    Conflict = !TempunitPropagate();
  }
  
  std::sort(Model.begin(), Model.end(), abs_smaller_or_equal<long>());
  
  //if (Debug_Print2SATAssignments.IsSet())
  //  std::cout << "RESULT " << (Conflict ? "UNSAT " : "SAT ") << Model << "\n" ;
  
  return !Conflict;
}



void BinSatSolver::WriteInstance(const char* fn)
{
  /*
  std::ofstream out(fn);
  
  out << "Facts:";
  for (auto it = InitFacts.begin(); it != InitFacts.end(); ++it)
    out << " " << toNumber(*it);
  
  out << "\nAssumptions:";
  for (auto it = Assumptions.begin(); it != Assumptions.end(); ++it)
    out << " " << toNumber(*it);
  out << "\n";
  for (unsigned int i = 0, end = ImplGraph.size(); i < end; ++i)
  {
    ImplGraph[i].sort();
    ImplGraph[i].unique();
    for (auto it = ImplGraph[i].begin(); it != ImplGraph[i].end(); ++it)
    {
      if (std::abs(toNumber(i)) < std::abs(toNumber(*it)))
        out << toNumber(invert(i)) << " " << toNumber(*it) << "\n";
    }
  }

*/
}

///////////////////

BinSatSolver BSS;

const char* CurChar;
const char* EndChar;

std::vector< std::vector<Literal> > CLAUSES;
std::set<Literal> KnownFacts;

int NumberVariables = 0;

static void SkipWhitespace()
{
  while ((*CurChar >= 9 && *CurChar <= 13) || *CurChar == 32)
    ++CurChar;
}

int ParseInt() {
  int     val = 0;
  bool    neg = false;
  
  SkipWhitespace();
  
  if (*CurChar == '-')
  {
    neg = true;
    ++CurChar;
  }
  else if (*CurChar == '+')
    ++CurChar;
  
  if (*CurChar < '0' || *CurChar > '9')
  {
    fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *CurChar);
    exit(3);
  }
  
  while (*CurChar >= '0' && *CurChar <= '9')
  {
    val = val*10 + (*CurChar - '0');
    ++CurChar;
  }
  
  NumberVariables = std::max(NumberVariables, val);
  
  return neg ? -val : val;
}

bool ReachedEOF()
{
  return CurChar == EndChar;
}

void SkipLine()
{
  while (true)
  {
    if (ReachedEOF()) return;
    if (*CurChar == '\n')
    {
      ++CurChar;
      return;
    }
    
    ++CurChar;
  }
}

void ReadClause(std::vector<Literal>& lits) {
  int parsed_lit;
  lits.clear();
  
  while (true)
  {
    parsed_lit = ParseInt();
    
    if (parsed_lit == 0) break;
    lits.push_back(parsed_lit);
    }
}




static void parse_DIMACS_main() {
  std::vector<Literal> lits;
  
  while (true)
  {
    SkipWhitespace();
    
    if (ReachedEOF())
      break;
    
    if (*CurChar == 'p')
    {
      SkipLine();
    }
    
    if (*CurChar == 'c')
      SkipLine();
    else
    {
      ReadClause(lits);
      
      if (lits.size() == 1)
      {
	KnownFacts.insert(lits[0]);
      }
      
      if (lits.size() <= 2)
	CLAUSES.push_back(lits);
      
      /*
      if (lits.size() == 1)
	BSS.AddFact(lits[0]);
      else if (lits.size() == 2)
	BSS.AddClause(lits[0], lits[1]);
      */
      // Otherwise, we ignore it
    }
  }
}


///////////////////

int CNFFileHandle;
const char* BeginChar;

void OpenCNFFile(const char* filename)
{
  CNFFileHandle = ::open(filename, O_RDONLY);
  
  if (CNFFileHandle == -1) 
  {
    std::cerr << "Cannot open CNF file.\n";
    exit(4);
  }
  
  struct stat sbuf;
  if (::fstat(CNFFileHandle, &sbuf) == -1) 
  {
    std::cerr << "Error while retrieving file statistics.\n";
    exit(5);
  }
  
  CurChar = static_cast<const char*>(::mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, CNFFileHandle, 0));
  
  if (CurChar == MAP_FAILED)
  {
    std::cerr << "Error while mapping file.\n";
    exit(6);
  }
  
  EndChar = CurChar + sbuf.st_size;
  BeginChar = CurChar;
}

void CloseCNFFile()
{
  ::munmap(const_cast<char*>(CurChar), EndChar-BeginChar);
  ::close(CNFFileHandle);
}

int main(int argc, char **argv)
{
  OpenCNFFile(argv[1]);
  parse_DIMACS_main();
  
  BSS.Setup(NumberVariables+1);
  
  for (std::vector< std::vector<Literal> >::iterator it = CLAUSES.begin(), end = CLAUSES.end(); it != end; ++it)
  {
    if (it->size() == 1)
    {
      BSS.AddFact((*it)[0]);
    }
    else if (it->size() == 2)
    {
      BSS.AddClause((*it)[0], (*it)[1]);
    }
    else
    {
      std::cerr << "SIZE ?? " << it->size() << std::endl;
    }
  }
  
  bool Result = BSS.Solve();
  
  if (Result)
  {
    std::vector<Literal> C = BSS.GetComputedConsequences();
    
    std::cout << "v ";
    for (std::vector<Literal>::iterator it = C.begin(), end = C.end(); it != end; ++it)
    {
      if (KnownFacts.find(*it) == KnownFacts.end())
	std::cout << " " << *it;
    }
    std::cout << " 0" << std::endl;
    
  }
  else
  {
    std::cout << "UNSAT" << std::endl;
  }
  
  CloseCNFFile();
}

#endif


void Coprocessor::TwoSatSolver::destroy()
{
  vector<char>().swap( tempVal );
  vector<char>().swap( permVal );
  deque<Lit>().swap(  unitQueue);
  deque<Lit>().swap( tmpUnitQueue);
}
