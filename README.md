# ccore allocator library

A library containing many different allocators using a simple allocator interface:

```c++
virtual void* allocate(u32 size, u32 align) = 0;  ///< Allocate memory with alignment
virtual void void deallocate(void* p) = 0;         ///< Deallocate/Free memory
```

Some allocators in this package:

* TLSF allocator, Two-Level Segregate Fit
* Frame allocator, per-frame allocator
* Linear allocator, linear allocator designed for temporary memory
