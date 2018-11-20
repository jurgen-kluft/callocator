# xcore allocator library

A library containing many different allocators using a simple allocator interface:

* ```virtual void*		allocate(xsize_t size, u32 align) = 0;			///< Allocate memory with alignment```
* ```virtual void		deallocate(void* p) = 0;						///< Deallocate/Free memory```

Some allocators in this package:

* dlmalloc (<ftp://g.oswego.edu/pub/misc/malloc.c>)
* tlsf (<https://github.com/mattconte/tlsf>)
* allocator with seperated bookkeeping
* forward (like a ring buffer)
* fixed size allocator
* freelist
* indexed allocator (higher level can use indices instead of pointers to save memory)
* memento (a debug layer)
