/**************************************************************************************[Circuit.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Circuit.h"

static const char* _cat = "CP3 CIRCUIT";

// options
static BoolOption opt_ExO      (_cat, "cp3_extExO",      "extract ExO gates", true);
static BoolOption opt_genAND   (_cat, "cp3_genAND",      "extract generic AND gates", true);
static BoolOption opt_HASUM    (_cat, "cp3_extHASUM",    "extract half adder sum bit", true);
static BoolOption opt_BLOCKED  (_cat, "cp3_extBlocked",  "extract gates, that can be found by blocked clause addition", true);
static BoolOption opt_NegatedI (_cat, "cp3_extNgtInput", "extract gates, where inputs come from the same variable", true);
static BoolOption opt_Implied  (_cat, "cp3_extImplied",  "extract half adder sum bit", true);

using namespace Coprocessor;

const static bool debug_out = false; // print output to screen

Circuit::Circuit(ClauseAllocator& _ca)
: ca (_ca)
{}

int Circuit::extractGates(CoprocessorData& data, vector< Circuit::Gate >& gates)
{
  // create BIG
  big = new BIG( );
  big->create(ca,data,data.getClauses(),data.getLEarnts());
  
  if( opt_Implied ) big->generateImplied(data);
  
  // create the list of all ternary / quadrary clauses
  ternaries.resize( data.nVars() * 2 );
  quads.resize( data.nVars() * 2 );
  for ( int v = 0 ; v < 2*data.nVars(); ++v )
  {
    ternaries[v].clear();
    quads[v].clear();
  }
  for ( int p = 0 ; p < 2; ++ p ) {
    vec<CRef>& list = p == 0 ? data.getClauses() : data.getLEarnts() ;
    for( int i = 0 ; i < list.size(); ++ i ) {
      const Clause& c = ca[ list[i] ];
      if( c.can_be_deleted() ) continue;
      if( c.size() ==3 ) {
	for( int j = 0 ; j < 3; ++ j )
	  ternaries[ toInt(c[j]) ].push_back( ternary(j==0 ? c[2] : c[0] ,
                                                      j==1 ? c[2] : c[1]) );
      } else if (c.size() == 4 ) {
	for( int j = 0 ; j < 4; ++ j ) 
	  quads[ toInt(c[j]) ].push_back( quad( j == 0 ? c[3] : c[0],
						 j == 1 ? c[3] : c[1],
					         j == 2 ? c[3] : c[2]) );
      }
    }
  }
  
  // try to find all gates with output variable v for all the variables  
  for ( Var v = 0 ; v < data.nVars(); ++v )
    getGatesWithOutput(v,gates, data);
}

void Circuit::getGatesWithOutput(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{
  data.ma.resize(2*data.nVars());
  if( opt_ExO) getExOGates(v,gates, data);

  getANDGates(v,gates, data);
  getITEGates(v,gates, data);
  getXORGates(v,gates, data);
  
  if( opt_HASUM ) getHASUMGates(v,gates, data);
  if( debug_out ) cerr << "c after variable " << v+1 << " found " << gates.size() << " gates" << endl;
}

void Circuit::getANDGates(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{
  // check for v <-> A and B
  for( int p = 0; p < 2 ; ++ p ) {
    data.ma.nextStep();
    data.lits.clear();
    int blockedSize = -1; // size of the AND gate that has been found with a blocked gate
    Lit pos = mkLit( v, p == 1 ); // pos -> a AND b AND ... => binary clauses [-pos, a],[-pos,b], ...
    Lit* list = big->getArray(pos);
    const int listSize = big->getSize(pos);
    data.ma.setCurrentStep( toInt(~pos) ); 
    // find all binary clauses for gate with positive output "pos"
    for( int i = 0 ; i < listSize; ++i ) {
     data.ma.setCurrentStep( toInt(list[i]) ); 
     data.lits.push_back( list[i] );
    }
    if( data.lits.size() > 1 ) { // work on found binary clauses!
      if( !opt_genAND && data.lits.size() != 2 ) continue;  // do not consider gates that are larger than with binary inputs
      // positive blocked clause is not present, if there are no more negative occurrences of the output literal of the gate!
      if( opt_BLOCKED ) {
	if( data[ ~pos ] == data.lits.size() ) { // all occurrences in binary clauses!
	  gates.push_back( Gate(data.lits, (data.lits.size() == 2 ? Gate::AND : Gate::GenAND), Gate::POS_BLOCKED, pos) );
	  if( debug_out ) cerr << "c found posBlocked AND gate with output var " << v + 1 << endl;
	}
	blockedSize = data.lits.size();
	continue; // already found the gate! (there is only one blocked gate per variable ?!)
      }
      if( opt_genAND ) {
	const vector<CRef>& cList = data.list(pos);  // all clauses C with pos \in C
	for( int i = 0 ; i < cList.size(); ++i ) {   // there can be multiple full encoded gates per variable
	  const Clause& c = ca[ cList[i] ];
	  if( c.can_be_deleted() || c.size () < 3 ) continue; // only consider larger clauses, or new ones
	  int marked = 0;
	  for ( int j = 0 ; j < c.size(); ++j ) 
	    if( data.ma.isCurrentStep( toInt(~c[j]) ) 
	      || big->implies( pos, c[j] ) ) marked ++; // check whether all literals inside the clause are marked
	    else break;
	  if( marked == c.size() ) {
	    gates.push_back( Gate( c, c.size() == 3 ? Gate::AND : Gate::GenAND, Gate::FULL, pos ) ); // add the gate
	    if( debug_out ) cerr << "c found FULL AND gate with output var " << v + 1 << endl; 
	    data.log.log( 1, "clause for gate" ,c );
	  }
	}
      } else {
	const vector<ternary>& cList = ternaries[ toInt(pos) ]; // all clauses C with pos \in C
	for( int i = 0 ; i < cList.size(); ++i ) {  // there can be multiple gates per variable
	  const ternary& c = cList[i];
	  // either, literal is marked (and thus in binary list), or its implied wrt. sampled BIG
	  if( ( data.ma.isCurrentStep(toInt(~c.l1)) || big->implies(pos, c.l1 ) ) // since implied does not always work, also check alternative!
	    &&( data.ma.isCurrentStep(toInt(~c.l2)) || big->implies(pos, c.l2 ) ) ){
	    gates.push_back( Gate( pos, c.l1, c.l2, Gate::AND, Gate::FULL) ); // add the gate
	    if( debug_out ) cerr << "c found FULL ternary only AND gate with output var " << v + 1 << endl; 
	  }
	}
      }
    // only look for blocked if enabled, and for more than ternary, if enabled
    } else if(opt_BLOCKED && (! opt_genAND || data.lits.size() == 2 )) { // handle case where all binary clauses are blocked (and removed) and only the large clause is present
      // [a,-b,-c],[-a,b],[-a,c] : binary clauses are blocked if there is no other occurrence with positive a!!
      int count = 0;
      for( int i = 0 ; i < data.list(pos).size(); ++i ) 
	count = ( ca[ data.list(pos)[i] ].can_be_deleted() ? count : count + 1 );
      if( count == 1 ) {
	gates.push_back( Gate( data.lits, data.lits.size() == 2 ? Gate::AND : Gate::GenAND, Gate::NEG_BLOCKED, pos ) );
	if( debug_out ) cerr << "c found NEG_BLOCKED AND gate with output var " << v + 1 << endl; 
      }
    }
  } // end pos/neg for loop
}

void Circuit::getExOGates(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{
  int oldGates = gates.size();
  // check for v <-> ITE( s,t,f )
  for( int p = 0; p < 2 ; ++ p ) {
    Lit pos = mkLit( v, p == 1 );
    vector<CRef>& list = data.list(pos);
    for( int i = 0 ; i < list.size(); ++ i ) {
      const Clause& c = ca[list[i]]; 
      if( c.can_be_deleted() || c.size() == 2 ) continue;
      data.ma.nextStep();
      bool cont = false;
      for( int j = 0 ; j < c.size() ; ++ j ) {
	if( var(c[j]) < v ) { cont = true; break; } // not smallest variable in this clause!
	data.ma.setCurrentStep( toInt( ~c[j] ) );
      }
      if( cont ) continue;
      
      if( opt_BLOCKED ) {
	// for each variable this clause is the only positive occurrence!
	bool found = true;
	int count = 0 ;
	for( int j = 0 ; j < c.size(); ++ j ) {
	  const Lit&l = c[j];
	  vector<CRef>& lList = data.list(l);
	  count = 0 ;
	  for( int k = 0 ; k < lList.size(); ++ k ) {
	    const Clause& ck = ca[lList[k]];
	    if( ck.can_be_deleted() ) continue;
	    if ( count ++ == 2 || lList[k] != list[i] ) // not the current clause, or more than one clause
	      { found = false; break; }
	  }
	  assert( (!found || count == 1) && "if for the current literal the situation for blocked has been found, one clause has been found" );
	}
	if( found ) {
	  gates.push_back( Gate( c, Gate::ExO, Gate::NEG_BLOCKED ) );
	}
	
      }
      
      bool found = true;
      for( int j = 0 ; j < c.size() ; ++ j ) {
	const Lit& l = c[j];
	const Lit* lList = big->getArray(l);
	const int lListSize = big->getSize(l);
	int count = 0;
	for( int k = 0 ; k < lListSize; ++ k )
	  count = (data.ma.isCurrentStep( toInt(lList[k]) ) || big->implies(l, lList[k]) ) ? count+1 : count;
	if( count + 1 < c.size() ) { found = false; break; } // not all literals can be found by this literal!
      }
      if( !found ) continue;
      else gates.push_back( Gate( c, Gate::ExO, Gate::FULL ) );
    }
    
    if( !opt_BLOCKED ) continue;
    // check whether the binary clauses of this variable produce another blocked gate
    Lit * pList = big->getArray(pos);
    const int pListSize = big->getSize(pos);
    data.lits.clear(); // will be filled with all the literals that form an amo with pos
    data.lits.push_back( ~pos );
    for( int j = 0 ; j < pListSize; ++ j )
      data.lits.push_back( pList[j] );
    for( int j = 0 ; j < data.lits.size(); ++ j )
    {
      const Lit l = data.lits[j];
      data.ma.nextStep();
      Lit * lList = big->getArray(~l);
      const int lListSize = big->getSize(~l);
      data.ma.setCurrentStep( toInt(l) ); // mark all literals that are implied by l
      for( int k = 0 ; k < lListSize; ++ k )
        data.ma.setCurrentStep( toInt(lList[k]) );
      int kept = 0;         // store number of kept elements
      int continueHere = 0; // store where to proceed
      for( int k = 0 ; k < data.lits.size(); ++ k ){
	const Lit dl = data.lits[k];
	if( data.ma.isCurrentStep( toInt(dl) ) 
	  || big->implies(l,dl) ) { // check, if the current literal is marked, or if its implied via BIG
	  continueHere = dl == l ? kept : continueHere; 
	  data.lits[kept++] = data.lits[k];
	} else {
	  // cerr << "c did not find " << dl << " in list of " << ~l << endl; 
	}
      }
      data.lits.resize(kept); // keep only the elements that are still marked!
      j = continueHere;       // ++j from for loop is still called
    }
    // check smallest criterion:
    for( int j = 0 ; j < data.lits.size(); ++ j )
      if( var(data.lits[j]) < v ) { data.lits.clear(); break; }
    if( data.lits.size() < 3 )  continue; // only consider clauses with more literals!
    // check whether for each literal there are only these occurrences of binary clauses in the formula
    bool found = true;
    int count = 0;
    for( int j = 0 ; j < data.lits.size(); ++ j )
    {
      const Lit& l = data.lits[j];
      vector<CRef>& lList = data.list(l);
      count = 0 ;
      for( int k = 0 ; k < lList.size(); ++ k ) {
	const Clause& ck = ca[lList[k]];
	if( ck.can_be_deleted() ) continue;
	if ( count ++ == data.lits.size() || ck.size() != 2 ) // not the current clause, or more than one clause
	  { found = false; break; }
      }
      // count has to be data.lits.size(), otherwise data structures are invalid!
      assert( (!found || count + 1== data.lits.size()) && "if for the current literal the situation for blocked has been found, exactly the n binary clauses have to be found!" );
    }
    if( found ) {
      for( int j = 0 ; j < data.lits.size() ; ) data.lits[j] = ~data.lits[j++];
      gates.push_back( Gate( data.lits, Gate::ExO, Gate::POS_BLOCKED ) );
    }
  }
}


void Circuit::getHASUMGates(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{

}

void Circuit::getITEGates(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{
  // ITE(s,t,f) == ITE(-s,f,t) => scanning for these gates is simple
  
 // possible clauses:
 // -s,-t,x     s,-f,x    (red)-t,-f,x for positive
 // -s,t,-x     s,f,-x    (red)t,f,-x  for negative
 // reduced forms:
 // -s,-t,x       -f,x  // latter subsumes the above ternary clause -> ITE gate is entailed!
 // t,-s,-x       f,-x  // latter subsumes the above ternary clause -> ITE gate is entailed!
 //  
  int oldGates = gates.size();
  // check for v <-> ITE( s,t,f )
  for( int p = 0; p < 2 ; ++ p ) {
    data.ma.nextStep();
    int blockedSize = -1; // size of the AND gate that has been found with a blocked gate
    Lit pos = mkLit( v, p == 1 ); // pos -> a AND b AND ... => binary clauses [-pos, a],[-pos,b], ...
    const vector<ternary>& cList = ternaries[ toInt(pos) ]; // all clauses C with pos \in C
    for( int i = 0 ; i < cList.size(); ++i ) {  // there can be multiple gates per variable
      data.lits.clear(); // will store the candidates for 'f'
      const ternary& c = cList[i]; // ternary has the variables X, S and T
      for( int posS = 0 ; posS < 2; posS ++ ) { // which of the other two literals is the selector?
	   Lit s = posS == 0 ? ~c.l1 : ~c.l2;
	   Lit t = posS == 0 ? ~c.l2 : ~c.l1;
	   Lit x = pos;
	   if( debug_out ) cerr << "try to find ["<<i<<"] " << x << " <-> ITE( " << s << "," << t << ", ?)" << endl;
	   // TODO: continue here
	   // try to find f by first checking binary clauses, afterwards check ternary clauses!
	   // if allowed, also try to find by blocked!

	   data.ma.nextStep();
	   data.lits.clear();
	   // collect all other 'f' candidates in the ternary list!
	   for( int j = 0 ; j < cList.size(); ++ j ) {
	     if( i == j ) continue;
	     const ternary& cand = cList[j]; // ternary has the variables X, S and T
	     if( debug_out ) cerr << "candidate ["<<j<<"] : [" << x << "," << cand.l1 << "," << cand.l2 << "]" << endl;
	     if( cand.l1 == s || cand.l2 == s ) {
	       Lit fCandidate = ~toLit(cand.l1.x ^ cand.l2.x ^ s.x);
	       bool sameVar = (var(fCandidate) == var(s)) || (var(fCandidate) == var(t));
	       if( (!sameVar || opt_NegatedI) && ! data.ma.isCurrentStep( toInt(fCandidate) ) ) {
	         data.lits.push_back(fCandidate); 
		 data.ma.setCurrentStep(toInt(fCandidate) );
		 if( debug_out ) cerr << "found f-candidate: " << fCandidate << " sameVar=" << sameVar << endl;
	       }
	     }
	   }
	   // check all binary clauses with [x,?] as ? = -f
	   int preBinaryFs = data.lits.size();
	   if( true ) { // TODO: binary representations cannot be applied with blocked!
	    Lit * list = big->getArray(~x);
	    const int listSize = big->getSize(~x);
	    for( int j = 0 ; j < listSize; ++ j ) {
	      if( data.ma.isCurrentStep(toInt(list[j])) ) continue; // do not add literals twice!
	      data.lits.push_back( ~list[j] );
	      if( debug_out ) cerr << "added candidate " << ~list[j] << " by implication " << ~pos << " -> " << list[j] << endl;
	    }
	   }
	   int countPos = 0;
	   bool nonTernary = false;
	   bool redundantTernary = false;
	   // try to verify f candidates!
	   if( opt_BLOCKED ) { // try to extract blocked gates!
             if( debug_out ) cerr << "c blocked check with candidates: " << data.lits.size() << " including ternary cands: " << preBinaryFs << endl;
	     if( data.lits.size() == 1 && preBinaryFs == 1) { // ITE gate can be blocked only if there is a single f candidate!
	      for( int j = 0 ; j < data.list( pos ).size(); ++ j ) {
		const Clause& bClause = ca[data.list(pos)[j]];
		if( bClause.can_be_deleted() ) continue;
		countPos = countPos + 1; 
		nonTernary = (bClause.size() != 3) ? true : nonTernary;
		if( (~data.lits[0] == bClause[0] || ~data.lits[0] == bClause[1] || ~data.lits[0] == bClause[2] )
		  && (~t == bClause[0] || ~t == bClause[1] || ~t == bClause[2] ) )
		    redundantTernary = true;
	      }
	      if(  !nonTernary && (countPos == 2 || (countPos == 3 && redundantTernary)) ) {
		if( debug_out ) cerr << "c current ITE gate is implied with blocked clause! ternaries: " << countPos << " found redundant: " << redundantTernary << endl; 
		// Lit x, Lit s, Lit t, Lit f, const Coprocessor::Circuit::Gate::Type _type, const Coprocessor::Circuit::Gate::Encoded e
		gates.push_back( Gate(x,s,t,data.lits[0], Gate::ITE, Gate::NEG_BLOCKED) );
		continue;
	      }
	      if( debug_out ) cerr << "counted occurrences: " << countPos << " including " << nonTernary << " nonTernary clauses, found redundant: " << redundantTernary << endl;
	     }
	   }
	   // current gate is not blocked -> scan for remaining clauses
	   for( int j = 0 ; j < data.lits.size(); ++ j ) {
	    const Lit& f = data.lits[j];
	    if( debug_out ) cerr << "c verify cand[" << j << "] = " << f << endl;
	    // look for these clauses (or binary versions!) -s,t,-x     s,f,-x (or f,-x)
	    vector<ternary>& nList = ternaries[ toInt( ~pos ) ];
	    bool foundFirst=false, foundSecond=false;
	    for( int k = 0 ; k < nList.size(); ++ k )  {
		const ternary& cand = nList[k];
		if( debug_out ) cerr << "c check ternary " << ~pos << "," << cand.l1 << "," << cand.l2 << "  1st: " << foundFirst << " 2nd: " << foundSecond << endl;
		if( (cand.l1 == ~s || cand.l2 == ~s)  && ( cand.l1.x ^ cand.l2.x ^ (~s).x == t.x ) ) foundFirst = true;
		else if ((cand.l1 == s || cand.l2 == s)  && ( cand.l1.x ^ cand.l2.x ^ s.x == f.x ) ) foundSecond = true;
	    }
	    if( ! foundFirst || ! foundSecond ) {
	      if( debug_out ) cerr << "c found not all in ternaries" << endl;
	      if( ! foundFirst ) { // try to find clause [t,-x], or [-s,-x]
		if( big->isOneChild(x,t,~s) )
		  { foundFirst = true; break; }
	      }
	      if(!foundFirst) foundFirst = (big->implies(x,t) || big->implies(x,~s) ) ? true : foundFirst;
	      if( ! foundSecond ) { // try to find clause [f,-x] or [s,-x]
	        if( big->isOneChild(x,f,s) )
		  { foundSecond = true; break; }
	      }
	      if(!foundSecond) foundSecond = (big->implies(x,f) || big->implies(x,s) ) ? true : foundSecond;
	    }
	    if( !foundFirst || !foundSecond ) { // f candidate not verified -> remove gate!!
              if( debug_out ) cerr << "c could not verify " << data.lits[j] << " 1st: " << foundFirst << " 2nd: " << foundSecond << endl;
	      data.lits[j] = data.lits[ data.lits.size() -1 ];
	      data.lits.pop_back();
	      --j;
	    }
	   }
	   // add all and gates!
	   for( int j = 0 ; j < data.lits.size(); ++ j ) {
	     gates.push_back( Gate(x,s,t,data.lits[j], Gate::ITE, Gate::FULL) );
	   }
      }
    }
  
  }
  
  // check redundancy!
  for( int i = oldGates + 1; i < gates.size(); ++ i ) {
    for( int j = oldGates ; j < i; ++ j ) {
      if( var(gates[j].s()) == var(gates[i].s()) ) {
	if((   gates[j].s() == ~gates[i].s() 
	    && gates[j].x() ==  gates[i].x() 
	    && gates[j].f() ==  gates[i].t()
	    && gates[j].t() ==  gates[i].f() )  
	 || (  gates[j].s() ==  gates[i].s() 
	    && gates[j].x() == ~gates[i].x() 
	    && gates[j].t() == ~gates[i].t() 
	    && gates[j].f() == ~gates[i].f() )
	 || (  gates[j].s() == ~gates[i].s() 
	    && gates[j].x() == ~gates[i].x() 
	    && gates[j].t() == ~gates[i].f() 
	    && gates[j].f() == ~gates[i].t() ) )
	{ // gates are equivalent! ITE(s,t,f) = ITE(-s,f,t) = -ITE(s,-t,-f) = -ITE(-s,-f,-t)
	  gates[i] = gates[ gates.size() - 1 ];
	  gates.pop_back();
	  --i; break;
	}
      }
    }
  }
}

void Circuit::getXORGates(const Var v, vector< Circuit::Gate >& gates, CoprocessorData& data)
{
  // cerr << "c find XOR for variable " << v+1 << endl;
  int oldGates = gates.size();
  // check for v <-> ITE( s,t,f )
  for( int p = 0; p < 2 ; ++ p ) {
    Lit a = mkLit( v, p == 1 ); // XOR(a,b,c) = 1
    const vector<ternary>& cList = ternaries[ toInt(a) ]; // all clauses C with pos \in C
    bool found [4]; found[0] = true; // found first clause!
    for( int i = 0 ; i < cList.size(); ++ i ) 
    {
      found[1] = found[2] = found[3] = false;
      cerr << "c check [" << i << "/" << cList.size() << "] for " << a << endl;
      bool binary = false;
      const Lit b = cList[i].l1; const Lit c = cList[i].l2;
      if( var(b) == var(c) || var(b) == var(a) || var(a) == var(c) ) continue; // just to make sure!
      //test whether all the other clauses can be found as well
      for( int j = i+1; j < cList.size(); ++ j ) {
	const ternary& tern = cList[j];
	if( (tern.l1 == ~b || tern.l2 == ~b) && ( tern.l1 == ~c || tern.l2 == ~c ) )
	  { found[1] = true; break; }
      }
      if( found[1] ) cerr << "c found first clause as ternary" << endl;
      if ( !found[1] ) { // check for 2nd clause in implications
	if( big->implies(a,~b) || big->implies(a,~c)  ) found[1] = true;
	else { // not found in big
	  if( big->isOneChild(~a,~b,~c ) )
	    { found[1] = true; binary=true;break; }
	  else  { // found[1] is still false!
	    if( big->implies(b,~c) ) found[1] = true;
	    else {
		if( big->isChild(b,~c) )
		  { found[1] = true; binary=true;break; }
	    }
	  }
	}
      }
      if( !found[1] ) continue; // this clause does not contribute to an XOR!
      cerr << "c found first two clauses, check for next two" << endl;
      // check whether we can find the other's by blocked clause analysis
      if( !binary && opt_BLOCKED ) {
	  int countPos = 0;
	  for( int j = 0 ; j < data.list( a ).size(); ++ j ) {
	    const Clause& aClause = ca[data.list(a)[j]];
	    if( aClause.can_be_deleted() ) continue;
	    if( aClause.size() != 3 ) { countPos = 3; break; }
	    countPos = countPos + 1; 
	  }
	  if(  countPos == 2 ) {
	    if( debug_out ) cerr << "c current XOR gate is implied with blocked clauses! ternaries: " << countPos << endl; 
	    // Lit x, Lit s, Lit t, Lit f, const Coprocessor::Circuit::Gate::Type _type, const Coprocessor::Circuit::Gate::Encoded e
	    gates.push_back( Gate(a,b,c, Gate::XOR, Gate::POS_BLOCKED) );
	    continue;
	  }
     }
     // TODO: find full representation!
     const vector<ternary>& naList = ternaries[ toInt(~a) ]; // all clauses C with pos \in C
     for( int j = 0; j < naList.size(); ++ j ) {
	const ternary& tern = naList[j];
	if( (tern.l1 == ~b || tern.l2 == ~b) && ( tern.l1 == c || tern.l2 == c ) ) // found [-a,-b,c]
	  { found[2] = true; }
	else if( (tern.l1 == b || tern.l2 == b) && ( tern.l1 == ~c || tern.l2 == ~c ) ) // found [-a,b,-c]
	  { found[3] = true; }
     }
     cerr << "c found in ternaries: 3rd: " << (int)found[2] << " 4th: " << (int)found[3] << endl;
     if ( !found[2] ) { // check for 2nd clause in implications
	if( big->implies(~a,~b) || big->implies(~a,c) ) { binary=true; found[2] = true; }
	else {
	  if( big->isOneChild(a,~b,c) )
	    { found[2] = true; binary=true;break; }
	  else { // found[2] is still false!
	    if( big->implies(~b,c) ) { binary=true; found[2] = true; }
	    else {
	      if( big->isChild(b,c) ){ found[2] = true; binary=true;break; }
	    }
	  }
	}
     }
     if( !found[2] ) continue; // clause [-a,-b,c] not found
     if ( !found[3] ) { // check for 2nd clause in implications
	if( big->implies(~a,b) || big->implies(~a,~c) ) { binary=true; found[3] = true; }
	else {
	  if( big->isOneChild(a,b,~c)){ found[3] = true; binary=true;break; }
	  else {
	    if( big->implies(b,~c) ) { binary=true; found[3] = true; }
	    else {
	      if( big->isChild(~b,~c) )
	      { found[3] = true; binary=true;break; }
	    }
	  }
	}
      }
      if( !found[3] ) continue; // clause [-a,b,-c] not found
      if( var(a) < var(c) && var(a) < var(b))
      { // for fully encoded gates we do not need to add all, but only one representation
        cerr << "c current XOR gate is fully encodeds: " << endl; 
        gates.push_back( Gate(a,b,c, Gate::XOR, Gate::FULL) );
      }
    }
  }
  // remove redundant gates!
  for( int i = oldGates + 1; i < gates.size(); ++ i ) {
    for( int j = oldGates ; j < i; ++ j ) {
      if(  (var(gates[i].b()) == var(gates[j].b()) || var(gates[i].b()) == var(gates[j].c()) )
	&& (var(gates[i].c()) == var(gates[j].b()) || var(gates[i].c()) == var(gates[j].c()) ) )
      {
	// gates have same variables, check for same polarity. if true, kick later gate out!
	bool pol = sign( gates[i].a() ) ^ sign( gates[i].b() ) ^ sign( gates[i].c() );
	if( pol == sign( gates[j].a() ) ^ sign( gates[j].b() ) ^ sign( gates[j].c() ) )
	{ // gates are equivalent! XOR(a,b,c) with same polarity
	  gates[i] = gates[ gates.size() - 1 ];
	  gates.pop_back();
	  --i; break;
	}
      } else {
	
      }
    }
  }
}

Circuit::Gate::~Gate()
{
}


Circuit::Gate::Gate(const Clause& c, const Circuit::Gate::Type _type, const Circuit::Gate::Encoded e, const Lit output)
: type(_type), encoded(e)
{
  if( _type == Gate::GenAND || _type == Gate::ExO ) {
    data.e.size = (type == Gate::ExO ? c.size() : c.size() - 1); 
    if( debug_out ) cerr << "c create generic clause gate with " << data.e.size << " inputs" <<  endl;
    assert( (type != Gate::ExO || output == lit_Undef ) && "ExO gates do not have an output" );
    data.e.x = output; // in case of ExO, it should be lit_Undef
    data.e.externLits = (Lit*) malloc( data.e.size * sizeof(Lit) );
    int j = 0;
    for( int i = 0; i < c.size(); ++ i )
      if( c[i] != output ) data.e.externLits[j++] = (type == Gate::ExO) ? c[i] : ~c[i];
  } else if ( _type == Gate::XOR ) {
    assert( c.size() == 3 && "xor clause has to have four literals" );
    data.lits[0] = output;
    int j = 1;
    for( int i = 0 ; i < 3; ++i )
      if( c[i] != output ) data.lits[j++] = c[i];
  } else if ( _type == Gate::ITE ) {
    assert( false && "this constructor is not suitable for ITE gates!" );
  } else if ( _type == Gate::HA_SUM ) {
    assert( c.size() == 4 && "half adder sum clause has to have four literals" );
    data.lits[0] = output;
    int j = 1;
    for( int i = 0 ; i < 4; ++i )
      if( c[i] != output ) data.lits[j++] = c[i];
  } else if ( _type == AND ) {
    assert( c.size() == 3 && "AND gate can only be generated from a ternary clause" );
    if( debug_out ) cerr << "c create AND gate from ternary clause" << endl;
    x() = output;
    a() = c[0] == output ? ~c[1] : ~c[0];
    b() = c[0] == output || c[1] == output ? ~c[2] : ~c[1];
  }
}

Circuit::Gate::Gate(const vector< Lit >& c, const Circuit::Gate::Type _type, const Circuit::Gate::Encoded e, const Lit output)
: type(_type), encoded(e)
{
  if( _type == Gate::GenAND || _type == Gate::ExO ) {
    data.e.size = c.size(); 
    assert( (type != Gate::ExO || output == lit_Undef ) && "ExO gates do not have an output" );
    if(  debug_out) cerr << "c create generic vector gate with " << data.e.size << " inputs with output " << output <<  endl;
    data.e.x = output; // in case of ExO, it should be lit_Undef
    data.e.externLits = (Lit*) malloc( data.e.size * sizeof(Lit) );
    for( int i = 0; i < c.size(); ++ i )
      data.e.externLits[i] = (type == Gate::ExO) ? c[i] : ~c[i];
  } else if ( _type == Gate::XOR ) {
    assert( c.size() == 3 && "xor clause has to have four literals" );
    data.lits[0] = output;
    int j = 1;
    for( int i = 0 ; i < 3; ++i )
      if( c[i] != output ) data.lits[j++] = c[i];
  } else if ( _type == Gate::ITE ) {
    assert( false && "this constructor is not suitable for ITE gates!" );
  } else if ( _type == Gate::HA_SUM ) {
    assert( c.size() == 4 && "half adder sum clause has to have four literals" );
    data.lits[0] = output;
    int j = 1;
    for( int i = 0 ; i < 4; ++i )
      if( c[i] != output ) data.lits[j++] = c[i];
  } else if ( _type == AND ) {
    x() = output;
    a() = c[0] == output ? c[1] : c[0];
    b() = c[0] == output || c[1] == output ? c[2] : c[1];
  }
}

Circuit::Gate::Gate(Lit _x, Lit _s, Lit _t, Lit _f, const Circuit::Gate::Type _type, const Circuit::Gate::Encoded e)
: type(_type), encoded(e)
{
  assert( (_type == ITE || _type == HA_SUM) && "This constructur can be used for ITE gates only" );
  x() = _x; s() = _s; t() = _t; f() = _f;
}

Circuit::Gate::Gate(Lit _x, Lit _a, Lit _b, const Circuit::Gate::Type _type, const Circuit::Gate::Encoded e)
: type(_type), encoded(e)
{
//  assert( false && "This constructor is not yet implemented (should be for faster construction!)" );
  assert( (type == AND || type == XOR ) && "This constructor can only be used for AND or XOR gates" );
  a() =  _a; b() = _b; 
  if( type == AND ) x() = _x;
  else c() = _x;
}

Circuit::Gate& Circuit::Gate::operator=(const Circuit::Gate& other)
{
  type = other.type;
  encoded = other.encoded;
  if( type == ExO || type == GenAND ) {
    data.e.x = other.data.e.x;
    data.e.externLits = other.data.e.externLits;
    data.e.size = other.data.e.size;
  } else memcpy( data.lits, other.data.lits, sizeof(Lit) * 4 );
  return *this;
}

Circuit::Gate::Gate(const Circuit::Gate& other)
{
  type = other.type;
  encoded = other.encoded;
  if( type == ExO || type == GenAND ) {
    data.e.x = other.data.e.x;
    data.e.externLits = other.data.e.externLits;
    data.e.size = other.data.e.size;
  } else memcpy( data.lits, other.data.lits, sizeof(Lit) * 4 );
}

void Circuit::Gate::destroy()
{
  if( (type == Gate::GenAND || type == Gate::ExO ) && data.e.externLits != 0  ) 
  {  ::free ( data.e.externLits ); data.e.externLits = 0 ; }
}


void Circuit::Gate::print(ostream& stream)
{
  switch( encoded ) {
    case FULL: stream << "full "; break;
    case POS_BLOCKED: stream << "posB "; break;
    case NEG_BLOCKED: stream << "negB "; break;
  }
  if( type != XOR && type != ExO ) {
    stream << getOutput() << " <-> ";
    switch( type ) {
      case AND:    stream << "AND( " << a() << " , " << b() << " )" << endl; break;
      case GenAND: stream << "g-AND("; 
        for( int i = 0 ;  i < data.e.size; ++ i ) 
	  stream << " " << data.e.externLits[i] ;
	stream << " )" << endl;
        break;
      case ITE:    stream << "ITE( " << s() << " , " << t() << " " << f() << " )" << endl; break;
    }
  } else if( type == XOR ) {
    stream << "XOR( " << a() << " , " << b() << " " << c() << " )" << endl;
  } else {
    stream << "ExO("; 
    for( int i = 0 ;  i < data.e.size; ++ i ) stream << " " << data.e.externLits[i] ;
    stream << " )" << endl;
  }
}
