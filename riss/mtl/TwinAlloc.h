/*************************************************************************************************
Copyright (c) 2015, Norbert Manthey
**************************************************************************************************/
// idea introduced in: SunOS by Jeff Bonwick

#ifndef RISS_RISS_TWINALLOC_H_
#define RISS_TWINALLOC_H_

#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <cassert>

#include <cstdio>

#define SLAB_PAGE_SIZE 4096 // be aware of malloc overhead for next page!
#define SLAB_BINS 7                // have 7 different bins
#define SLAB_MALLOC_MAXSIZE 512    // have <= 2^7 to <= 2^9, the rest is for original malloc

/** memory allocator that treats small sizes differently, and returns original malloc-memory for larger sizes
 *  have SLAB lists for <=32 bytes, <=64bytes <=128bytes, <=256bytes, <=512bytes
 */
class TwinAllocator
{

    template <int T>
    union element {
        char data[ T ]; // only used as storage, types with constructor are not allowed
        element* next;
    };

    bool initialized ;              // indicate that the pages have been initialized
    void** _head    [SLAB_BINS];    // begin of list of pages
    void** _lastPage[SLAB_BINS];    // stores begin of first page

    void** _nextCell[SLAB_BINS];    // memory cell which is returned next
    void** _lastCell[SLAB_BINS];    // last free memory cell

    int counts [SLAB_BINS + 1];     // count distribution

    void* initPage(int bin, uint32_t size);

    /** find correct bin for given size of memory
     * @param size size of memory that should be allocated, will be set to bin-maximum
     * @return number of bin where this size can be found
     **/
    int findBin(uint32_t& size)
    {
        // find correct bin, set size to bin-maximum (extra function?)
        uint32_t bin = 0 ;
        for (; bin < SLAB_BINS; bin ++) {
            if (size <= binMaxSize(bin)) { break; }
        }
//    std::cerr << "c used bin: " << bin << " for size " << size << " results in block: " << bin - SLAB_BINS << " with blocksize " << ( 1 << bin) << std::endl;
        size = binMaxSize(bin);

        assert(bin < SLAB_BINS && "stay in bin bounds");
//    std::cerr << "returned bin: " << bin << " and size: " << size << std::endl;
        return bin;
    }

    // return size of a bin
    int binMaxSize(int bin)
    {
        return 1 << (bin + 3);
    }

    /** setup basic pages*/
    void initFirstPages()
    {

        // std::cerr << std::endl << "c initialize twin alloc (already done: " << initialized << ")" << std::endl;

        if (initialized) { return; }

        // setup all initial pages for all sizes
        for (uint32_t i = 0 ; i < SLAB_BINS; ++i) {
            _head[i] = 0;
            _lastPage[i] = (void**)valloc(SLAB_PAGE_SIZE);

            if (_lastPage[i] == nullptr) {
                assert(false && "ran out of memory");
                exit(3);
            }

            memset(_lastPage[i], 0, SLAB_PAGE_SIZE);

            const uint32_t pageElements = ((SLAB_PAGE_SIZE - 8) / binMaxSize(i));   // think of the one pointer that needs to store the next page!
            _nextCell[i] = &(_lastPage[i][1]);
            // std::cerr << "c setup for bin " << i << " with size " << binMaxSize(i) ) << std::endl;
            _lastCell[i] = reinterpret_cast<void **>(reinterpret_cast<unsigned long>(&(_lastPage[i][1])) + (pageElements - 1) *  binMaxSize(i)) ;
            // std::cerr << "set lastCell[" << i << "] to "  << std::hex << _lastCell[i] << std::dec <<  ", page(" << pageElements << " eles) base: " << std::hex << _lastPage[i] << std::dec << std::endl;
            _head[i] = _lastPage[i];
        }

        for (uint32_t i = 0; i <= SLAB_BINS; ++ i) {
            counts [i] = 0;
        }

        initialized = true;
    }

  public:
    TwinAllocator() : initialized(false)
    {
        initFirstPages();
    }

    ~TwinAllocator()
    {

        if (!initialized) { return; }

        for (uint32_t i = sizeof(void*) ; i < SLAB_BINS; ++i) {
            while (_head[i] != _lastPage[i]) {
                void** tmp = (void**)(*(_head[i]));
                free((void*)_head[i]);
                _head[i] = tmp;
            }
            // dont forget very last page!
            free(_head[i]);
        }
    }

    /// get memory of the specified size
    void* get(uint32_t size);

    /// free memory of the specified size
    void  release(void* adress, uint32_t size);

    /// resize memory of the specified size, return pointer to memory with the new size
    void* resize(void* adress, uint32_t new_size, uint32_t size);
};

inline void* TwinAllocator::initPage(int bin, uint32_t size)
{
    assert(size <= SLAB_MALLOC_MAXSIZE && "stay in range");
    // std::cerr << "init page for" << size << " bytes" <<std::endl;
    void** ptr  = (void**)valloc(SLAB_PAGE_SIZE);

    if (ptr == nullptr) {
        return nullptr;
    }

    // std::cerr << "c init another page for size " << size << " and bin " << bin << " at " << std::hex << ptr << std::dec << std::endl;

    *(_lastPage[bin]) = ptr;

    _lastPage[bin] = (void**) * (_lastPage[bin]);
    memset(_lastPage[bin], 0, SLAB_PAGE_SIZE);

    assert(size == binMaxSize(bin) && "work with intern data");

    const uint32_t pageElements = ((SLAB_PAGE_SIZE - 8) / size);  // think of the one pointer that needs to store the next page!
    _nextCell[bin] = &(_lastPage[bin][1]);
    _lastCell[bin] = reinterpret_cast<void **>(reinterpret_cast<unsigned long>(&(_lastPage[bin][1])) + (pageElements - 1) * size);
    // std::cerr << "set lastCell[" << bin << "] to "  << std::hex << _lastCell[bin] << std::dec <<  ", page(" << pageElements << " eles) base: " << std::hex << _lastPage[bin] << std::dec << std::endl;
    /*_lastCell[bin] = &( _lastPage[bin][pageElements - 1 ] );*/
    return _lastPage[bin];
}


inline void* TwinAllocator::get(uint32_t size)
{
    // std::cerr << "c twinalloc get " << size << std::endl;

    // check size and set real one!
    if (size > SLAB_MALLOC_MAXSIZE) {
        counts [SLAB_BINS] ++;
        return malloc(size);
    }
    if (size < sizeof(void*)) { size = sizeof(void*); }

    if (! initialized) { initFirstPages(); }   // make sure the object is initialized

    int bin = findBin(size);

    counts [bin] ++;

    void** t = _nextCell[bin];
    if (_nextCell[bin] != _lastCell[bin]) {
        _nextCell[bin] = ((*(_nextCell[bin])) == 0)
                         ? /* TODO: fix this expression according to bin!*/ /*_nextCell[bin] + sizeof( void* )*/

                         reinterpret_cast<void **>(reinterpret_cast<unsigned long>(_nextCell[bin]) + size)

                         : (void**)(*(_nextCell[bin]));
    } else {
        if (nullptr == initPage(bin, size)) { return nullptr; }   // tell outside that there was no more memory
    }

    // std::cerr << "get " << size << " bytes at " << std::hex << t << " set new_ele to " << _nextCell[bin] << std::dec << std::endl;
    // std::cerr << " with content: " << *((unsigned int*)(_nextCell[bin])) << std::endl;

    return t;
}


inline void TwinAllocator::release(void* adress, uint32_t size)
{
    // std::cerr << "release" << size << " bytes at " << std::hex << adress << std::dec << std::endl;
    // check size and set real one!
    if (size > SLAB_MALLOC_MAXSIZE) { return free(adress); }
    if (size < sizeof(void*)) { size = sizeof(void*); }

    assert(adress != nullptr && adress != 0 && "cannot free memory to 0");

    int bin = findBin(size);

    // set content of last Cell to jump to next cell
    // std::cerr << "c write " << std::hex << adress << " into " << &(_lastCell[bin]) << std::endl;
    *(_lastCell[bin])
        = adress;
    // std::cerr << "c set last cell to " << adress << std::dec << std::endl;
    // jump to new last cell
    _lastCell[bin]
        = (void**)adress;
}

inline void* TwinAllocator::resize(void* adress, uint32_t new_size, uint32_t size)
{
    // if the same bin can still be used, use it again
    if (adress != 0 && new_size <= SLAB_MALLOC_MAXSIZE && findBin(size) == findBin(new_size)) { return adress; }

    // std::cerr << "resize " << std::hex << adress << " from " << std::dec << size << " to " << new_size << std::endl;
    // get new memory
    void* mem = get(new_size);

    if (mem == nullptr) { return nullptr; }   // if there is no more space, tell the outside

    if (adress != 0) {   // otherwise nothing to be copied
        uint32_t smaller = (new_size < size) ? new_size : size;

        // copy memory!
        // std::cerr << "copy " << smaller << " bytes" << std::endl;
        ::memcpy(mem, adress, smaller * sizeof(char));

        for (int i = 0; i < smaller * sizeof(char); ++i) {
            char c = ((char*)mem)[i];
            char d = ((char*)adress)[i];
            assert(c == d && "should be the same");
        }

        // free old memory
        release(adress, size);
    }
    // return memory
    return mem;
}


extern TwinAllocator twinAlloc;
/*
 *
 * provide usual malloc functionality
 *
 */
inline TwinAllocator& getTwinAllocator()
{
    static TwinAllocator twinAlloc;
    return twinAlloc;
}

inline void* slab_malloc(uint32_t size)
{
    return getTwinAllocator().get(size);
}

inline void slab_free(void* adress, uint32_t size)
{
    getTwinAllocator().release(adress, size);
}

inline void* slab_realloc(void* adress, uint32_t new_size, uint32_t size)
{
    return getTwinAllocator().resize(adress, new_size, size);
}

#endif
