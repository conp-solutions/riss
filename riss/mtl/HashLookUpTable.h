/*******************************************************************************[HashLookUpTable.h]
Copyright (c) 2015, Norbert Manthey

***************************************************************************************************

Copyright (c) 2006-2010, Niklas Sorensson

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

#ifndef RISS_Minisat_MultiHashLookUpTable_h
#define RISS_Minisat_MultiHashLookUpTable_h

#include "riss/mtl/IntTypes.h"
#include "riss/mtl/Vec.h"

#include "riss/mtl/Map.h"

namespace Riss
{

//=================================================================================================
// Default hash/equals functions for multiple types
//
template<class K, class L> struct EqualKL { bool     operator()(const K& k1, const L& k2) const { return k1 == k2; } };
template<class K, class L> struct DeepEqualKL { bool     operator()(const K* k1, const L* k2) const { return *k1 == *k2; } };

//=================================================================================================
/** HashLookUpTable, where two different types of keys are supported
 *  one key for being stored in the hash table, and another for comparisons
 *
 *  Note: If the two clases are the same, use the usual map!
 */
template<class K, class L, class H = Hash<K>, class I = Hash<L>, class E = Equal<K>, class F = EqualKL<K, L> >
class HashLookUpTable
{
  public:

  private:
    H          hash;      // usual hash function
    E          equals;    // usual equals method for K

    I          hashL;     // hash function for L
    F          equalsKL;  // equals function for K and L

    vec<K>* table;
    int        cap;
    int        size;

    // Don't allow copying (error prone):
    HashLookUpTable<K, L, H, I, E, F>&  operator = (HashLookUpTable<K, L, H, I, E, F>& other) { assert(0); }
    HashLookUpTable(HashLookUpTable<K, L, H, I, E, F>& other) { assert(0); }

    bool    checkCap(int new_size) const { return new_size > cap; }

    int32_t index(const K& k) const { return hash(k) % cap; }

    int32_t index(const L& k) const { return hash(k) % cap; }

    void   _insert(const K& k)
    {
        vec<K>& ps = table[index(k)];
        ps.push(); ps.last() = k;
    }

    void    rehash()
    {
        const vec<K>* old = table;

        int old_cap = cap;
        int newsize = primes[0];
        for (int i = 1; newsize <= cap && i < nprimes; i++) {
            newsize = primes[i];
        }

        table = new vec<K>[newsize];
        cap   = newsize;

        for (int i = 0; i < old_cap; i++) {
            for (int j = 0; j < old[i].size(); j++) {
                _insert(old[i][j].key);
            }
        }

        delete [] old;

        // printf(" --- rehashing, old-cap=%d, new-cap=%d\n", cap, newsize);
    }


  public:

    HashLookUpTable() : table(nullptr), cap(0), size(0) {}
    HashLookUpTable(const H& h, const E& e) : hash(h), equals(e), table(nullptr), cap(0), size(0) {}
    ~HashLookUpTable() { delete [] table; }

    // PRECONDITION: the key must *NOT* exist in the map.
    void insert(const K& k) { if (checkCap(size + 1)) { rehash(); }  _insert(k); size++; }

    bool has(const K& k) const
    {
        if (size == 0) { return false; }
        const vec<K>& ps = table[index(k)];
        for (int i = 0; i < ps.size(); i++)
            if (equals(ps[i].key, k)) {
                return true;
            }
        return false;
    }

    bool hasL(const L& k) const
    {
        if (size == 0) { return false; }
        const vec<K>& ps = table[index(k)];
        for (int i = 0; i < ps.size(); i++)
            if (equals(ps[i].key, k)) {
                return true;
            }
        return false;
    }

    /** if there is only one element, return immediately */
    bool hasLone(const L& k) const
    {
        if (size == 0) { return false; }
        const vec<K>& ps = table[index(k)];
        if (ps.size() == 1) { return; }   // has to be the same element based on the above assumption
        for (int i = 0; i < ps.size(); i++)
            if (equals(ps[i].key, k)) {
                return true;
            }
        return false;
    }

    // PRECONDITION: the key must exist in the map.
    void remove(const K& k)
    {
        assert(table != nullptr);
        vec<K>& ps = table[index(k)];
        int j = 0;
        for (; j < ps.size() && !equals(ps[j].key, k); j++) {};
        assert(j < ps.size());
        ps[j] = ps.last();
        ps.pop();
        size--;
    }

    /** remove based on other key type */
    void removeL(const L& k)
    {
        assert(table != nullptr);
        vec<K>& ps = table[index(k)];
        int j = 0;
        for (; j < ps.size() && !equalsKL(ps[j].key, k); j++) {};
        assert(j < ps.size());
        ps[j] = ps.last();
        ps.pop();
        size--;
    }

    void clear()
    {
        cap = size = 0;
        delete [] table;
        table = nullptr;
    }

    int  elems() const { return size; }
    int  bucket_count() const { return cap; }

    // NOTE: the hash and equality objects are not moved by this method:
    void moveTo(HashLookUpTable& other)
    {
        delete [] other.table;

        other.table = table;
        other.cap   = cap;
        other.size  = size;

        table = nullptr;
        size = cap = 0;
    }

    // NOTE: given a bit more time, I could make a more C++-style iterator out of this:
    const vec<K>& bucket(int i) const { return table[i]; }
};

//=================================================================================================
}

#endif
