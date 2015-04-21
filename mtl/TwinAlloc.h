/*************************************************************************************************
Copyright (c) 2015, Norbert Manthey
**************************************************************************************************/
// idea introduced in: SunOS by Jeff Bonwick

#ifndef _TWINALLOC_H_
#define _TWINALLOC_H_

#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <cassert>

#define SLAB_PAGE_SIZE (4096 - 24) // be aware of malloc overhead for next page!
#define SLAB_BINS 5                // have 5 different bins
#define SLAB_MALLOC_MAXSIZE 512    // have <= 2^5 to <= 2^9, the rest is for original malloc

/** memory allocator that treats small sizes differently, and returns original malloc-memory for larger sizes
 *  have SLAB lists for <=32 bytes, <=64bytes <=128bytes, <=256bytes, <=512bytes
 */
class TwinAllocator{

	template <int T>
	union element{
		char data[ T ];	// only used as storage, types with constructor are not allowed
		element* next;
	};
	
	void** _head    [SLAB_BINS];	// begin of list of pages
	void** _lastPage[SLAB_BINS];	// stores begin of first page
	
	void** _nextCell[SLAB_BINS];	// memory cell which is returned next
	void** _lastCell[SLAB_BINS];	// last free memory cell
	
	void* initPage(int bin, uint32_t size);
	
	/** find correct bin for given size of memory
	 * @param size size of memory that should be allocated, will be set to bin-maximum 
	 * @return number of bin where this size can be found
	 **/
	int findBin( uint32_t& size ) {
	  // find correct bin, set size to bin-maximum (extra function?)
	  int bin = 5;
	  for( ; bin < 9; bin ++ ) {
	    if( size <= (1<<bin) ) break;
	  }
	  
// 	  std::cerr << "c used bin: " << bin << " for size " << size << " results in block: " << bin - 5 << " with blocksize " << ( 1 << bin) << std::endl;
	  
	  size = ( 1 << bin );
	  bin = bin - 5;
	  
	  assert( bin >= 0 && "stay in bin bounds" );
// 	  std::cerr << "returned bin: " << bin << " and size: " << size << std::endl;
	  return bin;
	}
	
public:
	TwinAllocator(){
		// setup all initial pages for all sizes
		for( uint32_t i = 0 ; i < SLAB_BINS; ++i ){
		  _head[i] = 0;
		  _lastPage[i] = (void**)valloc( SLAB_PAGE_SIZE );
		  
		  if( _lastPage[i] == NULL ) {
		    assert( false && "ran out of memory" );
		    exit(3);
		  }
		  
		  memset( _lastPage[i], 0, SLAB_PAGE_SIZE );
	  
		  const uint32_t pageElements = ( (SLAB_PAGE_SIZE-8)/( 1 << (5+i) )); // think of the one pointer that needs to store the next page!
		  _nextCell[i] = &( _lastPage[i][1] );
		  std::cerr << "c setup for bin " << i << " with size " << (1 << (5+i) ) << std::endl;
		  _lastCell[i] = reinterpret_cast<void **>( reinterpret_cast<unsigned long>( &(_lastPage[i][1]) ) + (pageElements - 1) * (1 << (5+i)) );
		  // std::cerr << "set lastCell[" << i << "] to "  << std::hex << _lastCell[i] << std::dec <<  ", page(" << pageElements << " eles) base: " << std::hex << _lastPage[i] << std::dec << std::endl;		
		  _head[i] = _lastPage[i];
		}
	}
	
	~TwinAllocator(){
		for( uint32_t i = sizeof( void* ) ; i < SLAB_BINS; ++i ){
			while( _head[i] != _lastPage[i] )
			{
				void** tmp = (void**)(*(_head[i]));
				free( (void*)_head[i] );
				_head[i] = tmp;
			}
			// dont forget very last page!
			free( _head[i] );
		}
	}
	
	/// get memory of the specified size
	void* get(uint32_t size);
	
	/// free memory of the specified size
	void  release( void* adress, uint32_t size );
	
	/// resize memory of the specified size, return pointer to memory with the new size
	void* resize( void* adress, uint32_t new_size, uint32_t size );
};

inline void* TwinAllocator::initPage(int bin, uint32_t size)
{
	assert( size <= SLAB_MALLOC_MAXSIZE && "stay in range" );
	// std::cerr << "init page for" << size << " bytes" <<std::endl;
	void** ptr  = (void**)valloc( SLAB_PAGE_SIZE );
	
	if( ptr == NULL ) {
	  return NULL;
	}
	
// 	std::cerr << "c init another page for size " << size << " and bin " << bin << std::endl;
	
	*(_lastPage[bin]) = ptr;
	
	_lastPage[bin] = (void**) *(_lastPage[bin]);
	memset( _lastPage[bin], 0, SLAB_PAGE_SIZE );
	
	const uint32_t pageElements = ( (SLAB_PAGE_SIZE-8)/size); // think of the one pointer that needs to store the next page!
	_nextCell[bin] = &( _lastPage[bin][1] );
	_lastCell[bin] = reinterpret_cast<void **>( reinterpret_cast<unsigned long>( &(_lastPage[bin][1]) ) + (pageElements - 1) * size );
		// std::cerr << "set lastCell[" << size << "] to "  << std::hex << _lastCell[bin] << std::dec <<  ", page(" << pageElements << " eles) base: " << std::hex << _lastPage[bin] << std::dec << std::endl;		
	/*_lastCell[bin] = &( _lastPage[bin][pageElements - 1 ] );*/
	return _lastPage[bin];
}


inline void* TwinAllocator::get(uint32_t size)
{
	// check size and set real one!
	if( size >= SLAB_MALLOC_MAXSIZE ) return malloc(size);
	if( size < sizeof( void* ) ) size = sizeof( void* );
	
	int bin = findBin( size );
	
	void** t = _nextCell[bin];
	if( _nextCell[bin] != _lastCell[bin] ){
		_nextCell[bin] = ( (*(_nextCell[bin]))==0 ) 
			? /* TODO: fix this expression according to bin!*/ /*_nextCell[bin] + sizeof( void* )*/

					reinterpret_cast<void **>( reinterpret_cast<unsigned long>(_nextCell[bin]) + size)
			
				: (void**)(*(_nextCell[bin]));
	} else {
		initPage(bin,size);
	}
	// std::cerr << "get" << size << " bytes at " << std::hex << t << " set new_ele to " << _nextCell[size] << std::dec << std::endl;
	// std::cerr << " with content: " << *((unsigned int*)(_nextCell[size])) << std::endl;
	return t;
}


inline void TwinAllocator::release( void* adress, uint32_t size )
{
	// std::cerr << "release" << size << " bytes at " << std::hex << adress << std::dec << std::endl;
	// check size and set real one!
	if( size >= SLAB_MALLOC_MAXSIZE ) return free( adress );
	if( size < sizeof( void* ) ) size = sizeof( void* );
	
	assert( adress != NULL && adress != 0 && "cannot free memory to 0" );
	
	int bin = findBin( size );

	// set content of last Cell to jump to next cell
	*(_lastCell[bin]) 
						= adress;
	// jump to new last cell
	_lastCell[bin] 
						= (void**)adress;
}

inline void* TwinAllocator::resize( void* adress, uint32_t new_size, uint32_t size )
{
	// std::cerr << "resize " << std::hex << adress << " from " << std::dec << size << " to " << new_size << std::endl;
	// get new memory
	void* mem = get(new_size);
	uint32_t smaller = ( new_size < size ) ? new_size : size;
	// copy memory!
	memcpy( mem, adress, smaller );
	// free old memory
	release( adress, size );
	// return memory
	return mem;
}


extern TwinAllocator twinAlloc;

/*
 *
 * provide usual malloc functionality
 *
 */
inline void* slab_malloc(uint32_t size)
{
	return twinAlloc.get( size );
}

inline void slab_free( void* adress, uint32_t size )
{
	twinAlloc.release( adress, size );
}

inline void* slab_realloc( void* adress, uint32_t new_size, uint32_t size )
{
	return twinAlloc.resize( adress, new_size, size );
}

#endif
