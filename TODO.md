# TODO for xallocator

- Virtual Memory, Multi-Threaded allocator <https://github.com/r-lyeh-archived/ltalloc>


Memory:

- Initialize the full allocator:
  - Dedicated-FSA size=16 allocator used to allocate nodes for our btree implementation 
    that is used by our generic FSA allocator.
  - Generic-FSA Allocator
    8/12/x/20/24/28/32/36/../64/72/80/88/96/104/112/120/128/.../2048
  - Medium Allocator
  - Large Allocator
  - Giant Allocator

- Make it multi-thread safe
- Make it suitable for GPU

