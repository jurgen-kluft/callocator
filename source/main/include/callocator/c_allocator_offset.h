#ifndef __C_ALLOCATOR_OFFSET_ALLOCATOR_H__
#define __C_ALLOCATOR_OFFSET_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    struct arena_t;

    namespace noffset
    {
        static constexpr u32 NUM_TOP_BINS         = 32;
        static constexpr u32 BINS_PER_LEAF        = 8;
        static constexpr u32 TOP_BINS_INDEX_SHIFT = 3;
        static constexpr u32 LEAF_BINS_INDEX_MASK = 0x7;
        static constexpr u32 NUM_LEAF_BINS        = NUM_TOP_BINS * BINS_PER_LEAF;

        struct allocation_t
        {
            static constexpr u32 NO_SPACE = 0xffffffff;

            u32 offset   = NO_SPACE;
            u32 metadata = NO_SPACE; // internal: node index
        };

        struct storage_report_t
        {
            u32 totalFreeSpace;
            u32 largestFreeRegion;
        };

        struct full_storage_report_t
        {
            struct region_t
            {
                u32 size;
                u32 count;
            };

            region_t freeRegions[NUM_LEAF_BINS];
        };

        struct node_t;
        struct neighbor_t;

        struct allocator_t
        {
            arena_t*    m_allocator;
            u32         m_size;
            u32         m_maxAllocs;
            u32         m_freeStorage;
            u32         m_usedBinsTop;
            u8          m_usedBins[NUM_TOP_BINS];
            u32         m_binIndices[NUM_LEAF_BINS];
            node_t*     m_nodes;
            neighbor_t* m_neighbors;
            u32*        m_nodeUsed;
            u32         m_freeIndex;
            u32         m_freeListHead;
            u32         m_freeOffset;
        };

        allocator_t* create(u32 size, u32 maxAllocs = 128 * 1024);
        void         teardown(allocator_t* allocator);
        void         reset(allocator_t* allocator);

        allocation_t          allocate(allocator_t* allocator, u32 size);
        void                  free(allocator_t* allocator, allocation_t allocation);
        u32                   allocationSize(allocator_t* allocator, allocation_t allocation);
        storage_report_t      storageReport(allocator_t* allocator);
        full_storage_report_t storageReportFull(allocator_t* allocator);

    } // namespace noffset
} // namespace ncore

#endif // __C_GFX_COMMON_OFFSET_ALLOCATOR_H__
