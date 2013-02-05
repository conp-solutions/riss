/**************************************************************************************[Probing.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Probing.h"


using namespace Coprocessor;

Probing::Probing(ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Coprocessor::CoprocessorData& _data, Solver& _solver)
: Technique( _ca, _controller )
, data( _data )
, solver( _solver )
, probeLimit(20000)
{

}

bool Probing::probe()
{
  // resize data structures
  prPositive.growTo( data.nVars() );

  
  CRef thisConflict = CRef_Undef;
  
  // resetup solver
  
  // clean cp3 data structures!
  
  
  for( Var v = 0 ; v < data.nVars(); ++v ) variableHeap.push_back(v);
  
  cerr << "sort literals in probing queue according to some heuristic (BIG roots first, ... )" << endl;
  
  // probe for each variable 
  while(  variableHeap.size() > 0 && !data.isInterupted()  && (probeLimit > 0 || data.unlimited()) && data.ok() )
  {
    Var v = variableHeap.front();
    variableHeap.pop_front();
    
    assert( solver.decisionLevel() == 0 && "probing only at level 0!");
    const Lit posLit = mkLit(v,false);
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(posLit);
    thisConflict = prPropagate();
    if( thisConflict != CRef_Undef ) {
      prAnalyze();
      solver.cancelUntil(0);
      solver.propagate();
      continue;
    } else {
      if( prDoubleLook() ) continue; // something has been found, so that second polarity has not to be propagated
    }
    solver.assigns.copyTo( prPositive );
    // other polarity
    solver.cancelUntil(0);
    
    assert( solver.decisionLevel() == 0 && "probing only at level 0!");
    const Lit negLit = mkLit(v,true);
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(posLit);
    thisConflict = prPropagate();
    if( thisConflict != CRef_Undef ) {
      prAnalyze();
      solver.cancelUntil(0);
      solver.propagate();
      continue;
    } else {
      if( prDoubleLook() ) continue; // something has been found, so that second polarity has not to be propagated
    }
    // could copy to prNegatie here, but we do not do this, but refer to the vector inside the solver instead
    vec<lbool>& prNegative = solver.assigns;
    
    assert( solver.decisionLevel() == 1 && "");
    
    data.lits.clear();
    data.lits.push_back(negLit); // equivalent literals
    doubleLiterals.clear();	  // NOTE re-use for unit literals!
    // look for necessary literals, and equivalent literals (do not add literal itself)
    for( int i = solver.trail_lim[0] + 1; i < solver.trail.size(); ++ i )
    {
      // check whether same polarity in both trails, or different polarity and in both trails
      const Var tv = var( solver.trail[ i ] );
      
      if( solver.assigns[ tv ] == prPositive[tv] ) {
	doubleLiterals.push_back(solver.trail[ i ] );
      } else if( solver.assigns[ tv ] == l_True && prPositive[tv] == l_False ) {
	data.lits.push_back( solver.trail[ i ] ); // equivalent literals
      }
    }
    
    // tell coprocessor about equivalent literals
    if( data.lits.size() > 1 )
      data.addEquivalences( data.lits );
    
    // propagate implied(necessary) literals inside solver
    if( solver.propagate() != CRef_Undef )
      data.setFailed();
  }
  
  // apply equivalent literals!
  if( data.getEquivalences().size() > 0 )
    cerr << "equivalent literal elimination has to be applied" << endl;
  
  // TODO: do asymmetric branching here!
  
  return true;
}

CRef Probing::prPropagate()
{
  return CRef_Undef;
}

bool Probing::prAnalyze()
{
  return true;
}

bool Probing::prDoubleLook()
{
  // TODO: sort literals in double lookahead queue, use only first n literals
  
  
  
  // first decision is not a failed literal
  return false;
}


void Probing::printStatistics( ostream& stream )
{
  stream << "c [PROBE] " << endl;   
}