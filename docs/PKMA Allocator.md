# PreKnowledge Memory Allocator

PreKnowledge Memory Allocator (PKMA) is a memory allocator that beforehand knows all allocation, and free requests, their moment and duration in time, and their size.

1. Sort all requests by their moment and duration in time (oldest and longest first). 
2. Iterate over all requests, when a request doesn't overlap with the previous one make it a child of that request (recursively)
3. Iterate over all requests and hand out memory addresses to the requests.
4. Done.

