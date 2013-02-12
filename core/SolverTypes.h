/***********************************************************************************[SolverTypes.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/


#ifndef Minisat_SolverTypes_h
#define Minisat_SolverTypes_h

#include <cstdio>
#include <assert.h>

#include "mtl/IntTypes.h"
#include "mtl/Alg.h"
#include "mtl/Vec.h"
#include "mtl/Map.h"
#include "mtl/Alloc.h"

// for parallel stuff
#include <pthread.h>
#include "coprocessor-src/LockCollection.h"

/// TODO remove after debug
#include <iostream>
using namespace std;


namespace Minisat {

//=================================================================================================
// Variables, literals, lifted booleans, clauses:


// NOTE! Variables are just integers. No abstraction here. They should be chosen from 0..N,
// so that they can be used as array indices.

typedef int Var;
#define var_Undef (-1)


struct Lit {
    int     x;

    // Use this as a constructor:
    friend Lit mkLit(Var var, bool sign = false);

    bool operator == (Lit p) const { return x == p.x; }
    bool operator != (Lit p) const { return x != p.x; }
    bool operator <  (Lit p) const { return x < p.x;  } // '<' makes p, ~p adjacent in the ordering.
};


inline  Lit  mkLit     (Var var, bool sign) { Lit p; p.x = var + var + (int)sign; return p; }
inline  Lit  operator ~(Lit p)              { Lit q; q.x = p.x ^ 1; return q; }
inline  Lit  operator ^(Lit p, bool b)      { Lit q; q.x = p.x ^ (unsigned int)b; return q; }
inline  bool sign      (Lit p)              { return p.x & 1; }
inline  int  var       (Lit p)              { return p.x >> 1; }

// Mapping Literals to and from compact integers suitable for array indexing:
inline  int  toInt     (Var v)              { return v; }
inline  int  toInt     (Lit p)              { return p.x; }
inline  Lit  toLit     (int i)              { Lit p; p.x = i; return p; }

//const Lit lit_Undef = mkLit(var_Undef, false);  // }- Useful special constants.
//const Lit lit_Error = mkLit(var_Undef, true );  // }

const Lit lit_Undef = { -2 };  // }- Useful special constants.
const Lit lit_Error = { -1 };  // }

inline void printLit(Lit l)
{
    fprintf(stderr,"%s%d", sign(l) ? "-" : "", var(l)+1);
}

//=================================================================================================
// Lifted booleans:
//
// NOTE: this implementation is optimized for the case when comparisons between values are mostly
//       between one variable and one constant. Some care had to be taken to make sure that gcc
//       does enough constant propagation to produce sensible code, and this appears to be somewhat
//       fragile unfortunately.

#define l_True  (lbool((uint8_t)0)) // gcc does not do constant propagation if these are real constants.
#define l_False (lbool((uint8_t)1))
#define l_Undef (lbool((uint8_t)2))

class lbool {
    uint8_t value;

public:
    explicit lbool(uint8_t v) : value(v) { }

    lbool()       : value(0) { }
    explicit lbool(bool x) : value(!x) { }

    bool  operator == (lbool b) const { return ((b.value&2) & (value&2)) | (!(b.value&2)&(value == b.value)); }
    bool  operator != (lbool b) const { return !(*this == b); }
    lbool operator ^  (bool  b) const { return lbool((uint8_t)(value^(uint8_t)b)); }

    lbool operator && (lbool b) const {
        uint8_t sel = (this->value << 1) | (b.value << 3);
        uint8_t v   = (0xF7F755F4 >> sel) & 3;
        return lbool(v); }

    lbool operator || (lbool b) const {
        uint8_t sel = (this->value << 1) | (b.value << 3);
        uint8_t v   = (0xFCFCF400 >> sel) & 3;
        return lbool(v); }

    friend int   toInt  (lbool l);
    friend lbool toLbool(int   v);
};
inline int   toInt  (lbool l) { return l.value; }
inline lbool toLbool(int   v) { return lbool((uint8_t)v);  }

//=================================================================================================
// Clause -- a simple class for representing a clause:

class Clause;
typedef RegionAllocator<uint32_t>::Ref CRef;

class Clause {
    struct ClauseHeader {
        unsigned mark      : 1; // TODO: for what is this mark used? its set to 1, if the clause can be deleted (by simplify) we could do the same thing!
        unsigned locked    : 1; // if set to 1, some thread has locked the clause
        unsigned learnt    : 1;
        // + write get_method()
        unsigned has_extra : 1;
        unsigned reloced   : 1;
        //unsigned size      : 27;
//
	// FIXME TODO: if modified-flag is introduced, handle it in HiddenTautologyElimination.cc!! ~line 310

	// unsigned ignored : 1; do not use such a flag for the preprocessor! copy clause immediately instead and use delete flag!
        unsigned can_subsume : 1;
        unsigned can_strengthen : 1;
        unsigned size : 24;
        }                            header;
    union { Lit lit; float act; uint32_t abs; CRef rel; } data[0];

    friend class ClauseAllocator;

    // NOTE: This constructor cannot be used directly (doesn't allocate enough memory).
    template<class V>
    Clause(const V& ps, bool use_extra, bool learnt) {
        header.mark      = 0;
	header.locked    = 0;
        header.learnt    = learnt;
        header.has_extra = use_extra;
        header.reloced   = 0;
        header.size      = ps.size();
        header.can_subsume = 1;
        header.can_strengthen = 1;

        for (int i = 0; i < ps.size(); i++)
            data[i].lit = ps[i];

        if (header.has_extra){
            if (header.learnt)
                data[header.size].act = 0;
            else
                calcAbstraction(); }
    }

public:
    void calcAbstraction() {
        assert(header.has_extra);
        uint32_t abstraction = 0;
        for (int i = 0; i < size(); i++)
            abstraction |= 1 << (var(data[i].lit) & 31);
        data[header.size].abs = abstraction;  }
    
    // thread safe copy for strengthening  
    void calcAbstraction(Lit first) {
        assert(header.has_extra);
        uint32_t abstraction = 0;
        abstraction |= 1 << (var(first) & 31);
        for (int i = 1; i < size(); i++)
            abstraction |= 1 << (var(data[i].lit) & 31);
        data[header.size].abs = abstraction; 
    }

    int          size        ()      const   { return header.size; }
    void         shrink      (int i)         { assert(i <= size()); if (header.has_extra) data[header.size-i] = data[header.size]; header.size -= i; }
    void         pop         ()              { shrink(1); }
    bool         learnt      ()      const   { return header.learnt; }
    bool         has_extra   ()      const   { return header.has_extra; }
    uint32_t     mark        ()      const   { return header.mark; }
    void         mark        (uint32_t m)    { header.mark = m; }
    const Lit&   last        ()      const   { return data[header.size-1].lit; }

    bool         reloced     ()      const   { return header.reloced; }
    CRef         relocation  ()      const   { return data[0].rel; }
    void         relocate    (CRef c)        { header.reloced = 1; data[0].rel = c; }

    // NOTE: somewhat unsafe to change the clause in-place! Must manually call 'calcAbstraction' afterwards for
    //       subsumption operations to behave correctly.
    Lit&         operator [] (int i)         { return data[i].lit; }
    Lit          operator [] (int i) const   { return data[i].lit; }
    operator const Lit* (void) const         { return (Lit*)data; }

    float&       activity    ()              { assert(header.has_extra); return data[header.size].act; }
    uint32_t     abstraction () const        { assert(header.has_extra); return data[header.size].abs; }
    void         print       (bool nl=false) const { for (int i = 0; i < size(); i++){
                                                 printLit(data[i].lit);
                                                 fprintf(stderr," ");
                                               }
                                               if(nl) fprintf(stderr,"\n"); }

    Lit          subsumes         (const Clause& other) const;
    bool         ordered_subsumes (const Clause& other) const;
    bool         ordered_equals   (const Clause& other) const;
    void         remove_lit       (const Lit p);
    void         strengthen       (Lit p);

    void    set_delete (bool b) 	    { if (b) header.mark = 1; else header.mark = 0;}
    void    set_learnt (bool b)         { header.learnt = b; }
    bool    can_be_deleted()     const  { return mark() == 1; }
    
    /** sort the literals in the clause 
     *  Note: do not use if clause is watched or reason!
     */
    void sort() {
      const uint32_t s = size();
      for (uint32_t j = 1; j < s; ++j)
      {
	  const Lit key = operator[](j);
	  int32_t i = j - 1;
	  while ( i >= 0 && toInt(operator[](i)) > toInt(key) )
	  {
	      operator[](i+1) = operator[](i);
	      i--;
	  }
	  operator[](i+1) = key;
      }
    }

    void    set_has_extra(bool b)       {header.has_extra = b;}

    bool    can_subsume()        const  { return header.can_subsume; }
    void    set_subsume(bool b)         { header.can_subsume = b; }
    bool    can_strengthen()     const  { return header.can_strengthen; }
    void    set_strengthen(bool b)      { header.can_strengthen = b; }
    
    /** spinlocks the clause by trying to set the locked bit 
     * Note: be very careful with the implementation, it relies on the header being 32 bit!
     *
     * @return true  if the lock was acquired
     *         false gave up locking, because first literal of clause has changed
     *               (only if first lit was specified)
     */
    bool    spinlock(const Lit first = lit_Undef) {
      ClauseHeader compare = header;
      compare.locked = 0;
      ClauseHeader setHeader = header;
      setHeader.locked = 1;
      assert( sizeof(ClauseHeader) == sizeof(uint32_t) && "data type sizes have to be equivalent") ;
      uint32_t* cHeader = (uint32_t*)(&compare);
      uint32_t* sHeader = (uint32_t*)(&setHeader);
      uint32_t* iHeader = (uint32_t*)(&header);
      while ( *iHeader != *cHeader || __sync_bool_compare_and_swap( iHeader, uint32_t(*cHeader), uint32_t(*sHeader) ) == false) {
        // integrity check on first literal to prevent deadlocks
        assert(size() > 0);
        if (first != lit_Undef && data[0].lit != first)
          return false;
	// renew header
	compare = header;
	setHeader = header;
	compare.locked = 0;
	setHeader.locked = 1;
	cHeader = (uint32_t*)(&compare);
	sHeader = (uint32_t*)(&setHeader);
	iHeader = (uint32_t*)(&header);
      }
      return true;
    }
    
    /** check whether the locked bit of the clause is set 
     *  Note: cannot be used to detect whether somebody has the lock, because not atmoic!
     */
    bool isLocked() const { return header.locked; }
    
    /** reset the header bit unlocked to indicate that the clause is unlocked */
    void unlock() {
      header.locked = 0;
    };

    void    removePositionUnsorted(int i)    { data[i].lit = data[ size() - 1].lit; shrink(1); if (has_extra() && !header.learnt) calcAbstraction(); }
    inline void removePositionSorted(int i)      { for (int j = i; j < size() - 1; ++j) data[j] = data[j+1]; shrink(1); if (has_extra() && !header.learnt) calcAbstraction(); }
    // thread safe version, that changes the first literal last
    inline void removePositionSortedThreadSafe(int i)
    {
      if (i == 0)
      {
         Lit second = data[1].lit;
         for (int j = 1; j < size() - 1; ++j) data[j] = data[j+1]; shrink(1);  
         if (has_extra() && !header.learnt) calcAbstraction(second);
         data[0].lit = second;
      }
      else removePositionSorted(i);      
    }
 
    //DebugOutput
#ifdef SUBDEBUG
    inline void print_clause() const ;
    inline void print_lit(int i) const ;
#endif
};

//=================================================================================================
// ClauseAllocator -- a simple class for allocating memory for clauses:

/**
 * A struct that stores a maximum and a minimum address in the Clause Allocator
 *
 */
class AllocatorReservation 
{
    friend class ClauseAllocator;

    uint32_t current;
    uint32_t upper_limit;

    AllocatorReservation(uint32_t _current, uint32_t _limit) : current(_current), upper_limit(_limit) {}
};

const CRef CRef_Undef = RegionAllocator<uint32_t>::Ref_Undef;
class ClauseAllocator : public RegionAllocator<uint32_t>
{
    static int clauseWord32Size(int size, bool has_extra){
        return (sizeof(Clause) + (sizeof(Lit) * (size + (int)has_extra))) / sizeof(uint32_t); }
 public:
    bool extra_clause_field;

    ClauseAllocator(uint32_t start_cap) : RegionAllocator<uint32_t>(start_cap), extra_clause_field(false){}
    ClauseAllocator() : extra_clause_field(false){}

    void moveTo(ClauseAllocator& to){
        to.extra_clause_field = extra_clause_field;
        RegionAllocator<uint32_t>::moveTo(to); }

    template<class Lits>
    CRef alloc(const Lits& ps, bool learnt = false)
    {
        assert(sizeof(Lit)      == sizeof(uint32_t));
        assert(sizeof(float)    == sizeof(uint32_t));
        bool use_extra = learnt | extra_clause_field;

        CRef cid = RegionAllocator<uint32_t>::alloc(clauseWord32Size(ps.size(), use_extra));
        new (lea(cid)) Clause(ps, use_extra, learnt);

        return cid;
    }

    // Deref, Load Effective Address (LEA), Inverse of LEA (AEL):
    Clause&       operator[](Ref r)       { return (Clause&)RegionAllocator<uint32_t>::operator[](r); }
    const Clause& operator[](Ref r) const { return (Clause&)RegionAllocator<uint32_t>::operator[](r); }
    Clause*       lea       (Ref r)       { return (Clause*)RegionAllocator<uint32_t>::lea(r); }
    const Clause* lea       (Ref r) const { return (Clause*)RegionAllocator<uint32_t>::lea(r); }
    Ref           ael       (const Clause* t){ return RegionAllocator<uint32_t>::ael((uint32_t*)t); }

    void free(CRef cid)
    {
        Clause& c = operator[](cid);
        RegionAllocator<uint32_t>::free(clauseWord32Size(c.size(), c.has_extra()));
    }

    void reloc(CRef& cr, ClauseAllocator& to)
    {
        Clause& c = operator[](cr);
        if (c.reloced()) { cr = c.relocation(); return; }

        cr = to.alloc(c, c.learnt());
        // Copy Flags
        to[cr].header = c.header;
        c.relocate(cr);
        // Check this, otherwise segfault
        if (!c.learnt())
            to[cr].header.has_extra = to.extra_clause_field;


        // Copy extra data-fields:
        // (This could be cleaned-up. Generalize Clause-constructor to be applicable here instead?)
        to[cr].mark(c.mark());
        if (to[cr].learnt())         to[cr].activity() = c.activity();
        else if (to[cr].has_extra()) to[cr].calcAbstraction();
    }

    // Methods for threadsafe parallel allocation in BVE / subsimp context of CP3
 private:
    int requiredMemory(int clauses, int literals, int learnts) const
    {
        assert(sizeof(Lit)      == sizeof(uint32_t));
        assert(sizeof(float)    == sizeof(uint32_t));
        return            (   sizeof(Clause) * clauses 
                            + sizeof(Lit)    * literals
                            + sizeof(Lit)    * learnts          // for extra field
                            + sizeof(Lit)    * (((int) extra_clause_field) * (clauses - learnts)) // if extra-field is used
                          ) / sizeof(uint32_t);
    }

    /**
     * Checks whether CA has enough memory for some clause allocations
     * @param clauses   total number of clauses (learnt and non-learnt)
     * @param literals  total number of literals
     * @param learnts   number of clauses that are learnt
     */ 
    bool hasFreeMemory(int clauses, int literals, int learnts) const
    {
        assert(clauses >= learnts);
        if (literals < 2 * clauses)
        {
            std::cerr << "c lits: " << literals << " clauses: " << clauses << endl; 
        }
        assert(literals >= 2 * clauses);
        if ( currentCap() >= size() + requiredMemory(clauses, literals, learnts) )
            return true;
        else
            return false;
    };

 public:
    /**
     * Reserves memory in the CA.
     * Assumes that just one function calls reserveMemory 
     *      -> secure this with a spin lock before calling
     *      -> Make shure that the calling thread has not locked CA_Lock
     */
    AllocatorReservation reserveMemory(int clauses, int literals, int learnts, ReadersWriterLock CA_Lock)
    {
        bool need_lock = ! hasFreeMemory(clauses, literals, learnts);
        if (need_lock)
            CA_Lock.writeLock();
        uint32_t start = RegionAllocator<uint32_t>::alloc(requiredMemory(clauses, literals, learnts));
        uint32_t limit = RegionAllocator<uint32_t>::size();
        if (need_lock)
            CA_Lock.writeUnlock();
        return AllocatorReservation(start, limit);
    }

    template<class Lits>
    CRef allocThreadsafe(AllocatorReservation & reservation, const Lits& ps, bool learnt = false)
    {
        assert(sizeof(Lit)      == sizeof(uint32_t));
        assert(sizeof(float)    == sizeof(uint32_t));
        assert(reservation.current < reservation.upper_limit && "no reserved space left");
        
        bool use_extra = learnt | extra_clause_field;
        
        assert(reservation.current + clauseWord32Size(ps.size(), use_extra) <= reservation.upper_limit && "requested clause size does not fit in reservation");
        
        CRef cid = reservation.current;
        new (lea(cid)) Clause(ps, use_extra, learnt);
        
        reservation.current += clauseWord32Size(ps.size(), use_extra);

        return cid;
    }
    
    void freeReservation(AllocatorReservation & reservation)
    {
        assert(reservation.current <= reservation.upper_limit);
        RegionAllocator<uint32_t>::free(reservation.upper_limit - reservation.current);
        reservation.current       = 0;
        reservation.upper_limit   = 0;
    }
};



//=================================================================================================
// OccLists -- a class for maintaining occurrence lists with lazy deletion:

template<class Idx, class Vec, class Deleted>
class OccLists
{
    vec<Vec>  occs;
    vec<char> dirty;
    vec<Idx>  dirties;
    Deleted   deleted;

 public:
    OccLists(const Deleted& d) : deleted(d) {}

    void  init      (const Idx& idx){ occs.growTo(toInt(idx)+1); dirty.growTo(toInt(idx)+1, 0); }
    // Vec&  operator[](const Idx& idx){ return occs[toInt(idx)]; }
    Vec&  operator[](const Idx& idx){ return occs[toInt(idx)]; }
    Vec&  lookup    (const Idx& idx){ if (dirty[toInt(idx)]) clean(idx); return occs[toInt(idx)]; }

    void  cleanAll  ();
    void  clean     (const Idx& idx);
    void  smudge    (const Idx& idx){
        if (dirty[toInt(idx)] == 0){
            dirty[toInt(idx)] = 1;
            dirties.push(idx);
        }
    }

    void  clear(bool free = true){
        occs   .clear(free);
        dirty  .clear(free);
        dirties.clear(free);
    }
};


template<class Idx, class Vec, class Deleted>
void OccLists<Idx,Vec,Deleted>::cleanAll()
{
    for (int i = 0; i < dirties.size(); i++)
        // Dirties may contain duplicates so check here if a variable is already cleaned:
        if (dirty[toInt(dirties[i])])
            clean(dirties[i]);
    dirties.clear();
}


template<class Idx, class Vec, class Deleted>
void OccLists<Idx,Vec,Deleted>::clean(const Idx& idx)
{
    Vec& vec = occs[toInt(idx)];
    int  i, j;
    for (i = j = 0; i < vec.size(); i++)
        if (!deleted(vec[i]))
            vec[j++] = vec[i];
    vec.shrink(i - j);
    dirty[toInt(idx)] = 0;
}


//=================================================================================================
// CMap -- a class for mapping clauses to values:


template<class T>
class CMap
{
    struct CRefHash {
        uint32_t operator()(CRef cr) const { return (uint32_t)cr; } };

    typedef Map<CRef, T, CRefHash> HashTable;
    HashTable map;

 public:
    // Size-operations:
    void     clear       ()                           { map.clear(); }
    int      size        ()                const      { return map.elems(); }


    // Insert/Remove/Test mapping:
    void     insert      (CRef cr, const T& t){ map.insert(cr, t); }
    void     growTo      (CRef cr, const T& t){ map.insert(cr, t); } // NOTE: for compatibility
    void     remove      (CRef cr)            { map.remove(cr); }
    bool     has         (CRef cr, T& t)      { return map.peek(cr, t); }

    // Vector interface (the clause 'c' must already exist):
    const T& operator [] (CRef cr) const      { return map[cr]; }
    T&       operator [] (CRef cr)            { return map[cr]; }

    // Iteration (not transparent at all at the moment):
    int  bucket_count() const { return map.bucket_count(); }
    const vec<typename HashTable::Pair>& bucket(int i) const { return map.bucket(i); }

    // Move contents to other map:
    void moveTo(CMap& other){ map.moveTo(other.map); }

    // TMP debug:
    void debug(){
        printf(" --- size = %d, bucket_count = %d\n", size(), map.bucket_count()); }
};


/*_________________________________________________________________________________________________
|
|  subsumes : (other : const Clause&)  ->  Lit
|
|  Description:
|       Checks if clause subsumes 'other', and at the same time, if it can be used to simplify 'other'
|       by subsumption resolution.
|
|    Result:
|       lit_Error  - No subsumption or simplification
|       lit_Undef  - Clause subsumes 'other'
|       p          - The literal p can be deleted from 'other'
|________________________________________________________________________________________________@*/
inline Lit Clause::subsumes(const Clause& other) const
{
    //if (other.size() < size() || (extra.abst & ~other.extra.abst) != 0)
    //if (other.size() < size() || (!learnt() && !other.learnt() && (extra.abst & ~other.extra.abst) != 0))
    assert(!header.learnt);   assert(!other.header.learnt);
    assert(header.has_extra); assert(other.header.has_extra);
    if (other.header.size < header.size || (data[header.size].abs & ~other.data[other.header.size].abs) != 0)
        return lit_Error;

    Lit        ret = lit_Undef;
    const Lit* c   = (const Lit*)(*this);
    const Lit* d   = (const Lit*)other;

    for (unsigned i = 0; i < header.size; i++) {
        // search for c[i] or ~c[i]
        for (unsigned j = 0; j < other.header.size; j++)
            if (c[i] == d[j])
                goto ok;
            else if (ret == lit_Undef && c[i] == ~d[j]){
                ret = c[i];
                goto ok;
            }

        // did not find it
        return lit_Error;
    ok:;
    }

    return ret;
}


inline bool Clause :: ordered_subsumes (const Clause & other) const
{
    int i = 0, j = 0;
    while (i < size() && j < other.size())
    {
        if (data[i].lit == other[j])
        {
            ++i;
            ++j;
        }
        // D does not contain c[i]
        else if (data[i].lit < other[j])
            return false;
        else
            ++j;
    }
    if (i == size())
        return true;
    else
        return false;
}

/**
 * Checks if two clauses consist of the same elements
 * Expects both clauses to be ordered
 */
inline bool Clause::ordered_equals (const Clause & other) const 
{
    if (size() != other.size())
        return false;
    else 
        for (int i = 0; i < size(); ++i)
            if (data[i].lit != other[i])
                return false;
    return true;
}

inline void Clause::remove_lit(const Lit p)
{   
    for (int i = 0; i < size(); ++i)
    {
        if(data[i].lit == p)
        {
            while(i < size()-1)
            {
                data[i] = data[i + 1];
                ++i;
            }
            break;
        }
    }
    shrink(1);
    if (has_extra() && size() > 1 && !header.learnt)
        calcAbstraction();
}

inline void Clause::strengthen(Lit p)
{
    remove(*this, p);
    calcAbstraction();
}



//=================================================================================================
}

#endif
