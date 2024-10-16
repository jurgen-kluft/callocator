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

## Object Component Allocator

The idea behind this allocator is that we can have objects that have components. This enables associating components (data) with objects in a dynamic way which means that you can add and remove components from objects at runtime. This is useful for example in game development where you have entities that have components like position, velocity, etc. But also for other things like a graph with node and edges where you want the graph to use to optimize different 
data sets, you can now create a GraphEdge as an object and decorate it with components that are part of your data-set.
