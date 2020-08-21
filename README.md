# xcore allocator library

A library containing many different allocators using a simple allocator interface:

```c++
virtual void* allocate(u32 size, u32 align) = 0;  ///< Allocate memory with alignment
virtual u32 void deallocate(void* p) = 0;         ///< Deallocate/Free memory
```

Some allocators in this package:

* dlmalloc (<ftp://g.oswego.edu/pub/misc/malloc.c>)
* tlsf (<https://github.com/mattconte/tlsf>)
* allocator with seperated bookkeeping
* forward (like a ring buffer)
* fixed size allocator
* freelist
* indexed allocator (higher level can use indices instead of pointers to save memory)
* memento (a debug layer)

## thoughts

### Debug Allocator

- Using While Freed
- Double Free
- Leak

### Tag Allocator

For a specific module you could make just the standard allocator:

```c++
class xalloc_tagged : public xalloc
{
public:
	xalloc_tagged(u32 tag, xtalloc* alloc) : m_tag(tag), m_alloc(alloc) {}

	virtual void* v_allocate(u32 size, u32 align) { return m_alloc->v_allocate(size, align, m_tag); }
	virtual u32   v_deallocate(void* ptr) { return m_alloc->v_deallocate(ptr, m_tag); }

private:
	u32      m_tag;
	xtalloc* m_alloc;
}
```

allocate(u32 size, u32 alignment, u32 tag);
deallocate(void* ptr, u32 tag);

### Allocation Analyzer

- Frame Based
  - Pair ( Alloc, Dealloc ) => Preallocate this or try and remove the allocation/deallocation. Use a Scratch Allocator or Frame Based Allocator.
- Multi Frame
  - Many allocations deallocations over many frames from the same 'module'/'file'
  - Many small/same-size allocations deallocations from the same 'module'/'file' ==> Use a 'Pool Allocator'
  - Many Frame N allocation, Frame N + X deallocation ==> Use a 'temporal-allocator'

