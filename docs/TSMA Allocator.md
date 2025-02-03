# Time Sequence Memory Allocator

Time Sequence Memory Allocator (TSMA) is a memory allocator that receives a full sequence of allocation, and free requests, their moment and duration in time, and their size. It will sort these allocations and determine the most optimal memory locations for each allocation.


- Allocations cannot overlap in time.
- Allocations can physically overlap in memory when they don't overlap in time.

Algorithm:

1. Sort all requests by their moment and duration in time (oldest and longest first, e.g. back to front). 
2. Iterate over all requests, when a request doesn't overlap with the previous one place it in the same group, otherwise create a new group.
3. Iterate over all requests and hand out memory addresses to the requests.
4. Done.

