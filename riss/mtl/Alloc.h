/*****************************************************************************************[Alloc.h]
Copyright (c) 2008-2010, Niklas Sorensson

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


#ifndef RISS_Minisat_Alloc_h
#define RISS_Minisat_Alloc_h

#include "riss/mtl/XAlloc.h"
#include "riss/mtl/Vec.h"
#include "riss/mtl/HPVec.h"

#include <iostream>
namespace Riss
{

//=================================================================================================
// Simple Region-based memory allocator:

template<class T>
class RegionAllocator
{
    T*        memory;
    uint32_t  sz;
    uint32_t  cap;
    uint32_t  wasted_;

    void capacity(uint32_t min_cap);

  public:
    // TODO: make this a class for better type-checking?
    typedef uint32_t Ref;
    enum { Ref_Undef = (UINT32_MAX >> 1)    };   // divide by 2, as we sometimes cut off the highest bit
    enum { Ref_Error = (UINT32_MAX >> 1) - 1};   // divide by 2, as we sometimes cut off the highest bit
    enum { Unit_Size = sizeof(uint32_t) };

    explicit RegionAllocator(uint32_t start_cap = 1024 * 1024) : memory(nullptr), sz(0), cap(0), wasted_(0) { capacity(start_cap); }
    ~RegionAllocator()
    {
        if (memory != nullptr) {
            ::free(memory);
        }
    }

    uint32_t currentCap() const      { return cap;}
    uint32_t size() const      { return sz; }
    uint32_t wasted() const      { return wasted_; }

    Ref      alloc(int size);
    void     free(int size)    { wasted_ += size; }

    // Deref, Load Effective Address (LEA), Inverse of LEA (AEL):
    T&       operator[](Ref r)         // if (r < 0 || r >= sz) std::cerr << "r " << r << " sz " << sz << std::endl;
    {
        assert(r < sz); return memory[r];
    }
    const T& operator[](Ref r) const   // if (r < 0 || r >= sz) std::cerr << "r " << r << " sz " << sz << std::endl;
    {
        assert(r < sz); return memory[r];
    }

    T*       lea(Ref r)       { assert(r < sz); return &memory[r]; }
    const T* lea(Ref r) const { assert(r < sz); return &memory[r]; }
    Ref      ael(const T* t)
    {
        assert((void*)t >= (void*)&memory[0] && (void*)t < (void*)&memory[sz - 1]);
        return (Ref)(t - &memory[0]);
    }

    /** reduce used space to exactly fit the space that is needed */
    void fitSize()
    {
        cap = sz;                                      // reduce capacity to the number of currently used elements
        memory = (T*)::realloc(memory, sizeof(T) * cap); // free resources
    }

    void     moveTo(RegionAllocator& to)
    {
        if (to.memory != nullptr) { ::free(to.memory); }
        to.memory = memory;
        to.sz = sz;
        to.cap = cap;
        to.wasted_ = wasted_;

        memory = nullptr;
        sz = cap = wasted_ = 0;
    }

    void     copyTo(RegionAllocator& to) const
    {
        to.capacity(cap);                           // ensure that there is enough space
        memcpy(to.memory, memory, sizeof(T) * cap); // copy memory content
        to.sz = sz;                                 // lazyly delete all elements that might have been there before (does not call destructor)
        to.cap = cap;
        to.wasted_ = wasted_;

    }

    void clear(bool clean = false)
    {
        sz = 0; wasted_ = 0;
        if (clean) {   // free used resources
            if (memory != nullptr) { ::free(memory); memory = nullptr; }
            cap = 0;
        }
    }
};

template<class T>
void RegionAllocator<T>::capacity(uint32_t min_cap)
{
    if (cap >= min_cap) { return; }

    uint32_t prev_cap = cap;
    while (cap < min_cap) {
        // NOTE: Multiply by a factor (13/8) without causing overflow, then add 2 and make the
        // result even by clearing the least significant bit. The resulting sequence of capacities
        // is carefully chosen to hit a maximum capacity that is close to the '2^32-1' limit when
        // using 'uint32_t' as indices so that as much as possible of this space can be used.
        uint32_t delta = ((cap >> 1) + (cap >> 3) + 2) & ~1;
        cap += delta;

        if (cap <= prev_cap) {
            throw OutOfMemoryException();
        }
    }
    // printf(" .. (%p) cap = %u\n", this, cap);

    assert(cap > 0);
    memory = (T*)::realloc(memory, sizeof(T) * cap);
    
    // we want to get an error if the allocation failed
    if( memory == 0 ) throw OutOfMemoryException();
}


template<class T>
typename RegionAllocator<T>::Ref
RegionAllocator<T>::alloc(int size)
{
    // printf("ALLOC called (this = %p, size = %d)\n", this, size); fflush(stdout);
    assert(size > 0);
    capacity(sz + size);

    uint32_t prev_sz = sz;
    sz += size;

    // Handle overflow:
    if (sz < prev_sz) {
        throw OutOfMemoryException();
    }

    return prev_sz;
}


//=================================================================================================
}

#endif
