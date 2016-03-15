/*****************************************************************************************[Alloc.h]
Copyright (c) 2015, Norbert Manthey
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


#ifndef RISS_Minisat_BlockAlloc_h
#define RISS_Minisat_BlockAlloc_h

#include "riss/mtl/XAlloc.h"
#include "riss/mtl/Vec.h"

#include <iostream>
namespace Riss
{

//=================================================================================================
// Block-based memory allocator, each block has size 2^k, blocks are stored in linked lists

template<class T>
class BlockAllocator
{
  public:
    /** elements to organize the memory */
    union MemoryItem {
        uint32_t nextBlock;
        T        element;
    };

  protected:
    MemoryItem* memory;     // actual block of memory
    uint32_t  sz;           // number of elements in the allocated memory block
    vec<int32_t> first;     // pointer to the first block of the given size ( size 2^k blocks are pointed to in first[k] ), -1 (any negative value) means there is no further element in the list

    /** compute the log2 of a number, by returning the number of trailing 0
     Note: works only for numbers with 2^k
     */
    uint64_t pseudoLog2(uint64_t n)
    {
        assert(n == (1 << __builtin_ctzl(n)) && "the argument of the function has to be a power of 2");
        return __builtin_ctzl(n);
    }

    /** reallocate memory
     @return pointer difference between the new block of memory and the old block of memory
     */
    T* realloc(uint64_t new_capacity);

    /** assign the elements of the given block
     @param remainingElements number of elements to be assigned
     @param firstFreeElement  index of the element in the memory structure that is still free
     @param upperLogBound     log2 of @remainingElements is not larger than this value
     */
    void assignRemainingBlock(const uint32_t firstFreeElement, uint32_t remainingElements, int upperLogBound);

  public:



    // TODO: make this a class for better type-checking?
    typedef uint32_t Ref;
    enum { Ref_Undef = (UINT32_MAX >> 1)    };   // divide by 2, as we sometimes cut off the highest bit
    enum { Ref_Error = (UINT32_MAX >> 1) - 1};   // divide by 2, as we sometimes cut off the highest bit
    enum { Unit_Size = sizeof(uint32_t) };

    explicit BlockAllocator(uint32_t start_cap = 1024 * 1024) : memory(nullptr), sz(0)
    {
        realloc(start_cap);
    }

    ~BlockAllocator()
    {
        if (memory != nullptr) {
            ::free(memory);
        }
    }

    /** allocate the given amount of memory (
     * Note: the content of the memory is not guaranteed to be 0!
     @param size number of allocated T-elements, must be 2^k
     @param diffPtr in case the underlying memory field has been reallocated, this pointer points to the difference of old and new memory fields (new - old)
     @return reference to the first element of the memory block that is assigned to this allocation
     */
    Ref      alloc(int size, T** diffPtr = nullptr);

    /** free the given memory
     @param ref reference to the block that should be freed
     @param size number of T elements that have been in the block (has to be 2^k)
     */
    void     free(const Ref& ref, int size);

    // Deref, Load Effective Address (LEA), Inverse of LEA (AEL):
    T&       operator[](Ref r)         // if (r < 0 || r >= sz) std::cerr << "r " << r << " sz " << sz << std::endl;
    {
        assert(r < sz); return memory[r].element;
    }
    const T& operator[](Ref r) const   // if (r < 0 || r >= sz) std::cerr << "r " << r << " sz " << sz << std::endl;
    {
        assert(r < sz); return memory[r].element;
    }

    T*       lea(Ref r)       { assert(r < sz); return &memory[r].element; }
    const T* lea(Ref r) const { assert(r < sz); return &memory[r].element; }
    Ref      ael(const T* t)
    {
        assert((void*)t >= (void*)&memory[0] && (void*)t < (void*)&memory[sz - 1]);
        return (Ref)(t - &memory[0]);
    }

    void     moveTo(BlockAllocator& to)
    {
        if (to.memory != nullptr) { ::free(to.memory); }
        to.memory = memory;
        to.sz = sz;
        first.moveTo(to.first);

        memory = nullptr;
        sz = 0;
    }

    void     copyTo(BlockAllocator& to) const
    {
        to.realloc(sz);                                     // ensure that there is enough space
        memcpy(to.memory, memory, sizeof(MemoryItem) * sz); // copy memory content
        to.sz = sz;                                           // lazyly delete all elements that might have been there before (does not call destructor)
        first.copyTo(to.first);
    }

    void clear(bool clean = false)
    {
        if (clean) {   // free used resources
            if (memory != nullptr) { ::free(memory); memory = nullptr; }
            sz = 0;
        } else {
            assert(sz == (1 << __builtin_ctz(sz)) && "the argument of the function has to be a power of 2");

            int ld2 = pseudoLog2(sz);
            assert(first.size() + 1 == ld2 && "all elements have been there initially");
            first.assign(-1);
            first[ ld2 ] = 0; // point to the full block
        }
    }

    int fullsize() const { return sz; }

    /** check whether free elements together sum up to total size */
    bool checkTotalSize() const
    {
        int totalElements = sz;
        for (int i = 0 ; i < first.size(); ++ i) {
            int count = 0;
            int start = first[i];
//  std::cerr << "c check 2^" << i << " ... starting with " << start << " ..." << std::endl;
            while (start >= 0) {
                count ++;
                totalElements -= (1 << i);
                start = memory[ start ] .nextBlock;
            }
//  std::cerr << "c left memory " << totalElements << " free blocks of size " << (1 << i) << ": " << count << std::endl;
        }
        return totalElements == 0;
    }
};

#if 0
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
    memory = (T*)xrealloc(memory, sizeof(T) * cap);
}
#endif

template<class T>
T*
BlockAllocator<T>::realloc(uint64_t new_capacity)
{
    if (new_capacity <= sz) { return nullptr; }
    assert(new_capacity == (1 << __builtin_ctz(new_capacity)) && "the argument of the function has to be a power of 2");
    MemoryItem* oldPtr = memory;
    memory = (MemoryItem*)::realloc(memory, sizeof(MemoryItem) * new_capacity);
    const int ld2 = pseudoLog2(new_capacity);
//   std::cerr << "c realloc with newCap: " << new_capacity << " and ld2 is " << ld2 << std::endl;
    uint32_t remainingElements = new_capacity - sz;

    first.growTo(ld2 + 1, -1);   // add "-1" for the new possible block sizes

    assignRemainingBlock(sz, remainingElements, ld2);   // assign the new memory into its blocks
    sz = new_capacity;                                  // store the new size

    return (T*)((T*)memory - (T*)oldPtr); // return the difference value
}

template<class T>
void BlockAllocator<T>::assignRemainingBlock(const uint32_t firstFreeElement, uint32_t remainingElements, int upperLogBound)
{
    do { // implement the recursion as iteratively

        while ((1 << upperLogBound) > remainingElements) { upperLogBound --; }     // find maximal block of 2^k that fits into the elements

        // we found the size of the new block
        const int blockSize = (1 << upperLogBound);                  // size of the current block
        remainingElements = remainingElements - blockSize;           // elements that are left after we took away the elements we needed
        const Ref blockStart = firstFreeElement + remainingElements; // careful, might overflow (twice)

        // update first-list
        memory[ blockStart ].nextBlock = first[ upperLogBound ];  // move current pointer to first element into memory block
        first [ upperLogBound ] = blockStart;                     // this memory block becomes the new head of the list of the given size

    } while (remainingElements > 0);

    assert(remainingElements == 0 && "the whole memory block should be assigned");
}

template<class T>
void BlockAllocator<T>::free(const Ref& ref, int size)
{
    // simply add the block to the correct list
    assert(size == (1 << __builtin_ctz(size)) && "the argument of the function has to be a power of 2");
    const int ld2 = pseudoLog2(size);
    assert(ld2 < first.size() && "element size must have been present already");

    memory[ ref ].nextBlock = first[ ld2 ];  // memorize current head of the list in new block
    first [ ld2 ] = ref;                     // store current block as new head of the list
}


template<class T>
typename BlockAllocator<T>::Ref
BlockAllocator<T>::alloc(int size, T** diffPtr)
{
    assert(size > 0 && "can allocate only positive number of elements");
    assert(size == (1 << __builtin_ctz(size)) && "the argument of the function has to be a power of 2");

    // determin k, such that 2^k >= size
    int initialK  = 0;
    while ((1 << initialK) < size) { ++initialK; };

    for (int iteration = 0 ; iteration < 2; ++ iteration) {
        int k = initialK;

        Ref returnValue = Ref_Undef;

        if (k < first.size()) {
            if (first[k] >= 0) {  // we have an element of the given block size

                returnValue = first[k];                  // use the head of the single linked list
                first[k] = memory[ first[k] ].nextBlock; // use the second element of the list as new head of the list
                return returnValue;                      // return the reference to the first element of the head of the old list

            } else {              // we do not have an element of the specified size in the list

                while (++k < first.size()) {   // increment the size and check for

                    if (first[k] >= 0) {
                        returnValue = first[k];                  // use the head of the single linked list
                        first[k] = memory[ first[k] ].nextBlock; // use the second element of the list as new head of the list
                        const int remainingBlockSize = (1 << k) - size;                    // size of the found block that is left over
                        assignRemainingBlock(returnValue + size, remainingBlockSize, k);   // add remaining space of the too-large block back to the allocator
                        return returnValue;                      // return the reference to the first element of the head of the old list
                    }

                }
                assert(k == first.size() && "should not jump over values");
            }
        }

        assert(iteration == 0 && "there has to be enough space after the first iteration!");
        // if we reach this position, there is not enough space left in the allocator - we need to realloc
        const int totalSize = sz + size;     // must not be 2^k, hence we cannot use the pseudoLog2 function
        while ((1 << k) <= totalSize) { k++; }

        // get more space, tell outside about offset between the two memory fields
        T* diff = realloc((1 << k));
        if (diffPtr != nullptr) { *diffPtr = diff; }

    }

    assert(false && "should never reach this position of the code");
    return Ref_Undef; // something went wrong
}


//=================================================================================================
}

#endif
