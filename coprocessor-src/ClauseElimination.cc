/*****************************************************************************[ClauseElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/ClauseElimination.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - CCE";

static IntOption opt_steps  (_cat, "cp3_cce_steps",  "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX));
static IntOption opt_level  (_cat, "cp3_cce_level",  "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE", 3, IntRange(0, 3));

ClauseElimination::ClauseElimination(ClauseAllocator& _ca, ThreadController& _controller)
: Technique( _ca, _controller )
{

}

void ClauseElimination::eliminate(CoprocessorData& data)
{
  if( opt_level == 0 ) return; // do not run anything!
  WorkData wData;
  
  for( int i = 0 ; i < data.getClauses().size(); ++ i )
  {
    eliminate(data, wData, data.getClauses()[i] );
  }
}

bool ClauseElimination::eliminate(CoprocessorData& data, ClauseElimination::WorkData& wData, Minisat::CRef cr)
{
  assert(opt_level > 0 && "level needs to be higher than 0 to run elimination" );
  // put all literals of the clause into the mark-array and the other vectors!
  const Clause& c = ca[cr];
  for( int i = 0; i < c.size(); ++ i ) {
    const Lit& l = c[i];
    wData.array.setCurrentStep( toInt(l) );
    wData.cla.push_back(l);
    if( !data.doNotTouch(var(l)) ) wData.toProcess.push_back(l);        // only if semantics is not allowed to be changed!
  }
  
  bool doRemoveClause = false;
  while( true ) {
    
    int oldProcessSize = wData.toProcess.size();
    for( int i = 0 ; i < wData.toProcess.size(); ++i ) {                          // do ALA for all literals in the array (sometimes also again!)
      const Lit l = wData.toProcess[i];
      const vector<CRef>& lList = data.list(l);
      for( int j = 0 ; j < lList.size(); ++ j ) { // TODO: add step counter here!
        if( lList[j] == cr ) continue;                                            // to not work on the same clause twice!
	const Clause& cj = ca[lList[j]];
	if( cj.size() > wData.toProcess.size() + 1 ) continue;                    // larger clauses cannot add a contribution to the array!
	Lit alaLit = lit_Undef;
	for( int k = 0 ; k < cj.size(); ++k ) {                                   // are all literals of that clause except one literal inside the array?
	  const Lit& lk = cj[k];
	  if( wData.array.isCurrentStep(toInt(lk)) ) continue;                    // literal is already in array
          else if (alaLit != lit_Undef ) { alaLit = lit_Error; break; }           // more than one hitting literal
          else alaLit = lk;                                                       // literal which can be used to extend the array
	}
	if( alaLit == lit_Undef ) { fprintf(stderr, "c CLA-clause %d is subsumed by some other clause %d!\n",cr,lList[j]); continue; }
	if( wData.array.isCurrentStep( toInt(~alaLit) ) ) {                       // complement of current literal is also in CLA(F,C)
	  doRemoveClause = true; break;
	} else {
	  wData.array.setCurrentStep( toInt(alaLit) );
	  if( !data.doNotTouch(var(alaLit)) ) wData.toProcess.push_back(alaLit);  // only if semantics is not allowed to be changed!
	}
      }
      if( doRemoveClause  ) break;
    } // tested all literals for ALA
    if( oldProcessSize == wData.toProcess.size() || opt_level < 2) break;         // no new literals found (or parameter does not allow for CLA)
    
    int oldClaSize = wData.cla.size();                                            // remember old value to check whether something happened!
    bool firstClause = true;
    for( int i = wData.nextAla ; i < wData.toProcess.size(); ++i ) {              // do CLA for all literals that have not been tested yet!
      const Lit l = wData.toProcess[i];
      const vector<CRef>& lList = data.list(l);
      int beforeCla = wData.cla.size();
      for( int j = 0 ; j < lList.size(); ++ j ) { // TODO: add step counter here!
        const Clause& cj = ca[lList[i]];
	if( firstClause ) {                                                       // first clause fills the array, the other shrink it again
          for( int j = 0 ; j < cj.size(); ++j ) {
	    if( wData.array.isCurrentStep( toInt(~cj[j] ) ) ) {
	      wData.cla.resize( oldClaSize );
	      goto ClaNextClause;
	    } else {
	      wData.cla.push_back(cj[j]);
	    }
	  }
	  firstClause = false;
	} else { // not the first clause
// TODO: not necessary?!          //int resetTo = wData.cla.size();  // store size of cla before this clause to be able to reset it
	  wData.helpArray.nextStep();
          for( int j = 0 ; j < cj.size(); ++j ){   // tag all literals inside the clause
            if( wData.array.isCurrentStep( toInt(~cj[j]) ) )  // this clause produces a tautological resolvent
	      goto ClaNextClause;
	    wData.helpArray.setCurrentStep( toInt(cj[j]) );
	  }

	  int current = oldClaSize;                                      // store current size of cla, so that it can be undone
	  for( int j = oldClaSize; j < wData.cla.size(); ++ j )          // only use literals inside cla that are tagged!
	    if( wData.helpArray.isCurrentStep( toInt(wData.cla[j]) ) )   // if the literal has been in the clause
	      wData.cla[current++] = wData.cla[j];                       // move the literal inside the cla vector
	  if( current == oldClaSize ) { // stop cla, because the set is empty
	    wData.cla.resize(oldClaSize); break;
	  }
	  wData.cla.resize( current );        // remove all other literals from the vector!

          // TODO: take care of the cla vector
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
    } // processed all literals inside the processed vector
    // TODO: if nothing new was found, break!
    if( oldClaSize == wData.cla.size() || doRemoveClause) break;  // nothing found, or ATE with the clause
  } // end: while(true)
  
  // take care of the ALA vector, check for ATE, done already above
    
  // if not ATE, try to check BCE with mark array!
  if( ! doRemoveClause && opt_level > 2) {
   // TODO: go for ABCE 
    
  }
  // TODO: put undo to real undo information!
  if( doRemoveClause ) {
    
  }
  
  // sketch of the method:
  // as long as possible, do ala!
    // always check for ATE!
  // then, do cla
    // always check for ATE!
  // finally, test abce!
  // if something happened, push to undo
}


void ClauseElimination::parallelElimination(CoprocessorData& data)
{
  assert( false && "Method not yet implemented" );
}

void* ClauseElimination::runParallelElimination(void* arg)
{
  assert( false && "Method not yet implemented" );
}

