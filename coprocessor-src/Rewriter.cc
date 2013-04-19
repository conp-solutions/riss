/*************************************************************************************[Rewriter.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#include "coprocessor-src/Rewriter.h"

#include <algorithm>

using namespace Coprocessor;

static const char* _cat = "COPROCESSOR 3 - REWRITE";

static IntOption  opt_min             (_cat, "cp3_rew_min"  ,"min occurrence to be considered", 7, IntRange(0, INT32_MAX));
static IntOption  opt_minAMO          (_cat, "cp3_rew_minA" ,"min size of altered AMOs", 5, IntRange(0, INT32_MAX));
static IntOption  opt_rewlimit        (_cat, "cp3_rew_limit","number of steps allowed for REW", 1200000, IntRange(0, INT32_MAX));

static BoolOption opt_rew_ratio           (_cat, "cp3_rew_ratio" ,"allow literals in AMO only, if their complement is not more frequent", true);
static BoolOption opt_rew_once            (_cat, "cp3_rew_once"  ,"rewrite each variable at most once! (currently: yes only!)", true);

#if defined CP3VERSION 
static const int debug_out = 0;
#else
static IntOption debug_out                 (_cat, "rew-debug",       "Debug Output of Rewriter", 0, IntRange(0, 4));
#endif
	
Rewriter::Rewriter(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data)
: Technique( _ca, _controller )
, data( _data )
, processTime(0)
, rewLimit(opt_rewlimit)
, steps(0)
, rewHeap( data )
{
}


bool Rewriter::process()
{
  MethodTimer mt(&processTime);
  bool didSomething = false;


  // setup own structures
  rewHeap.addNewElement(data.nVars() * 2);
  for( Var v = 0 ; v < data.nVars(); ++ v ) {
    if( data[  mkLit(v,false) ] >= opt_min ) if( !rewHeap.inHeap(toInt(mkLit(v,false))) )  rewHeap.insert( toInt(mkLit(v,false)) );
    if( data[  mkLit(v,true)  ] >= opt_min ) if( !rewHeap.inHeap(toInt(mkLit(v,true))) )   rewHeap.insert( toInt(mkLit(v,true))  );
  }

  assert( opt_rew_once && "other parameter setting that true is not supported at the moment" );
  
  // have a slot per variable
  data.ma.resize( data.nVars() );
  data.ma.nextStep();
  
  MarkArray inAmo;
  inAmo.create( 2 * data.nVars() );
  inAmo.nextStep();
  
  // create full BIG, also rewrite learned clauses!!
  BIG big;
  big.create( ca,data,data.getClauses(),data.getLEarnts() );
  
  vector< vector<Lit> > amos; // all amos that are collected
  
  if( debug_out > 0) cerr << "c run with " << rewHeap.size() << " elements" << endl;
  
  // for l in F
  while (rewHeap.size() > 0 && (data.unlimited() || rewLimit > steps) && !data.isInterupted() ) {

    
    /** garbage collection */
    data.checkGarbage();
    
    const Lit right = toLit(rewHeap[0]);
    assert( rewHeap.inHeap( toInt(right) ) && "item from the heap has to be on the heap");

    if( debug_out > 2 && rewHeap.size() > 0 ) cerr << "c [BVA] new first item: " << rewHeap[0] << " which is " << right << endl;
    rewHeap.removeMin();
    
    if( opt_rew_ratio && data[right] < data[~right] ) continue; // if ratio, do not consider literals with the wrong ratio
    if( opt_rew_once && data.ma.isCurrentStep( var(right) ) ) continue; // do not touch variable twice!
    if( data[ right ] < opt_minAMO ) continue; // no enough occurrences -> skip!
    const uint32_t size = big.getSize( ~right );
    if( debug_out > 2) cerr << "c check " << right << " with " << data[right] << " cls, and " << size << " implieds" << endl;
    if( size < opt_minAMO ) continue; // cannot result in a AMO of required size -> skip!
    Lit* list = big.getArray( ~right );

    // create first list right -> X == -right \lor X, ==
    inAmo.nextStep(); // new AMO
    data.lits.clear(); // new AMO
    data.lits.push_back(right); // contains list of negated AMO!
    for( int i = 0 ; i < size; ++ i ) {
      const Lit& l = list[i];
      if( data[ l ] < opt_minAMO ) continue; // cannot become part of AMO!
      if( big.getSize( ~l ) < opt_minAMO ) continue; // cannot become part of AMO!
      if( opt_rew_once && data.ma.isCurrentStep( var(l) ) ) continue; // has been used previously
      if( opt_rew_ratio && data[l] < data[~l] ) continue; // if ratio, do not consider literals with the wrong ratio
      data.lits.push_back( list[i] );
    }
    if( debug_out > 2) cerr << "c implieds: " << data.lits.size() << endl;
    
    // TODO: should sort list according to frequency in binary clauses - ascending, so that small literals are removed first, increasing the chance for this more frequent ones to stay!
    
    // check whether each literal can be inside the AMO!
    for( int i = 1 ; i < data.lits.size(); ++ i ) { // keep the very first, since it created the list!
      const Lit& l = data.lits[i];
      const uint32_t size2 = big.getSize( ~l );
      Lit* list2 = big.getArray( ~l );
      // if not all, disable this literal, remove it from data.lits
      for( int j = 0 ; j < size2; ++ j ) inAmo.setCurrentStep( toInt(list2[j]) );
      int j = 0;
      for( ; j<data.lits.size(); ++ j ) if( i!=j && data.lits[j] != lit_Undef && !inAmo.isCurrentStep( toInt( data.lits[j] ) ) ) break;
      if( j != data.lits.size() ) {
	if( debug_out > 3) cerr << "c reject [" <<i<< "]" << data.lits[i] << ", because failed with [" << j << "]" << data.lits[j] << endl;
	data.lits[i] = lit_Undef; // if not all literals are covered, disable this literal!
      }
    }
    
    // found an AMO!
    for( int i = 0 ; i < data.lits.size(); ++ i )
      if ( data.lits[i] == lit_Undef ) { data.lits[i] = data.lits[ data.lits.size() - 1 ]; data.lits.pop_back(); --i; }
    
    // use only even sized AMOs -> drop last literal!
    if( data.lits.size() % 2 == 1 ) data.lits.pop_back();
    
    if( data.lits.size() < opt_minAMO ) continue; // AMO not big enough -> continue!
    
    // remember that these literals have been used in an amo already!
    amos.push_back( data.lits );
    for( int i = 0 ; i < data.lits.size(); ++ i )
      amos[amos.size() -1][i] = ~amos[amos.size() -1][i]; // need to negate all!
    
    for( int i = 0 ; i < data.lits.size(); ++ i )
      data.ma.setCurrentStep( var(data.lits[i]) );
    
    cerr << "c found AMO (negated, == AllExceptOne): " << data.lits << endl;
  }
 
  inAmo.destroy();
}

Var Rewriter::nextVariable(char type)
{
  Var nextVar = data.nextFreshVariable(type);
  
  // enlarge own structures
  rewHeap.addNewElement(data.nVars());
  
  return nextVar;
}

void Rewriter::printStatistics(ostream& stream)
{
  stream << "c [STAT] REW " << processTime << "s, "
  << endl;
}


void Rewriter::destroy()
{
  rewHeap.clear(true);
}

bool Rewriter::checkPush(vec<Lit> & ps, const Lit l)
{
    if (ps.size() > 0)
    {
        if (ps.last() == l)
         return false;
        if (ps.last() == ~l)
         return true;
    }
    ps.push(l);
    return false;
}
  
bool Rewriter::ordered_subsumes (const vec<Lit>& c, const Clause & other) const
{
    int i = 0, j = 0;
    while (i < c.size() && j < other.size())
    {
        if (c[i] == other[j])
        {
            ++i;
            ++j;
        }
        // D does not contain c[i]
        else if (c[i] < other[j])
            return false;
        else
            ++j;
    }
    if (i == c.size())
        return true;
    else
        return false;
}

bool Rewriter::ordered_subsumes (const Clause & c, const vec<Lit>& other) const
{
    int i = 0, j = 0;
    while (i < c.size() && j < other.size())
    {
        if (c[i] == other[j])
        {
            ++i;
            ++j;
        }
        // D does not contain c[i]
        else if (c[i] < other[j])
            return false;
        else
            ++j;
    }
    if (i == c.size())
        return true;
    else
        return false;
}
  
bool Rewriter::hasDuplicate(vector<CRef>& list, const vec<Lit>& c)
{
  for( int i = 0 ; i < list.size(); ++ i ) {
    Clause& d = ca[list[i]];
    if( d.can_be_deleted() ) continue;
    int j = 0 ;
    if( d.size() == c.size() ) {
      while( j < c.size() && c[j] == d[j] ) ++j ;
      if( j == c.size() ) { 
	detectedDuplicates ++;
	return true;
      }
    }
    if( true ) { // check each clause for being subsumed -> kick subsumed clauses!
      if( d.size() < c.size() ) {
	detectedDuplicates ++;
	if( ordered_subsumes(d,c) ) return true; // the other clause subsumes the current clause!
      } if( d.size() > c.size() ) { // if size is equal, then either removed before, or not removed at all!
	if( ordered_subsumes(c,d) ) { 
	  d.set_delete(true);
	  data.removedClause(list[i]);
	  removedViaSubsubption ++;
	}
      }
    }
  }
  return false;
}

