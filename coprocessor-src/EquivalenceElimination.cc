/************************************************************************[EquivalenceElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/EquivalenceElimination.h"

#include <fstream>

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - EE";

static IntOption  opt_level            (_cat, "cp3_ee_level",      "EE on BIG, gate probing, structural hashing", 0, IntRange(0, 3));
static BoolOption opt_old_circuit      (_cat, "cp3_old_circuit",   "do old circuit extraction", false);
static BoolOption opt_eagerEquivalence (_cat, "cp3_eagerGates",    "do old circuit extraction", true);
static BoolOption opt_eeGateBigFirst   (_cat, "cp3_BigThenGate", "detect binary equivalences before going for gates", true);
/// enable this parameter only during debug!
static BoolOption debug_out            (_cat, "ee_debug", "write final circuit to this file",false);
static StringOption aagFile            (_cat, "ee_aag", "write final circuit to this file");

static const int eeLevel = 1;
/// temporary Boolean flag to quickly enable debug output for the whole file
/// const static bool debug_out = true; // print output to screen

EquivalenceElimination::EquivalenceElimination(ClauseAllocator& _ca, ThreadController& _controller, Propagation& _propagation, Coprocessor::Subsumption& _subsumption)
: Technique(_ca,_controller)
, gateSteps(0)
, gateTime(0)
, gateExtractTime(0)
, eeTime(0)
, steps(0)
, eqLitInStack(0)
, eqInSCC(0)
, eqIndex(0)
, isToAnalyze(0)
, propagation( _propagation )
, subsumption( _subsumption )
{}

void EquivalenceElimination::eliminate(Coprocessor::CoprocessorData& data)
{
  // TODO continue here!!
  
  eeTime  = cpuTime() - eeTime;
  
  isToAnalyze.resize( data.nVars(), 0 );
    
  data.ma.resize(2*data.nVars());
  
  // find SCCs and apply them to the "replacedBy" structure
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    eqDoAnalyze.push_back( mkLit(v,false) );
    isToAnalyze[ v ] = 1;
  }
  
  if( replacedBy.size() < data.nVars() ) { // extend replacedBy structure
    for( Var v = replacedBy.size(); v < data.nVars(); ++v )
      replacedBy.push_back ( mkLit(v,false) );
  }

 
  if( opt_level > 1  && data.ok() ) {
    
    bool moreEquivalences = true;
    
    // repeat until fixpoint?!
    int iter = 0;
    vector<Circuit::Gate> gates;
    while( moreEquivalences ) {

      for( int i = 0 ; i < gates.size(); ++ i ) {
	gates[i].destroy();
      }
      gates.clear();

      iter ++;
      // cerr << "c run " << iter << " round of circuit equivalences" << endl;

      if( debug_out ) {
	cerr << endl << "====================================" << endl;
	cerr << "intermediate formula before gates: " << endl;
	for( int i = 0 ; i < data.getClauses().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
	cerr << "c learnts: " << endl;
	for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
	cerr << "====================================" << endl << endl;
      }

      Circuit circ(ca); 
      gateExtractTime = cpuTime() - gateExtractTime;
      circ.extractGates(data, gates);
      gateExtractTime = cpuTime() - gateExtractTime;
      // cerr << "c found " << gates.size() << " gates" << endl ;
      if ( debug_out ) {
	cerr << endl << "==============================" << endl;
      data.log.log(eeLevel,"found gates", gates.size());
      for( int i = 0 ; i < gates.size(); ++ i ) {
	Circuit::Gate& gate = gates[i];
	// data.log.log(eeLevel,"gate output",gate.getOutput());
	if(debug_out) gate.print(cerr);
      }
      cerr << "==============================" << endl << endl;
      cerr << "c equivalences:" << endl;
      for ( Var v = 0 ; v < data.nVars(); ++v )
	if( mkLit(v,false) != getReplacement( mkLit(v,false) ) )
	  cerr << "c " << v+1 << " == " << getReplacement( mkLit(v,false) ) << endl;
      }

      vector<Lit> oldReplacedBy = replacedBy;
      //vector< vector<Lit> >* externBig
    
      if( opt_old_circuit ) {
	moreEquivalences = findGateEquivalences( data, gates );
	if( moreEquivalences )
	  if( debug_out ) cerr << "c found new equivalences with the gate method!" << endl;
      } else {
	if( debug_out ) cerr << "c run miter EQ method" << endl;
	moreEquivalences = findGateEquivalencesNew( data, gates );
	if( moreEquivalences )
	  if( debug_out ) cerr << "c found new equivalences with the gate method!" << endl;
	if( !data.ok() )
	  if( debug_out ) cerr << "state of formula is UNSAT!" << endl;
      }
      replacedBy = oldReplacedBy;
      
      if( !data.ok() ) { eeTime  = cpuTime() - eeTime; return; }
      // after we extracted more information from the gates, we can apply these additional equivalences to the forula!
      if ( !moreEquivalences && iter > 1 ) {
	// cerr << "c no more gate equivalences found" << endl;
	break;
      } else {
	// cerr << "c more equi " << moreEquivalences << " iter= " << iter << endl;
      }
      moreEquivalences = false;
      bool doRepeat = false;
      int eeIter = 0;
      do {  // will set literals that have to be analyzed again!
	findEquivalencesOnBig(data);                              // finds SCC based on all literals in the eqDoAnalyze array!
	doRepeat = applyEquivalencesToFormula(data, (iter == 1 && eeIter == 0) );   // in the first iteration, run subsumption/strengthening and UP!
	moreEquivalences = doRepeat || moreEquivalences;
	eeIter ++;
      } while ( doRepeat && data.ok() );
      // cerr << "c moreEquivalences in iteration " << iter << " : " << moreEquivalences << " with BIGee iterations " << eeIter << endl;
    }
    if( ((const char*)aagFile) != 0  )
      writeAAGfile(data);
  }
  
  //do binary reduction
  if( data.ok() ) {
    do { 
      findEquivalencesOnBig(data);                              // finds SCC based on all literals in the eqDoAnalyze array!
    } while ( applyEquivalencesToFormula(data ) && data.ok() ); // will set literals that have to be analyzed again!
    
    // cerr << "c ok=" << data.ok() << " toPropagate=" << data.hasToPropagate() <<endl;
    assert( (!data.ok() || !data.hasToPropagate() )&& "After these operations, all propagation should have been done" );
    
    
      if( debug_out ) {
	cerr << endl << "====================================" << endl;
	cerr << "FINAL FORMULA after ELIMINATE: " << endl;
	for( int i = 0 ; i < data.getClauses().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
	for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
	cerr << "====================================" << endl;
	cerr << "Solver Trail: " << endl;
	data.printTrail(cerr);
	cerr << endl << "====================================" << endl << endl;
	cerr << endl;
      }
     
    
  }
  eeTime  = cpuTime() - eeTime;
}

void EquivalenceElimination::initClause(const CRef cr)
{
  
}

bool EquivalenceElimination::findGateEquivalencesNew(Coprocessor::CoprocessorData& data, vector< Circuit::Gate >& gates)
{
  gateTime  = cpuTime() - gateTime;
  vector< vector<int32_t> > varTable ( data.nVars() ); // store for each variable which gates have this variable as input
  vector< unsigned int > bitType ( data.nVars(), 0 );  // upper 4 bits are a counter to count how often this variable has been considered already as output
 
  const bool enqOut = true;
  const bool enqInp  = true;
  
  if( debug_out ) cerr << "c has to Propagate: " << data.hasToPropagate() << endl;
  
  int oldEquivalences = data.getEquivalences().size();
  
  if( opt_eeGateBigFirst ) {
    if( debug_out ) cerr << "c do BIG extraction " << endl;
    do { 
      findEquivalencesOnBig(data);                              // finds SCC based on all literals in the eqDoAnalyze array!
    } while ( applyEquivalencesToFormula(data ) && data.ok() ); // will set literals that have to be analyzed again!
  }
  
  // have gates per variable
  for( int i = 0 ; i < gates.size() ; ++ i ) {
   const Circuit::Gate& g = gates[i];
   switch( g.getType() ) {
     case Circuit::Gate::AND:
       // do not consider gate outputs of blocked gates!
       if( g.isFull() ) bitType[ var(g.x()) ] = bitType[ var(g.x()) ] | 1; // set output bit
       varTable[ var( g.a() ) ].push_back(i);
       varTable[ var( g.b() ) ].push_back(i);
       break;
     default:
       break;
   }
  }
  
  // have a queue of the current gates, perform per type
  Circuit::Gate::Type currentType = Circuit::Gate::AND;
  deque< int > queue;
  vector< Var > equiVariables; // collect all the other equivalent variables in this vector
  deque< Var > currentVariables;
  deque< Var > *currentPtr = &currentVariables;
  
  MarkArray active;
  active.create( data.nVars() );
  active.nextStep();

  MarkArray reactivated;
  reactivated.create( data.nVars() );
  reactivated.nextStep();
  
  const bool putAllAlways = true;
  
  bool isMiter = true;
  for( Var v = 0; v < data.nVars(); ++ v ) {
    if( !putAllAlways && bitType[v] != 0 ) continue;
    currentPtr->push_back(v); 
    if( !putAllAlways ) active.setCurrentStep(v);
    // Assumption: inside a miter, each pure input variable has to have an even number of gates!
    if( (varTable[v].size() & 1) != 0 ) { 
      isMiter = false;
      if( debug_out ) cerr << "c the given gate structure cannot be a miter, because variable " << v+1 << " has " << varTable[v].size() << " gates" << endl;
    }
  }
  
  int iter = 0;
  if( debug_out ) {
  cerr << "current queue: ";
  for( int i = 0 ; i < currentPtr->size(); ++ i ) cerr << currentVariables[i]+1 << " ";
  cerr << endl;
  }
  
  // structures to store temporary literals
  vector<Lit> upLits (2); upLits.clear();
  vector<Lit> eeLits (2); eeLits.clear();
  
  // as long as there are variables inside the queue
  while( currentPtr->size() > 0 ) {
      ++iter;
      if( iter & 127 == 0 ) cerr << "c at iteration " << iter << " there are " << currentPtr->size() << " variables" << endl;
      // iterate over the different gate types
      const Var v = currentPtr->front();
      currentPtr->pop_front();
      active.reset(v);
      // cerr << "c test variable " << v+1 << endl;
      if( debug_out ) cerr << "c check variable " << v+1 << " with " << varTable[v].size() << " gates and replace literal " << getReplacement( mkLit(v,false ) ) << endl;
      // for all gates with this input variable:
      for( int i = 0 ; i < varTable[v].size(); ++ i ) {
	Circuit::Gate& g = gates[ varTable[v][i] ];
	if( g.isInvalid() ) continue;
	// literals of the current gate
	if( debug_out ) cerr << "c check gate ";
	if( debug_out ) g.print(cerr);
	Lit a = getReplacement( g.a() ); Lit b = getReplacement( g.b() ); Lit x = getReplacement( g.x() ); 
	if( debug_out ) cerr << "c WHICH is rewritten " << x << " <-> AND(" << a << "," << b << ")" << endl;
	
	// assigned value
	if ( data.value(a) != l_Undef || data.value(b) != l_Undef || data.value(x) != l_Undef) {
	  if( debug_out ) cerr << "c gate has assigned inputs" << endl;
	  if ( data.value(a) == l_True ) {
	    if( opt_eagerEquivalence ) setEquivalent(b,x);
	    data.addEquivalences( x,b );
// 	    b = getReplacement( g.b() );
// 	    x = getReplacement( g.x() );
	  } else if ( data.value(a) == l_False ) {
	    if( enqOut )data.enqueue(~x);  
	  }
	  if ( data.value(b) == l_True ) {
	    if( opt_eagerEquivalence ) setEquivalent(a,x);
	    data.addEquivalences( x,a );
// 	    a = getReplacement( g.a() );
// 	    x = getReplacement( g.x() );
	  } else if ( data.value(b) == l_False ) {
	    if( enqOut )data.enqueue(~x);  
	  } else if ( data.value(x) == l_True) {
	    if( enqInp ) data.enqueue(a);  
	    if( enqInp ) data.enqueue(b);  
	  }
	  // do not reason with assigned gates!
	  continue;
	}
	// somehow same inputs
	if( a == b ) {
	  if( debug_out ) cerr << "c found equivalence based on equivalent inputs " << x << " <-> AND(" << a << "," << b << ")" << endl; 
	  if( opt_eagerEquivalence ) setEquivalent(a,x);
	  data.addEquivalences(x,a);
// 	  a = getReplacement( g.a() );
// 	  x = getReplacement( g.x() );
	} else if ( a == ~b ) {
	  if( debug_out ) cerr << "c find an unsatisfiable G-gate based on complementary inputs " << x << " <-> AND(" << a << "," << b << ")" << endl;  
	  if( enqOut )data.enqueue(~x);
	} 
// These rules are unsound!
// 	else if ( x == a ) {
// 	  if( debug_out ) cerr << "c equi inputs G-gate " << x << " <-> AND(" << a << "," << b << ")" << endl;  
// 	  if( opt_eagerEquivalence ) setEquivalent(b,x);
// // 	  b = getReplacement( g.b() );
// // 	  x = getReplacement( g.x() );
// 	  data.addEquivalences(x,b);
// 	} else if ( x == b ) {
// 	  if( debug_out ) cerr << "c equi inputs G-gate " << x << " <-> AND(" << a << "," << b << ")" << endl;  
// 	  if( opt_eagerEquivalence ) setEquivalent(a,x);
// // 	  a = getReplacement( g.a() );
// // 	  x = getReplacement( g.x() );
// 	  data.addEquivalences(x,a);
// 	} 
	else if ( x == ~a || x == ~b) {
	  if( debug_out ) cerr << "c find an unsatisfiable G-gate based on complementary input to output" << endl; 
	  if( enqOut )data.enqueue(~x);
	} 

	
	// compare to all other gates of this variable:
	for( int j = i+1 ; j < varTable[v].size(); ++ j ) {
	  Circuit::Gate& other = gates [varTable[v][j]] ;
	  if( other.isInvalid() ) continue;
	  if( other.getType() != Circuit::Gate::AND ) continue;
	  if( debug_out ) cerr << "c with OTHER [" << varTable[v][j] << "," << j << "] ";
	  if( debug_out ) other.print(cerr);
 	  Lit oa = getReplacement( other.a() ); 
 	  Lit ob = getReplacement( other.b() ); 
 	  Lit ox = getReplacement( other.x() ); 
	  if( debug_out ) cerr << "c WHICH is rewritten " << ox << " <-> AND(" << oa << "," << ob << ")" << endl;
	  // assigned value
	  if ( data.value(oa) != l_Undef || data.value(ob) != l_Undef || data.value(ox) != l_Undef) {
	    if( debug_out ) { cerr << "c gate has assigned inputs" << endl; other.print(cerr); }
	    if ( data.value(oa) == l_True ) {
	      if( debug_out ) cerr << "[   0]" << endl;
	      if( opt_eagerEquivalence ) setEquivalent(ob,ox);
	      data.addEquivalences( ox,ob );
// 	      ob = getReplacement( other.b() ); 
// 	      ox = getReplacement( other.x() );
	    } else if ( data.value(oa) == l_False ) {
	      if( debug_out ) cerr << "[   1]" << endl;
	      if( enqOut )data.enqueue(~ox);  
	    }
	    if ( data.value(ob) == l_True ) {
	      if( debug_out ) cerr << "[   2]" << endl;
	      if( opt_eagerEquivalence ) setEquivalent(oa,ox);
	      data.addEquivalences( ox,oa );
// 	      oa = getReplacement( other.a() ); 
// 	      ox = getReplacement( other.x() );
	    } else if ( data.value(ob) == l_False ) {
	      if( debug_out ) cerr << "[   3]" << endl;
	      if( enqOut ) data.enqueue(~ox);  
	    } else if ( data.value(ox) == l_True) {
	      if( debug_out ) cerr << "[   4]" << endl;
	      if( enqInp ) data.enqueue(oa);  
	      if( enqInp ) data.enqueue(ob);  
	    }
	    // do not reason with assigned gates!
	    continue;
	  }
	  
	  // do some statistics
	  gateSteps ++;
	  
	  /// do simplify gate!
	  if( oa == ob ) {
	    if( debug_out ) cerr << "c found equivalence based on equivalent inputs" << endl;
	    if( debug_out ) cerr << "[   5]" << endl;
	    if( opt_eagerEquivalence ) setEquivalent(oa,ox);
	    data.addEquivalences(ox,oa);
// 	    oa = getReplacement( other.a() ); 
// 	    ox = getReplacement( other.x() );
	  } else if ( oa == ~ob ) { // this rule holds!
	    if( debug_out ) cerr << "c find an unsatisfiable O-gate based on complementary inputs " << ox << " <-> AND(" << oa << "," << ob << ")" << endl; 
	    if( debug_out ) cerr << "[   6]" << endl;
	    if( enqOut ) data.enqueue(~ox);
	  } 
// these rules are unsound
// 	  else if ( ox == oa ) {
// 	    if( debug_out ) cerr << "[   7]" << endl;
// 	    if( opt_eagerEquivalence ) setEquivalent(ob,ox);
// // 	    ob = getReplacement( other.b() ); 
// // 	    ox = getReplacement( other.x() );
// 	    data.addEquivalences(ox,ob);
// 	  } else if ( ox == ob ) {
// 	    if( debug_out ) cerr << "[   8]" << endl;
// 	    if( opt_eagerEquivalence ) setEquivalent(oa,ox);
// // 	    oa = getReplacement( other.a() ); 
// // 	    ox = getReplacement( other.x() );
// 	    data.addEquivalences(ox,oa);
// 	  }
	  else if ( ox == ~oa || ox == ~ob) { // this rule holds!
	    if( debug_out ) cerr << "[   9]" << endl;
	    if( debug_out ) cerr << "c find an unsatisfiable O-gate based on complementary input to output " << ox << " <-> AND(" << oa << "," << ob << ")" << endl; 
	    if( enqOut ) data.enqueue(~ox);
	  } 

	  
	  // handle all equivalence cases!
	  eeLits.clear(); upLits.clear();
	  if ( (oa == a && ob == b) || (oa == b && ob == a ) ) {
	    // usual equivalence of outputs!
	    if( debug_out ) cerr << "[  10]" << endl;
	    eeLits.push_back(x); eeLits.push_back(ox);
	    // both gates are valid -> 
	    if( debug_out ) { cerr << "c invalidate "; other.print(cerr); }
	    other.invalidate();
	    
	  } else if( var(oa) == var(a) || var(oa) == var(b) || var(ob) == var(a) || var(ob) == var(b) ) {
	    // TODO: implement all cases!
	    // extra cases where at least one input matches!
	    if(  (oa == a && ob == x) // x<->AND(a,c) , and c <-> AND(a,x) => c == x !!!
	      || (ob == b && oa == x)
	      || (b == ob && a == ox)
	      || (a == oa && b == ox)
	      ) {
	      if( debug_out ) cerr << "[  11] match one input, and output is another input: " << x << " <-> AND(" << a << "," << b << ")  vs " << ox << " <-> AND(" << oa << "," << ob << ")" << endl;
	      eeLits.push_back(x);eeLits.push_back(ox);
	    } else if ( ox == ~x && 
	      ( (oa == ~a && ob==~b ) || (oa == ~b && ob ==~a) )
	    ) {
	      if( debug_out ) cerr << "[  12]" << endl;
	      // x <-> AND(a,b) and -x <-> AND(-a,-b) => x=a=b!
	      eeLits.push_back(x);eeLits.push_back(a); // every two literals represent an equivalent pair
	      eeLits.push_back(x);eeLits.push_back(b);
	    } 
// 	      else if( (oa == ~a && ob == ~b ) || (oa==~b && ob==~a) ){
// 	      if( x == ox ) { data.enqueue(~ox);  
// 	      } else {
// 		// both gates cannot be active at the same time! -> add a new learned clause!
// 		eeLits.clear();
// 		if( ox < x ) { eeLits.push_back(~ox);eeLits.push_back(~x); }
// 		else { eeLits.push_back(~x);eeLits.push_back(~ox); }
// 		CRef lc = ca.alloc(eeLits, true);
// 		assert ( ca[lc].size() == 2 && "new learned clause has to be binary!" );
// 		data.addClause(lc);
// 		data.getLEarnts().push(lc);
// 		eeLits.clear();
// 		if( debug_out ) cerr << "c add clause " << ca[lc] << endl;
// 		if( isToAnalyze[ var(x) ] == 0 ) { eqDoAnalyze.push_back(x); isToAnalyze[ var(x) ] = 1; }
// 		if( isToAnalyze[ var(ox) ] == 0 ) { eqDoAnalyze.push_back(ox); isToAnalyze[ var(ox) ] = 1; }
// 	      }
// 	    } 
	    else if( (oa == a && ob == ~b) || (oa == b && ob == ~a)  || (oa == ~a && ob == b)  || (oa == ~b && ob == a) ) {
	      // derive a new gate from the given ones!
	      int oldGates = gates.size();
	      if( oa == a ) {
		// constructor expects literals of the ternary representative clause
		if( debug_out ) cerr << "[  13]" << endl;
		if( x == ~ox ) {
		  cerr << "c complementary outputs, one complementary input, other input equal " << ox << " <-> AND(" << ob << "," << oa << ")" << endl;
		  data.enqueue(a); // handle gates where the input would be complementary
		}
		else gates.push_back( Circuit::Gate( ~a, x, ox, Circuit::Gate::AND, Circuit::Gate::FULL) );
	      } else if ( ob == b ) {
		if( debug_out ) cerr << "[  14]" << endl;
		if( x == ~ox ) {
		  cerr << "c complementary outputs, one complementary input, other input equal " << ox << " <-> AND(" << ob << "," << oa << ")" << endl;
		  data.enqueue(b); // handle gates where the input would be complementary
		}
		else gates.push_back( Circuit::Gate( ~b, x, ox, Circuit::Gate::AND, Circuit::Gate::FULL) );
	      } else if ( ob == a ) {
		if( debug_out ) cerr << "[  15]" << endl;
		if( x == ~ox ) {
		  cerr << "c complementary outputs, one complementary input, other input equal " << ox << " <-> AND(" << ob << "," << oa << ")" << endl;
		  data.enqueue(a); // handle gates where the input would be complementary
		}
		else gates.push_back( Circuit::Gate( ~a, x, ox, Circuit::Gate::AND, Circuit::Gate::FULL) );
	      } else if ( oa == b ) {
		if( debug_out ) cerr << "[  16]" << endl;
		if( x == ~ox ) {
		  cerr << "c complementary outputs, one complementary input, other input equal " << ox << " <-> AND(" << ob << "," << oa << ")" << endl;
		  data.enqueue(b); // handle gates where the input would be complementary
		}
		else gates.push_back( Circuit::Gate( ~b, x, ox, Circuit::Gate::AND, Circuit::Gate::FULL) );
	      }
	      if( gates.size() > oldGates ) {
		if( debug_out ) {
		  cerr << "c added new gate";
		  gates[ oldGates ].print(cerr);
		}
		// TODO: what to do with the new gate? for now, put it into the lists of the other variables!
		if( !active.isCurrentStep(var(x)) && !reactivated.isCurrentStep(var(x)) ) { 
		  if( debug_out ) {
		    cerr << "c reactivate varible " << x << " because of gate ";
		    gates[ oldGates ].print(cerr);
		  }
		  currentPtr->push_back( var(x) ); active.setCurrentStep(var(x));
		  reactivated.setCurrentStep(var(x));
		}
		if( !active.isCurrentStep(var(ox)) && !reactivated.isCurrentStep(var(ox)) ) { 
		  if( debug_out ) {
		    cerr << "c reactivate varible " << ox << " because of gate ";
		    gates[ oldGates ].print(cerr);
		  }
		  currentPtr->push_back( var(ox) ); active.setCurrentStep(var(ox));
		  reactivated.setCurrentStep(var(ox));
		}
		// TODO: move to front?
		varTable[ var(x)].push_back( oldGates );
		varTable[var(ox)].push_back( oldGates );
	      }

	    } else if( (x == oa && ob == ~a)
	       || (x == oa && oa == ~b)
	       || (x == ob && oa == ~a)
	       || (x == ob && oa == ~b)
	      
	    ) {
	      if( debug_out ) cerr << "[  17]" << endl;
	      // the output of a gate together with a complementary input in another gate cannot be satisfied -> other gate is unsat!
	      data.enqueue(~ox);
	    }
	    else {
	      if( debug_out ) {
		if( var(x) == var(ox) ||
		  ( (var(a) == var(oa) && var(b) == var(ob)) || (var(b) == var(oa) && var(a) == var(ob)) )
		|| ( var(x) == var(oa) && (var(a) == var(ob) || var(b) == var(ob) ) )
		|| ( var(x) == var(ob) && (var(a) == var(oa) || var(b) == var(oa) ) ) ) {
		  cerr << "c UNHANDLED CASE with two gates:" << endl;
		  cerr << x << " <-> AND(" << a << ", " << b << ")" << endl
		      << ox << " <-> AND(" << oa << ", " << ob << ")" << endl;
		}
	      }
	    }
	  }
	  
	  
	    while ( eeLits.size() >= 2 ) {
	      const Lit x = eeLits[eeLits.size()-1]; const Lit ox = eeLits[eeLits.size()-2];
	      eeLits.pop_back();eeLits.pop_back();
	      if( var(x) != var(ox) ) {
		if( opt_eagerEquivalence ) setEquivalent(x,ox);
		data.addEquivalences(x,ox);
		// put smaller variable in queue, if not already present
		Var minV = var(x) < var(ox) ? var(x) : var(ox);
		Var maxV = (minV ^ var(x) ^ var(ox));
		if( debug_out ) cerr << "c equi: " << x << " == " << ox << " min=" << minV+1 << " max=" << maxV+1 << endl;
		if( !putAllAlways && ! active.isCurrentStep(minV) ) {
		  active.setCurrentStep(minV);
		  currentPtr->push_back(minV);
		  if( debug_out ) cerr << "c re-activate output variable " << minV + 1 << endl;
		}
		// moves gates from greater to smaller!
		for( int k = 0 ; k < varTable[maxV].size(); ++k )
		  varTable[minV].push_back( varTable[maxV][k] );
		varTable[maxV].clear();
	      } else {
		if( x == ~ox ) {
		  data.setFailed();
		  cerr << "c failed, because AND miter procedure found that " << x << " is equivalent to " << ox << endl;
		  gateTime  = cpuTime() - gateTime;
		  return true;
		} else {
		  if( debug_out ) cerr << "c found equivalence " << x << " == " << ox << " again" << endl;
		}
	      }
	    }
	  
	}
	
      }
  }
  
  if( debug_out ) cerr << "c OLD equis: " << oldEquivalences << "  current equis: " << data.getEquivalences().size() << " hasToPropagate" << data.hasToPropagate() << endl;
  gateTime  = cpuTime() - gateTime;
  return (data.getEquivalences().size() - oldEquivalences > 0) || data.hasToPropagate();
}

bool EquivalenceElimination::findGateEquivalences(Coprocessor::CoprocessorData& data, vector< Circuit::Gate > gates)
{
  int oldEquivalences = data.getEquivalences().size();
  
  /** a variable in a circuit can participate in non-clustered gates only, or also participated in clustered gates */
  vector< vector<int32_t> > varTable ( data.nVars() ); // store for each variable which gates have this variable as input
  // store for each variable, whether this variable has a real output (bit 1=output, bit 2=clustered, bit 3 = stamped)
  vector< unsigned int > bitType ( data.nVars(), 0 ); // upper 4 bits are a counter to count how often this variable has been considered already as output
  
  for( int i = 0 ; i < gates.size() ; ++ i ) {
   const Circuit::Gate& g = gates[i];
   switch( g.getType() ) {
     case Circuit::Gate::AND:
       bitType[ var(g.x()) ] = bitType[ var(g.x()) ] | 1; // set output bit
       varTable[ var(g.a()) ].push_back(i);
       varTable[ var(g.b()) ].push_back(i);
       break;
     case Circuit::Gate::ExO:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
         bitType[v] = bitType[v] | 2; // set clustered bit
         varTable[v]. push_back(i);
       }
       break;
     case Circuit::Gate::GenAND:
       bitType[ var(g.getOutput()) ] = bitType[ var(g.getOutput()) ] | 1; // set output bit
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
         varTable[v]. push_back(i);
       }
       break;
     case Circuit::Gate::ITE:
       bitType[ var(g.x()) ] = bitType[ var(g.x()) ] | 1; // set output bit
       varTable[ var(g.s()) ].push_back(i);
       varTable[ var(g.t()) ].push_back(i);
       varTable[ var(g.f()) ].push_back(i);
       break;
     case Circuit::Gate::XOR:
       bitType[ var( g.a()) ] = bitType[ var( g.a()) ] | 2 ; // set clustered bit!
       bitType[ var( g.b()) ] = bitType[ var( g.b()) ] | 2 ; // set clustered bit!
       bitType[ var( g.c()) ] = bitType[ var( g.c()) ] | 2 ; // set clustered bit!
       varTable[var( g.a())]. push_back(i);
       varTable[var( g.b())]. push_back(i);
       varTable[var( g.c())]. push_back(i);
       break;
     case Circuit::Gate::FA_SUM:
       bitType[ var( g.a()) ] = bitType[ var( g.a()) ] | 2 ; // set clustered bit!
       bitType[ var( g.b()) ] = bitType[ var( g.b()) ] | 2 ; // set clustered bit!
       bitType[ var( g.c()) ] = bitType[ var( g.c()) ] | 2 ; // set clustered bit!
       bitType[ var( g.x()) ] = bitType[ var( g.x()) ] | 2 ; // set clustered bit!
       varTable[var( g.a())]. push_back(i);
       varTable[var( g.b())]. push_back(i);
       varTable[var( g.c())]. push_back(i);
       varTable[var( g.x())]. push_back(i);
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
  }
  
  // TODO: remove clustered gates, if they appear for multiple variables!
  
  if( false ) {
   for( Var v = 0 ; v < data.nVars(); ++ v ) {
     cerr << "c var " << v+1 << " role: " << ( ((bitType[v]) & 2) != 0 ? "clustered " : "") << ( (bitType[v] & 1) != 0 ? "output" : "") << "("<< (int)bitType[v] << ") with dependend gates: " << endl;
     for( int i = 0 ; i < varTable[v].size(); ++ i )
       gates[ varTable[v][i] ].print(cerr);
   }
  }
  
  return false;
  
  cerr << "c ===== START SCANNING ======= " << endl;
  
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
     cerr << "c var " << v+1 << " role: " << "("<< (int)bitType[v] << ")" << endl;
  }
  
  vector< Var > inputVariables;
  deque< int > queue;
  // collect pure input variables!
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( 0 == bitType[v] ) {
      if( varTable[v].size() == 0 ) continue; // use only if it helps enqueuing gates!
      cerr << "c use for initial scanning: " << v+1 << " with " << ((int)bitType[v]) << endl; 
      inputVariables.push_back(v);
      bitType[v] = (bitType[v] | 4); // stamp
      // todo: stamp variable?!
    }
  }
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( (bitType[v] & 5) == 0  ) { // not yet stamped, and no output
      inputVariables.push_back(v);
      if( varTable[v].size() == 0 ) continue; // use only if it helps enqueuing gates!
      cerr << "c use for initial clustered scanning: " << v+1 << " with " << ((int)bitType[v]) << endl; 
      bitType[v] = (bitType[v] | 4); // stamp
      // todo: stamp variable?!
    }
  }
  
  // fill queue to work with
  for( int i = 0 ; i < inputVariables.size(); ++ i ) {
    const Var v = inputVariables[i];
    for( int j = 0 ; j < varTable[v].size(); ++ j ) {
      Circuit::Gate& g = gates[ varTable[v][j] ]; 
      if( !g.isInQueue() ) 
        { queue.push_back( varTable[v][j] ); 
          g.putInQueue();
	  cerr << "c put gate into queue: ";
	  g.print(cerr);
	  // mark all inputs for clustered gates!
	  if( g.getType() == Circuit::Gate::XOR ) {
	    bitType[var(g.a())] = (bitType[var(g.a())] | 4); // stamp
	    bitType[var(g.b())] = (bitType[var(g.b())] | 4); // stamp
	    bitType[var(g.c())] = (bitType[var(g.c())] | 4); // stamp
	  } else if( g.getType() == Circuit::Gate::ExO ) {
	    for( int j = 0 ; j < g.size(); ++ j ) {
	      bitType[var(g.get(j))] = (bitType[var(g.get(j))] | 4); // stamp
	    }
	  } else if( g.getType() == Circuit::Gate::FA_SUM ){
	    bitType[var(g.a())] = (bitType[var(g.a())] | 4); // stamp
	    bitType[var(g.b())] = (bitType[var(g.b())] | 4); // stamp
	    bitType[var(g.c())] = (bitType[var(g.c())] | 4); // stamp
	    bitType[var(g.x())] = (bitType[var(g.x())] | 4); // stamp
	  }
	}
    }
  }
  
  if( queue.empty() && gates.size() > 0 ) {
    // not pure inputs available, start with all clustered gates!
    for( int i = 0 ; i < gates.size(); ++ i ) {
      Circuit::Gate& g = gates[ i ]; 
      if(  g.getType() == Circuit::Gate::XOR || g.getType() == Circuit::Gate::ExO || g.getType() == Circuit::Gate::FA_SUM ) 
        { queue.push_back( i ); 
          g.putInQueue();
	  cerr << "c put gate into queue: ";
	  g.print(cerr);
	  if( g.getType() == Circuit::Gate::XOR ) {
	    bitType[var(g.a())] = (bitType[var(g.a())] | 4); // stamp
	    bitType[var(g.b())] = (bitType[var(g.b())] | 4); // stamp
	    bitType[var(g.c())] = (bitType[var(g.c())] | 4); // stamp
	  } else if( g.getType() == Circuit::Gate::ExO ) {
	    for( int j = 0 ; j < g.size(); ++ j ) {
	      bitType[var(g.get(j))] = (bitType[var(g.get(j))] | 4); // stamp
	    }
	  } else {
	    bitType[var(g.a())] = (bitType[var(g.a())] | 4); // stamp
	    bitType[var(g.b())] = (bitType[var(g.b())] | 4); // stamp
	    bitType[var(g.c())] = (bitType[var(g.c())] | 4); // stamp
	    bitType[var(g.x())] = (bitType[var(g.x())] | 4); // stamp
	  }
	}
    }
  }
  
  // if there are no clustered gates, there is something different wrong -> add all gates, stamp all variables!
  if( queue.empty() && gates.size() > 0 ) {
    // not pure inputs available, start with all clustered gates!
    for( int i = 0 ; i < gates.size(); ++ i ) {
      Circuit::Gate& g = gates[ i ]; 
      queue.push_back( i ); 
          g.putInQueue();
	  cerr << "c put gate into queue: ";
	  g.print(cerr);
    }
    for( Var v = 0 ; v < data.nVars(); ++ v ) {
	bitType[v] = (bitType[v] | 4); // stamp
    }
  }
  
  if( queue.empty() ) cerr << "c there are no gates to start from, with " << gates.size() << " initial gates!" << endl;
  assert ( (gates.size() == 0 || queue.size() > 0) && "there has to be at least one gate to start from!" ); 
  while( !queue.empty() && data.ok() ) {
    const int gateIndex = queue.front();
    Circuit::Gate& g = gates[ queue.front() ];
    queue.pop_front();
    // check number of being touched, do not process a gate too often!
    if( g.touch() > 16 ) {
      cerr << "c looked at the gate 16 times: ";
      g.print(cerr);
      continue; // do not look at the gate too often!
    }
     
    // check whether we are allowed to work on this gate already
    if( allInputsStamped( g, bitType ) ) {
      // calculate equivalences based on gate type, stamp output variable and enqueue successor gates
      processGate( data, g, gates, queue, bitType, varTable );
      // enqueueSucessorGates(g,queue,gates,bitType,varTable);
    } else {
      cerr << "c not all inputs are available for "; g.print(cerr );
       queue.push_back(gateIndex);
    }
  }
  
  // just in case some unit has been found, we have to propagate it!
  if( data.hasToPropagate() )
    propagation.propagate(data);
  
  return oldEquivalences < data.getEquivalences().size();
}

bool EquivalenceElimination::allInputsStamped(Circuit::Gate& g, std::vector< unsigned int >& bitType)
{
   int count = 0;
   switch( g.getType() ) {
     case Circuit::Gate::AND:
       return (bitType[ var(g.a()) ] & 4) != 0 && (bitType[ var(g.b()) ] & 4) != 0 ;
       break;
     case Circuit::Gate::ExO:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
	 // one bit must not be stamped!
         if( (bitType[ v ] & 4) == 0 ) 
	   if ( count != 0 ) return false; else count++;
       }
       cerr << "c all inputs of ExO are stamped: "; g.print(cerr);
       return true;
     case Circuit::Gate::GenAND:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
	 if( (bitType[ v ] & 4) == 0 ) return false;
       }
       return true;
     case Circuit::Gate::ITE:
       if(  (bitType[ var(g.s()) ] & 4) == 0  
	 || (bitType[ var(g.t()) ] & 4) == 0  
	 || (bitType[ var(g.f()) ] & 4) == 0 ) return false;
       return true;
     case Circuit::Gate::XOR:
	if((bitType[ var(g.a()) ] & 4) == 0 ) count ++;
	if((bitType[ var(g.b()) ] & 4) == 0 ) count ++;  
	if((bitType[ var(g.c()) ] & 4) == 0 ) count ++;
	if( count > 1 ) return false;
       return true;
     case Circuit::Gate::FA_SUM:
	if((bitType[ var(g.a()) ] & 4) == 0 ) count ++;
	if((bitType[ var(g.b()) ] & 4) == 0 ) count ++;  
	if((bitType[ var(g.c()) ] & 4) == 0 ) count ++;
	if((bitType[ var(g.x()) ] & 4) == 0 ) count ++;
	if( count > 1 ) return false;
       return true;
     case Circuit::Gate::INVALID:
       return false;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
       return false;
   }
   return false;
}

bool EquivalenceElimination::checkEquivalence(const Circuit::Gate& g1, const Circuit::Gate& g2, Lit& e1, Lit& e2)
{
  return false;
}

void EquivalenceElimination::enqueueSucessorGates( Circuit::Gate& g, std::deque< int > queue, std::vector<Circuit::Gate>& gates, std::vector< unsigned int >& bitType, vector< vector<int32_t> >& varTable)
{
switch( g.getType() ) {
     case Circuit::Gate::AND:
       {const Var v = var( g.x() );
        bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
       break;}
     case Circuit::Gate::ExO:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	const Var v = var( g.get(j) );
        bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
       }
       break;
     case Circuit::Gate::GenAND:
     {
	const Var v = var( g.getOutput() );
        bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
       break;}
     case Circuit::Gate::ITE:
     {const Var v = var( g.x() );
        bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
       break;}
     case Circuit::Gate::XOR:
     {for ( int i = 0 ; i < 3; ++ i ) {
	 const Var v = ((i==0) ? var(g.a()) : ((i==1) ? var(g.b()) : var(g.c()) )  );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
       }
       break;}
     case Circuit::Gate::FA_SUM:
       assert( false && "This gate type has not been implemented (yet)" );
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
}

void EquivalenceElimination::processGate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector<int32_t> >& varTable)
{
  switch( g.getType() ) {
     case Circuit::Gate::AND:
       return processANDgate(data,g,gates,queue,bitType,varTable);
     case Circuit::Gate::ExO:
       return processExOgate(data,g,gates,queue,bitType,varTable);
     case Circuit::Gate::GenAND:
       return processGenANDgate(data,g,gates,queue,bitType,varTable);
     case Circuit::Gate::ITE:
       return processITEgate(data,g,gates,queue,bitType,varTable);
     case Circuit::Gate::XOR:
       return processXORgate(data,g,gates,queue,bitType,varTable);
     case Circuit::Gate::FA_SUM:
       return processFASUMgate(data,g,gates,queue,bitType,varTable);
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
}

void EquivalenceElimination::processANDgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
  // cerr << "c compare " << queue.size() << " gates against current AND gate ";
//   g.print(cerr);
  
  // TODO: x<->AND(a,c) , and c <-> AND(a,x) => c == x !!!
  
  Lit a = getReplacement( g.a() ); 
  Lit b = getReplacement( g.b() ); 
  Lit x = getReplacement( g.x() ); 
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::AND ) continue;

    Lit oa = getReplacement( other.a() ); 
    if( oa != a && oa != b ) continue; // does not match another input!
    Lit ob = getReplacement( other.b() ); 
    if( (oa == b && ob != a) || ( oa == a && ob != b ) ) continue; // does not match another input!
    Lit ox = getReplacement( other.x() ); 
    
//     cerr << "c gates are equivalent: " << endl;
//     g.print(cerr);
//     other.print(cerr);
    
    if( var(x) != var(ox) ) {
      setEquivalent(x,ox);
      data.addEquivalences(x,ox);
      cerr << "c equi: " << x << " == " << ox << endl;
      // old or new method?
      if( active ==0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	const Var v = var( other.x() );
	bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
      } else {
	if( !active->isCurrentStep(   var(x)) ) {
	  active->setCurrentStep(     var(x)); 
	  activeVariables->push_back( var(x));
	}
	if( !active->isCurrentStep(   var(ox)) ) {
	  active->setCurrentStep(     var(ox)); 
	  activeVariables->push_back( var(ox) );
	}
      }
      
    } else {
      if( x == ~ox ) {
	data.setFailed();
	cerr << "c failed, because AND procedure found that " << x << " is equivalent to " << ox << endl;
      } else {
// 	cerr << "c found equivalence " << x << " == " << ox << " again" << endl;
      }
    }
    
  }
  
  if( active == 0 && activeVariables == 0 ) {
    // new equivalent gate -> enqueue successors!
    const Var v = var( g.x() );
    bitType[v] = bitType[v] | 4; // stamp output bit!
    // enqueue gates, if not already inside the queue:
    for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
      if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	gates[ varTable[ v ][i] ].putInQueue();
	queue.push_back( varTable[ v ][i] );
      }
    }
  }
}

void EquivalenceElimination::processExOgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
//   cerr << "c compare " << queue.size() << " gates against current ExO gate ";
//   g.print(cerr);
  
  Lit lits[g.size()];
  for( int i = 0 ; i < g.size(); ++ i )
    lits[i] = getReplacement(g.get(i));
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::ExO ) continue;
    if( other.size() != g.size() ) continue; // cannot say anything about different sizes

//     cerr << "c compare two ExOs of size " << other.size() << " : "; other.print(cerr);

    // hit each input:
    int hit = 0;
    Lit freeLit = lit_Undef;
    for( int j = 0 ; j < other.size(); ++ j ) {
      const Lit r = getReplacement(other.get(j));
//       cerr << "c find lit " << r << " (hit already: " << hit << ")" << endl;
      for( int k = hit; k < other.size(); ++ k ) {
	if( r == lits[k] ) { lits[k] = lits[hit] ; lits[hit] = r; ++ hit; break; }
      }
//       if( hit > 0 ) cerr << "c hit after search: " << hit << " hit lit= " << lits[hit-1] << endl;
      if( hit < j ) break;
      else if( hit == j && freeLit == lit_Undef) freeLit = other.get(j);
    }
    if( hit + 1 < other.size() ) continue; // not all inputs match!
//     cerr << "c final hit: " << hit << " corr. lit: " << lits[hit] << endl;

    if( var( lits[ g.size() -1 ] ) != var( freeLit ) ) {
	setEquivalent(lits[ g.size() -1 ],freeLit);
	data.addEquivalences(lits[ g.size() -1 ],freeLit);
	cerr << "c equi: " << lits[ g.size() -1 ] << " == " << freeLit << endl;
	
	if( active == 0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	for( int j = 0 ; j < other.size(); ++ j ) {
	  const Var v = var( other.get(j) );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
	}
	
      } else {
	if( !active->isCurrentStep(   var(lits[ g.size() -1 ])) ) {
	  active->setCurrentStep(     var(lits[ g.size() -1 ])); 
	  activeVariables->push_back( var(lits[ g.size() -1 ]));
	}
	if( !active->isCurrentStep(   var(freeLit)) ) {
	  active->setCurrentStep(     var(freeLit)); 
	  activeVariables->push_back( var(freeLit) );
	}
      }
	
	
      } else {
	if( lits[ g.size() -1 ] == ~freeLit ) {
	  data.setFailed();
	  cerr << "c failed, because ExO procedure found that " << lits[ g.size() -1 ] << " is equivalent to " << freeLit << endl;
// 	  cerr << "c corresponding gates: " << endl;
// 	  g.print(cerr);
// 	  other.print(cerr);
	  return;
	} else {
// 	  cerr << "c found equivalence " << lits[ g.size() -1 ] << " == " << freeLit << " again" << endl;
	}
      }
  }
  
  if( active == 0 && activeVariables == 0 ) {
    // new equivalent gate -> enqueue successors!
    for( int j = 0 ; j < g.size(); ++ j ) {
	  const Var v = var( g.get(j) );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
    }
  }
}

void EquivalenceElimination::processGenANDgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
//   cerr << "c compare " << queue.size() << " gates against current GenAND gate ";
//   g.print(cerr);
  
  Lit lits[g.size()];
  for( int i = 0 ; i < g.size(); ++ i )
    lits[i] = getReplacement(g.get(i));
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::GenAND ) continue;
    if( other.size() != g.size() ) continue; // cannot say anything about different sizes

    // hit each input:
    int hit = 0;
    for( int j = 0 ; j < other.size(); ++ j ) {
      const Lit r = getReplacement(other.get(j));
      for( int k = hit; k < other.size(); ++ k ) {
	if( r == lits[hit] ) { lits[hit] = lits[k]; lits[k] = r; ++ hit; break; }
      }
      if( hit != j+1 ) break;
    }
    if( hit != other.size() ) continue; // not all inputs match!

    if( var( g.getOutput() ) != var( other.getOutput() ) ) {
	setEquivalent(g.getOutput(),other.getOutput());
	data.addEquivalences(g.getOutput(),other.getOutput());
	cerr << "c equi: " << g.getOutput() << " == " << other.getOutput() << endl;
      if( active == 0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	  const Var v = var( other.getOutput() );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
      } else {
	if( !active->isCurrentStep(   var(g.getOutput())) ) {
	  active->setCurrentStep(     var(g.getOutput()) ); 
	  activeVariables->push_back( var(g.getOutput()) );
	}
	if( !active->isCurrentStep(   var(other.getOutput())) ) {
	  active->setCurrentStep(     var(other.getOutput()) ); 
	  activeVariables->push_back( var(other.getOutput()) );
	}
      }
	
	
      } else {
	if( g.getOutput() == ~other.getOutput() ) {
	  data.setFailed();
	  cerr << "c failed, because GenAND procedure found that " << g.getOutput() << " is equivalent to " << other.getOutput() << endl;
// 	  cerr << "c corresponding gates: " << endl;
// 	  g.print(cerr);
// 	  other.print(cerr);
	  return;
	} else {
// 	  cerr << "c found equivalence " << g.getOutput() << " == " << other.getOutput() << " again" << endl;
	}
      }
  }
  
  if( active == 0 && activeVariables == 0 ) {
// new equivalent gate -> enqueue successors!
	  const Var v = var( g.getOutput() );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
  }

}

void EquivalenceElimination::processITEgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
//   cerr << "c compare " << queue.size() << " gates against current ITE gate ";
//   g.print(cerr);
  
  const Lit s = getReplacement( g.s() ); 
  const Lit t = getReplacement( g.t() ); 
  const Lit f = getReplacement( g.f() ); 
  const Lit x = getReplacement( g.x() ); 
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::ITE ) continue;

    const Lit os = getReplacement( other.s() ); 
    if( var(s) != var(os) ) continue; // the selector variable has to be the same!!
    const Lit ot = getReplacement( other.t() ); 
    if( var(t) != var(ot) && var(ot) != var(f) ) continue; // has to match one of the inputs!
    const Lit of = getReplacement( other.f() ); 
    if(  (var(t) == var(ot) && var(of) != var(f) )
      || (var(f) == var(ot) && var(of) != var(t) )) continue; // inputs have to match inputs!
    Lit ox = getReplacement( other.x() ); 
    if( x == ox ) continue; // can only define output, but no other bits of the gates!
    // equivalence could be either positive or negative
    enum Match {
      POS,
      NONE,
      NEG
    };
    Match match = NONE;
    if( s == os ) {
      if( t == ot && f == of ) match = POS;
      else if ( t == ~ot && f == ~of ) match = NEG;
    } else {
      if( f == ot && t == of ) match = POS;
      else if ( t == ~of && f == ~ot ) match = NEG;
    }
    if( match == NONE ) continue;
    else if( match == NEG ) ox = ~ox;
   
    if( var(x) != var(ox) ) {
      setEquivalent(x,ox);
      data.addEquivalences(x,ox);
      cerr << "c equi: " << x << " == " << ox << endl;
      
      if( active == 0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	const Var v = var( other.x() );
	bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
      } else {
	if( !active->isCurrentStep(   var(x)) ) {
	  active->setCurrentStep(     var(x) ); 
	  activeVariables->push_back( var(x) );
	}
	if( !active->isCurrentStep(   var(ox)) ) {
	  active->setCurrentStep(     var(ox) ); 
	  activeVariables->push_back( var(ox) );
	}
      }
      
    } else {
      if( x == ~ox ) {
	data.setFailed();
	cerr << "c failed, because ITE procedure found that " << x << " is equivalent to " << ox << endl;
//	cerr << "c equi gate 1: " << x  << " = ITE(" << s  << "," << t  << "," << f  << ")" << endl;
// 	cerr << "c equi gate 2: " << ox << " = ITE(" << os << "," << ot << "," << of << ")" << endl;
      } else {
// 	cerr << "c found equivalence " << x << " == " << ox << " again" << endl;
      }
    }
    
  }
  
  if( active == 0 && activeVariables == 0 ) {
    // this gate has been processed!
    const Var v = var( x );
    bitType[v] = bitType[v] | 4; // stamp output bit!
    // enqueue gates, if not already inside the queue:
    for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
      if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	gates[ varTable[ v ][i] ].putInQueue();
	queue.push_back( varTable[ v ][i] );
      }
    }
  }
}

void EquivalenceElimination::processXORgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
//   cerr << "c compare " << queue.size() << " gates against current XOR gate ";
//   g.print(cerr);
  
  Lit lits[3];
  lits[0] = getReplacement( g.a() ); 
  lits[1] = getReplacement( g.b() ); 
  lits[2] = getReplacement( g.c() ); 
  bool pol = sign(lits[0]) ^ sign(lits[1]) ^ sign(lits[2]);
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::XOR  // xor is either equivalent to another xor
      && other.getType() != Circuit::Gate::FA_SUM ) continue; // or subsumes equivalent FA_SUM

//     cerr << "c other gate: "; other.print(cerr);
    
    if( other.getType() == Circuit::Gate::XOR ) {
      int hit = 0;
      Lit freeLit = lit_Undef;
      const Lit oa = getReplacement( other.a() ); 
      for( int j = 0; j < 3; ++ j ) {
	if( var(lits[j]) == var(oa) ) { const Lit tmp = lits[0]; lits[0] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      if( hit == 0 ) freeLit = oa;
//       if( freeLit != lit_Undef ) cerr << "c freelit = " << freeLit << endl;
      const Lit ob = getReplacement( other.b() ); 
      for( int j = hit; j < 3; ++ j ) {
	if( var(lits[j]) == var(ob) ) { const Lit tmp = lits[hit]; lits[hit] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      if( hit < 2 )  // check for the free literal
	if( hit == 0 ) { /*cerr << "failed with one of literal " << oa << " ," << ob << endl;*/ continue; } // this xor gate does not match 2 variables!
	else if( freeLit == lit_Undef ) freeLit = ob ; // this literal is the literal that does not match!

//       if( freeLit != lit_Undef ) cerr << "c freelit = " << freeLit << endl;
      const Lit oc = getReplacement( other.c() ); 
      for( int j = hit; j < 3; ++ j ) {
	if( var(lits[j]) == var(oc) ) { const Lit tmp = lits[hit]; lits[hit] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      if( hit < 3 )  // check for the free literal
	if( hit < 2 ) {/*cerr << "failed with one of literal " << oa << " ," << ob << ", " << oc << endl; */continue; } // this xor gate does not match 2 variables!
      // might be the case that all variables match!
      if( freeLit == lit_Undef ) freeLit = oc ; // this literal is the literal that does not match!

//       if( freeLit != lit_Undef ) cerr << "c freelit = " << freeLit << endl;
      // get right polarity!
//       cerr << "c pol= " << pol << " new pol= " << (int)(sign(oa) ^ sign(ob) ^ sign(oc) ^ sign(freeLit) ) << endl;
      if( (pol ^ sign(lits[2] ) ) != (sign(oa) ^ sign(ob) ^ sign(oc) ^ sign(freeLit) ) ) freeLit = ~freeLit;
      
      if( var(lits[2]) != var(freeLit) ) {
	setEquivalent(lits[2],freeLit);
	data.addEquivalences(lits[2],freeLit);
	cerr << "c equi: " << lits[2] << " == " << freeLit << endl;
	
      if (active==0 && activeVariables==0) {
	// new equivalent gate -> enqueue successors!
	for( int j = 0 ; j < 3; ++ j ) { // enqueue all literals!
	  const Var v = var( j == 0 ? other.a() : ((j == 1 ) ? other.b() : other.c() ));
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
	}
      } else {
	if( !active->isCurrentStep(   var(lits[2])) ) {
	  active->setCurrentStep(     var(lits[2]) ); 
	  activeVariables->push_back( var(lits[2]) );
	}
	if( !active->isCurrentStep(   var(freeLit)) ) {
	  active->setCurrentStep(     var(freeLit) ); 
	  activeVariables->push_back( var(freeLit) );
	}
      }
	
      } else {
	if( lits[2] == ~freeLit ) {
	  data.setFailed();
	  cerr << "c failed, because XOR procedure found that " << lits[2] << " is equivalent to " << freeLit << endl;
	  return;
	} else {
	  //cerr << "c found equivalence " << lits[2] << " == " << freeLit << " again" << endl;
	}
      }
    } else {
      int hit = 0;
      Lit freeLit = lit_Undef;
      
      bool qPol = false;
      for( int j = 0 ; j < 4; ++ j ) { // enqueue all literals!
        const Lit ol = ( j == 0 ? other.a() : ((j == 1 ) ? other.b() : (j==3? other.c() : other.x()) ) );
	for ( int k = hit; k < 3; ++ k )
	  if( var(ol) == var(lits[k]) ) // if this variable matches, remember that it does! collect the polarity!
	    { const Lit tmp = lits[hit]; lits[hit] = lits[k]; lits[k] = tmp; hit++; pol = pol ^ sign(ol); break; }
	if( hit != j + 1 )
	  if( freeLit == lit_Undef ) freeLit = ol;
	  else { freeLit == lit_Error; break; }
      }
      if( freeLit == lit_Error ) continue; // these gates do not match!
      
      if( pol == qPol ) freeLit = ~freeLit;
      if( l_False == data.enqueue(freeLit) ) return;
      cerr << "c" << endl << "c found the unit " << freeLit << " based on XOR reasoning" << "c NOT HANDLED YET!" << endl << "c" << endl;
      cerr << "c corresponding gates: " << endl;
      g.print(cerr);
      other.print(cerr);
    }
  }
  
  if (active==0 && activeVariables==0) {
      for( int j = 0 ; j < 3; ++ j ) { // enqueue all literals!
	const Var v = var( lits[j] );
	bitType[v] = bitType[v] | 4; // stamp output bit!
	// enqueue gates, if not already inside the queue:
	for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	  if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	    gates[ varTable[ v ][i] ].putInQueue();
	    queue.push_back( varTable[ v ][i] );
	  }
	}
      }
  }
}

void EquivalenceElimination::processFASUMgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue, std::vector< unsigned int >& bitType, vector< vector< int32_t > >& varTable, MarkArray* active, std::deque< Var >* activeVariables)
{
//   cerr << "c compare " << queue.size() << " gates against current FASUM gate ";
//   g.print(cerr);
  
  Lit lits[4];
  lits[0] = getReplacement( g.a() ); 
  lits[1] = getReplacement( g.b() ); 
  lits[2] = getReplacement( g.c() ); 
  lits[3] = getReplacement( g.x() ); 
  bool pol = sign(lits[0]) ^ sign(lits[1]) ^ sign(lits[2]) ^ sign(lits[3]);
  
  for( int i = 0 ; i < queue.size(); ++ i ) {
    const Circuit::Gate& other = gates [queue[i]] ;
    if( other.getType() != Circuit::Gate::XOR  // xor is either equivalent to another xor
      && other.getType() != Circuit::Gate::FA_SUM ) continue; // or subsumes equivalent FA_SUM

//     cerr << "c other gate: "; other.print(cerr);
    
    if( other.getType() == Circuit::Gate::FA_SUM ) {
      int hit = 0;
      Lit freeLit = lit_Undef;
      const Lit oa = getReplacement( other.a() ); 
      for( int j = 0; j < 4; ++ j ) {
	if( var(lits[j]) == var(oa) ) { const Lit tmp = lits[0]; lits[0] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      if( hit == 0 ) freeLit = oa;
//       if( freeLit != lit_Undef ) cerr << "c 1 freelit = " << freeLit << endl;
      const Lit ob = getReplacement( other.b() ); 
      for( int j = hit; j < 4; ++ j ) {
	if( var(lits[j]) == var(ob) ) { const Lit tmp = lits[hit]; lits[hit] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      if( hit < 2 )  // check for the free literal
	if( hit == 0 ) {/*cerr << "failed with one of literal " << oa << " ," << ob << endl; */continue; } // this xor gate does not match 3 variables!
	else if( freeLit == lit_Undef ) freeLit = ob ; // this literal is the literal that does not match!

//       if( freeLit != lit_Undef ) cerr << "c 2 freelit = " << freeLit << endl;
      const Lit oc = getReplacement( other.c() ); 
      for( int j = hit; j < 4; ++ j ) {
	if( var(lits[j]) == var(oc) ) { const Lit tmp = lits[hit]; lits[hit] = lits[j]; lits[j] = tmp; hit++; break; } 
      }

      if( hit < 3 )  // check for the free literal
	if( hit < 2 ) {/*cerr << "failed with one of literal " << oa << " ," << ob << ", " << oc << endl;*/ continue; } // this xor gate does not match 3 variables!
	else if( freeLit == lit_Undef ) freeLit = oc ; // this literal is the literal that does not match!
      
//       if( freeLit != lit_Undef ) cerr << "c 3 freelit = " << freeLit << endl;
      const Lit od = getReplacement( other.x() ); 
      for( int j = hit; j < 4; ++ j ) {
	if( var(lits[j]) == var(od) ) { const Lit tmp = lits[hit]; lits[hit] = lits[j]; lits[j] = tmp; hit++; break; } 
      }
      
      if( hit < 3 ) {/*cerr << "failed with one of literal " << oa << " ," << ob << ", " << oc << " ," << od << endl;*/ continue; } // this xor gate does not match 3 variables!
      // might be the case that all variables match!
      if( freeLit == lit_Undef ) freeLit = od ; // this literal is the literal that does not match!

//       if( freeLit != lit_Undef ) cerr << "c 4 freelit = " << freeLit << endl;
      // get right polarity!
//       cerr << "c pol= " << pol << " new pol= " << (int)(sign(oa) ^ sign(ob) ^ sign(oc) ^ sign(od) ^ sign(freeLit) ) << endl;
      if( (pol ^ sign(lits[3] ) ) != (sign(oa) ^ sign(ob) ^ sign(oc) ^ sign(od) ^ sign(freeLit) ) ) freeLit = ~freeLit;
      
      if( var(lits[3]) != var(freeLit) ) {
	setEquivalent(lits[3],freeLit);
	data.addEquivalences(lits[3],freeLit);
	cerr << "c equi: " << lits[3] << " == " << freeLit << endl;
	
      if( active == 0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	for( int j = 0 ; j < 4; ++ j ) { // enqueue all literals!
	  const Var v = var( j == 0 ? other.a() : ((j == 1 ) ? other.b() : (j==3? other.c() : other.x()) ) );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
	}
      } else {
	if( !active->isCurrentStep(   var(lits[3])) ) {
	  active->setCurrentStep(     var(lits[3]) ); 
	  activeVariables->push_back( var(lits[3]) );
	}
	if( !active->isCurrentStep(   var(freeLit)) ) {
	  active->setCurrentStep(     var(freeLit) ); 
	  activeVariables->push_back( var(freeLit) );
	}
      }
	
      } else {
	if( lits[3] == ~freeLit ) {
	  data.setFailed();
	  cerr << "c failed, because FASUM procedure found that " << lits[3] << " is equivalent to " << freeLit << endl;
	  return;
	} else {
	 // cerr << "c found equivalence " << lits[3] << " == " << freeLit << " again" << endl;
	}
      }
    } else {
      cerr << " FA_SUM vs XOR: Not implemented yet" << endl;
      // TODO: is the small xor subsumes the big one, the neagtion of the remaining literal has to be unit propagated!
      int hit = 0;
      Lit freeLit = lit_Undef;
      
      bool qPol = false;
      for( int j = 0 ; j < 3; ++ j ) { // enqueue all literals!
        const Lit ol = ( j == 0 ? other.a() : ((j == 1 ) ? other.b() : other.c() ) );
	for ( int k = hit; k < 4; ++ k )
	  if( var(ol) == var(lits[k]) ) // if this variable matches, remember that it does! collect the polarity!
	    { const Lit tmp = lits[hit]; lits[hit] = lits[k]; lits[k] = tmp; hit++; pol = pol ^ sign(ol); break; }
      }
      if( hit < 3 ) continue; // these gates do not match!
      freeLit = lits[3];
      
      if( pol ^ sign( freeLit ) == qPol ) freeLit = ~freeLit;
      if( l_False == data.enqueue(freeLit) ) return;
      cerr << "c" << endl << "c found the unit " << freeLit << " based on XOR reasoning" << "c NOT HANDLED YET!" << endl << "c" << endl;
      cerr << "corresponding gates:" << endl;
      g.print(cerr);
      other.print(cerr);
    }

  }
  
  if( active == 0 && activeVariables == 0 ) {
	// new equivalent gate -> enqueue successors!
	for( int j = 0 ; j < 4; ++ j ) { // enqueue all literals!
	  const Var v = var( j == 0 ? g.a() : ((j == 1 ) ? g.b() : (j==3? g.c() : g.x()) ) );
	  bitType[v] = bitType[v] | 4; // stamp output bit!
	  // enqueue gates, if not already inside the queue:
	  for( int i = 0 ; i < varTable[ v ].size(); ++ i ) {
	    if(! gates[ varTable[ v ][i] ].isInQueue() ) {
	      gates[ varTable[ v ][i] ].putInQueue();
	      queue.push_back( varTable[ v ][i] );
	    }
	  }
	}
  }
  
}


void EquivalenceElimination::findEquivalencesOnBig(CoprocessorData& data, vector< vector<Lit> >* externBig)
{
  // create / resize data structures:
  if( eqLitInStack == 0 ) eqLitInStack = (char*) malloc( sizeof( char ) * data.nVars() * 2 );
  else  eqLitInStack = (char*) realloc( eqLitInStack, sizeof( char ) * data.nVars() * 2 );
  if( eqInSCC == 0 ) eqInSCC = (char*) malloc( sizeof( char ) * data.nVars()  );
  else  eqInSCC = (char*) realloc( eqInSCC, sizeof( char ) * data.nVars()  );
  eqNodeIndex.resize( data.nVars() * 2, -1 );
  eqNodeLowLinks.resize( data.nVars() * 2, -1 );
  
  // reset all data structures
  eqIndex = 0;
  eqNodeIndex.assign( eqNodeIndex.size(), -1 );
  eqNodeLowLinks.assign( eqNodeLowLinks.size(), -1 );
  memset( eqLitInStack, 0, sizeof(char) * data.nVars() );
  memset( eqInSCC, 0, sizeof(char) * data.nVars() );
  eqStack.clear();
  
  BIG big;
  if( externBig == 0 ) big.create(ca, data, data.getClauses(), data.getLEarnts() );
  
  if( debug_out ) {
     cerr << "c to process: ";
     for( int i = 0 ; i < eqDoAnalyze.size(); ++ i ) {
        cerr << eqDoAnalyze[i] << " ";
     }
      if( debug_out ) {
	cerr << endl << "====================================" << endl;
	cerr << "intermediate formula before gates: " << endl;
	for( int i = 0 ; i < data.getClauses().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
	for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
	cerr << "====================================" << endl << endl;
      }
     cerr << endl;
  }
  
  int count = 0 ;
  while( !eqDoAnalyze.empty() )
  {
      Lit randL = eqDoAnalyze[ eqDoAnalyze.size() -1 ];
      if( data.randomized() ) { // shuffle an element back!
	uint32_t index = rand() % eqDoAnalyze.size();
	eqDoAnalyze[ eqDoAnalyze.size() -1 ] = eqDoAnalyze[index];
	eqDoAnalyze[index] = randL;
	randL = eqDoAnalyze[ eqDoAnalyze.size() -1 ];
      }
      eqDoAnalyze.pop_back();
      isToAnalyze[ var(randL) ] = 0;
      
      const Lit l = randL;
      // filter whether the literal should be tested!

      // skip literal, if it is already in a SCC or it does not imply anything
      if( eqInSCC[ var(l) ] == 1 ) continue;
      if( data.doNotTouch( var(l) ) ) continue;
      // compute SCC
      eqCurrentComponent.clear();
      // if there is any SCC, add it to SCC, if it contains more than one literal
      eqTarjan(l,l,data,big,externBig);
  }
}

#define MININ(x,y) (x) < (y) ? (x) : (y)

void EquivalenceElimination::eqTarjan(Lit l, Lit list, CoprocessorData& data, BIG& big, vector< vector< Lit > >* externBig)
{
    eqNodeIndex[toInt(l)] = eqIndex;
    eqNodeLowLinks[toInt(l)] = eqIndex;
    eqIndex++;
    eqStack.push_back(l);
    eqLitInStack[ toInt(l) ] = 1;
    if( debug_out ) cerr << "c run tarjan on " << l << endl;
    if( externBig != 0 ) {
      const vector<Lit>& impliedLiterals =  (*externBig)[ toInt(list) ];
      for(uint32_t i = 0 ; i < impliedLiterals.size(); ++i)
      {
        const Lit n = impliedLiterals[i];
        if(eqNodeIndex[toInt(n)] == -1){
          eqTarjan(n, n, data,big,externBig);
          eqNodeLowLinks[toInt(l)] = MININ( eqNodeLowLinks[toInt(l)], eqNodeLowLinks[toInt(n)]);
        } else if( eqLitInStack[ toInt(n) ] == 1 ){
          eqNodeLowLinks[toInt(l)] = MININ(eqNodeLowLinks[toInt(l)], eqNodeIndex[toInt(n)]);
        }
      }
    } else {
      const Lit* impliedLiterals = big.getArray(list);
      const uint32_t impliedLiteralsSize = big.getSize(list);
      for(uint32_t i = 0 ; i < impliedLiteralsSize; ++i)
      {
        const Lit n = impliedLiterals[i];
	if( debug_out ) cerr << "c next implied lit from " << l << " is " << n << " [" << i << "/" << impliedLiteralsSize << "]" << endl;
        if(eqNodeIndex[toInt(n)] == -1){
          eqTarjan(n, n, data,big,externBig);
          eqNodeLowLinks[toInt(l)] = MININ( eqNodeLowLinks[toInt(l)], eqNodeLowLinks[toInt(n)]);
        } else if( eqLitInStack[ toInt(n) ] == 1 ){
          eqNodeLowLinks[toInt(l)] = MININ(eqNodeLowLinks[toInt(l)], eqNodeIndex[toInt(n)]);
        }
      }
    }

    // TODO: is it possible to detect failed literals?
    
     if(eqNodeLowLinks[toInt(l)] == eqNodeIndex[toInt(l)]){
         Lit n;
	 eqCurrentComponent.clear();
         do{
             n = eqStack[eqStack.size() - 1]; // *(eqStack.rbegin());
             eqStack.pop_back();
             eqLitInStack[ toInt(n) ] = 0;
             eqInSCC[ var(n) ] = 1;
             eqCurrentComponent.push_back(n);
         } while(n != l);
	 if( eqCurrentComponent.size() > 1 ) {
	  data.addEquivalences( eqCurrentComponent );
	 }
     }
}


Lit EquivalenceElimination::getReplacement( Lit l) const
{
  while ( var(replacedBy[var(l)]) != var(l) ) l = sign(l) ? ~replacedBy[var(l)] : replacedBy[var(l)]; // go down through the whole hierarchy!
  return l;
}

bool EquivalenceElimination::setEquivalent(Lit representative, Lit toReplace)
{
  const Lit r = getReplacement(representative);
  const Lit s = getReplacement(toReplace);
  if( r == ~s ) return false;
  if( debug_out ) cerr << "c ee literals: " << representative << " ( -> " << r << ") is representative for " << toReplace << " ( -> " << s << ")" << endl;
  if( r < s ) {
    replacedBy[ var(s) ] = ( sign(s) ? ~r : r ); // propagate forward!  
  } else {
    replacedBy[ var(r) ] = ( sign(r) ? ~s : s ); // propagate forward!  
  }
  /*
  replacedBy[ var(toReplace) ] = ( sign(toReplace) ? ~r : r );
  replacedBy[ var(s) ] = ( sign(s) ? ~r : r ); // propagate forward!
  */
  return true;
}

bool EquivalenceElimination::applyEquivalencesToFormula(CoprocessorData& data, bool force)
{
  bool newBinary = false;
  bool resetVariables = false;
  if( data.getEquivalences().size() > 0 || force) {
   
    // TODO: take care of units that have to be propagated, if an element of an EE-class has already a value!
    isToAnalyze.resize( data.nVars(), 0 );
    data.ma.resize(2*data.nVars());
    
    if( replacedBy.size() < data.nVars() ) { // extend replacedBy structure
      for( Var v = replacedBy.size(); v < data.nVars(); ++v )
	replacedBy.push_back ( mkLit(v,false) );
    }
    
   vector<Lit>& ee = data.getEquivalences();
   
   if( debug_out ) {
      if( debug_out ) {
	cerr << endl << "====================================" << endl;
	cerr << "intermediate formula before APPLYING Equivalences: " << endl;
	for( int i = 0 ; i < data.getClauses().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
	cerr << "c learnts: " << endl;
	for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
	  if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
	cerr << "====================================" << endl << endl;
      }
      
     cerr << "c equivalence stack: " << endl;
    for( int i = 0 ; i < ee.size(); ++ i ) {
      if( ee[i] == lit_Undef ) cerr << endl;
      else cerr << " " << ee[i];
    }
    cerr << endl;
   }
   
   int start = 0, end = 0;
   for( int i = 0 ; i < ee.size(); ++ i ) {
     if( ee[i] == lit_Undef ) {
       // handle current EE class!
       end = i - 1;
       Lit repr = getReplacement(ee[start]);
	for( int j = start ; j < i; ++ j ) // select minimum!
	{
	  data.ma.nextStep();
	  repr =  repr < getReplacement(ee[j]) ? repr : getReplacement(ee[j]);
	  data.ma.setCurrentStep( toInt( ee[j] ) );
	}

       // check whether a literal has also an old replacement that has to be re-considered!
       data.lits.clear();
       
       for( int j = start ; j < i; ++ j ) {// set all equivalent literals
	 const Lit myReplace = getReplacement(ee[j]);
	 if( ! data.ma.isCurrentStep( toInt( myReplace )) )
	   data.lits.push_back(myReplace); // has to look through that list as well!
	 if( ! setEquivalent(repr, ee[j] ) ) { data.setFailed(); return newBinary; }
       }
       
       if(debug_out)
       for( int j = start ; j < i; ++ j ) {// set all equivalent literals
         cerr << "c replace " << (sign(ee[j]) ? "-" : "" ) << var(ee[j]) + 1 << " by " << (sign(getReplacement(ee[j])) ? "-" : "" ) << var(getReplacement(ee[j])) + 1 << endl;
       }
       
       int dataElements = data.lits.size();
       for( int j = start ; j < i; ++ j ) {
	 Lit l = ee[j];
	 // first, process all the clauses on the list with old replacement variables
	 if( j == start && dataElements > 0 ) {
	   l = data.lits[ --dataElements ]; 
	   if( debug_out) cerr << "c process old replace literal " << l << endl;
	   j--;
         }
	 
	 if( l == repr ) continue;
	 data.log.log(eeLevel,"work on literal",l);
	 // if( getReplacement(l) == repr )  continue;
	 // TODO handle equivalence here (detect inconsistency, replace literal in all clauses, check for clause duplicates!)
	 for( int pol = 0; pol < 2; ++ pol ) { // do for both polarities!
	 vector<CRef>& list = pol == 0 ? data.list( l ) : data.list( ~l );
	 for( int k = 0 ; k < list.size(); ++ k ) {
	  Clause& c = ca[list[k]];
	  if( c.can_be_deleted() ) {
	    if( debug_out ) cerr << "c skip clause " << c << " it can be deleted already" << endl;
	    continue; // do not use deleted clauses!
	  }
	  data.log.log(eeLevel,"analyze clause",c);
	  if( debug_out ) cerr << "c analyze clause " << c << endl;
          bool duplicate  = false;
	  bool getsNewLiterals = false;
          Lit tmp = repr;
	  // TODO: update counter statistics for all literals of the clause!
          for( int m = 0 ; m < c.size(); ++ m ) {
	    if( c[m] == repr || c[m] == ~repr) { duplicate = true; continue; } // manage that this clause is not pushed into the list of clauses again!
	    const Lit tr = getReplacement(c[m]);
	    if( tr != c[m] ) getsNewLiterals = true;
	    c[m] = tr;
	  }
	  
	   const int s = c.size(); // sort the clause
	   for (int m = 1; m < s; ++m)
	   {
	      const Lit key = c[m];
	      int n = m - 1;
	      while ( n >= 0 && toInt(c[n]) > toInt(key) )
		  c[n+1] = c[n--];
	      c[n+1] = key;
	   }
	   
	   int n = 1;
	   for( int m = 1; m < s; ++ m ) {
	     if( c[m-1] == ~c[m] ) { 
	       if( debug_out ) cerr << "c ee deletes clause " << c << endl;
	       c.set_delete(true); 
	       goto EEapplyNextClause;
	    } // this clause is a tautology
	     if( c[m-1] != c[m] ) c[n++] = c[m];
	   }
           c.shrink(s-n);
	   
	   if( c.size() == 2 )  { // take care of newly created binary clause for further analysis!
	     newBinary = true;
	     if( isToAnalyze[ var(c[0]) ] == 0 ) {
	       eqDoAnalyze.push_back(~c[0]);
	       isToAnalyze[ var(c[0]) ] = 1;
	       if( debug_out ) cerr << "c EE re-enable ee-variable " << var(c[0])+1 << endl;
	     }
             if( isToAnalyze[ var(c[1]) ] == 0 ) {
	       eqDoAnalyze.push_back(~c[1]);
	       isToAnalyze[ var(c[1]) ] = 1;
	       if( debug_out ) cerr << "c EE re-enable ee-variable " << var(c[1])+1 << endl;
	     }
	   } else if (c.size() == 1 ) {
	     if( data.enqueue(c[0]) == l_False ) return newBinary; 
	   } else if (c.size() == 0 ) {
	     data.setFailed(); 
	     return newBinary; 
	   }
	  data.log.log(eeLevel,"clause after sort",c);
	  
	  if( !duplicate ) {
// 	    cerr << "c give list of literal " << (pol == 0 ? repr : ~repr) << " for duplicate check" << endl;
	    if( !hasDuplicate( data.list( (pol == 0 ? repr : ~repr)  ), c )  ) {
	      data.list( (pol == 0 ? repr : ~repr) ).push_back( list[k] );
	      if( getsNewLiterals ) {
		if( data.addSubStrengthClause( list[k] ) ) resetVariables = true;

	      }
	    } else {
 	      if( debug_out ) cerr << "c clause has duplicates: " << c << endl;
	      c.set_delete(true);
	      data.removedClause(list[k]);
	    }
	  } else {
// 	    cerr << "c duplicate during sort" << endl; 
	  }
	  
EEapplyNextClause:; // jump here, if a tautology has been found
	 }
	 } // end polarity
       }
       
       // clear all occurrence lists of certain ee class literals
       for( int j = start ; j < i; ++ j ) {
	 if( ee[j] == repr ) continue;
	 if( !data.doNotTouch(var(ee[j])) ) {
	 data.addToExtension( ~repr , ee[j] );
	 data.addToExtension( repr , ~ee[j]);
	 }
	 for( int pol = 0; pol < 2; ++ pol ) // clear both occurrence lists!
	   (pol == 0 ? data.list( ee[j] ) : data.list( ~ee[j] )).clear();
	 if( debug_out ) cerr << "c cleared list of var " << var( ee[j] ) + 1 << endl;
       }

       // TODO take care of untouchable literals!
       for( int j = start ; j < i; ++ j ) {
         
       }
       
       start = i+1;
       
       // the formula will change, thus, enqueue everything
       if( data.hasToPropagate() || subsumption.hasWork() ) {
	 resetVariables = true;
       }
// TODO necessary here?
//        // take care of unit propagation and subsumption / strengthening
//        if( data.hasToPropagate() ) { // after each application of equivalent literals perform unit propagation!
// 	 if( propagation.propagate(data,true) == l_False ) return newBinary;
//        }
//        subsumption.subsumeStrength(data);
//        if( data.hasToPropagate() ) { // after each application of equivalent literals perform unit propagation!
// 	 if( propagation.propagate(data,true) == l_False ) return newBinary;
//        }
       
     } // finished current class, continue with next EE class!
   }
   
   // TODO: take care of the doNotTouch literals inside the stack, which have been replaced still -> add new binary clauses!
   
   ee.clear(); // clear the stack, because all EEs have been processed
       
  }
  
   // if force, or there has been equis:
       // take care of unit propagation and subsumption / strengthening
    if( data.hasToPropagate() ) { // after each application of equivalent literals perform unit propagation!
	 resetVariables = true;
	 newBinary = true; // potentially, there are new binary clauses
	 if( propagation.propagate(data,true) == l_False ) return newBinary;
    }
    if( subsumption.hasWork() ) {
	subsumption.subsumeStrength();
	newBinary = true; // potentially, there are new binary clauses
    	resetVariables = true;
    }
    if( data.hasToPropagate() ) { // after each application of equivalent literals perform unit propagation!
      resetVariables = true;
      newBinary = true; // potentially, there are new binary clauses
      if( propagation.propagate(data,true) == l_False ) return newBinary;
    }

    // the formula will change, thus, enqueue everything
    if( resetVariables ) {
    // re-enable all literals (over-approximation) 
      for( Var v = 0 ; v < data.nVars(); ++ v ) {
	  if( isToAnalyze[ v ] == 0 ) {
		eqDoAnalyze.push_back( mkLit(v,false) );
		isToAnalyze[ v ] = 1;
	  }
      }
    }
  
  if( debug_out ) cerr << "c APLLYing Equivalences terminated with new binaries: " << newBinary << endl;
  return newBinary || force;
}

bool EquivalenceElimination::hasDuplicate(vector<CRef>& list, const Clause& c)
{
  bool irredundant = !c.learnt();
//   cerr << "c check for duplicates: " << c << " (" << c.size() << ") against " << list.size() << " candidates" << endl;
  for( int i = 0 ; i < list.size(); ++ i ) {
    Clause& d = ca[list[i]];
//     cerr << "c check " << d << " [del=" << d.can_be_deleted() << " size=" << d.size() << endl;
    if( d.can_be_deleted() || d.size() != c.size() ) continue;
    int j = 0 ;
    while( j < c.size() && c[j] == d[j] ) ++j ;
    if( j == c.size() ) { 
//       cerr << "c found partner" << endl;
      if( irredundant && d.learnt() ) {
	d.set_delete(true); // learned clauses are no duplicate for irredundant clauses -> delete learned!
	return false;
      }
      if( debug_out ) cerr << "c find duplicate " << d << " for clause " << c << endl;
      return true;
    }
//     cerr << "c clause " << c << " is not equal to " << d << endl;
  }
  return false;
}


void EquivalenceElimination::writeAAGfile(CoprocessorData& data)
{
  // get the circuit!
  vector<Circuit::Gate> gates;
  Circuit circ(ca); 
  circ.extractGates(data, gates);
  unsigned char type [ data.nVars() ];
  memset( type,0, sizeof(unsigned char) * data.nVars() );
  
  for( int i = 0 ; i < gates.size(); ++ i ) {
    const Circuit::Gate& g = gates[i];
    if( g.getType() != Circuit::Gate::AND ) continue;
    type [ var(g.x()) ] = ( type [ var(g.x()) ] | 1 ); // set output
    type [ var(g.a()) ] = ( type [ var(g.a()) ] | 2 ); // set input
    type [ var(g.b()) ] = ( type [ var(g.b()) ] | 2 ); // set input
  }
  vector <Var> pureInputs;
  vector <Var> pureOutputs;
  
  for( Var v = 0 ; v < data.nVars(); ++v ) {
    if( type[v] == 1 ) pureOutputs.push_back(v);
    if( type[v] == 2 ) pureInputs.push_back(v);
  }
  cerr << "AAG:" << endl
       << " maxV=" << data.nVars()
       << " inputs=" << pureInputs.size()
       << " outputs=" << pureOutputs.size()
       << " gates=" << gates.size() << endl;
  
  ofstream AAG( aagFile );
  AAG << "aag "
      << data.nVars() << " "
      << pureInputs.size() << " "
      << "0 "
      << pureOutputs.size() << " "
      << gates.size() << endl;
  // input
  for( int i = 0 ; i< pureInputs.size(); ++ i )
    AAG << (pureInputs[i]+1)*2 << endl;
  // output
  for( int i = 0 ; i< pureOutputs.size(); ++ i )
    AAG << (pureOutputs[i]+1)*2 << endl;
  // gates
  for( int i = 0; i < gates.size(); ++ i ) {
    const Circuit::Gate& g = gates[i];
    int lx = (var( g.x()) + 1) * 2 + ( sign(g.x()) ? 1 : 0);
    int la = (var( g.a()) + 1) * 2 + ( sign(g.a()) ? 1 : 0);
    int lb = (var( g.b()) + 1) * 2 + ( sign(g.b()) ? 1 : 0);
    AAG << lx << " " << la << " " << lb << endl;
  }
  AAG.close();
}


void EquivalenceElimination::printStatistics(ostream& stream)
{
  stream << "c [STAT] EE " << eeTime << " s, " << steps << " steps" << endl;
  stream << "c [STAT] EE-gate " << gateTime << " s, " << gateSteps << " steps, " << gateExtractTime << " extractGateTime, " << endl;
}
