/************************************************************************[EquivalenceElimination.cc]
Copyright (c) 2012, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/EquivalenceElimination.h"

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - EE";

static IntOption opt_level  (_cat, "cp3_ee_level",  "EE on BIG, gate probing, structural hashing", 3, IntRange(0, 3));

static const int eeLevel = 1;
const static bool debug_out = true; // print output to screen

EquivalenceElimination::EquivalenceElimination(ClauseAllocator& _ca, Coprocessor::ThreadController& _controller, Propagation& _propagation)
: Technique(_ca,_controller)
, eqLitInStack(0)
, eqInSCC(0)
, eqIndex(0)
, isToAnalyze(0)
, propagation( _propagation )
{}

void EquivalenceElimination::eliminate(Coprocessor::CoprocessorData& data)
{
  // TODO continue here!!
  
  if( isToAnalyze == 0 ) isToAnalyze = (char*) malloc( sizeof( char ) * data.nVars()  );
  else isToAnalyze = (char*) realloc( isToAnalyze, sizeof( char ) * data.nVars()  );
  memset( isToAnalyze, 0 , sizeof(char) * data.nVars() );

  // find SCCs and apply them to the "replacedBy" structure
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    eqDoAnalyze.push_back( mkLit(v,false) );
    isToAnalyze[ v ] = 1;
  }
  
  if( replacedBy.size() < data.nVars() ) { // extend replacedBy structure
    for( Var v = replacedBy.size(); v < data.nVars(); ++v )
      replacedBy.push_back ( mkLit(v,false) );
  }
  
  if( false ) {
   cerr << "intermediate formula before eq: " << endl;
   for( int i = 0 ; i < data.getClauses().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
   for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
     if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
  }
  
  do { 
    findEquivalencesOnBig(data);                              // finds SCC based on all literals in the eqDoAnalyze array!
  } while ( applyEquivalencesToFormula(data ) && data.ok() ); // will set literals that have to be analyzed again!
  
  if( opt_level > 1 ) {
    
    if( true ) {
    cerr << "intermediate formula before gates: " << endl;
    for( int i = 0 ; i < data.getClauses().size(); ++ i )
      if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getClauses()[i] ] << endl;
    for( int i = 0 ; i < data.getLEarnts().size(); ++ i )
      if( !ca[  data.getClauses()[i] ].can_be_deleted() ) cerr << ca[  data.getLEarnts()[i] ] << endl;    
    }
    
    Circuit circ(ca); 
    vector<Circuit::Gate> gates;
    circ.extractGates(data, gates);
    data.log.log(eeLevel,"found gates", gates.size());
    for( int i = 0 ; i < gates.size(); ++ i ) {
      Circuit::Gate& gate = gates[i];
      // data.log.log(eeLevel,"gate output",gate.getOutput());
      if(debug_out) gate.print(cerr);
    }
    cerr << "c TODO TODO: for binary clauses, add the binary graph extension to BIG! TODO TODO" << endl;

    vector<Lit> oldReplacedBy = replacedBy;
    //vector< vector<Lit> >* externBig
    
    if( findGateEquivalences( data, gates ) )
      cerr << "c found new equivalences with the gate method!" << endl;
    
    replacedBy = oldReplacedBy;
    
    // after we extracted more information from the gates, we can apply these additional equivalences to the forula!
    while ( applyEquivalencesToFormula(data ) && data.ok() ) {  // will set literals that have to be analyzed again!
      findEquivalencesOnBig(data);                              // finds SCC based on all literals in the eqDoAnalyze array!
    }
  }
}

void EquivalenceElimination::initClause(const CRef cr)
{
  
}

bool EquivalenceElimination::findGateEquivalences(Coprocessor::CoprocessorData& data, vector< Circuit::Gate > gates)
{
  /** a variable in a circuit can participate in non-clustered gates only, or also participated in clustered gates */
  vector< vector<int32_t> > varTable ( data.nVars() ); // store for each variable which gates have this variable as input
  // store for each variable, whether this variable has a real output (bit 1=output, bit 2=clustered, bit 3 = stamped)
  vector< unsigned char > bitType ( data.nVars(), 0 ); // upper 4 bits are a counter to count how often this variable has been considered already as output
  
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
     case Circuit::Gate::HA_SUM:
       assert( false && "This gate type has not been implemented (yet)" );
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
  }
  
  // TODO: remove clustered gates, if they appear for multiple variables!
  
  if( true ) {
   for( Var v = 0 ; v < data.nVars(); ++ v ) {
     cerr << "c var " << v+1 << " role: " << ( bitType[v] & 2 != 0 ? "clustered " : "") << ( bitType[v] & 1 != 0 ? "output" : "") << " with dependend gates: " << endl;
     for( int i = 0 ; i < varTable[v].size(); ++ i )
       gates[ varTable[v][i] ].print(cerr);
   }
  }
  cerr << "c ===== START SCANNING ======= " << endl;
  
  vector< Var > inputVariables;
  deque< int > queue;
  // collect pure input variables!
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( bitType[v] == 0 ) {
      inputVariables.push_back(v);
      bitType[v] = bitType[v] | 4; // stamp
      // todo: stamp variable?!
      cerr << "c use for initial scanning: " << v << endl; 
    }
  }
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( bitType[v] & 2 != 0 ) {
      inputVariables.push_back(v);
      bitType[v] = bitType[v] | 4; // stamp
      // todo: stamp variable?!
      cerr << "c use for initial clustered scanning: " << v << endl; 
    }
  }
  
  // fill queue to work with
  for( int i = 0 ; i < inputVariables.size(); ++ i ) {
    const Var v = inputVariables[i];
    for( int j = 0 ; j < varTable[v].size(); ++ j ) {
      Circuit::Gate& g = gates[ varTable[v][j] ]; 
      if( !g.isInQueue() ) 
        { queue.push_back( varTable[v][j] ); g.putInQueue(); }
    }
  }
  
  if( queue.empty() ) cerr << "c there are no gates to start from, with " << gates.size() << " initial gates!" << endl;
  assert ( (gates.size() == 0 || queue.size() > 0) && "there has to be at least one gate to start from!" ); 
  while( !queue.empty() ) {
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
      processGate( data, g, gates, queue );
      enqueueSucessorGates(g,queue,gates,bitType,varTable);
      // TODO: stamp (all)output variable(s)
      // TODO: add gates with this variable as input. Be careful with adding a (clustered) gate again - have a counter per variable?! (bitType upper 4 bits?)
    } else {
       queue.push_back(gateIndex);
    }
  }
}

bool EquivalenceElimination::allInputsStamped(Circuit::Gate& g, std::vector< unsigned char > bitType)
{
   switch( g.getType() ) {
     case Circuit::Gate::AND:
       return bitType[ var(g.a()) ] & 4 != 0 && bitType[ var(g.b()) ] & 4 != 0 ;
       break;
     case Circuit::Gate::ExO:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
         if( bitType[ v ] & 4 == 0 ) return false;
       }
       return true;
     case Circuit::Gate::GenAND:
       for( int j = 0 ; j<g.size() ; ++ j ) {
	 const Var v = var( g.get(j) );
	 if( bitType[ v ] & 4 == 0 ) return false;
       }
       return true;
     case Circuit::Gate::ITE:
       if(  bitType[ var(g.s()) ] & 4 == 0  
	 || bitType[ var(g.t()) ] & 4 == 0  
	 || bitType[ var(g.f()) ] & 4 == 0 ) return false;
       return true;
     case Circuit::Gate::XOR:
       if(  bitType[ var(g.a()) ] & 4 == 0  
	 || bitType[ var(g.b()) ] & 4 == 0  
	 || bitType[ var(g.c()) ] & 4 == 0 ) return false;
       return true;
     case Circuit::Gate::HA_SUM:
       assert( false && "This gate type has not been implemented (yet)" );
       return false;
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

void EquivalenceElimination::enqueueSucessorGates( Circuit::Gate& g, std::deque< int > queue, std::vector<Circuit::Gate>& gates, std::vector< unsigned char > bitType, vector< vector<int32_t> >& varTable)
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
     case Circuit::Gate::HA_SUM:
       assert( false && "This gate type has not been implemented (yet)" );
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
}

void EquivalenceElimination::processGate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{
  switch( g.getType() ) {
     case Circuit::Gate::AND:
       return processANDgate(data,g,gates,queue);
     case Circuit::Gate::ExO:
       return processExOgate(data,g,gates,queue);
     case Circuit::Gate::GenAND:
       return processGenANDgate(data,g,gates,queue);
     case Circuit::Gate::ITE:
       return processITEgate(data,g,gates,queue);
     case Circuit::Gate::XOR:
       return processXORgate(data,g,gates,queue);
     case Circuit::Gate::HA_SUM:
       assert( false && "This gate type has not been implemented (yet)" );
       break;
     case Circuit::Gate::INVALID:
       break;
     default:
       assert( false && "This gate type cannot be handled (yet)" );
   }
}

void EquivalenceElimination::processANDgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{
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
    
    cerr << "c gates are equivalent: " << endl;
    g.print(cerr);
    other.print(cerr);
    
    if( var(x) != var(ox) ) {
      setEquivalent(x,ox);
      data.addEquivalences(x,ox);
      cerr << "c equi: " << x << " == " << ox << endl;
    } else {
      if( x == ~ox ) {
	data.setFailed();
	cerr << "c failed, because procedure found that " << x << " is equivalent to " << ox << endl;
      } else {
	cerr << "c found equivalence " << x << " == " << ox << " again" << endl;
      }
    }
    
  }
}

void EquivalenceElimination::processExOgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{

}

void EquivalenceElimination::processGenANDgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{

}

void EquivalenceElimination::processITEgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{

}

void EquivalenceElimination::processXORgate(CoprocessorData& data, Circuit::Gate& g, vector< Circuit::Gate >& gates, std::deque< int >& queue)
{

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
        if(eqNodeIndex[toInt(n)] == -1){
          eqTarjan(n, n, data,big,externBig);
          eqNodeLowLinks[toInt(l)] = MININ( eqNodeLowLinks[toInt(l)], eqNodeLowLinks[toInt(n)]);
        } else if( eqLitInStack[ toInt(n) ] == 1 ){
          eqNodeLowLinks[toInt(l)] = MININ(eqNodeLowLinks[toInt(l)], eqNodeIndex[toInt(n)]);
        }
      }
    }

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
  replacedBy[ var(toReplace) ] = ( sign(toReplace) ? ~r : r );
  replacedBy[ var(s) ] = ( sign(s) ? ~r : r ); // propagate forward!
  return true;
}

bool EquivalenceElimination::applyEquivalencesToFormula(CoprocessorData& data)
{
  bool newBinary = false;
  if( data.getEquivalences().size() > 0 ) {
   
   vector<Lit>& ee = data.getEquivalences();
   int start = 0, end = 0;
   for( int i = 0 ; i < ee.size(); ++ i ) {
     if( ee[i] == lit_Undef ) {
       // handle current EE class!
       end = i - 1;
       Lit repr = getReplacement(ee[start]);
       for( int j = start ; j < i; ++ j ) // select minimum!
	 repr =  repr < getReplacement(ee[j]) ? repr : getReplacement(ee[j]);

       for( int j = start ; j < i; ++ j ) {// set all equivalent literals
	 if( ! setEquivalent(repr, ee[j] ) ) { data.setFailed(); return newBinary; }
       }
       
       if(debug_out)
       for( int j = start ; j < i; ++ j ) {// set all equivalent literals
         cerr << "c replace " << (sign(ee[j]) ? "-" : "" ) << var(ee[j]) + 1 << " by " << (sign(getReplacement(ee[j])) ? "-" : "" ) << var(getReplacement(ee[j])) + 1 << endl;
       }
       
       for( int j = start ; j < i; ++ j ) {
	 Lit l = ee[j];
	 data.log.log(eeLevel,"work on literal",l);
	 // if( getReplacement(l) == repr )  continue;
	 // TODO handle equivalence here (detect inconsistency, replace literal in all clauses, check for clause duplicates!)
	 for( int pol = 0; pol < 2; ++ pol ) { // do for both polarities!
	 vector<CRef>& list = pol == 0 ? data.list( ee[j] ) : data.list( ~ee[j] );
	 for( int k = 0 ; k < list.size(); ++ k ) {
	  Clause& c = ca[list[k]];
	  if( c.can_be_deleted() ) continue; // do not use deleted clauses!
	  data.log.log(eeLevel,"analyze clause",c);
          bool duplicate  = false;
          Lit tmp = repr;
	  // TODO: update counter statistics for all literals of the clause!
          for( int m = 0 ; m < c.size(); ++ m ) {
	    if( c[m] == repr || c[m] == ~repr) { duplicate = true; continue; } // manage that this clause is not pushed into the list of clauses again!
	    c[m] = getReplacement(c[m]);
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
	     if( c[m-1] == ~c[m] ) { c.set_delete(true); goto EEapplyNextClause; } // this clause is a tautology
	     if( c[m-1] != c[m] ) c[n++] = c[m];
	   }
           c.shrink(s-n);
	   
	   if( c.size() == 2 )  { // take care of newly created binary clause for further analysis!
	     newBinary = true;
	     if( isToAnalyze[ var(c[0]) ] == 0 ) {
	       eqDoAnalyze.push_back(~c[0]);
	       isToAnalyze[ var(c[0]) ] = 1;
	     }
             if( isToAnalyze[ var(c[1]) ] == 0 ) {
	       eqDoAnalyze.push_back(~c[1]);
	       isToAnalyze[ var(c[1]) ] = 1;
	     }
	   } else if (c.size() == 1 ) {
	     if( data.enqueue(c[0]) == l_False ) return newBinary; 
	   } else if (c.size() == 0 ) {
	     data.setFailed(); 
	   }
	  data.log.log(eeLevel,"clause after sort",c);
	  
	  if( !duplicate ) {
	    // TODO: check whether this clause is already inside the list!
	    data.list( repr ).push_back( list[k] );
	  }
	  
EEapplyNextClause:; // jump here, if a tautology has been found
          if( c.can_be_deleted() ) data.log.log(eeLevel,"clause has complementary literals: ", c);
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
       }

       // TODO take care of untouchable literals!
       for( int j = start ; j < i; ++ j ) {
         
       }
       
       start = i+1;
       
       if( data.hasToPropagate() ) { // after each application of equivalent literals perform unit propagation!
	 if( propagation.propagate(data) == l_False ) return newBinary;
       }
       
     } // finished current class, continue with next EE class!
   }
   
   // TODO: take care of the doNotTouch literals inside the stack, which have been replaced still -> add new binary clauses!
   
   ee.clear(); // clear the stack, because all EEs have been processed
  }
  return newBinary;
}

