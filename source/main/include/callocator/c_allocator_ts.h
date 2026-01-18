#ifndef __C_ALLOCATOR_TS_ALLOCATOR_H__
#define __C_ALLOCATOR_TS_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    class alloc_t;

    namespace nts
    {
        // Problem setup
        //
        // You have N requests i = 1..N.
        // Each request has:
        //
        // start time sᵢ,
        // end time eᵢ (assume half-open [sᵢ, eᵢ) unless you specify otherwise),
        // size (height) wᵢ (must be allocated contiguously in memory).
        //
        // We need to assign each request an offset yᵢ ≥ 0 so that for any two overlapping intervals in time, their memory ranges do not overlap:
        // If intervals i and j overlap in time, then [yᵢ, yᵢ + wᵢ) and [yⱼ, yⱼ + wⱼ) must be disjoint.
        // Goal: minimize the required memory M = sup_t (max allocated address in use at time t).

        // time points are in the range [0, 65535]
        struct allocation_t
        {
            u16 alloc_time;
            u16 free_time;
            u32 alloc_size;
            u16 index;
            u16 parent;  // this is used by the algorithm
            u32 address; // this is the result
        };

        // Process the allocations sequence and compute the address for each allocation
        u32 g_process_sequence(allocation_t* const allocations, u32 num_allocations, alloc_t* allocator);
    } // namespace nts

} // namespace ncore

#endif
