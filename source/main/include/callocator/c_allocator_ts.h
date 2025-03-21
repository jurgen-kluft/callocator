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
