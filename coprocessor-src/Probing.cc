/**************************************************************************************[Probing.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Probing.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - PROBE";

static IntOption pr_uip            (_cat, "pr-uips",   "perform learning if a conflict occurs up to x-th UIP (-1 = all )", -1, IntRange(-1, INT32_MAX));
static BoolOption pr_double        (_cat, "pr-double", "perform double look-ahead",true);
static BoolOption pr_probe         (_cat, "pr-probe",  "perform probing",true);
static BoolOption pr_vivi          (_cat, "pr-vivi",   "perform clause vivification",true);
static IntOption pr_keepLearnts    (_cat, "pr-keepL",  "keep conflict clauses in solver (0=no,1=learnt,2=original)", 2, IntRange(0,2));
static IntOption pr_keepImplied    (_cat, "pr-keepI",  "keep clauses that imply on level 1 (0=no,1=learnt,2=original)", 2, IntRange(0,2));
static IntOption pr_viviPercent    (_cat, "pr-viviP",  "percent of max. clause size for clause vivification", 80, IntRange(0,100));
static IntOption pr_viviLimit      (_cat, "pr-viviL",  "step limit for clause vivification", 5000000,  IntRange(0, INT32_MAX));




static BoolOption debug_out            (_cat, "pr-debug", "debug output for probing",false);

Probing::Probing(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation, Solver& _solver)
: Technique( _ca, _controller )
, data( _data )
, solver( _solver )
, propagation ( _propagation )
, probeLimit(20000)
, processTime(0)
, l1implied(0)
, l1failed(0)
, l1learntUnit(0)
, l1ee(0)
, l2failed(0)
, l2ee(0)
, totalL2cand(0)
, probes(0)
, l2probes(0)
, viviTime(0)
, viviLits(0)
, viviCls(0)
, viviCands(0)
, viviLimits(pr_viviLimit)
, viviSize(0)
{

}

bool Probing::probe()
{
  MethodTimer mt( &processTime );
  
  if( ! pr_probe && ! pr_vivi ) return data.ok();
  
  // resetup solver
  reSetupSolver();
  
  data.ma.resize( data.nVars() * 2 );
  
  // safe the polarity for phase saving
  vec<char> polarity;
  solver.polarity.copyTo( polarity );

  // run probing?
  if( pr_probe ) {
    probing();
  }
  
  // run clause vivification?
  if( pr_vivi ) {
    clauseVivification();
  }

  // restore the polarity for phase saving
  polarity.copyTo(solver.polarity);
    
    
  // clean solver again!
  cleanSolver();
  
  sortClauses();
  if( propagation.propagate(data,true) == l_False){
    if( debug_out ) cerr << "c propagation failed" << endl;
    data.setFailed();
  }
  
  return data.ok();
}

CRef Probing::prPropagate( bool doDouble )
{
  // prepare tracking ternary clause literals
  doubleLiterals.clear();
  data.ma.nextStep();
  
  CRef    confl     = CRef_Undef;
    int     num_props = 0;
    solver.watches.cleanAll();

    if( debug_out ) cerr << "c head: " << solver.qhead << " trail elements: " << solver.trail.size() << endl;
    
    while (solver.qhead < solver.trail.size()){
        Lit            p   = solver.trail[solver.qhead++];     // 'p' is enqueued fact to propagate.
        vec<Solver::Watcher>&  ws  = solver.watches[p];
        Solver::Watcher        *i, *j, *end;
        num_props++;

	if( debug_out ) cerr << "c for lit " << p << " have watch with " << ws.size() << " elements" << endl;
	
        for (i = j = (Solver::Watcher*)ws, end = i + ws.size();  i != end;){
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (solver.value(blocker) == l_True){
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            CRef     cr        = i->cref;
            Clause&  c         = ca[cr];
            Lit      false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Solver::Watcher w     = Solver::Watcher(cr, first);
            if (first != blocker && solver.value(first) == l_True){
                *j++ = w; continue; }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (solver.value(c[k]) != l_False){
                    c[1] = c[k]; c[k] = false_lit;
                    solver.watches[~c[1]].push(w);
		    // track "binary clause" that has been produced by a ternary clauses!
		    if( doDouble && c.size() == 3 ) {
		      if( !data.ma.isCurrentStep(toInt(c[0])) ) {
			doubleLiterals.push_back(c[0]);
			data.ma.setCurrentStep(toInt(c[0]));
		      }
		      if( !data.ma.isCurrentStep(toInt(c[1])) ) {
			doubleLiterals.push_back(c[1]);
			data.ma.setCurrentStep(toInt(c[1]));
		      }
		    }
                    goto NextClause; }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (solver.value(first) == l_False){
                confl = cr;
                solver.qhead = solver.trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            }else
                solver.uncheckedEnqueue(first, cr);

        NextClause:;
        }
        ws.shrink(i - j);
    }


    return confl;
}

bool Probing::prAnalyze( CRef confl )
{
    learntUnits.clear();
    // do not do analysis -- return that nothing has been learned
    if( pr_uip == 0 && solver.decisionLevel() == 0) return false;
  
    // genereate learnt clause - extract all units!
    int pathC = 0;
    Lit p     = lit_Undef;
    unsigned uips = 0;
    
    // Generate conflict clause:
    //
    data.lits.clear();
    data.lits.push_back(lit_Undef);      // (leave room for the asserting literal)
    
    int index   = solver.trail.size() - 1;

    do{
        assert(confl != CRef_Undef); // (otherwise should be UIP)
        
        // take care of level 2 decision!
        if( confl != CRef_Undef ) {
        
	  Clause& c = ca[confl];

	  for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
	      Lit q = c[j];

	      if (!solver.seen[var(q)] && solver.level(var(q)) > 0){
		  solver.varBumpActivity(var(q));
		  solver.seen[var(q)] = 1;
		  if (solver.level(var(q)) >= solver.decisionLevel())
		      pathC++;
		  else
		      data.lits.push_back(q);
	      }
	  }
        
	}
        // Select next clause to look at:
        while (!solver.seen[var(solver.trail[index--])]);
        p     = solver.trail[index+1];
        confl = solver.reason(var(p));
        solver.seen[var(p)] = 0;
        pathC--;

	// this works only is the decision level is 1
	if( pathC == 0 ) {
	  if( solver.decisionLevel() == 1) {
	   if( debug_out ) cerr << "c possible unit: " << ~p << endl;
	   uips ++;
	   // learned enough uips - return. if none has been found, return false!
	   if( pr_uip != -1 && uips > pr_uip ) return learntUnits.size() == 0 ? false : true;
	   learntUnits.push(~p);
	  } else {
	    cerr << "[TODO] have more advanced learning routine for level 1 ?!?" << endl;
	    break;
	  }
	}
	
    }while (index >= solver.trail_lim[0] ); // pathC > 0 finds unit clauses
    data.lits[0] = ~p;
  
  // print learned clause
  if( debug_out ) {
    cerr << "c probing returned conflict: ";
    for( int i = 0 ; i < data.lits.size(); ++ i ) cerr << " " << data.lits[i];
    cerr << endl;
  }

  // NOTE no need to add the unit again, has been done in the loop already!
  
  return true;
}

bool Probing::prDoubleLook(Lit l1decision)
{
  // reject double lookahead, if necessary!
  if( ! pr_double ) return false;
  
  // TODO: sort literals in double lookahead queue, use only first n literals
  cerr << "sort literals in double queue according to some heuristic (occurrences, ... )" << endl;
  CRef thisConflict = CRef_Undef;
  
  for( int i = 0 ; i < doubleLiterals.size(); ++ i )
  {
    Var v = var(doubleLiterals[i]);
    if( solver.value(v) != l_Undef ) continue; // no need to check assigned variable!

    // cerr << "c test variable " << v + 1 << endl;

    assert( solver.decisionLevel() == 1 && "probing only at level 1!");
    const Lit posLit = doubleLiterals[i];
    cerr << "c deciding " << posLit << " at level 1" << endl;
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(posLit);
    l2probes++;
    probeLimit = probeLimit > 0 ? probeLimit - 1 : 0;
    thisConflict = prPropagate();
    if( debug_out ) cerr << "c conflict after level 2 propagate " << posLit << " present? " << (thisConflict != CRef_Undef) << " trail elements: " << solver.trail.size() << endl;
    if( thisConflict != CRef_Undef ) {
      l2failed++;
      prAnalyze( thisConflict );
      int backtrack_level = 1;
      if( data.lits.size() == 0 ) {
	data.lits.push_back(~l1decision);
	data.lits.push_back(~posLit);
      } else {
	if( data.lits.size() == 1 ) backtrack_level = 0;
	else backtrack_level = solver.level( var( data.lits[1] ) );
      }
      cerr << "c learnt clause at level " << solver.decisionLevel() << " with jump to " << backtrack_level << " after decideing " << l1decision << " and " << posLit << " ";
      for( int i = 0 ; i < data.lits.size(); ++i ) cerr << " " << data.lits[i];
      cerr << endl;
      solver.cancelUntil(backtrack_level);
      if (data.lits.size() == 1){
	solver.enqueue(data.lits[0]);
      }else{
	CRef cr = ca.alloc(data.lits, false);
	solver.clauses.push(cr);
	solver.attachClause(cr);
	data.addClause(cr);
	l2conflicts.push_back(cr);
	solver.uncheckedEnqueue(data.lits[0], cr);
      }
      solver.propagate();
      if( solver.decisionLevel() == 1 ) {i--;continue;}
      else return true;
    }
    solver.assigns.copyTo( prL2Positive );
    // other polarity
    solver.cancelUntil(1);
    
    assert( solver.decisionLevel() == 1 && "probing only at level 1!");
    const Lit negLit = ~posLit;
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(negLit);
    l2probes++;
    probeLimit = probeLimit > 0 ? probeLimit - 1 : 0;
    thisConflict = prPropagate();
    if( debug_out ) cerr << "c conflict after level 2 propagate " << negLit << " present? " << (thisConflict != CRef_Undef) << " trail elements: " << solver.trail.size() << endl;
    if( thisConflict != CRef_Undef ) {
      l2failed++;
      prAnalyze( thisConflict );
      int backtrack_level = 1;
      if( data.lits.size() == 0 ) {
	data.lits.push_back(~l1decision);
	data.lits.push_back(~negLit);
      } else {
	if( data.lits.size() == 1 ) backtrack_level = 0;
	else backtrack_level = solver.level( var( data.lits[1] ) );
      }
      cerr << "c learnt clause at level " << solver.decisionLevel() << " with jump to " << backtrack_level << " after decideing " << l1decision << " and " << negLit << " ";
      for( int i = 0 ; i < data.lits.size(); ++i ) cerr << " " << data.lits[i];
      cerr << endl;
      solver.cancelUntil(backtrack_level);
      if (data.lits.size() == 1){
	solver.enqueue(data.lits[0]);
      }else{
	CRef cr = ca.alloc(data.lits, true);
	solver.clauses.push(cr);
	solver.attachClause(cr);
	data.addClause(cr);
	l2conflicts.push_back(cr);
	solver.uncheckedEnqueue(data.lits[0], cr);
      }
      solver.propagate();
      if( solver.decisionLevel() == 1 ) {i--;continue;}
      else return true;
    } else {
      solver.cancelUntil(1);
    }
    // could copy to prNegatie here, but we do not do this, but refer to the vector inside the solver instead
    vec<lbool>& prNegative = solver.assigns;
    
    if( solver.decisionLevel() != 1 ) cerr << "c resulted in decision level " << solver.decisionLevel() << endl;
    assert( solver.decisionLevel() == 1 && "double look-ahead can only be continued at level 1!");
    
    // prepare new learnt clause!
    learntUnits.clear();
    learntUnits.push();
    learntUnits.push( ~l1decision );
    unsigned oldl2implieds = l2implieds.size();
    // look for necessary literals, and equivalent literals (do not add literal itself)
    for( int i = solver.trail_lim[1] + 1; i < solver.trail.size(); ++ i )
    {
      // check whether same polarity in both trails, or different polarity and in both trails
      const Var tv = var( solver.trail[ i ] );
      if( debug_out ) cerr << "c compare variable " << tv + 1 << ": pos = " << toInt(prL2Positive[tv]) << " neg = " << toInt(solver.assigns[tv]) << endl;
      if( solver.assigns[ tv ] == prL2Positive[tv] ) {
	if( debug_out ) cerr << "c l2 implied literal: " << l1decision << " -> " << solver.trail[i] << endl;
	learntUnits[0] = solver.trail[ i ];
	l2implied ++;
	CRef cr = ca.alloc(learntUnits, false);
	data.addClause(cr);
	l2implieds.push_back(cr);
      } 
      
    }
    
    solver.cancelUntil(1);
    
    if( debug_out ) {
      cerr << "c current trail: ";
      for( int k = 0 ; k < solver.trail.size(); ++ k )
	cerr << " " << solver.trail[k];
      cerr << endl;
    }
    
    // add all found implied literals!
    for( int i = oldl2implieds ; i < l2implieds.size(); ++i )
    {
      CRef cr = l2implieds[i];
      if( debug_out ) cerr << "c add clause " << ca[cr] << endl;
      solver.clauses.push(cr);
      solver.attachClause(cr);
      solver.uncheckedEnqueue(ca[cr][0], cr);
    }

    // propagate implied(necessary) literals on level 1 inside solver
    thisConflict = solver.propagate() ;
    if( thisConflict != CRef_Undef ) {
      // perform l1 analysis - as in usual probing routine
      l1failed++;
      if( !prAnalyze( thisConflict ) ) {
	learntUnits.push( ~posLit );
      }
      solver.cancelUntil(0);
      // tell system about new units!
      for( int i = 0 ; i < learntUnits.size(); ++ i ) data.enqueue( learntUnits[i] );
      if( !data.ok() ) break; // interrupt loop, if UNSAT has been found!
      l1learntUnit+=learntUnits.size();
      if( solver.propagate() != CRef_Undef ) { data.setFailed(); break; }
      
      return true; 
    }
    
  }
  
  // first decision is not a failed literal
  return false;
}

void Probing::cleanSolver()
{
  // clear all watches!
  for (int v = 0; v < solver.nVars(); v++)
    for (int s = 0; s < 2; s++)
      solver.watches[ mkLit(v, s) ].clear();

  solver.learnts_literals = 0;
  solver.clauses_literals = 0;
}

void Probing::reSetupSolver()
{
int j = 0;

    // check whether reasons of top level literals are marked as deleted. in this case, set reason to CRef_Undef!
    if( solver.trail_lim.size() > 0 )
      for( int i = 0 ; i < solver.trail_lim[0]; ++ i )
        if( solver.reason( var(solver.trail[i]) ) != CRef_Undef )
          if( ca[ solver.reason( var(solver.trail[i]) ) ].can_be_deleted() )
            solver.vardata[ var(solver.trail[i]) ].reason = CRef_Undef;

    // give back into solver
    for (int i = 0; i < solver.clauses.size(); ++i)
    {
        const CRef cr = solver.clauses[i];
        Clause & c = ca[cr];
	assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
        if (!c.can_be_deleted())
        {
            assert( c.mark() == 0 && "only clauses without a mark should be passed back to the solver!" );
            if (c.size() > 1)
            {
		// do not watch literals that are false!
		int j = 2;
		for ( int k = 0 ; k < 2; ++ k ) { // ensure that the first two literals are undefined!
		  if( solver.value( c[k] ) == l_False ) {
		    for( ; j < c.size() ; ++j )
		      if( solver.value( c[j] ) != l_False ) 
		        { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
		  }
		}
		assert( (solver.value( c[0] ) != l_False || solver.value( c[1] ) != l_False) && "Cannot watch falsified literals" );
                solver.attachClause(cr);
            }
            else if (solver.value(c[0]) == l_Undef)
                solver.uncheckedEnqueue(c[0]);
	    else if (solver.value(c[0]) == l_False )
	    {
	      // assert( false && "This UNSAT case should be recognized before re-setup" );
	      solver.ok = false;
	    }
        }
    }
    int c_old = solver.clauses.size();

    int learntToClause = 0;
    j = 0;
    for (int i = 0; i < solver.learnts.size(); ++i)
    {
        const CRef cr = solver.learnts[i];
        Clause & c = ca[cr];
        assert( c.size() != 0 && "empty clauses should be recognized before re-setup" );
        if (c.can_be_deleted()) continue;
	
        
	  assert( c.mark() == 0 && "only clauses without a mark should be passed back to the solver!" );
	  if (!c.learnt()) { // move subsuming clause from learnts to clauses
	    c.set_has_extra(true);
            c.calcAbstraction();
            learntToClause ++;
	    if (c.size() > 1)
            {
              solver.clauses.push(cr);
            }
 	  }
	
        if (c.size() > 1) {
	  // do not watch literals that are false!
	  int j = 2;
	  for ( int k = 0 ; k < 2; ++ k ) { // ensure that the first two literals are undefined!
	    if( solver.value( c[k] ) == l_False ) {
	      for( ; j < c.size() ; ++j )
		if( solver.value( c[j] ) != l_False ) 
		  { const Lit tmp = c[k]; c[k] = c[j]; c[j] = tmp; break; }
	    }
	  }
	  assert( (solver.value( c[0] ) != l_False || solver.value( c[1] ) != l_False) && "Cannot watch falsified literals" );
	  solver.attachClause(cr);
	} else if (solver.value(c[0]) == l_Undef)
          solver.uncheckedEnqueue(c[0]);
	else if (solver.value(c[0]) == l_False )
	{
	  // assert( false && "This UNSAT case should be recognized before re-setup" );
	  solver.ok = false;
	}
    }
    
   if( false ) {
      cerr << "c trail before probing: ";
      for( int i = 0 ; i< solver.trail.size(); ++i ) 
      {
	cerr << solver.trail[i] << " " ;
      }
      cerr << endl;
      
      if( true ) {
      cerr << "formula: " << endl;
      for( int i = 0 ; i < data.getClauses().size(); ++ i )
	if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
      for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
      }
      
      cerr << "c watch lists: " << endl;
      for (int v = 0; v < solver.nVars(); v++)
	  for (int s = 0; s < 2; s++) {
	    const Lit l = mkLit(v, s == 0 ? false : true );
	    cerr << "c watch for " << l << endl;
	    for( int i = 0; i < solver.watches[ l ].size(); ++ i ) {
	      cerr << ca[solver.watches[l][i].cref] << endl;
	    }
	  }
    }
}



void Probing::printStatistics( ostream& stream )
{
  stream << "c [STAT] PROBING(1) " 
  << processTime << " s, "
  <<  l1implied << " implied "
  <<  l1failed << " l1Fail, "
  <<  l1learntUnit << " l1Units "
  <<  l1ee << " l1EE "
  <<  l2failed << " l2Fail "
  <<  l2ee  << " l2EE "
  << endl;   
  
  stream << "c [STAT] PROBING(2) " 
  << totalL2cand << " l2cands "
  <<  probes << " probes "
  <<  l2probes << " l2probes "
  << probeLimit << " stepsLeft "
  << endl;   
  
  stream << "c [STAT] VIVI " 
  << viviTime << "  s, "
  <<  viviLits << " lits, "
  <<  viviCls << " cls, "
  << viviCands << " candidates, "
  << viviLimits << " stepsLeft, "
  << viviSize << " viviSize"
  << endl;   
}

void Probing::sortClauses()
{
  uint32_t clausesSize = solver.clauses.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    Clause& c = ca[solver.clauses[i]];
    if( c.can_be_deleted() ) continue;
    const uint32_t s = c.size();
    for (uint32_t j = 1; j < s; ++j)
    {
        const Lit key = c[j];
        int32_t i = j - 1;
        while ( i >= 0 && toInt(c[i]) > toInt(key) )
        {
            c[i+1] = c[i];
            i--;
        }
        c[i+1] = key;
    }
  }

  clausesSize = solver.learnts.size();
  for (int i = 0; i < clausesSize; ++i)
  {
    Clause& c = ca[solver.learnts[i]];
    if( c.can_be_deleted() ) continue;
    const uint32_t s = c.size();
    for (uint32_t j = 1; j < s; ++j)
    {
        const Lit key = c[j];
        int32_t i = j - 1;
        while ( i >= 0 && toInt(c[i]) > toInt(key) )
        {
            c[i+1] = c[i];
            i--;
        }
        c[i+1] = key;
    }
  }
}

void Probing::probing()
{
  
  // resize data structures
  prPositive.growTo( data.nVars() );
  
  
  CRef thisConflict = CRef_Undef;
  
  for( Var v = 0 ; v < data.nVars(); ++v ) variableHeap.push_back(v);
  
  cerr << "sort literals in probing queue according to some heuristic (BIG roots first, activity, ... )" << endl;
  
  // probe for each variable 
  Lit repeatLit = lit_Undef;
  while(  variableHeap.size() > 0 && !data.isInterupted()  && (probeLimit > 0 || data.unlimited()) && data.ok() )
  {
    // repeat variable, if demanded - otherwise take next from heap!
    Var v = var(repeatLit);
    if( repeatLit == lit_Undef ) {
      v = variableHeap.front();
      variableHeap.pop_front();
    }
    repeatLit = lit_Undef;
    
    if( solver.value(v) != l_Undef ) continue; // no need to check assigned variable!

    // cerr << "c test variable " << v + 1 << endl;

    assert( solver.decisionLevel() == 0 && "probing only at level 0!");
    const Lit posLit = mkLit(v,false);
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(posLit);
    probes++;
    probeLimit = probeLimit > 0 ? probeLimit - 1 : 0;
    thisConflict = prPropagate();
    if( debug_out ) cerr << "c conflict after propagate " << posLit << " present? " << (thisConflict != CRef_Undef) << " trail elements: " << solver.trail.size() << endl;
    if( thisConflict != CRef_Undef ) {
      l1failed++;
      if( !prAnalyze( thisConflict ) ) {
	learntUnits.push( ~posLit );
      }
      solver.cancelUntil(0);
      // tell system about new units!
      for( int i = 0 ; i < learntUnits.size(); ++ i ) data.enqueue( learntUnits[i] );
      if( !data.ok() ) break; // interrupt loop, if UNSAT has been found!
      l1learntUnit+=learntUnits.size();
      if( solver.propagate() != CRef_Undef ) { data.setFailed(); break; }
      continue;
    } else {
      totalL2cand += doubleLiterals.size();
      if( prDoubleLook(posLit) ) { repeatLit = posLit; continue; } // something has been found, so that second polarity has not to be propagated
    }
    solver.assigns.copyTo( prPositive );
    // other polarity
    solver.cancelUntil(0);
    
    assert( solver.decisionLevel() == 0 && "probing only at level 0!");
    const Lit negLit = mkLit(v,true);
    solver.newDecisionLevel();
    solver.uncheckedEnqueue(negLit);
    probes++;
    probeLimit = probeLimit > 0 ? probeLimit - 1 : 0;
    thisConflict = prPropagate();
    if( debug_out ) cerr << "c conflict after propagate " << negLit << " present? " << (thisConflict != CRef_Undef) << " trail elements: " << solver.trail.size() << endl;
    if( thisConflict != CRef_Undef ) {
      l1failed++;
      if( !prAnalyze( thisConflict ) ) {
	learntUnits.push( ~negLit );
      }
      solver.cancelUntil(0);
      // tell system about new units!
      for( int i = 0 ; i < learntUnits.size(); ++ i ) data.enqueue( learntUnits[i] );
      l1learntUnit+=learntUnits.size();
      if( !data.ok() ) break; // interrupt loop, if UNSAT has been found!
      if( solver.propagate() != CRef_Undef ) { data.setFailed(); break; }
      continue;
    } else {
      totalL2cand += doubleLiterals.size();
      if( prDoubleLook(negLit) ) { repeatLit = negLit; continue; } // something has been found, so that second polarity has not to be propagated
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
      if( debug_out )cerr << "c compare variable " << tv + 1 << ": pos = " << toInt(prPositive[tv]) << " neg = " << toInt(solver.assigns[tv]) << endl;
      if( solver.assigns[ tv ] == prPositive[tv] ) {
	if( debug_out )cerr << "c implied literal " << solver.trail[i] << endl;
	doubleLiterals.push_back(solver.trail[ i ] );
	l1implied ++;
      } else if( solver.assigns[ tv ] == l_True && prPositive[tv] == l_False ) {
	if( debug_out )cerr << "c equivalent literals " << negLit << " == " << solver.trail[i] << endl;
	data.lits.push_back( solver.trail[ i ] ); // equivalent literals
	l1ee++;
      } else if( solver.assigns[ tv ] == l_False && prPositive[tv] == l_True ) {
	if( debug_out )cerr << "c equivalent literals " << negLit << " == " << ~solver.trail[i] << endl;
	data.lits.push_back( ~solver.trail[ i ] ); // equivalent literals
	l1ee++;
      }
    }
    
    if( debug_out ) {
    cerr << "c trail: " << solver.trail.size() << " trail_lim[0] " << solver.trail_lim[0] << " trail_lim[1] " << solver.trail_lim[1] << endl;
    cerr << "c found implied literals " << doubleLiterals.size() << "  out of " << solver.trail.size() - solver.trail_lim[0] << endl;
    }
    
    solver.cancelUntil(0);
    // add all found implied literals!
    for( int i = 0 ; i < doubleLiterals.size(); ++i )
    {
      const Lit l = doubleLiterals[i];
      if( solver.value( l ) == l_False ) { data.setFailed(); break; }
      else if ( solver.value(l) == l_Undef ) {
	solver.uncheckedEnqueue(l);
      }
    }

    // propagate implied(necessary) literals inside solver
    if( solver.propagate() != CRef_Undef )
      data.setFailed();
    
    // tell coprocessor about equivalent literals
    if( data.lits.size() > 1 )
      data.addEquivalences( data.lits );
    
  }
  
  // TODO apply equivalent literals!
  if( data.getEquivalences().size() > 0 )
    cerr << "equivalent literal elimination has to be applied" << endl;
  
}



void Probing::clauseVivification()
{
  MethodTimer vTimer ( &viviTime );
  
  uint32_t maxSize = 0;
  for( uint32_t i = 0 ; i< data.getClauses().size() && (data.unlimited() || viviLimits>0); ++ i ) {
    const CRef ref = data.getClauses()[i];
    Clause& clause = ca[ ref ];
    if( clause.can_be_deleted() ) continue;
    maxSize = clause.size() > maxSize ? clause.size() : maxSize;
  }
  
  maxSize = (maxSize * pr_viviPercent) / 100;
  maxSize = maxSize > 2 ? maxSize : 3;	// do not process binary clauses, because they are used for propagation
  viviSize = maxSize;
  
  for( uint32_t i = 0 ; i< data.getClauses().size() && (data.unlimited() || viviLimits>0); ++ i ) {
    const CRef ref = data.getClauses()[i];
    Clause& clause = ca[ ref ];
    if( clause.can_be_deleted() ) continue;
    if( clause.size() < maxSize ) continue;
    
    assert (clause.size() > 2 && "clauses for vivification have to be larger than 2" );
    assert( solver.decisionLevel() == 0 && "asymmetric branching only at level 0!" );
    
    viviCands ++;
    if( debug_out ) cerr << "c work on clause " << clause << endl;
    bool hasAssigned = false;
    data.lits.clear();
    for( int j = 0 ; j < clause.size(); ++ j ) {
      data.lits.push_back( ~clause[j] );
      if( solver.value( data.lits[j]) != l_Undef ) { hasAssigned = true; break ; }
    }
    if( hasAssigned ) continue; // do not consider those clauses!
    
    if( data.randomized() ) {
        // shuffle!
	for( int j = 0; j + 1 < data.lits.size(); ++ j ) {
	  const unsigned tmp = rand() % data.lits.size() ;
	  const Lit tmpL = data.lits[j];
	  data.lits[j] =  data.lits[tmp];
	  data.lits[tmp] = tmpL;
	}
    } else {
      // TODO: find a nice heuristic here!! 
    }
    
    // remove one literal, because otherwise this would fail always!
    unsigned subClauseLimit = data.lits.size(); // usually, nothing can be removed
    Lit removedLit = data.lits[ data.lits.size() - 1 ];
    data.lits.pop_back();
    
    // calculate for all propagated literals!
    viviLimits = ( viviLimits > 0 ) ? viviLimits - 1 : 0;
    
    // detach this clause, so that it cannot be used for unit propagation
    solver.detachClause(ref, true);
    
    bool viviConflict = false;
    bool viviNextLit = false;
    solver.newDecisionLevel();
    for( int j = 0; j < data.lits.size(); ++ j ) {
      
      if( debug_out ) {
      cerr << "c literals to work with (curr=" << j << "): ";
      for( int k = 0 ; k < data.lits.size(); ++ k ) 
	cerr << " " << data.lits[k] << " @" << toInt(solver.value(data.lits[k]));
      cerr << endl;
      cerr << "c current trail: ";
      for( int k = 0 ; k < solver.trail.size(); ++ k )
	cerr << " " << solver.trail[k];
      cerr << endl;
      }
      
      // check whether one of the other literals is already "broken"
      for( int k = j; k < data.lits.size(); ++ k ) {
	if( solver.value(data.lits[k]) == l_False ) {
	  if( debug_out ) cerr << "c found failing literal " << data.lits[k] << endl;
	  data.lits[j] = data.lits[k];
	  subClauseLimit = j+1;
	  viviConflict = true;
	  break;
	} else if ( solver.value(data.lits[k]) == l_True ) {
	  if( debug_out ) cerr << "c found satisfied literal " << data.lits[k] << " (" << k << ") " << endl;
	  data.lits[k] = data.lits[ data.lits.size() - 1 ];
	  if( j == k ) --j;
	  --k;
	  data.lits.pop_back();
	  subClauseLimit --;
	  if( debug_out ) cerr << "c continue with lit(" << j << ") and " << data.lits.size() << " lits" << endl;
	  viviNextLit = true;
	  break; // TODO: performance could be improved by analysing literals from back to front! -> quadratic to linear?!
	}
      }
      if( viviConflict ) break; // found a subsuming sub clause -> stop
      else if ( viviNextLit ) continue; // found a tautologic literal -> continue with next literal!
      
      const Lit thisLit = data.lits[j];
      if( debug_out ) cerr << "c enqueue " << thisLit << endl;
      solver.uncheckedEnqueue(thisLit);
      
      if( debug_out ) {
	cerr << "c current trail (after enqueue): ";
	for( int k = 0 ; k < solver.trail.size(); ++ k )
	  cerr << " " << solver.trail[k];
	cerr << endl;
      }
      
      CRef confl = solver.propagate();
      if( confl != CRef_Undef ) {
	if( debug_out ) {
	  cerr << "c found conflict [" << confl << "]" << ca[confl] << endl;
	  cerr << "c current trail (after propagate): ";
	  for( int k = 0 ; k < solver.trail.size(); ++ k )
	    cerr << " " << solver.trail[k];
	  cerr << endl;
	}
	// clauselits[0] AND ... AND data.lits[j] -> false => new sub clause!
	subClauseLimit = j+1;
	if( debug_out )cerr << "c propagation resulted in conflict -> set lits set to " << j << endl;
	viviConflict = true;
	break;
      }
    } // stop such that there is nothing to be done with the clause any more!
    
    // jump back to clean trail
    solver.cancelUntil(0);
    
    if( subClauseLimit != clause.size() ) {
      // TODO: remove clause and data structures so that they meet the sub clause! 
      viviLits += clause.size() - subClauseLimit;
      viviCls ++;
      
      data.lits.resize(subClauseLimit);
      
      if( !viviConflict ) data.lits.push_back(removedLit);
      
      if( debug_out ) {
      cerr << "c replace clause " << clause << " with ";
      for( int j = 0 ; j < data.lits.size(); ++ j ) 
	cerr << " " << ~data.lits[j];
      cerr << endl;
      }
      
      data.ma.nextStep();
      for( int j = 0 ; j < data.lits.size(); ++ j )
	data.ma.setCurrentStep( toInt( ~data.lits[j] ) );
      
      int keptLits = 0;
      for( int j = 0 ; j < clause.size(); ++ j ) {
	// if literal is not there any more, remove clause index from structure
	if( !data.ma.isCurrentStep( toInt(clause[j]) ) ) {
	  data.removeClauseFrom(ref, clause[j]) ;
	} else {
	  // otherwise, keep literal inside clause
	  clause[keptLits++] = clause[j]; 
	}
      }
      clause.shrink( clause.size() - keptLits );
      clause.sort();
      if( debug_out ) cerr << "c new clause: " << clause << endl;
    }
    
    // detach this clause, so that it cannot be used for unit propagation
    if( clause.size() == 0 ) {
      data.setFailed();
    } else if ( clause.size() == 1 ) {
      data.enqueue( clause[0] );
    } else solver.attachClause(ref);

    if( !data.ok() ) break;
  } // end for clause in data.getClauses()
 
}
