/**********************************************************************************[SharedMemVec.h]
Copyright (c) 2015, Norbert Manthey, LGPL v2, see LICENSE
Copyright (c) 2003-2007, Niklas Een, Niklas Sorensson
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

#ifndef RISS_Minisat_Vec_h
#define RISS_Minisat_Vec_h

#include <assert.h>
#include <new>

#include "riss/mtl/IntTypes.h"
#include "riss/mtl/XAlloc.h"
#include "riss/mtl/BlockAlloc.h"

namespace Riss {

    //=================================================================================================
    // Automatically resizable arrays
    //
    // NOTE! Don't use this vector on datatypes that cannot be re-located in memory (with realloc)
    //
    template <class T>
    class SharedMemVec {
        BlockAllocator<T>& allocator; // underlying block of memory, which is shared by multiple vectors
        int32_t start;                // first element of the used data
        int32_t end;                  // first element behind the used data

        // Don't allow copying (error prone):
        SharedMemVec<T>& operator=(SharedMemVec<T>& other) {
            assert(0);
            return *this;
        }
        SharedMemVec(SharedMemVec<T>& other) {
            assert(0);
        }

    public:
        // Constructors:
        SharedMemVec(BlockAllocator<T>& sharedAllocator)
            : allocator(sharedAllocator)
            , start(0)
            , end(0) {
        }
        explicit SharedMemVec(BlockAllocator<T>& sharedAllocator, int size)
            : allocator(sharedAllocator)
            , start(0)
            , end(0) {
            init(size);
        }
        ~SharedMemVec() {
            clear(true);
        }

        // Size operations:
        int size(void) const {
            return end - start;
        }

        /** when shrinking the vector, elements behind the vector itself have to be assigned >0! */
        void shrink(int nelems) {
            assert(nelems <= size());
            for (int i = end - nelems; i < end; i++) {
                allocator[i] = -1;
            };
            end = end - nelems;
        }

        void init(int size);
        void clear(bool dealloc = false);

        bool empty() const {
            return size() == 0;
        }

        // Stack interface:
        T* push(const T& elem); //  { if (sz == cap) { capacity(sz + 1); }  data[sz++] = elem; }
        void pop(void);         //   { assert(sz > 0); sz--, data[sz].~T(); }

        const T& last(void) const {
            allocator[end - 1];
        }
        T& last(void) {
            allocator[end - 1];
        }

        // Vector interface:

        /** access data via the index into the memory allocator */
        const T& operator[](int index) const {
            assert(start <= index && index < end);
            return allocator[index];
        }
        T& operator[](int index) {
            assert(start <= index && index < end);
            return allocator[index];
        }

        /** access data via the index from 0 to size of the vector */
        const T& at(int index) const {
            assert(0 <= index && index < end - start);
            return allocator[start + index];
        }
        T& at(int index) const {
            assert(0 <= index && index < end - start);
            return allocator[start + index];
        }

        // Duplicatation (preferred instead):
        void copyTo(SharedMemVec<T>& copy) const {
            if (copy.size() < size())
                copy.growTo(sz);
            for (int i = 0; i < end - start; i++) {
                copy[copy.start + i] = allocator[start + i];
            };
        }

        /** move data to other vector, first free space of the other vector, then simply use the area of this vector */
        void moveTo(SharedMemVec<T>& dest) {
            dest.clear(true);
            dest.start = start;
            dest.end = end;
            start = 0;
            end = 0;
        }

        /** swap content of two vectors, by swapping the indexes to start and end */
        void swap(SharedMemVec<T>& other) {
            const int tmpstart = other.start;
            const int tmpend = other.end;
            other.start = start;
            other.end = end;
            start = tmpstart;
            end = tmpend;
        }
    };

    template <class T>
    void SharedMemVec<T>::capacity(int min_cap) {
        if (cap >= min_cap) {
            return;
        }
        int add = imax((min_cap - cap + 1) & ~1, ((cap >> 1) + 2) & ~1); // NOTE: grow by approximately 3/2
        if (add > INT_MAX - cap || ((data = (T*)::realloc(data, (cap += add) * sizeof(T))) == nullptr) && errno == ENOMEM) {
            throw OutOfMemoryException();
        }
    }

    template <class T>
    void SharedMemVec<T>::growTo(int size, const T& pad) {
        if (sz >= size) {
            return;
        }
        capacity(size);
        for (int i = sz; i < size; i++) {
            data[i] = pad;
        }
        sz = size;
    }

    template <class T>
    void SharedMemVec<T>::growTo(int size) {
        if (sz >= size) {
            return;
        }
        capacity(size);
        for (int i = sz; i < size; i++) {
            new (&data[i]) T();
        }
        sz = size;
    }

    template <class T>
    void SharedMemVec<T>::clear(bool dealloc) {
        if (data != nullptr) {
            for (int i = 0; i < sz; i++) {
                data[i].~T();
            }
            sz = 0;
            if (dealloc) {
                free(data), data = nullptr, cap = 0;
            }
        }
    }

    //=================================================================================================
} // namespace Riss

#endif
