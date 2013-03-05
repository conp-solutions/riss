/*****************************************************************************[ClauseElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/ClauseElimination.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - CCE";

static IntOption opt_steps         (_cat, "cp3_cce_steps", "Number of steps that are allowed per iteration", INT32_MAX, IntRange(-1, INT32_MAX));
static IntOption opt_level         (_cat, "cp3_cce_level", "none, ALA+ATE, CLA+ATE, ALA+CLA+BCE", 3, IntRange(0, 3));
static IntOption opt_ccePercent    (_cat, "cp3_cce_sizeP", "percent of max. clause size for clause elimination (excluding)", 40, IntRange(0,100));


#if defined CP3VERSION  
static const int debug_out = 0;
#else
static IntOption debug_out (_cat, "cce-debug", "debug output for clause elimination",0, IntRange(0,4) );
#endif

static const int cceLevel = 1;

ClauseElimination::ClauseElimination(ClauseAllocator& _ca, ThreadController& _controller)
: Technique( _ca, _controller )
, steps(0)
, processTime(0)
, removedClauses(0)
, removedBceClauses(0)
, removedNonEEClauses(0)
, cceSize(0)
, candidates(0)
{

}

void ClauseElimination::process(CoprocessorData& data)
{
  processTime = cpuTime() - processTime;
  modifiedFormula = false;
  if( !data.ok() ) return;
  // TODO: have a better scheduling here! (if a clause has been removed, potentially other clauses with those variables can be eliminated as well!!, similarly to BCE!)
  if( opt_level == 0 ) return; // do not run anything!

  uint32_t maxSize = 0;
  for( uint32_t i = 0 ; i< data.getClauses().size() ; ++ i ) {
    const CRef ref = data.getClauses()[i];
    Clause& clause = ca[ ref ];
    if( clause.can_be_deleted() ) continue;
    maxSize = clause.size() > maxSize ? clause.size() : maxSize;
  }
  
  cceSize = (maxSize * opt_ccePercent) / 100;
  cceSize = cceSize > 2 ? cceSize : 3;	// do not process binary clauses
  if( debug_out > 0 ) cerr << "c work on clauses larger than " << cceSize << " maxSize= " << maxSize << endl;
  WorkData wData( data.nVars() );
  for( int i = 0 ; i < data.getClauses().size(); ++ i )
  {
    if( ca[ data.getClauses()[i] ].size() <= cceSize ) continue; // work only on very large clauses to eliminate them!
    if( ca[ data.getClauses()[i] ].can_be_deleted() ) continue;
    if( debug_out > 1) cerr << "c work on clause " << ca[ data.getClauses()[i] ] << endl;
    candidates ++;
    eliminate(data, wData, data.getClauses()[i] );
    if( !data.unlimited() && steps > opt_steps ) break;
  }
  steps += wData.steps;
  removedBceClauses += wData.removedBceClauses;
  removedClauses += wData.removedClauses;
  removedNonEEClauses += wData.removedNonEEClauses;
  processTime = cpuTime() - processTime;
}

bool ClauseElimination::eliminate(CoprocessorData& data, ClauseElimination::WorkData& wData, Minisat::CRef cr)
{
  assert(opt_level > 0 && "level needs to be higher than 0 to run elimination" );
  // put all literals of the clause into the mark-array and the other vectors!
  const Clause& c = ca[cr];
  if( c.can_be_deleted() ) return false;
  if( debug_out > 2 ) cerr << "c process " << c << endl;
  wData.reset();
  data.log.log( cceLevel, "do cce on", c );
  for( int i = 0; i < c.size(); ++ i ) {
    const Lit& l = c[i];
    wData.array.setCurrentStep( toInt(l) );
    wData.cla.push_back(l);
    if( !data.doNotTouch(var(l)) ) wData.toProcess.push_back(l);        // only if semantics is not allowed to be changed!
/*
    wData.toUndo.push_back(lit_Undef);
    wData.toUndo.push_back(l);
    cerr << "c CLA add to current extension: " << l << endl;
    for( int k = 0 ; k < beforeCla; ++ k ) {
      if( l != wData.cla[k] ) wData.toUndo.push_back( wData.cla[k] );
      */
  }
  
  int oldProcessSize = -1; // initially, do both ALA and CLA!
  bool doRemoveClause = false;
  bool preserveEE = true;
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
    
    bool foundMoreThanOneClause = false;
    int outerOldClaSize = wData.cla.size();
    data.log.log(cceLevel,"start process iteration with cla size", wData.cla.size() );
    for( int i = wData.nextAla ; i < wData.toProcess.size(); ++i ) {              // do CLA for all literals that have not been tested yet!
      bool firstClause = true;
      
      int oldClaSize = wData.cla.size(); // remember old value to check whether something happened!
      const Lit l = ~wData.toProcess[i];
      if( debug_out > 2 )  cerr << "c work on literal " << l << endl;
      assert( l != lit_Undef && l != lit_Error && "only real literals can be in the queue" );
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
	      data.log.log(cceLevel,"reject, because tautology",cj);
	      if( debug_out ) cerr << "c reduce CLA back to old size, because literal " << cj[j] << " leads to tautology " << endl;
	      goto ClaNextClause;
	    } else if( !wData.array.isCurrentStep( toInt( cj[j] ) ) ) { // add only, if not already in!
	      // FIXME: ensure that a literal is not added twice!
	      wData.cla.push_back(cj[j]);
	      if( debug_out ) cerr << "c add " << cj[j] << " to CLA" << endl;
	    }
	  }
	  if( wData.cla.size() == beforeCla ) goto ClaNextClause; // no new literal -> jump to next clause
	  firstClause = false; // only had first clause, if not tautology!
	  data.log.log(cceLevel,"increased(/kept) cla to",wData.cla.size());
	} else { // not the first clause
          data.log.log(cceLevel,"analyze as following",cj);
// TODO: not necessary?!          //int resetTo = wData.cla.size();  // store size of cla before this clause to be able to reset it
	  wData.helpArray.nextStep();
          for( int j = 0 ; j < cj.size(); ++j ){   // tag all literals inside the clause
	     if( cj[j] == l ) continue; // do not tag the literal itself!
            if( wData.array.isCurrentStep( toInt(~cj[j]) ) ){  // this clause produces a tautological resolvent
	      data.log.log(cceLevel,"reject, because tautology",cj);	      
	      if( debug_out ) cerr << "c reduce CLA back to old size, because literal " << cj[j] << " leads to tautology " << endl;
	      goto ClaNextClause;
	    }
	    wData.helpArray.setCurrentStep( toInt(cj[j]) );
	  }
	  foundMoreThanOneClause = true;
	  int current = oldClaSize;                                      // store current size of cla, so that it can be undone
	  for( int j = oldClaSize; j < wData.cla.size(); ++ j )          // only use literals inside cla that are tagged!
	    if( wData.helpArray.isCurrentStep( toInt(wData.cla[j]) ) ){   // if the literal has been in the clause
	      data.log.log( cceLevel, "keep in cla this time", wData.cla[j] );
              wData.cla[current++] = wData.cla[j];                       // move the literal inside the cla vector
	    }
	  if( current == oldClaSize ) { // stop cla, because the set is empty
	    if( debug_out > 0 ) cerr << "c do not add any literals under current literal" << endl;
	    wData.cla.resize(oldClaSize); break;
	  } else {
	    if( debug_out > 0 ) cerr << "c kept " << current - oldClaSize << " literals" << endl;
	  }
	  wData.cla.resize( current );        // remove all other literals from the vector!
	}
ClaNextClause:;
      } // processed all clauses of current literal inside of CLA
      if( debug_out && !foundMoreThanOneClause ) cerr << "c found only one resolvent clause -- could do variable elimination!" << endl;
      // TODO: if something new has been found inside CLA, add it to the toBeProcessed vector, run ALA again!
      if( beforeCla < wData.cla.size() ) {
	preserveEE = false;
	// push current cla including the literal that led to it to the undo information
	wData.toUndo.push_back(lit_Undef);
	wData.toUndo.push_back( ~l );
	if( debug_out ) cerr << "c CLA add to current extension: " << ~l << endl;
	for( int k = 0 ; k < beforeCla; ++ k ) {
	  if( ~l != wData.cla[k] ) wData.toUndo.push_back( wData.cla[k] );
	  if( wData.array.isCurrentStep( toInt(~wData.cla[k]) ) ) { 
	    doRemoveClause = true;
	    if( debug_out )  cerr << "c found remove clause because it became tautology" << endl;
	  }
	}
	// these lines have been added after the whole algorithm!
	for( int k = beforeCla; k < wData.cla.size(); ++ k ) {
	  wData.toProcess.push_back( wData.cla[k] );
	  wData.array.setCurrentStep( toInt(wData.cla[k]) );
	}
	if( debug_out ) {
	  cerr << "c current undo: " << endl;
	  for( int k = 1 ; k < wData.toUndo.size(); ++ k ) {  
	    if( wData.toUndo[k] == lit_Undef ) cerr << endl;
	    else cerr << " " << wData.toUndo[k];
	  }
	  cerr << endl;
	}
      }
      if( doRemoveClause ) break;
      oldProcessSize = wData.toProcess.size();                    // store the size, so that CLA can recognize whether there is more to do
    } // processed all literals inside the processed vector
    data.log.log(cceLevel,"end process iteration with cla size", wData.cla.size() );
    if( outerOldClaSize == wData.cla.size() || doRemoveClause) break;  // nothing found, or ATE with the clause -> stop
  } // end: while(true)
  
  // take care of the ALA vector, check for ATE, done already above
    
  // if not ATE, try to check BCE with mark array!
  if( ! doRemoveClause && opt_level > 2) {
    // TODO: go for ABCE 
    for( int i = 0 ; i < wData.cla.size() ; ++i ) {
       if( doRemoveClause = markedBCE( data, wData.cla[i], wData.array ) ) {
	 preserveEE = false;
	 wData.toUndo.push_back(lit_Undef);
	 wData.toUndo.push_back(wData.cla[i]);
	 cerr << "c BCE sucess on literal " << wData.cla[i] << endl;
	 for( int j = 0 ; j < wData.cla.size(); ++ j )
	   if( wData.cla[i] != wData.cla[j] ) wData.toUndo.push_back( wData.cla[j] );
	 break; // need only removed by BCE once
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
    if( debug_out > 1 ) cerr << "clause " << ca[cr] << " is removed by cce" << endl;
    data.log.log( cceLevel, "cce was able to remove clause", ca[cr]);
    ca[cr].set_delete(true);
    data.removedClause( cr );
    assert( (wData.toUndo.size() > 0 || preserveEE ) && "should not add empty undo information to extension!");
    if( wData.toUndo.size() > 0 ) data.addExtensionToExtension( wData.toUndo );
    wData.removedClauses ++;
    if( !preserveEE ) wData.removedNonEEClauses ++;
    modifiedFormula = true;
    return true;
  } else {
    if( debug_out > 2 )  cerr << "clause " << ca[cr] << " cannot be removed by cce" << endl;
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

void ClauseElimination::destroy()
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
			    << removedNonEEClauses << " nonEE-cls, "
			    << removedBceClauses << " bce-cls, "
			    << steps << " steps "
			    << cceSize << " rejectSize "
			    << candidates << " candidates "
			    << endl;
}
