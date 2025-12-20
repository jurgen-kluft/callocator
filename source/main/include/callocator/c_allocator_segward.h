#ifndef __C_SEGWARD_ALLOCATOR_H__
#define __C_SEGWARD_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    // Forward Segmented Allocator (life time limited allocator)
    //
    // The forward segmented allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that doesn't differ much, like allocating data to receive UDP/TCP packets then very soon deallocating them.
    // This means that if you are tracking all allocations and deallocations on a linear time-line, you would see a cloud of allocations
    // moving forward in time. With this you can see that at some point in time the cursor will wrap around to the beginning of the
    // memory, where it will continue allocating from.
    // So this is very similar to the forward allocator, but this one doesn't use any linked list to track allocations, and doesn't
    // do any kind of merging on deallocation. Instead it uses segments of memory, where all segments live in the same virtual memory
    // address space. Each segment can be committed and decommitted independently, so when the cursor wraps around and hits a segment
    // that is still in use, it can request a new segment, commit it and continue allocating from there.
    // So when allocating most of the time, the only things that happens is that the cursor is moved forward, and a counter is that
    // is part of the segment is incremented. This makes allocation very very fast, and the same goes for deallocation, where the only
    // the counter is decremented. When a segment's counter reaches zero, the segment is marked as RETIRED, and can be reused or fully
    // decommitted when needed. An empty segment is marked as EMPTY, and an active segment is marked as ACTIVE.
    //
    // This allocator is blazingly fast in allocating O(1) and deallocating O(1).
    //
    // Requirements for creating the segmented forward allocator:
    // - segment constraints:
    //   - segment size must be a power of two
    //   - 4KB <= segment size <= 1GB
    //   - minimum number of segments >= 3
    //   - minimum segments <= number of segments < 32768
    //   - allocation sizes must always be <= (segment size / 64)
    //   - allocation alignment must be kept to a minimum (e.g. 1, 2, 4, 8, 16)
    // - 0 <= number of allocations per segment < 32768
    // - total size must be at least 3 times the segment size
    // - allocation alignment must be a power of two, at least 8 and less than (segment-size / 256)
    // - configure this allocator so that a segment can hold N allocations (N >= 256 at a minimum)

    namespace nsegward
    {
        struct allocator_t;
        allocator_t* create(int_t segment_size, int_t total_size);
        void         destroy(allocator_t* allocator);
        void*        allocate(allocator_t* a, u32 size, u32 alignment);
        void         deallocate(allocator_t* a, void* ptr);
    } // namespace nsegward

}; // namespace ncore

#endif /// __C_SEGWARD_ALLOCATOR_H__
