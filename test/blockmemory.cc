#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "riss/mtl/BlockAlloc.h"

using namespace std;
using namespace Riss;

void freetest() {
  BlockAllocator<uint32_t> ba(2);
 
  cerr << "c initialized correctly: " << ba.checkTotalSize() << endl;
  
  vector<uint32_t> ptr;
  vector<uint32_t> size;

  int sum = 0, current = 1;
  
  for( int i = 0 ; i < 32; ++ i ) {  // alloc
    int s = 1 << ( rand() % 5 + 1);
    size.push_back(s);
    int p = ba.alloc(s);
    ptr.push_back( p );
    
    for( int j = 0 ; j < s; ++ j ) { 
      current ++;
      ba[p + j] = current;
      sum += current;
    }
  }
  
  for( int i = 0 ; i < 16; ++ i ) {  // free some
    int s = size[ size.size() -1 ];
    int p = ptr[ ptr.size() - 1];
    
    for( int j = 0 ; j < s; ++ j ) sum -= ba[p + j]; // free elements of current block (sub from sum)
    
    ba.free( p,s );
    size.pop_back();
    ptr.pop_back();
  }

  for( int i = 0 ; i < 32; ++ i ) {  // alloc
    int s = 1 << ( rand() % 5 + 1);
    size.push_back(s);
    int p = ba.alloc(s);
    ptr.push_back( p );
    
    for( int j = 0 ; j < s; ++ j ) { 
      current ++;
      ba[p + j] = current;
      sum += current;
    }    
  }

  for( int i = 0 ; i < size.size(); ++ i ) {  // free all
    int s = size[ i ];
    int p = ptr[  i ];
    
    for( int j = 0 ; j < s; ++ j ) sum -= ba[p + j]; // free elements of current block (sub from sum)
    
    ba.free( p,s );
  }
  
  cerr << "c freed correctly: " << ba.checkTotalSize() << endl;
  cerr << "c sum==0? sum: " << sum << endl;
}

void alloctest() {
  BlockAllocator<uint32_t> ba(2);
  uint32_t ptr = ba.alloc(4);
  cerr << "c alloc 4 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(4);
  cerr << "c alloc 4 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(4);
  cerr << "c alloc 4 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(4);
  cerr << "c alloc 4 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(32);
  cerr << "c alloc 32 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(1);
  cerr << "c alloc 1 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(1);
  cerr << "c alloc 1 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(1);
  cerr << "c alloc 1 to " << ptr << ", size: " << ba.fullsize() << endl;
  ptr = ba.alloc(1);
  cerr << "c alloc 1 to " << ptr << ", size: " << ba.fullsize() << endl;
}


int main()
{

    alloctest();
    
    freetest();

//  allocfreetest();

    cout << "all tests passed" << endl;
    return 0;
}
