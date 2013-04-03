/***********************************************************************************[Subsumption.cch]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Propagation.h"

using namespace Coprocessor;

static int upLevel = 1;


static const char* _cat = "COPROCESSOR 3 - UP";

#if defined CP3VERSION  
static const int debug_out = 0;
#else
static IntOption debug_out        (_cat, "up-debug", "debug output for propagation",0, IntRange(0,4) );
#endif

Propagation::Propagation( ClauseAllocator& _ca, ThreadController& _controller )
: Technique( _ca, _controller )
, processTime(0)
, lastPropagatedLiteral( 0 )
, removedClauses(0)
, removedLiterals(0)
{
}

void Propagation::reset()
{
  lastPropagatedLiteral = 0;
}


lbool Propagation::process(CoprocessorData& data, bool sort, Heap<VarOrderBVEHeapLt> * heap, const Var ignore)
{
  processTime = cpuTime() - processTime;
  modifiedFormula = false;
  if( !data.ok() ) return l_False;
  Solver* solver = data.getSolver();
  // propagate all literals that are on the trail but have not been propagated
  for( ; lastPropagatedLiteral < solver->trail.size(); lastPropagatedLiteral ++ )
  {
    const Lit l = solver->trail[lastPropagatedLiteral];
    if (debug_out) cerr << "c UP propagating " << l << endl;
    data.log.log(upLevel,"propagate literal",l);
    // remove positives
    vector<CRef> & positive = data.list(l);
    for( int i = 0 ; i < positive.size(); ++i )
    {

      Clause & satisfied = ca[positive[i]];
      if (ca[ positive[i] ].can_be_deleted()) // only track yet-non-deleted clauses
          continue;
      if( debug_out ) cerr << "c UP remove " << ca[ positive[i] ] << endl;
      ++removedClauses; // = ca[ positive[i] ].can_be_deleted() ? removedClauses : removedClauses + 1;
      ca[ positive[i] ].set_delete(true);
      modifiedFormula = true;
      //data.removedClause( positive[i] );
      data.removedClause( positive[i], heap, ignore);
      // TODO : necessary? -> just performance trade-off
      /*for (int lit = 0; lit < satisfied.size(); ++lit)
      {
          if (satisfied[lit] != l)
            data.removeClauseFrom(positive[i], satisfied[lit]);
      }*/
    }
    vector<CRef>().swap(positive); // free physical space of positive
    
    const Lit nl = ~l;
    int count = 0;
    vector<CRef> & negative = data.list(nl);

    for( int i = 0 ; i < negative.size(); ++i )
    {
      Clause& c = ca[ negative[i] ];
      //what if c can be deleted? -> continue
      if (c.can_be_deleted())
          continue;
      // sorted propagation, no!
      if ( !c.can_be_deleted() ) 
      {
        for ( int j = 0; j < c.size(); ++ j ) 
          if ( c[j] == nl ) 
          { 
	        if( debug_out ) cerr << "c UP remove " << nl << " from " << c << endl;
	        if (!sort) c.removePositionUnsorted(j);
            else c.removePositionSorted(j);
	        modifiedFormula = true;
	        count ++;
	        break;
	      }
	      // tell subsumption / strengthening about this modified clause
	      data.addSubStrengthClause(negative[i]);
      }
      // unit propagation
      if ( c.size() == 0 || (c.size() == 1 &&  solver->value( c[0] ) == l_False) ) 
      {
        data.setFailed();   // set state to false
        //-> this stops just the inner loop!
        //break;              // abort unit propagation
        if (debug_out) cerr << "c UNSAT by UP" << endl;
        processTime = cpuTime() - processTime;
        return l_False;
      } else if( c.size() == 1 ) 
      {
         if( solver->value( c[0] ) == l_Undef && debug_out ) 
             cerr << "c UP enqueue " << c[0] << " with previous value " 
                  << (solver->value( c[0] ) == l_Undef ? "undef" : (solver->value( c[0] ) == l_False ? "unsat" : " sat ") ) << endl;
	     if( solver->value( c[0] ) == l_Undef ) solver->uncheckedEnqueue(c[0]);
      }
    }
    // update formula data!
    vector<CRef>().swap(negative); // free physical scace of negative
    data.removedLiteral(nl, count, heap, ignore);
    removedLiterals += count;
  }
  
//    for (int i = 0; i < clause_list.size(); ++i)
//    {
//         Clause & c = ca[clause_list[i]];
//         int k = 0;
//         for (int l = 0; l < c.size(); ++l)
//         {
//             if (value(c[l]) != l_False)
//                 c[k++] = c[l];
//         }
//         c.shrink(c.size() - k);
//             if (c.has_extra())
//             c.calcAbstraction();
//    }
  solver->qhead = lastPropagatedLiteral;
  processTime = cpuTime() - processTime;
  return data.ok() ? l_Undef : l_False;
}

void Propagation::initClause( const CRef cr ) {}

void Propagation::printStatistics(ostream& stream)
{
  stream << "c [STAT] UP " << processTime << " s, " << lastPropagatedLiteral << " units, " << removedClauses << " cls, "
			    << removedLiterals << " lits" << endl;
}
