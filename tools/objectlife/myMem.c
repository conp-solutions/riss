
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdlib.h> 
#include <dlfcn.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <malloc.h>

#include <iostream>
#include <fstream>

#include <sys/time.h>
#include <time.h>

#include <inttypes.h>

inline uint64_t get_microseconds()
{
	timeval t;
	gettimeofday (&t, NULL);
	return t.tv_sec * 1000000 + t.tv_usec;
}


static uint64_t program_starttime = get_microseconds();
static uint64_t allocatedBytes    = 0;
static uint64_t maxAllocatedBytes = 0;

// writes to std::cerr if overloaded functions have been used
// #define DODEBUG


#ifdef DODEBUG
	#define PRINTF( msg ) std::cerr <<  msg << std::endl
#else
	#define PRINTF( msg )
#endif


// malloc
typedef void* (*malloc_type)(size_t size);

typedef void* (*calloc_type)(size_t count, size_t size);

typedef void (*free_type)(void *ptr);

typedef void* (*realloc_type)(void *ptr, size_t size);

typedef void* (*reallocf_type)(void *ptr, size_t size);

typedef void* (*valloc_type)(size_t size);

/**
	system functions
*/

// sbrk
typedef void* (*sbrk_type)(intptr_t increment); 

//mmap
typedef void* (*mmap_type)(void *addr, size_t len, int prot, int flags, int fildes, off_t off); 

//mmap2
typedef void* (*mmap2_type)(void *start, size_t length, int prot, int flags, int fd, off_t pgoffset);

//munmap
typedef int (*munmap_type)(void *start, size_t length);

//exit
typedef void (*exit_type)(int returnCode);


std::ofstream alloc_file( "allocs.dat" );
std::ofstream free_file( "free.dat" );

void* malloc(size_t size)
{
	// printf("1\n");
	static malloc_type orig = 0;
	if (0 == orig) {
		orig = (malloc_type)dlsym (RTLD_NEXT, "malloc");
		// printf("set malloc to %lu\n",orig);
		assert (orig && "original malloc function not found");
	}
	void * ret = orig (size);
//	PRINTF( "own MALLOC" );
	alloc_file << std::hex << ret << std::dec << " " << size << " " << get_microseconds() - program_starttime << " M" << std::endl;
	allocatedBytes += size;
	maxAllocatedBytes = maxAllocatedBytes >= allocatedBytes ? maxAllocatedBytes : allocatedBytes;
	return ret;
}

/*
void* calloc(size_t count, size_t size)
{
	// printf("2\n");
	static calloc_type orig = 0;
	if (0 == orig) {
		orig = (calloc_type)dlsym (RTLD_NEXT, "calloc");
		// printf("set calloc to %lu\n",orig);
		if( orig == (calloc_type)calloc ) printf("2.2\n");
		assert (orig && "original calloc function not found");
	}
	PRINTF( "own CALLOC" );
	void * ret = orig (count, size);
	alloc_file << std::hex << ret << std::dec << " " << count * size << " " << get_microseconds() - program_starttime << " C" << std::endl;
	return ret;
}
*/
void free(void *ptr)
{
	// printf("3\n");
	static free_type orig = 0;
	if (0 == orig) {
		orig = (free_type)dlsym (RTLD_NEXT, "free");
		if( orig == 0 ) printf("3.1\n");
		assert (orig && "original free function not found");
	}
	PRINTF( "own FREE" );
	free_file << std::hex << ptr << std::dec << " " << get_microseconds() - program_starttime << " F" << std::endl;
	orig (ptr);
}

void* realloc(void *ptr, size_t size)
{
	// printf("4\n");
	static realloc_type orig = 0;
	if (0 == orig) {
		orig = (realloc_type)dlsym (RTLD_NEXT, "realloc");
		if( orig == 0 ) printf("4.1\n");
		assert (orig && "original realloc function not found");
	}
	PRINTF( "own REALLOC" );
	free_file << std::hex << ptr << std::dec << " " << get_microseconds() - program_starttime << " R" << std::endl;
	void * ret = orig ( ptr, size);
	alloc_file << std::hex << ret << std::dec << " " << size << " " << get_microseconds() - program_starttime <<  " R" << std::endl;
	allocatedBytes += size;
	maxAllocatedBytes = maxAllocatedBytes >= allocatedBytes ? maxAllocatedBytes : allocatedBytes;
	return ret;
}

void* reallocf(void *ptr, size_t size)
{
	// printf("5\n");
	static reallocf_type orig = 0;
	if (0 == orig) {
		orig = (reallocf_type)dlsym (RTLD_NEXT, "reallocf");
		if( orig == 0 ) printf("5.1\n");
		assert (orig && "original reallocf function not found");
	}
	PRINTF( "own REALLOCF" );
	free_file << std::hex << ptr << std::dec << " " << get_microseconds() - program_starttime <<  " RF" << std::endl;
	void * ret = orig ( ptr, size);
	alloc_file << std::hex << ret << std::dec << " " << size << " " << get_microseconds() - program_starttime <<  " RF" << std::endl;
	allocatedBytes += size;
	maxAllocatedBytes = maxAllocatedBytes >= allocatedBytes ? maxAllocatedBytes : allocatedBytes;
	return ret;
}

void* valloc(size_t size)
{
	// printf("6\n");
	static valloc_type orig = 0;
	if (0 == orig) {
		orig = (valloc_type)dlsym (RTLD_NEXT, "valloc");
		assert (orig && "original valloc function not found");
	}
	PRINTF( "own VALLOC" );
	void * ret = orig (size);
	alloc_file << std::hex << ret << std::dec << " " << size << " " << get_microseconds() - program_starttime <<  " V" << std::endl;
	allocatedBytes += size;
	maxAllocatedBytes = maxAllocatedBytes >= allocatedBytes ? maxAllocatedBytes : allocatedBytes;
	return ret;
}


/**
 system routines
*/


void* sbrk(intptr_t increment)
{
	static sbrk_type orig = 0;
	// printf("use own sbrk\n");
	if (0 == orig) {
		orig = (sbrk_type)dlsym (RTLD_NEXT, "sbrk");
		assert (orig && "original sbrk function not found");
	}
	alloc_file << "sbrk" << std::endl;
	return orig (increment);
}

void* mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	static mmap_type orig = 0;
	// printf("use own mmap\n");
	if (0 == orig) {
		orig = (mmap_type)dlsym (RTLD_NEXT, "mmap");
		assert (orig && "original mmap function not found");
	}
	alloc_file << "mmap\n" << std::endl;
	return orig ( addr,len, prot,flags,fildes, off);
}

void* mmap2(void *start, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
	static mmap2_type orig = 0;
	// printf("use own mmap2\n");
	if (0 == orig) {
		orig = (mmap2_type)dlsym (RTLD_NEXT, "mmap2");
		assert (orig && "original mmap2 function not found");
	}
	alloc_file << "mmap2\n" << std::endl;
	return orig ( start,length, prot,flags,fd, pgoffset);
}

int munmap(void *start, size_t length)
{
	static munmap_type orig = 0;
	// printf("use own munmap\n");
	if (0 == orig) {
		orig = (munmap_type)dlsym (RTLD_NEXT, "munmap");
		assert (orig && "original munmap function not found");
	}
	alloc_file << "munmap\n" << std::endl;
	return orig ( start,length);
}


/*
ssize_t read (int fd, void *buf, size_t count) {
	static read_type orig = 0;
	if (0 == orig) {
		orig = (read_type)dlsym (RTLD_NEXT, "read");
		assert (orig && "original read function not found");
	}
	return orig (fd, buf, count);
}
*/

/*
int exit(int returnCode)
{
	static exit_type orig = 0;
	// printf("use own munmap\n");
	if (0 == orig) {
		orig = (exit_type)dlsym (RTLD_NEXT, "exit");
		assert (orig && "original exit function not found");
	}
	alloc_file << "exit - current: " << allocatedBytes << " max: " << maxAllocatedBytes << "\n" << std::endl;
	return orig ( returnCode );
}
*/
