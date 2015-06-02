#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "riss/mtl/TwinAlloc.h"

using namespace std;


void allocfreetest()
{
    cout << "test allocation for 10000 memory pieces between 1 and 1024 bytes..." << endl;

    vector<void*> ptrs;
    vector<int> sizes;

    TwinAllocator alloc;

    for ( int i = 0; i < 1000; i++ ) {
        int size = (rand() % 510) + 1;
        sizes.push_back( size);
        cerr << "c size[" << i << "]: " << size << endl;
        ptrs.push_back ( alloc.get( size ) );
    }

    // free memory
    for ( int i = 0 ; i < 1000; i++ ) {
        cerr << "c free[" << i << "] " << sizes[i] << endl;
        alloc.release( ptrs[i], sizes[i] );
    }

}

void* samesize()
{
    vector<void*> ptrs;
    TwinAllocator alloc;

    cout << "c run samesize test" << endl;

    for ( int i = 0 ; i < 3000; i ++ ) {
        ptrs.push_back( alloc.get( 32 ) );
        *((int*)(ptrs[ ptrs.size() - 1 ])) = 5;
    }

    for ( int i =  0 ; i < 3000; ++i ) {
        alloc.release( ptrs[i], 32 );
    }
    return 0;
}

int main ()
{

    samesize();

//  allocfreetest();

    cout << "all tests passed" << endl;
    return 0;
}
