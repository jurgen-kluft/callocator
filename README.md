# ccore allocator library

A library containing some  allocators using the allocator interface from ccore:

```c++
virtual void* allocate(u32 size, u32 align) = 0;  ///< Allocate memory with alignment
virtual void void deallocate(void* p) = 0;         ///< Deallocate/Free memory
```

This package contains:

* TLSF allocator, Two-Level Segregate Fit
* Frame allocator, per-frame allocator
* Linear allocator, linear allocator designed for temporary memory
* Object-Component allocator, allocator for managing objects that have components