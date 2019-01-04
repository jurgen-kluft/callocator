# Virtual Memory Allocator

These are just thoughts on a virtual memory allocator

1. CPU Fixed-Size-Allocator, 4 <= SIZE <= 16KiB
   16KiB page size
2. Virtual Memory: FreeList that can grow/shrink physical pages

Let's say an APP has 160 GB of address space and it has the following behaviour:

1. Many small allocations (FSA Heap)
2. CPU and GPU allocations (different calls, different page settings, VirtualAlloc/XMemAlloc)
3. Categories of GPU resources have different min/max size, alignment requirements, count and frequency

## Fixed Size Allocator

Not too hard to make multi-thread safe using atomics where the only hard multi-threading problem is page commit/decommit.

Page Size = 16 KB or 64 KB
FSA  = 512 MB Address Space / page size = 8192 pages  
Address space, BEGIN - END  

```c++
struct UsedPage {
    u16     m_freelist;
    u16     m_refcount;
};
struct FreePage {
    u16     m_prev;
    u16     m_next;
};
struct Page {
    union {
        UsedPage m_used;
        FreePage m_free;
    };
};

struct FSA
{
    enum {
        MIN_SIZE = 16,
        MAX_SIZE = 16 * 1024,
        INC_SIZE = 16,
        PAGE_SIZE = 16 * 1024,
        ADDR_RANGE = 512 * 1024 * 1024
    };
    Page        m_pages[ADDR_RANGE / PAGE_SIZE];
    u16         m_pages_freelist;
    u32         m_sizes_freelist[MAX_SIZE / INC_SIZE];

    VirtualMemory*  m_vmem;
};

```

Page[8192]  
Pages free-list = u32 head  
Used page = 4 bytes = ref-count  
FreeLists = u32 head[16384 / 16]  
Min = 16, Max = 16384  
Count = 16384 / 16 = 1024  

- Tiny implementation [+]
- Very low wastage [+]
- Makes use of flexible memory [+]
- Fast [+]
- Difficult to detect memory corruption [-]

## Medium Size Allocator

All other sizes go here:

- Non-contiguous virtual pages
- Grows and shrinks
- Traditional doubly linked list with headers
- Unsuitable for GPU memory
- Headers stored with data
- Pow2 free lists


## Large Size Allocator

Features:

- Reserves huge virtual address space (160GB)
- Each table divided into equal sized slots
- Maps and unmaps 64kB pages on demand
- Guarantees contiguous memory

Pros and Cons:

- No headers [+]
- Simple implementation (~200 lines of code) [+]
- No fragmentation [+]
- Size rounded up to page size [-]
- Mapping and unmapping kernel calls relatively slow [-]

## Clear Values (1337 speak)

- memset to byte value
  - Keep it memorable
- 0xFA – Flexible memory Allocated
- 0xFF – Flexible memory Free
- 0xDA – Direct memory Allocated
- 0xDF – Direction memory Free
- 0xA1 – memory ALlocated
- 0xDE – memory DEallocated

## Notes

PS4 = 994 GB address space

*/