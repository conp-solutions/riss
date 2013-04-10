/*************************************************************************************[Unhiding.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Unhiding.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - UNHIDE";

static IntOption  opt_uhdIters     (_cat, "cp3_uhdIters",     "Number of iterations for unhiding", 3, IntRange(0, INT32_MAX));
static BoolOption opt_uhdTrans     (_cat, "cp3_uhdTrans",     "Use Transitive Graph Reduction (buggy)", false);
static BoolOption opt_uhdUHLE      (_cat, "cp3_uhdUHLE",      "Use Unhiding+Hidden Literal Elimination", true);
static BoolOption opt_uhdUHTE      (_cat, "cp3_uhdUHTE",      "Use Unhiding+Hidden Tautology Elimination", true);
static BoolOption opt_uhdNoShuffle (_cat, "cp3_uhdNoShuffle", "Do not perform randomized graph traversation", false);
static BoolOption opt_uhdEE        (_cat, "cp3_uhdEE",        "Use equivalent literal elimination (buggy)", false);

#if defined CP3VERSION  
static const int opt_uhdDebug = 0;
#else
static IntOption  opt_uhdDebug     (_cat, "cp3_uhdDebug",     "Debug Level of Unhiding", 0, IntRange(0, 3));
#endif


Unhiding::Unhiding(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation, Subsumption& _subsumption, EquivalenceElimination& _ee)
: Technique( _ca, _controller )
, data( _data )
, propagation( _propagation )
, subsumption( _subsumption )
, ee( _ee )
, uhdTransitive(opt_uhdTrans)
, unhideIter(opt_uhdIters)
, doUHLE(opt_uhdUHLE)
, doUHTE(opt_uhdUHTE)
, uhdNoShuffle(opt_uhdNoShuffle)
, uhdEE(opt_uhdEE)
, removedClauses(0)
, removedLiterals(0)
, removedLits(0)
, unhideTime(0)
{

}

static void shuffleVector( Lit* adj, const int adjSize )
{
  for( uint32_t i = 0 ;  i+1<adjSize; ++i ) {
    const uint32_t rnd = rand() % adjSize;
    const Lit tmp = adj[i];
    adj[i] = adj[rnd];
    adj[rnd] = tmp;
  }
}
// uhdNoShuffle

uint32_t Unhiding::linStamp( const Lit literal, uint32_t stamp, bool& detectedEE )
{
  int32_t level = 0; // corresponds to position of current literal in stampQueue
  
  // lines 1 to 4 of paper
  stamp ++;
  stampInfo[ toInt( literal ) ].dsc = stamp;
  stampInfo[ toInt( literal ) ].obs = stamp;
  // bool flag = true;
  Lit* adj = big.getArray(literal);
  if (!uhdNoShuffle) shuffleVector( adj, big.getSize(literal) );
  stampEE.push_back(literal);
  stampQueue.push_back(literal);
  level ++;
  while( level >= unhideEEflag.size() ) unhideEEflag.push_back(1);
  unhideEEflag[level] = 1;
  // end lines 1-4 of paper
  
  
  while( !stampQueue. empty() )
  {
    const Lit l = stampQueue.back();
    const Lit* adj = big.getArray(l);
    const int adjSize = big.getSize(l);
    /*
    cerr << "c [UHD-A] current literal " << l << " at level " << level << " and index " << stampInfo[ toInt(l) ].index << "/" << adjSize << endl;
    cerr << "c adj:";
    for( uint32_t i = 0 ; i < adjSize ; ++ i ) cerr << " " << adj[i];
    cerr << endl;
    cerr << "c lastSeen=" << stampInfo[ toInt(l) ].lastSeen << endl;
    */
    
    // recover index, if some element in the adj of l has been removed by transitive edge reduction
    if( adjSize > 0 && stampInfo[ toInt(l) ].lastSeen != lit_Undef && stampInfo[ toInt(l) ].index > 0 && adj[stampInfo[ toInt(l) ].index-1] != stampInfo[ toInt(l) ].lastSeen )
    {
      for( stampInfo[ toInt(l) ].index = 0; stampInfo[ toInt(l) ].index < adjSize; stampInfo[ toInt(l) ].index ++ ) {
	if( adj[stampInfo[ toInt(l) ].index] == stampInfo[ toInt(l) ].lastSeen ) break;
      }
      stampInfo[ toInt(l) ].index ++ ;
      assert( adj[stampInfo[ toInt(l) ].index-1] == stampInfo[ toInt(l) ].lastSeen );
    }
    
    for( ; stampInfo[ toInt(l) ].index < adjSize; )
    {
      
      const Lit l1 = adj[stampInfo[ toInt(l) ].index];
      // cerr <<"c [UHD-A] child: " << l1 << " @" <<  stampInfo[ toInt(l) ].index << "/" << adjSize << endl;
      ++ stampInfo[ toInt(l) ].index;
      
      if( uhdTransitive && stampInfo[ toInt(l) ].dsc < stampInfo[ toInt(l1) ].obs ) {
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] l.dsc=" << stampInfo[ toInt(l) ].dsc << " l1.obs=" << stampInfo[ toInt(l1) ].obs << endl;
	data.removedClause( ~l, l1 );
	modifiedFormula = true;
	big.removeEdge(~l, l1);
	if( opt_uhdDebug > 1 ) { cerr << "c [UHD-A] remove transitive edge " << ~l << "," << l1 << " and reduce index to " << stampInfo[ toInt(l) ].index - 1 << endl; }
	-- stampInfo[ toInt(l) ].index;
	continue;
      }
      
      if( stampInfo[ toInt( stampInfo[ toInt(l) ].root) ].dsc <= stampInfo[ toInt( ~l1 ) ].obs ) {
	Lit lfailed=l;
	while( stampInfo[ toInt(lfailed) ].dsc > stampInfo[ toInt( ~l1 ) ].obs ) lfailed = stampInfo[ toInt(lfailed) ].parent;
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] found failed literal " << lfailed << " enqueue " << ~lfailed << endl;
	if( l_False == data.enqueue( ~lfailed ) ) {  return stamp; }
	
	if( stampInfo[ toInt( ~l1 ) ].dsc != 0 && stampInfo[ toInt( ~l1 ) ].fin == 0 ) continue;
      }
      
      if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] enqueue " << l1 << " with dsc=" << stampInfo[ toInt(l1) ].dsc << " index=" << stampInfo[ toInt(l1) ].index << " state ok? " << data.ok() << endl;
      if( stampInfo[ toInt(l1) ].dsc == 0 ) {
	stampInfo[ toInt(l) ].lastSeen = l1; // set information if literals are pushed back
	stampInfo[ toInt(l1) ].parent = l;
	stampInfo[ toInt(l1) ].root = stampInfo[ toInt(l) ].root;
	/*
	stamp = recStamp( l1, stamp, detectedEE );
	*/
	// lines 1 to 4 of paper
	stamp ++;
	stampInfo[ toInt(l1) ].dsc = stamp;
	stampInfo[ toInt(l1) ].obs = stamp;
	assert( stampInfo[ toInt(l1) ].index == 0 && "if the literal is pushed on the queue, it should not been visited already" );
	// do not set index or last seen, because they could be used somewhere below in the path already!
	Lit* adj = big.getArray(l1);
	if (!uhdNoShuffle) shuffleVector( adj, big.getSize(l1) );
	stampEE.push_back(l1);
	stampQueue.push_back(l1);
	level ++;
	while( level >= unhideEEflag.size() ) unhideEEflag.push_back(1);
	unhideEEflag[level] = 1;
	// end lines 1-4 of paper
	
	goto nextUHDqueueElement;
      }

      // detection of EE is moved to below!
      if( level > 0 ) { // if there are still literals on the queue
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] no special case, check EE and stamp!" << endl;
	if( uhdEE && stampInfo[ toInt(l1) ].fin == 0 && stampInfo[ toInt(l1) ].dsc < stampInfo[ toInt(l) ].dsc  && !data.doNotTouch( var(l) ) && !data.doNotTouch( var(l1) )  ) {
	  stampInfo[ toInt(l) ].dsc = stampInfo[ toInt(l1) ].dsc;
	  if( opt_uhdDebug > 1 ) cerr << "c found equivalent literals " << l << " and " << l1 << ", set flag at level " << level << " to 0!" << endl;
	  unhideEEflag[level] = 0;
	  detectedEE = true;
	}
      }
      stampInfo[ toInt(l) ].obs = stamp;
    }
    
    if( stampInfo[ toInt(l) ].index >= adjSize )
    {
      // do EE detection on current level
      stampClassEE.clear();
      Lit l1 = lit_Undef;
      if( unhideEEflag[level] == 1 || level == 1) {
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] collect EE literals" << endl;
	stamp ++;
	do {
	  l1 = stampEE.back();
	  stampEE.pop_back();
	  stampClassEE.push_back( l1 );
	  stampInfo[ toInt(l1) ].dsc = stampInfo[ toInt(l) ].dsc;
	  stampInfo[ toInt(l1) ].fin = stamp;
	} while( l1 != l );
	if( stampClassEE.size() > 1 ) {
	  // also collect all the literals from the current path
	  if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] found eq class of size " << stampClassEE.size() << endl;
	  data.addEquivalences( stampClassEE );
	    
	  detectedEE = true;
	  }
      }
      
      // do detection for EE on previous level
      
      stampQueue.pop_back(); // pop the current literal
      level --;
      if( level > 0 ) { // if there are still literals on the queue
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] return to level " << level << " and literal " << stampQueue.back() << endl;
	Lit parent = stampQueue.back();
	assert( parent == stampInfo[ toInt(l) ].parent && "parent of the node needs to be previous element in stamp queue" );
	
	// do not consider do-not-touch variables for being equivalent!
	if( uhdEE && stampInfo[ toInt(l) ].fin == 0 && stampInfo[ toInt(l) ].dsc < stampInfo[ toInt(parent) ].dsc && !data.doNotTouch( var(l) ) && !data.doNotTouch( var(parent) ) ) {
	  stampInfo[ toInt(parent) ].dsc = stampInfo[ toInt(l) ].dsc;
	  if( opt_uhdDebug > 1 ) cerr << "c found equivalent literals " << parent << " and " << l << ", set flag at level " << level << " to 0!" << endl;
	  unhideEEflag[level] = 0;
	  detectedEE = true;
	}
      }
      stampInfo[ toInt(l) ].obs = stamp;
    } else {
      assert(false); 
    }
    

  
    nextUHDqueueElement:;
  }
  
  assert( level == 0 && "after leaving the sub tree, the level has to be 0" );
  
  return stamp;
}


uint32_t Unhiding::recStamp( const Lit l, uint32_t stamp, bool& detectedEE )
{
  stamp ++;
  stampInfo[ toInt(l) ].dsc = stamp;
  stampInfo[ toInt(l) ].obs = stamp;
  bool flag = true;
  Lit* adj = big.getArray(l);
  const int adjSize = big.getSize(l);
  stampEE.push_back(l);
  
  for( uint32_t i = 0 ; i < adjSize; ++ i )
  {
    const Lit l1 = adj[i];
    if( stampInfo[ toInt(l) ].dsc < stampInfo[ toInt(l1) ].obs ) {
      data.removedClause( ~l, l1);
      big.removeEdge(~l, l1); // automatically done by removeClause
      modifiedFormula = true;
      i --;
      continue;
    }
    
    if( stampInfo[ toInt( stampInfo[ toInt(l) ].root) ].dsc <= stampInfo[ toInt( ~l1 ) ].obs ) {
      Lit lfailed=l;
      while( stampInfo[ toInt(lfailed) ].dsc > stampInfo[ toInt( ~l1 ) ].obs ) lfailed = stampInfo[ toInt(lfailed) ].parent;
      if( l_False == data.enqueue( ~lfailed ) ) {  return stamp; }
      
      if( stampInfo[ toInt( ~l1 ) ].dsc != 0 && stampInfo[ toInt( ~l1 ) ].fin == 0 ) continue;
    }
    
    if( stampInfo[ toInt(l1) ].dsc == 0 ) {
      stampInfo[ toInt(l1) ].parent = l;
      stampInfo[ toInt(l1) ].root = stampInfo[ toInt(l) ].root;
      stamp = recStamp( l1, stamp, detectedEE );
    }
    
    if( stampInfo[ toInt(l1) ].fin == 0 && stampInfo[ toInt(l1) ].dsc < stampInfo[ toInt(l) ].dsc ) {
      stampInfo[ toInt(l) ].dsc = stampInfo[ toInt(l1) ].dsc;
      flag = false;
    }
    
    stampInfo[ toInt(l1) ].obs = stamp;
    
  }
  
  stampClassEE.clear();
  Lit l1 = lit_Undef;
  if( flag ) {
    stamp ++;
    do {
      l1 = stampEE.back();
      stampEE.pop_back();
      stampClassEE.push_back( l1 );
      stampInfo[ toInt(l1) ].dsc = stampInfo[ toInt(l) ].dsc;
      stampInfo[ toInt(l1) ].fin = stamp;
    } while( l1 != l );
    if( stampClassEE.size() > 1 ) {
      if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] found eq class of size " << stampClassEE.size() << endl;
	data.addEquivalences( stampClassEE );
      }
  }
  return stamp;
}

uint32_t Unhiding::stampLiteral( const Lit literal, uint32_t stamp, bool& detectedEE )
{
  stampQueue.clear();
  stampEE.clear();
  unhideEEflag.clear();
  
  // do nothing if a literal should be stamped again, but has been stamped already
  if( stampInfo[ toInt( literal ) ].dsc != 0 ) return stamp;
  
  return linStamp(literal,stamp,detectedEE);
}

void Unhiding::sortStampTime( Lit* literalArray, const uint32_t size )
{
  // insertion sort
  const uint32_t s = size;
  for (uint32_t j = 1; j < s; ++j)
  {
        const Lit key = literalArray[j];
	const uint32_t keyDsc = stampInfo[ toInt(key) ].dsc;
        int32_t i = j - 1;
        while ( i >= 0 && stampInfo[ toInt(literalArray[i]) ].dsc > keyDsc )
        {
            literalArray[i+1] = literalArray[i];
            i--;
        }
        literalArray[i+1] = key;
  }
}

bool Unhiding::unhideSimplify()
{
  bool didSomething = false;

  if( opt_uhdDebug > 1 ) {
    cerr << "c STAMP info: " << endl;
    for( Var v = 0; v < data.nVars(); ++ v ) {
      Lit lit = mkLit(v,false);
      bool show = true;
      show = stampInfo[ toInt(lit) ].dsc + 1 != stampInfo[ toInt(lit) ].fin || show;
      lit=~lit;
      show = stampInfo[ toInt(lit) ].dsc + 1 != stampInfo[ toInt(lit) ].fin || show;

      if( show ) {
	lit=~lit;
	cerr << "c " << lit << " : \tdsc=" << stampInfo[ toInt(lit) ].dsc << " \tfin=" << stampInfo[ toInt(lit) ].fin << " \tobs=" << stampInfo[ toInt(lit) ].obs << " \troot=" << stampInfo[ toInt(lit) ].root << " \tprt=" << stampInfo[ toInt(lit) ].parent << endl;
	lit=~lit;
	cerr << "c " << lit << " : \tdsc=" << stampInfo[ toInt(lit) ].dsc << " \tfin=" << stampInfo[ toInt(lit) ].fin << " \tobs=" << stampInfo[ toInt(lit) ].obs << " \troot=" << stampInfo[ toInt(lit) ].root << " \tprt=" << stampInfo[ toInt(lit) ].parent << endl;
      }
    }
  }
  if( opt_uhdDebug > 1 ) {
    cerr << "c active formula" << endl;
    for( uint32_t i = 0 ; i < data.getClauses().size(); ++ i ) 
    {
      // run UHTE before UHLE !!  (remark in paper)
      const uint32_t clRef =  data.getClauses()[i] ;
      Clause& clause = ca[clRef];
      if( clause.can_be_deleted() ) continue;
      cerr << clause << endl;
    }
  }
  
  // removes ignored clauses, destroys mark-to-delete clauses
  uint32_t j = 0;
  
  // TODO implement second for loop to iterate also over learned clauses and check whether one of them is not learned!
  
  for( uint32_t i = 0 ; i < data.getClauses().size() && !data.isInterupted() ; ++ i ) 
  {
    // run UHTE before UHLE !!  (remark in paper)
    const uint32_t clRef =  data.getClauses()[i] ;
    Clause& clause = ca[clRef];
    if( clause.can_be_deleted() ) continue;

    if( opt_uhdDebug > 0 ) { cerr << "c [UHD] work on " << clause << " state ok? " << data.ok() << endl; }
    
    const uint32_t cs = clause.size();
    Lit Splus  [cs];
    Lit Sminus [cs];

    for( uint32_t ci = 0 ; ci < cs; ++ ci ) {
      Splus [ci] = clause[ci];
      Sminus[ci] = ~clause[ci];
    }
    sortStampTime( Splus , cs );
    sortStampTime( Sminus, cs );
    
    if( opt_uhdDebug > 1 ) {
      if( opt_uhdDebug > 1 ) {
	cerr << "c Splus: " << endl;
	for(uint32_t p = 0; p < cs; ++ p) {
	  cerr << p << " " << Splus[p] << " dsc=" << stampInfo[ toInt(Splus[p]) ].dsc << " fin=" << stampInfo[ toInt(Splus[p]) ].fin << endl;
	} ;
	cerr << "c Sminus: " << endl;
	for(uint32_t n = 0; n < cs; ++ n) {
	  cerr << n << " " << Sminus[n] << " dsc=" << stampInfo[ toInt(Sminus[n]) ].dsc << " fin=" << stampInfo[ toInt(Sminus[n]) ].fin << endl;
	} ;
      }
      
      for( uint32_t ci = 0 ; ci + 1 < cs; ++ ci )  {
	assert( stampInfo[ toInt(Splus[ci])  ].dsc <= stampInfo[ toInt(Splus[ci+1]) ].dsc && "literals have to be sorted" );
	assert( stampInfo[ toInt(Sminus[ci]) ].dsc <= stampInfo[ toInt(Sminus[ci+1])].dsc && "literals have to be sorted" );
      }
    }
    
    // initial positions in the arrays
    uint32_t pp = 0, pn = 0;

    if( doUHTE ){
      bool UHTE = false;
      // UHTE( clause ), similar to algorithm in paper
      Lit lpos = Splus [pp];
      Lit lneg = Sminus[pn];
      
      if( opt_uhdDebug > 1 ) {
	cerr << "c Splus: " << endl;
	for(uint32_t p = 0; p < cs; ++ p) {
	  cerr << p << " " << Splus[p] << " dsc=" << stampInfo[ toInt(Splus[p]) ].dsc << " fin=" << stampInfo[ toInt(Splus[p]) ].fin << endl;
	} ;
	cerr << "c Sminus: " << endl;
	for(uint32_t n = 0; n < cs; ++ n) {
	  cerr << n << " " << Sminus[n] << " dsc=" << stampInfo[ toInt(Sminus[n]) ].dsc << " fin=" << stampInfo[ toInt(Sminus[n]) ].fin << endl;
	} ;
      }
      
      while( true )
      {
	// line 4
	if( stampInfo[  toInt(lneg)  ].dsc > stampInfo[  toInt(lpos)  ].dsc ) {
	  if( pp + 1 == cs ) break;
	  lpos = Splus[ ++ pp ];
	// line 7
	} else if ( 
		(stampInfo[ toInt(lneg) ].fin < stampInfo[ toInt(lpos) ].fin )
		|| 
		( cs == 2 && 
				( (lpos == ~lneg || stampInfo[ toInt(lpos) ].parent == lneg)   || (stampInfo[ toInt(~lneg) ].parent == ~lpos ) )
		) 
		  )
	{
	  if( pn + 1 == cs ) break;
	  lneg = Sminus [ ++ pn ];
	} else { 
	  if( opt_uhdDebug > 1 ) cerr << "c UHTE: lneg=" << lneg << " lpos=" << lpos << " finStamps (neg<pos): " << stampInfo[ toInt(lneg) ].fin << " < " << stampInfo[ toInt(lpos) ].fin << "  pos=neg.complement: " << lpos << " == " << ~lneg << " or lpos.par == lneg: " << stampInfo[ toInt(lpos) ].parent << " == " << lneg << endl;
	  UHTE = true; break; 
	}
      }

      if( UHTE ) {
	clause.set_delete( true );
	modifiedFormula = true;
	if( opt_uhdDebug > 1 ){  cerr << "c [UHTE] remove " << clause << cerr << endl; }
	data.removedClause(clRef);
	if( clause.size() == 2 ) big.removeEdge(clause[0],clause[1]);
	// if a clause has been removed, call
	removedClauses ++;
	removedLiterals += clause.size();
	didSomething = true;
	continue;
      }
    }

    if( doUHLE != 0 ) {
      // C = UHLE(C)
      bool UHLE=false;
      
      if( doUHLE == 1 || doUHLE == 3 )
      {
	pp = cs;
	uint32_t finished = stampInfo[ toInt(Splus[cs-1]) ].fin;
	Lit finLit = Splus[cs-1];
	
	for(pp = cs-1 ; pp > 0; -- pp ) {
	  const Lit l = Splus[ pp - 1];
	  const uint32_t fin = stampInfo[  toInt(l)  ].fin;
	  if( fin > finished ) {
	    if( opt_uhdDebug > 0 ){  cerr << "c [UHLE-P] remove " << l << " because finish time of " << finLit << " from " << clause << endl; }
	    data.removeClauseFrom( clRef, l );
	    data.removedLiteral(l);
	    removedLits++;
	    modifiedFormula = true;
	    if( clause.size() == 2 ) big.removeEdge(clause[0],clause[1]);
	    clause.remove_lit(l);
	    // tell subsumption / strengthening about this modified clause
	    data.addSubStrengthClause(clRef);
	    // if you did something useful, call
	    removedLiterals ++;
	    didSomething = true;
	    UHLE=true;
	  } else {
	    finished = fin;
	    finLit = l;
	  }
	}
      }
      
      if( doUHLE == 2 || doUHLE == 3 )
      {
	const uint32_t csn = clause.size();
	if( csn != cs ) { // if UHLEP has removed literals, recreate the Sminus list
	  for( uint32_t ci = 0 ; ci < csn; ++ ci ) {
	    Sminus[ci] = ~clause[ci];
	  }
	  sortStampTime( Sminus, csn );
	}
	
	if( opt_uhdDebug > 1 ) {
	 for(pn = 0; pn < csn; ++ pn) {
	   cerr << pn << " " << Sminus[pn] << " " << stampInfo[ toInt(Sminus[pn]) ].dsc << " - " << stampInfo[ toInt(Sminus[pn]) ].fin << endl;
	 }
	}
	
	pn = 0;
	uint32_t finished = stampInfo[ toInt(Sminus[0]) ].fin;
	if( opt_uhdDebug > 1 ) cerr << "c set finished to " << finished << " with lit " << Sminus[0] << endl;
	Lit finLit = Sminus[ 0 ];
	for(pn = 1; pn < csn; ++ pn) {
	  const Lit l = Sminus[ pn ];
	  const uint32_t fin = stampInfo[  toInt(l)  ].fin;
	  if( fin < finished ) {
	    if( opt_uhdDebug > 1 ){  cerr << "c [UHLE-N] remove " << ~l << " because of fin time " << fin << " of " <<  l << " and finLit " << finLit << " [" << finished << "] from " << clause << endl; }
	    data.removeClauseFrom(clRef,~l);
	    data.removedLiteral(~l);
	    modifiedFormula = true;
	    if( clause.size() == 2 ) big.removeEdge(clause[0],clause[1]);
	    clause.remove_lit( ~l );
	    removedLits++;
	    // tell subsumption / strengthening about this modified clause
	    data.addSubStrengthClause(clRef);
	    // if you did something useful, call
	    removedLiterals ++;
	    didSomething = true;
	    UHLE=true;
	  } else {
	    finished = fin;
	    finLit = l;
	    if( opt_uhdDebug > 1 ) cerr << "c set finished to " << finished << " with lit " << l << endl;
	  }
	}
      }
      
      if( UHLE ) { 
	data.updateClauseAfterDelLit( clause );
	if( clause.size() == 0 ) {
	  data.setFailed(); return didSomething;
	} else {
	  if( clause.size() == 1 ) {
	    if( l_False == data.enqueue(clause[0] ) ) { return didSomething; }
	  }
	}
      }
    }
    
  } // end for clause in formula
  
  // shrink formula again!
  // cerr << "c UHD reduce to " << j << " from " << formula->size() << endl;
  // formula->resize(j);
  
  return didSomething;
}

void Unhiding::process (  )
{
  MethodTimer unhideTimer( &unhideTime );
  modifiedFormula = false;
  if( !data.ok() ) return;
  
  stampInfo.resize( 2*data.nVars() );
  unhideEEflag.resize( 2*data.nVars() );
  
  for( uint32_t iteration = 0 ; iteration < unhideIter; ++ iteration )
  {
    // TODO: either re-create BIG, or do clause modifications after algorithm finished
    // be careful here - do not use learned clauses, because they could be removed, and then the whole mechanism breaks
    big.recreate(ca, data, data.getClauses() );    
    
    if( opt_uhdDebug > 2 ) {
      cerr << "c iteration " << iteration << " formula, state ok? " << data.ok() << endl;
      for( int i = 0 ; i < data.getClauses().size(); ++ i ) {
	if( !ca[ data.getClauses()[i] ].can_be_deleted() ) cerr << ca[ data.getClauses()[i] ] << endl;
      }
    }
    
    bool foundEE = false;
    uint32_t stamp = 0 ;
    
    // represents rool literals!
    data.lits.clear();
    // reset all present variables, collect all roots in binary implication graph
    for ( Var v = 0 ; v < data.nVars() && !data.isInterupted() ; ++ v ) 
    {
      for( int p = 0 ; p < 2; ++ p ) {
	const Lit pos =  mkLit(v, p == 1);
	stampInfo [ toInt(pos) ].dsc = 0;
	stampInfo [ toInt(pos) ].fin = 0;
	stampInfo [ toInt(pos) ].obs = 0;
	stampInfo [ toInt(pos) ].index = 0;
	stampInfo [ toInt(pos) ].lastSeen = lit_Undef;
	stampInfo [ toInt(pos) ].root = pos;
	stampInfo [ toInt(pos) ].parent = pos;
	if( big.getSize(pos) == 0 ) data.lits.push_back(~pos);
      }
    }
    
    // do stamping for all roots, shuffle first
    const uint32_t ts = data.lits.size();
    if( !uhdNoShuffle ) {
      for( uint32_t i = 0 ; i < ts; i++ ) { const uint32_t rnd=rand()%ts; const Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd]=tmp; }
    }
    // stamp shuffled data.lits
    for ( uint32_t i = 0 ; i < ts; ++ i ) 
    {
      stamp = stampLiteral(data.lits[i],stamp,foundEE);
    }
    // stamp all remaining literals, after shuffling
    data.lits.clear();
    for ( Var v = 0 ; v < data.nVars(); ++ v ) 
    {
      const Lit pos =  mkLit(v,false);
      if( stampInfo [ toInt(pos) ].dsc == 0 ) data.lits.push_back(pos);
      const Lit neg =  ~pos;
      if( stampInfo [ toInt(neg) ].dsc == 0 ) data.lits.push_back(neg);
    }

    // stamp shuffled data.lits
    const uint32_t ts2 = data.lits.size();
    if( !uhdNoShuffle ) {
      for( uint32_t i = 0 ; i < ts2; i++ ) { const uint32_t rnd=rand()%ts2; const Lit tmp = data.lits[i]; data.lits[i] = data.lits[rnd]; data.lits[rnd]=tmp; }
    }
    for ( uint32_t i = 0 ; i < ts2; ++ i ) 
    {
      if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] call stamping for literal " << data.lits[i] << ", dsc=" << stampInfo [ toInt(data.lits[i]) ].dsc << endl;
      stamp = stampLiteral(data.lits[i],stamp,foundEE);
    }
    
    if( opt_uhdDebug > 0 ) cerr << "c [UHD] foundEE: " << foundEE << endl;
    
    // propagating might destroy the information of the stamps?
    if( false &&  data.ok() ) {
      if( data.hasToPropagate() ) {
	if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] run UP before simplification, state ok? " << data.ok() << endl;
	propagation.process(data,true);
	modifiedFormula = modifiedFormula || propagation.appliedSomething();
      }
    }
    
    if( data.ok() && unhideSimplify() ) {
      if( data.ok() ) {
	if( data.hasToPropagate() ) {
	  if( opt_uhdDebug > 1 ) cerr << "c [UHD-A] run UP before simplification" << endl;
	  propagation.process(data,true);
	  modifiedFormula = modifiedFormula || propagation.appliedSomething();
	}
      } else {
	if( opt_uhdDebug > 0 ) cerr << "c [UHD] ok: " << data.ok() << endl;
      }
    }
    
    // run independent of simplify method
    
    if( foundEE ) {
      if( opt_uhdDebug > 1 ) cerr << "c [UHD] call equivalence elimination" << endl;
      if( data.getEquivalences().size() > 0 ) {
	modifiedFormula = modifiedFormula || ee.appliedSomething();
	ee.applyEquivalencesToFormula(data);
      }
    }
    
  } // next iteration ?!

  if( opt_uhdDebug > 2 ) {
    cerr << "c final formula with solver state " << data.ok() << endl;
    for( int i = 0 ; i < data.getClauses().size(); ++ i ) {
      if( !ca[ data.getClauses()[i] ].can_be_deleted() ) cerr << "(" << data.getClauses()[i] << ") " << ca[ data.getClauses()[i] ] << endl;
    }
  }
  
  return;

}


void Unhiding::printStatistics( ostream& stream )
{
  stream << "c [STAT] UNHIDE " 
  << unhideTime << " s, "
  << removedClauses << " cls, "
  << removedLiterals << " totalLits, "
  << removedLits << " lits "
  << endl;   
}


void Unhiding::destroy()
{
  big.~BIG();
  vector<literalData>().swap( stampInfo );
	  
  /// queue of literals that have to be stamped in the current function call
  deque< Lit >().swap(  stampQueue );
  /// equivalent literals during stamping
  vector< Lit >().swap(  stampEE );
  vector< Lit >().swap(  stampClassEE );
  vector< char >().swap(  unhideEEflag );
}
