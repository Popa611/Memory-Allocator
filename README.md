# Memory Allocator
This allocator supports **only 32bit or 64bit** systems.

A simple memory allocator which receives a pointer to an already allocated block of memory (heap) and its size. This means there are **no system calls (brk, sbrk...)** and it only manages the given block of memory.

This implementation uses the "**free list**" concept. The free list is a doubly linked list and
the memory is gradually divided into chunks based on what size the user needs. The heap keeps 
a head pointer to the first chunk in the free list. Each chunk has a header which contains
a size of space for user data in bytes. A chunk in the free list also has to contain 2 pointers - 
one pointing to the next chunk and one to the previous chunk in the free list. These pointers are
overwritten by user data when removed from the free list and returned to the user.

When the **user requests memory**, a proper chunk is found using the best-fit approach and removed from the free list.
A pointer to the chunk + 8 bytes is returned to the user. The first 8 bytes of the chunk is the header containing
the size (+ padding in case of x86 systems so that the returned address is 8 bytes aligned).

When the **user wants to deallocate memory**, we compute the beginning of the chunk by subtracting 8 bytes from the
passed pointer. Then the chunk is inserted into the free list into the proper spot (based on addresses). This is
useful for merging adjacent chunks in the free list when they are right next to each other in the actual memory.
This merging **minimizes fragmentation**.
  
This implementation on x86 is a little bit slower than the std allocator tested on matrix operations according to microbenchmarks using std::chrono.
However on x64 it looks like it is a little bit faster on the same matrix operations. Everything is tested on Windows 10, Intel Pentium CPU N3710 1.60GHz, 1 CPU, 4 logical and 4 physical cores.

When no suitable chunk exists during allocation -> bad_alloc exception is thrown.  
When deallocating already free memory -> undefined behaviour.
