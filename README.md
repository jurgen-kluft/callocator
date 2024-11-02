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
* Stack allocator, stack based allocator for fast allocation and deallocation
* Object-Component allocator, allocator for managing objects that have components
* Segmented (2^N) allocator, allocate memory with sizes 2^N - 2^M, out of a memory range of 2^O

## TLSF Allocator

This is an implementation of the TLSF allocator, Two-Level Segregate Fit, which is a memory allocator that is designed to be fast and efficient for real-time systems. It is a general-purpose memory allocator that can be used in embedded systems, game development, and other applications where performance is critical.

## Frame Allocator

This allocator is designed to be used as a per-frame allocator. It is useful for allocating temporary memory that is only needed for the duration of a single frame. This can be useful for things like rendering, physics, or other systems that need to allocate temporary memory for a single frame and then free it at the end of the frame.

## Linear Allocator

This allocator is allocating forward and merges free memory, it is bounded and very fast, it is not multithread safe. It is useful for allocating memory that is only needed for a short period of time and can be deallocated all at once. This can be useful for things like loading assets, parsing data, or other tasks where you need to allocate a bunch of memory and then free it all at once.

## Stack Allocator

This allocator is a stack-based allocator that can only be used through the use of a 'scope'. It is useful for allocating memory similar to stack memory, all allocated memory will be released when the scope is destroyed. This can be useful for things like temporary memory that is only needed for a short period of time and can be deallocated all at once.

## Object Component Allocator

The idea behind this allocator is that we can have objects that have components. This enables associating components (data) with objects in a dynamic way which means that you can add and remove components from objects at runtime. This is useful for example in game development where you have entities that have components like position, velocity, etc. But also for other things like a graph with node and edges where you want the graph to use to optimize different 
data sets, you can now create a GraphEdge as an object and decorate it with components that are part of your data-set.

## Segmented Allocator

If you need to allocate sizes with power of 2 [2^N, 2^M] out of a memory range with size 2^O, then this allocator can do that.
