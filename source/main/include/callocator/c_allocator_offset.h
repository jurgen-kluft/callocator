#ifndef __C_ALLOCATOR_OFFSET_ALLOCATOR_H__
#define __C_ALLOCATOR_OFFSET_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    class alloc_t;

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
            u32 metadata = NO_SPACE;  // internal: node index
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

        class allocator_t
        {
        public:
            allocator_t(alloc_t* allocator, u32 size, u32 maxAllocs = 128 * 1024);
            allocator_t(allocator_t&& other);
            ~allocator_t();

            void setup();
            void teardown();
            void reset();

            allocation_t          allocate(u32 size);
            void                  free(allocation_t allocation);
            u32                   allocationSize(allocation_t allocation) const;
            storage_report_t      storageReport() const;
            full_storage_report_t storageReportFull() const;

            DCORE_CLASS_PLACEMENT_NEW_DELETE

        private:
            u32  insertNodeIntoBin(u32 size, u32 dataOffset);
            void removeNodeFromBin(u32 nodeIndex);

            inline bool isNodeUsed(u32 index) const { return (m_nodeUsed[index >> 5] & (1 << (index & 31))) != 0; }
            inline void setNodeUsed(u32 index) { m_nodeUsed[index >> 5] |= (1 << (index & 31)); }
            inline void setNodeUnused(u32 index) { m_nodeUsed[index >> 5] &= ~(1 << (index & 31)); }

            struct node_t
            {
                static constexpr u32 NIL = 0xffffffff;

                u32 dataOffset  = 0;
                u32 dataSize    = 0;
                u32 binListPrev = NIL;
                u32 binListNext = NIL;
            };

            struct neighbor_t
            {
                u32 prev;
                u32 next;
            };

            alloc_t*    m_allocator;
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
    }  // namespace ngfx
}  // namespace ncore

#endif  // __C_GFX_COMMON_OFFSET_ALLOCATOR_H__
