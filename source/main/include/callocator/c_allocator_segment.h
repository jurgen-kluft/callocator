#ifndef __CALLOCATOR_SEGMENTED_H__
#define __CALLOCATOR_SEGMENTED_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    class alloc_t;

    struct segment_alloc_t
    {
        struct level_t
        {
            u64* m_bin0;           // The binmap, level 0
            u64* m_bin1;           //
            u64* m_bin2;           //
            s32  m_number_of_bits; // The number of bits in this level
            s32  m_padding;        // Padding to 16 bytes
        };

        s8       m_min_size_shift;   // The minimum size of a segment in log2
        s8       m_max_size_shift;   // The maximum size of a segment in log2
        s8       m_total_size_shift; // The total size of the segment in log2
        s8       m_num_sizes;        // The number of sizes available
        u32      m_size_free;        // One bit per size, indicating if there is one or more free segments at that size.
        level_t* m_levels;           // One level per size
    };

    void segment_initialize(segment_alloc_t* sa, alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size);
    bool segment_allocate(segment_alloc_t* sa, s64 size, s64& offset);
    bool segment_deallocate(segment_alloc_t* sa, s64 ptr, s64 size);
    void segment_teardown(segment_alloc_t* sa, alloc_t* allocator);
} // namespace ncore

#endif
