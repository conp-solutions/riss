/*****************************************************************************[ClauseElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/ClauseElimination.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - CCE";

static IntOption opt_steps  (_cat, "cp3_cce_steps",  "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX));
static IntOption opt_level  (_cat, "cp3_cce_level",  "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE", 3, IntRange(0, 3));

static const int cceLevel = 1;

ClauseElimination::ClauseElimination(ClauseAllocator& _ca, ThreadController& _controller)
: Technique( _ca, _controller )
, steps(0)
, processTime(0)
, removedClauses(0)
, removedBceClauses(0)
{

}

void ClauseElimination::eliminate(CoprocessorData& data)
{
  processTime = cpuTime() - processTime;
  // TODO: have a better scheduling here! (if a clause has been removed, potentially other clauses with those variables can be eliminated as well!!, similarly to BCE!)
  if( opt_level == 0 ) return; // do not run anything!
  WorkData wData( data.nVars() );
  for( int i = 0 ; i < data.getClauses().size(); ++ i )
  {
    eliminate(data, wData, data.getClauses()[i] );
    if( !data.unlimited() && steps > opt_steps ) break;
  }
  steps += wData.steps;
  removedBceClauses += wData.removedBceClauses;
  removedClauses += wData.removedClauses;
  processTime = cpuTime() - processTime;
}

bool ClauseElimination::eliminate(CoprocessorData& data, ClauseElimination::WorkData& wData, Minisat::CRef cr)
{
  assert(opt_level > 0 && "level needs to be higher than 0 to run elimination" );
  // put all literals of the clause into the mark-array and the other vectors!
  const Clause& c = ca[cr];
  if( c.can_be_deleted() ) return false;
  wData.reset();
  data.log.log( cceLevel, "do cce on", c );
  for( int i = 0; i < c.size(); ++ i ) {
    const Lit& l = c[i];
    wData.array.setCurrentStep( toInt(l) );
    wData.cla.push_back(l);
    if( !data.doNotTouch(var(l)) ) wData.toProcess.push_back(l);        // only if semantics is not allowed to be changed!
  }
  
  int oldProcessSize = -1; // initially, do both ALA and CLA!
  bool doRemoveClause = false;
  while( true ) {
    
    for( int i = 0 ; i < wData.toProcess.size(); ++i ) {                          // do ALA for all literals in the array (sometimes also again!)
      const Lit l = wData.toProcess[i];
      assert( l != lit_Undef && l != lit_Error && "only real literals can be in the queue" );
      data.log.log(cceLevel, "ALA check list of literal",l);
      const vector<CRef>& lList = data.list(l);
      for( int j = 0 ; j < lList.size(); ++ j ) { // TODO: add step counter here!
        if( lList[j] == cr ) continue;                                            // to not work on the same clause twice!
	const Clause& cj = ca[lList[j]];
	if( cj.can_be_deleted() ) continue;                                       // act as the clause is not here
	wData.steps ++;
	data.log.log(cceLevel,"analyze",cj);
	if( cj.size() > wData.toProcess.size() + 1 ) continue;                    // larger clauses cannot add a contribution to the array!
	Lit alaLit = lit_Undef;
	for( int k = 0 ; k < cj.size(); ++k ) {                                   // are all literals of that clause except one literal inside the array?
	  const Lit& lk = cj[k];
	  if( wData.array.isCurrentStep(toInt(lk)) ) continue;                    // literal is already in array
          else if (wData.array.isCurrentStep(toInt(~lk)) ) { alaLit = lit_Error; break; } // resolvent would be a tautology
          else if (alaLit != lit_Undef ) { alaLit = lit_Error; break; }           // more than one hitting literal
          else alaLit = lk;                                                       // literal which can be used to extend the array
	}
	if( alaLit == lit_Error ) continue; // to many additional literals inside the clause
	else if( alaLit == lit_Undef ) { doRemoveClause=true; break; }
	alaLit = ~alaLit;
	data.log.log(cceLevel, "found ALA literal",alaLit);
	if( wData.array.isCurrentStep( toInt(alaLit) ) ) continue;
	if( wData.array.isCurrentStep( toInt(~alaLit) ) ) {                       // complement of current literal is also in CLA(F,C)
	  doRemoveClause = true; break;
	} else {
	  data.log.log(cceLevel,"add literal to ALA", cj , alaLit);
	  wData.array.setCurrentStep( toInt(alaLit) );
	  if( !data.doNotTouch(var(alaLit)) ) wData.toProcess.push_back(alaLit);  // only if semantics is not allowed to be changed!
          // i = -1; break; // need to redo whole calculation, because one more literal has been added!
	  // exit(0);
	}
      }
      if( doRemoveClause  ) { 
	data.log.log(cceLevel,"ALA found tautology",c);
	break;
      }
    } // tested all literals for ALA
    if( oldProcessSize == wData.toProcess.size() || opt_level < 2) break;         // no new literals found (or parameter does not allow for CLA)
    
    int oldClaSize = wData.cla.size();                                            // remember old value to check whether something happened!
    bool firstClause = true;
    data.log.log(cceLevel,"start process iteration with cla size", wData.cla.size() );
    for( int i = wData.nextAla ; i < wData.toProcess.size(); ++i ) {              // do CLA for all literals that have not been tested yet!
      const Lit l = ~wData.toProcess[i];
      data.log.log(cceLevel,"CLA check literal", l);
      data.log.log(cceLevel,"start inner iteration with cla size", wData.cla.size() );
      const vector<CRef>& lList = data.list(l);
      int beforeCla = wData.cla.size();
      for( int j = 0 ; j < lList.size(); ++ j ) { // TODO: add step counter here!
        const Clause& cj = ca[lList[j]];
	if( cj.can_be_deleted() ) continue;                                       // act as the clause is not here
	wData.steps ++;
	if( firstClause ) {                                                       // first clause fills the array, the other shrink it again
          data.log.log(cceLevel,"analyze as first",cj);
          for( int j = 0 ; j < cj.size(); ++j ) {
	    if( cj[j] == l ) continue;
	    if( wData.array.isCurrentStep( toInt(~cj[j] ) ) ) {
	      wData.cla.resize( oldClaSize );
	      goto ClaNextClause;
	    } else {
	      wData.cla.push_back(cj[j]);
	    }
	  }
	  // cerr << "c increased cla to " << wData.cla.size() << endl;
	  firstClause = false;
	} else { // not the first clause
          data.log.log(cceLevel,"analyze as following",cj);
// TODO: not necessary?!          //int resetTo = wData.cla.size();  // store size of cla before this clause to be able to reset it
	  wData.helpArray.nextStep();
          for( int j = 0 ; j < cj.size(); ++j ){   // tag all literals inside the clause
            if( wData.array.isCurrentStep( toInt(~cj[j]) ) )  // this clause produces a tautological resolvent
	      goto ClaNextClause;
	    wData.helpArray.setCurrentStep( toInt(cj[j]) );
	  }

	  int current = oldClaSize;                                      // store current size of cla, so that it can be undone
	  for( int j = oldClaSize; j < wData.cla.size(); ++ j )          // only use literals inside cla that are tagged!
	    if( wData.helpArray.isCurrentStep( toInt(wData.cla[j]) ) ){   // if the literal has been in the clause
	      data.log.log( cceLevel, "keep in cla this time", wData.cla[j] );
              wData.cla[current++] = wData.cla[j];                       // move the literal inside the cla vector
	    }
	  if( current == oldClaSize ) { // stop cla, because the set is empty
	    wData.cla.resize(oldClaSize); break;
	  }
	  wData.cla.resize( current );        // remove all other literals from the vector!
	}
ClaNextClause:;
      } // processed all clauses of current literal inside of CLA
      // TODO: if something new has been found inside CLA, add it to the toBeProcessed vector, run ALA again!
      if( beforeCla < wData.cla.size() ) {
	// push current cla including the literal that led to it to the undo information
	wData.toUndo.push_back(lit_Undef);
	wData.toUndo.push_back(l);
	for( int i = 0 ; i < beforeCla; ++ i ) {
	  if( l != wData.cla[i] ) wData.toUndo.push_back( wData.cla[i] );
	  if( wData.array.isCurrentStep( toInt(~wData.cla[i]) ) ) { doRemoveClause = true; }
	}
      }
      if( doRemoveClause ) break;
      oldProcessSize = wData.toProcess.size();                    // store the size, so that CLA can recognize whether there is more to do
    } // processed all literals inside the processed vector
    data.log.log(cceLevel,"end process iteration with cla size", wData.cla.size() );
    if( oldClaSize == wData.cla.size() || doRemoveClause) break;  // nothing found, or ATE with the clause -> stop
  } // end: while(true)
  
  // take care of the ALA vector, check for ATE, done already above
    
  // if not ATE, try to check BCE with mark array!
  if( ! doRemoveClause && opt_level > 2) {
    // TODO: go for ABCE 
    for( int i = 0 ; i < wData.cla.size() ; ++i ) {
       if( doRemoveClause = markedBCE( data, wData.cla[i], wData.array ) ) {
	 wData.toUndo.push_back(lit_Undef);
	 wData.toUndo.push_back(wData.cla[i]);
	 for( int j = 0 ; j < wData.cla.size(); ++ j )
	   if( wData.cla[i] != wData.cla[j] ) wData.toUndo.push_back( wData.cla[j] );
       }
    }
    if( doRemoveClause ) {
      // TODO: take care that BCE clause is also written to undo information! 
      data.log.log( cceLevel, "cce with BCE was able to remove clause", ca[cr]);
      wData.removedBceClauses ++;
    }
  }
  // TODO: put undo to real undo information!
  if( doRemoveClause ) {
    data.log.log( cceLevel, "cce was able to remove clause", ca[cr]);
    ca[cr].set_delete(true);
    data.removedClause( cr );
    data.addToExtension( wData.toUndo );
    wData.removedClauses ++;
    return true;
  } else {
    return false; 
  }
}

bool ClauseElimination::markedBCE(const CoprocessorData& data, const Lit& l, const MarkArray& array)
{
  const Lit nl = ~l;
  for( int i = 0 ; i < data.list(nl).size(); ++ i )
    if( ! markedBCE(~l, ca[ data.list(nl)[i] ], array ) ) return false;
  return true;
}

bool ClauseElimination::markedBCE(const Lit& l, const Clause& c, const MarkArray& array)
{
  for( int i = 0 ; i < c.size(); ++ i ) 
    if( c[i] != l && array.isCurrentStep( toInt( ~c[i] )) ) return true;
  return false;
}

void ClauseElimination::initClause(const Minisat::CRef cr)
{

}

void ClauseElimination::parallelElimination(CoprocessorData& data)
{
  assert( false && "Method not yet implemented" );
}

void* ClauseElimination::runParallelElimination(void* arg)
{
  assert( false && "Method not yet implemented" );
}

void ClauseElimination::printStatistics(ostream& stream)
{
  stream << "c [STAT] CCE " << processTime << " s, " 
			    << removedClauses << " cls, " 
			    << removedBceClauses << " nonEE-cls, "
			    << steps << " steps"
			    << endl;
}