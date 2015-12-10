/****************************************************************************************[XAlloc.h]
Copyright (c) 2009-2010, Niklas Sorensson
Copyright (c) 2015, Norbert Manthey

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


#ifndef RISS_HUGEPAGEALLOC_h
#define RISS_HUGEPAGEALLOC_h

#include "riss/mtl/XAlloc.h"

namespace Riss
{

//=================================================================================================
// Simple layer on top of malloc/realloc to catch out-of-memory situtaions and provide some typing:

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

  
  #define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define ALIGN_TO_PAGE_SIZE(x) \
(((x) + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE * HUGE_PAGE_SIZE)

static inline
void *malloc_huge_pages(size_t size)
{
  // Use 1 extra page to store allocation metadata
  size_t real_size = ALIGN_TO_PAGE_SIZE(size + HUGE_PAGE_SIZE);
  char *ptr = (char *)mmap(NULL, real_size, PROT_READ | PROT_WRITE,
  MAP_PRIVATE | MAP_ANONYMOUS |
  MAP_POPULATE | MAP_HUGETLB, -1, 0);
  if (ptr == MAP_FAILED) {
  // The mmap() call failed. Try to malloc instead
  ptr = (char *)malloc(real_size);
  if (ptr == NULL) throw OutOfMemoryException(); // indicate that there is no memory left
  real_size = 0;
  }
  // Save real_size since mmunmap() requires a size parameter
  *((size_t *)ptr) = real_size;
  // Skip the page with metadata
  return ptr + HUGE_PAGE_SIZE;
}

static inline
void free_huge_pages(void *ptr)
{
  if (ptr == NULL) return;
  // Jump back to the page with metadata
  void *real_ptr = (char *)ptr - HUGE_PAGE_SIZE;
  // Read the original allocation size
  size_t real_size = *((size_t *)real_ptr);
  assert(real_size % HUGE_PAGE_SIZE == 0);
  if (real_size != 0)
  // The memory was allocated via mmap()
  // and must be deallocated via munmap()
  munmap(real_ptr, real_size);
  else
  // The memory was allocated via malloc()
  // and must be deallocated via free()
  free(real_ptr);
}

//=================================================================================================
}

#endif
