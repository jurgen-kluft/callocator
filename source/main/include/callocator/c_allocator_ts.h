#ifndef __C_ALLOCATOR_TSMA_ALLOCATOR_H__
#define __C_ALLOCATOR_TSMA_ALLOCATOR_H__
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
        typedef u16 PkTime;
        typedef u16 PkDuration;
        typedef u32 PkAllocSize;

        struct allocation_t
        {
            PkTime      alloc_time;
            PkTime      free_time;
            PkAllocSize alloc_size;
        };

        // Process the allocations sequence and return the addresses of the allocations
        void process_sequence(PkDuration sequence_duration, allocation_t* allocations, u32 num_allocations, alloc_t* allocator, u32* out_addresses);
    } // namespace npk

} // namespace ncore

#endif