#include "ccore/c_arena.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_offset.h"

namespace ncore
{
    // Implementation by Sebastian Aaltonen
    // https://github.com/sebbbi/OffsetAllocator

    // Note: Adjustments made to reduce memory consumption.
    namespace noffset
    {
        inline u32 lzcnt_nonzero(u32 v)
        {
#ifdef CC_COMPILER_MSVC
            unsigned long retVal;
            _BitScanReverse(&retVal, v);
            return 31 - retVal;
#else
            return __builtin_clz(v);
#endif
        }

        inline u32 tzcnt_nonzero(u32 v)
        {
#ifdef CC_COMPILER_MSVC
            unsigned long retVal;
            _BitScanForward(&retVal, v);
            return retVal;
#else
            return __builtin_ctz(v);
#endif
        }

        namespace nfloat
        {
            static constexpr u32 MANTISSA_BITS  = 3;
            static constexpr u32 MANTISSA_VALUE = 1 << MANTISSA_BITS;
            static constexpr u32 MANTISSA_MASK  = MANTISSA_VALUE - 1;

            u32 SizeToBinCeil(u32 size)
            {
                const s8 bin = (size >= 8) ? math::ilog2(size >> TOP_BINS_INDEX_SHIFT) : 0;
                size += ((1 << bin) - 1);                             // Round up to next leaf bin
                return (bin << TOP_BINS_INDEX_SHIFT) + (size >> bin); // Top bin index + leaf bin index
            }

            u32 SizeToBinFloor(u32 size)
            {
                const s8 bin = (size >= 8) ? math::ilog2(size >> TOP_BINS_INDEX_SHIFT) : 0;
                return (bin << TOP_BINS_INDEX_SHIFT) + (size >> bin); // Top bin index + leaf bin index
            }

            u32 BinToSize(u8 bin)
            {
                // top=((bin>>3) - 1), base=(1<<top), size=(base<<3 + (bin&7)<<top)
                const s8 top = ((bin >> TOP_BINS_INDEX_SHIFT) - 1);
                return top >= 0 ? ((((u32)1 << top) << 3) + ((u32)(bin & LEAF_BINS_INDEX_MASK) << top)) >> 1 : bin;
            }

            // Bin sizes follow floating point (exponent + mantissa) distribution (piecewise linear log approx)
            // This ensures that for each size class, the average overhead percentage stays the same
            u32 U32ToF32RoundUp(u32 size)
            {
                u32 exp      = 0;
                u32 mantissa = 0;

                if (size < MANTISSA_VALUE)
                {
                    // Denorm: 0..(MANTISSA_VALUE-1)
                    mantissa = size;
                }
                else
                {
                    // Normalized: Hidden high bit always 1. Not stored. Just like float.
                    u32 leadingZeros  = lzcnt_nonzero(size);
                    u32 highestSetBit = 31 - leadingZeros;

                    u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
                    exp                  = mantissaStartBit + 1;
                    mantissa             = (size >> mantissaStartBit) & MANTISSA_MASK;

                    u32 lowBitsMask = (1 << mantissaStartBit) - 1;

                    // Round up!
                    if ((size & lowBitsMask) != 0)
                        mantissa++;
                }

                return (exp << MANTISSA_BITS) + mantissa; // + allows mantissa->exp overflow for round up
            }

            u32 U32ToF32RoundDown(u32 size)
            {
                u32 exp      = 0;
                u32 mantissa = 0;

                if (size < MANTISSA_VALUE)
                {
                    // Denorm: 0..(MANTISSA_VALUE-1)
                    mantissa = size;
                }
                else
                {
                    // Normalized: Hidden high bit always 1. Not stored. Just like float.
                    u32 leadingZeros  = lzcnt_nonzero(size);
                    u32 highestSetBit = 31 - leadingZeros;

                    u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
                    exp                  = mantissaStartBit + 1;
                    mantissa             = (size >> mantissaStartBit) & MANTISSA_MASK;
                }

                return (exp << MANTISSA_BITS) | mantissa;
            }

            u32 F32ToU32(u32 floatValue)
            {
                u32 exponent = floatValue >> MANTISSA_BITS;
                u32 mantissa = floatValue & MANTISSA_MASK;
                if (exponent == 0)
                {
                    // Denorms
                    return mantissa;
                }
                else
                {
                    return (mantissa | MANTISSA_VALUE) << (exponent - 1);
                }
            }
        } // namespace nfloat

        // Utility functions
        static u32 s_findLowestSetBitAfter(u32 bitMask, u32 startBitIndex)
        {
            u32 maskBeforeStartIndex = (1 << startBitIndex) - 1;
            u32 maskAfterStartIndex  = ~maskBeforeStartIndex;
            u32 bitsAfter            = bitMask & maskAfterStartIndex;
            if (bitsAfter == 0)
                return allocation_t::NO_SPACE;
            return tzcnt_nonzero(bitsAfter);
        }

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

        static u32  insertNodeIntoBin(allocator_t* allocator, u32 size, u32 dataOffset);
        static void removeNodeFromBin(allocator_t* allocator, u32 nodeIndex);

        inline static bool isNodeUsed(allocator_t* allocator, u32 index) { return (allocator->m_nodeUsed[index >> 5] & (1 << (index & 31))) != 0; }
        inline static void setNodeUsed(allocator_t* allocator, u32 index) { allocator->m_nodeUsed[index >> 5] |= (1 << (index & 31)); }
        inline static void setNodeUnused(allocator_t* allocator, u32 index) { allocator->m_nodeUsed[index >> 5] &= ~(1 << (index & 31)); }

        allocator_t* create(u32 size, u32 maxAllocs)
        {
            ASSERT(size < 0x80000000); // Size must be less than 2^31

            uint_t total_size = sizeof(allocator_t);
            total_size += sizeof(node_t) * maxAllocs;
            total_size += sizeof(neighbor_t) * maxAllocs;
            total_size += sizeof(u32) * ((maxAllocs + 31) >> 5);

            arena_t* a = narena::new_arena(total_size, total_size);

            allocator_t* allocator = g_allocate<allocator_t>(a);

            allocator->m_size         = size;
            allocator->m_maxAllocs    = maxAllocs;
            allocator->m_freeStorage  = 0;
            allocator->m_usedBinsTop  = 0;
            allocator->m_nodes        = nullptr;
            allocator->m_neighbors    = nullptr;
            allocator->m_nodeUsed     = nullptr;
            allocator->m_freeIndex    = 0;
            allocator->m_freeListHead = node_t::NIL;
            allocator->m_freeOffset   = maxAllocs - 1;

            allocator->m_nodes     = g_allocate_array<node_t>(a, maxAllocs);
            allocator->m_neighbors = g_allocate_array<neighbor_t>(a, maxAllocs);
            allocator->m_nodeUsed  = g_allocate_array<u32>(a, ((maxAllocs + 31) >> 5));

            reset(allocator);
            return allocator;
        }

        void teardown(allocator_t* allocator)
        {
            narena::destroy(allocator->m_allocator);
        }

        void reset(allocator_t* allocator)
        {
            allocator->m_freeStorage = 0;
            allocator->m_usedBinsTop = 0;
            allocator->m_freeOffset  = allocator->m_maxAllocs - 1;

            for (u32 i = 0; i < NUM_TOP_BINS; i++)
                allocator->m_usedBins[i] = 0;

            for (u32 i = 0; i < NUM_LEAF_BINS; i++)
                allocator->m_binIndices[i] = node_t::NIL;

            allocator->m_freeIndex    = 0;
            allocator->m_freeListHead = node_t::NIL;

            // Start state: Whole storage as one big node
            // Algorithm will split remainders and push them back as smaller nodes
            insertNodeIntoBin(allocator, allocator->m_size, 0);
        }

        allocation_t allocate(allocator_t* allocator, u32 size)
        {
            // Out of allocations?
            if (allocator->m_freeOffset == 0)
            {
                allocation_t a;
                a.offset   = allocation_t::NO_SPACE;
                a.metadata = allocation_t::NO_SPACE;
                return a;
            }

            // Round up to bin index to ensure that alloc >= bin
            // Gives us min bin index that fits the size
            const u32 minBinIndex = nfloat::U32ToF32RoundUp(size);

            const u32 minTopBinIndex  = minBinIndex >> TOP_BINS_INDEX_SHIFT;
            const u32 minLeafBinIndex = minBinIndex & LEAF_BINS_INDEX_MASK;

            u32 topBinIndex  = minTopBinIndex;
            u32 leafBinIndex = allocation_t::NO_SPACE;

            // If top bin exists, scan its leaf bin. This can fail (NO_SPACE).
            if (allocator->m_usedBinsTop & (1 << topBinIndex))
            {
                leafBinIndex = s_findLowestSetBitAfter(allocator->m_usedBins[topBinIndex], minLeafBinIndex);
            }

            // If we didn't find space in top bin, we search top bin from +1
            if (leafBinIndex == allocation_t::NO_SPACE)
            {
                topBinIndex = s_findLowestSetBitAfter(allocator->m_usedBinsTop, minTopBinIndex + 1);

                // Out of space?
                if (topBinIndex == allocation_t::NO_SPACE)
                {
                    // return {.offset = allocation_t::NO_SPACE, .metadata = allocation_t::NO_SPACE};
                    allocation_t a;
                    a.offset   = allocation_t::NO_SPACE;
                    a.metadata = allocation_t::NO_SPACE;
                    return a;
                }

                // All leaf bins here fit the alloc, since the top bin was rounded up. Start leaf search from bit 0.
                // NOTE: This search can't fail since at least one leaf bit was set because the top bit was set.
                leafBinIndex = tzcnt_nonzero(allocator->m_usedBins[topBinIndex]);
            }

            const u32 binIndex = (topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex;

            // Pop the top node of the bin. Bin top = node.next.
            const u32 nodeIndex     = allocator->m_binIndices[binIndex];
            node_t&   node          = allocator->m_nodes[nodeIndex];
            const u32 nodeTotalSize = node.dataSize;
            node.dataSize           = size;
            setNodeUsed(allocator, nodeIndex);
            allocator->m_binIndices[binIndex] = node.binListNext;
            if (node.binListNext != node_t::NIL)
                allocator->m_nodes[node.binListNext].binListPrev = node_t::NIL;

            // Are 'node.binListNext' and 'node.binListPrev' still used after this?
            // Could we re-use them as neighbor next and prev?
            // If we can, then we can save another 8 bytes of memory per node.
            node.binListNext = 0xCDCDCDCD; // Clear bin list next
            node.binListPrev = 0xCDCDCDCD; // Clear bin list prev

            allocator->m_freeStorage -= nodeTotalSize;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (-%u) (allocate)\n", allocator->m_freeStorage, nodeTotalSize);
#endif

            // Bin empty?
            if (allocator->m_binIndices[binIndex] == node_t::NIL)
            {
                allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex); // Remove a leaf bin mask bit

                // All leaf bins empty?
                if (allocator->m_usedBins[topBinIndex] == 0)
                {
                    allocator->m_usedBinsTop &= ~(1 << topBinIndex); // Remove a top bin mask bit
                }
            }

            // Push back remaining N elements to a lower bin
            const u32 remainderSize = nodeTotalSize - size;
            if (remainderSize > 0)
            {
                const u32 newNodeIndex = insertNodeIntoBin(allocator, remainderSize, node.dataOffset + size);

                neighbor_t& neighbor = allocator->m_neighbors[nodeIndex];

                // Link new node after the current node so that we can merge them later if both are free
                // And update the old next neighbor to point to the new node (in middle)
                if (neighbor.next != node_t::NIL)
                    allocator->m_neighbors[neighbor.next].prev = newNodeIndex;
                allocator->m_neighbors[newNodeIndex].prev = nodeIndex;
                allocator->m_neighbors[newNodeIndex].next = neighbor.next;
                neighbor.next                             = newNodeIndex;
            }

            allocation_t a;
            a.offset   = node.dataOffset;
            a.metadata = nodeIndex;
            return a;
        }

        void free(allocator_t* allocator, allocation_t allocation)
        {
            ASSERT(allocation.metadata != allocation_t::NO_SPACE);
            if (!allocator->m_nodes)
                return;

            const u32   nodeIndex = allocation.metadata;
            node_t&     node      = allocator->m_nodes[nodeIndex];
            neighbor_t& neighbor  = allocator->m_neighbors[nodeIndex];

            // Double delete check
            ASSERT(isNodeUsed(allocator, nodeIndex));

            // Merge with neighbors...
            u32 offset = node.dataOffset;
            u32 size   = node.dataSize;

            if ((neighbor.prev != node_t::NIL) && (isNodeUsed(allocator, neighbor.prev) == false))
            {
                // Previous (contiguous) free node: Change offset to previous node offset. Sum sizes
                node_t&     prevNode     = allocator->m_nodes[neighbor.prev];
                neighbor_t& prevNeighbor = allocator->m_neighbors[neighbor.prev];
                offset                   = prevNode.dataOffset;
                size += prevNode.dataSize;

                // Remove node from the bin linked list and put it in the freelist
                removeNodeFromBin(allocator, neighbor.prev);

                ASSERT(prevNeighbor.next == nodeIndex);
                neighbor.prev = prevNeighbor.prev;
            }

            if ((neighbor.next != node_t::NIL) && (isNodeUsed(allocator, neighbor.next) == false))
            {
                // Next (contiguous) free node: Offset remains the same. Sum sizes.
                neighbor_t& nextNeighbor = allocator->m_neighbors[neighbor.next];
                node_t&     nextNode     = allocator->m_nodes[neighbor.next];
                size += nextNode.dataSize;

                // Remove node from the bin linked list and put it in the freelist
                removeNodeFromBin(allocator, neighbor.next);

                ASSERT(nextNeighbor.prev == nodeIndex);
                neighbor.next = (nextNeighbor.next);
            }

            const u32 nodeNext = neighbor.next;
            const u32 nodePrev = neighbor.prev;

            // Insert the removed node to freelist
#ifdef DEBUG_VERBOSE
            printf("Putting node %u into freelist[%u] (free)\n", nodeIndex, allocator->m_freeOffset + 1);
#endif
            // m_freeListHead is the head of the freelist. node.binListNext is the next node in the bin.
            if (allocator->m_freeListHead == node_t::NIL)
            {
                node.binListPrev          = node_t::NIL;
                node.binListNext          = node_t::NIL;
                allocator->m_freeListHead = nodeIndex;
            }
            else
            {
                node.binListPrev                                          = node_t::NIL;
                node.binListNext                                          = allocator->m_freeListHead;
                allocator->m_nodes[allocator->m_freeListHead].binListPrev = nodeIndex;
                allocator->m_freeListHead                                 = nodeIndex;
            }

            // Insert the (combined) free node to bin
            const u32 combinedNodeIndex = insertNodeIntoBin(allocator, size, offset);

            // Connect neighbors with the new combined node
            if (nodeNext != node_t::NIL)
            {
                allocator->m_neighbors[combinedNodeIndex].next = (nodeNext);
                allocator->m_neighbors[nodeNext].prev          = combinedNodeIndex;
            }
            if (nodePrev != node_t::NIL)
            {
                allocator->m_neighbors[combinedNodeIndex].prev = nodePrev;
                allocator->m_neighbors[nodePrev].next          = (combinedNodeIndex);
            }
        }

        u32 insertNodeIntoBin(allocator_t* allocator, u32 size, u32 dataOffset)
        {
            // Round down to bin index to ensure that bin >= alloc
            u32 binIndex = nfloat::U32ToF32RoundDown(size);

            u32 topBinIndex  = binIndex >> TOP_BINS_INDEX_SHIFT;
            u32 leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;

            // Bin was empty before?
            if (allocator->m_binIndices[binIndex] == node_t::NIL)
            {
                // Set bin mask bits
                allocator->m_usedBins[topBinIndex] |= 1 << leafBinIndex;
                allocator->m_usedBinsTop |= 1 << topBinIndex;
            }

            // Take a freelist node and insert on top of the bin linked list (next = old top)
            const u32 topNodeIndex = allocator->m_binIndices[binIndex];
            u32       nodeIndex    = node_t::NIL;
            if (allocator->m_freeListHead != node_t::NIL)
            {
                nodeIndex                 = allocator->m_freeListHead;
                allocator->m_freeListHead = allocator->m_nodes[nodeIndex].binListNext;
                if (allocator->m_freeListHead != node_t::NIL)
                    allocator->m_nodes[allocator->m_freeListHead].binListPrev = node_t::NIL;
            }
            else if (allocator->m_freeIndex < allocator->m_maxAllocs)
            {
                nodeIndex = allocator->m_freeIndex++;
            }
            else
            {
                // Out of allocations
                return node_t::NIL;
            }

#ifdef DEBUG_VERBOSE
            printf("Getting node %u from freelist[%u]\n", nodeIndex, allocator->m_freeOffset + 1);
#endif
            allocator->m_nodes[nodeIndex].dataOffset  = dataOffset;
            allocator->m_nodes[nodeIndex].dataSize    = size;
            allocator->m_nodes[nodeIndex].binListNext = topNodeIndex;
            allocator->m_nodes[nodeIndex].binListPrev = node_t::NIL;

            allocator->m_neighbors[nodeIndex].prev = node_t::NIL;
            allocator->m_neighbors[nodeIndex].next = node_t::NIL;
            setNodeUnused(allocator, nodeIndex);

            if (topNodeIndex != node_t::NIL)
                allocator->m_nodes[topNodeIndex].binListPrev = nodeIndex;
            allocator->m_binIndices[binIndex] = nodeIndex;

            allocator->m_freeStorage += size;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (+%u) (insertNodeIntoBin)\n", allocator->m_freeStorage, size);
#endif

            return nodeIndex;
        }

        void removeNodeFromBin(allocator_t* allocator, u32 nodeIndex)
        {
            node_t& node = allocator->m_nodes[nodeIndex];

            if (node.binListPrev != node_t::NIL)
            {
                // Easy case: We have previous node. Just remove this node from the middle of the list.
                allocator->m_nodes[node.binListPrev].binListNext = node.binListNext;
                if (node.binListNext != node_t::NIL)
                    allocator->m_nodes[node.binListNext].binListPrev = node.binListPrev;
            }
            else
            {
                // We are the first node in a bin. Find the bin.
                // Round down to bin index to ensure that bin >= alloc
                const u32 binIndex = nfloat::U32ToF32RoundDown(node.dataSize);

                const u32 topBinIndex  = binIndex >> TOP_BINS_INDEX_SHIFT;
                const u32 leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;

                allocator->m_binIndices[binIndex] = node.binListNext;
                if (node.binListNext != node_t::NIL)
                    allocator->m_nodes[node.binListNext].binListPrev = node_t::NIL;

                // Bin empty?
                if (allocator->m_binIndices[binIndex] == node_t::NIL)
                {
                    // Remove a leaf bin mask bit
                    allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);

                    // All leaf bins empty?
                    if (allocator->m_usedBins[topBinIndex] == 0)
                    {
                        // Remove a top bin mask bit
                        allocator->m_usedBinsTop &= ~(1 << topBinIndex);
                    }
                }
            }

            // Insert the node to freelist
#ifdef DEBUG_VERBOSE
            printf("Putting node %u into freelist[%u] (removeNodeFromBin)\n", nodeIndex, allocator->m_freeOffset + 1);
#endif
            if (allocator->m_freeListHead == node_t::NIL)
            {
                node.binListPrev          = node_t::NIL;
                node.binListNext          = node_t::NIL;
                allocator->m_freeListHead = nodeIndex;
            }
            else
            {
                node.binListPrev                                          = node_t::NIL;
                node.binListNext                                          = allocator->m_freeListHead;
                allocator->m_nodes[allocator->m_freeListHead].binListPrev = nodeIndex;
                allocator->m_freeListHead                                 = nodeIndex;
            }

            allocator->m_freeStorage -= node.dataSize;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (-%u) (removeNodeFromBin)\n", allocator->m_freeStorage, node.getDataSize());
#endif
        }

        u32 allocationSize(allocator_t* allocator, allocation_t allocation)
        {
            if (allocation.metadata == allocation_t::NO_SPACE || !allocator->m_nodes)
                return 0;

            return allocator->m_nodes[allocation.metadata].dataSize;
        }

        storage_report_t storageReport(allocator_t* allocator)
        {
            u32 largestFreeRegion = 0;
            u32 freeStorage       = 0;

            // Out of allocations? -> Zero free space
            if (allocator->m_freeOffset > 0)
            {
                freeStorage = allocator->m_freeStorage;
                if (allocator->m_usedBinsTop)
                {
                    u32 topBinIndex   = 31 - lzcnt_nonzero(allocator->m_usedBinsTop);
                    u32 leafBinIndex  = 31 - lzcnt_nonzero(allocator->m_usedBins[topBinIndex]);
                    largestFreeRegion = nfloat::F32ToU32((topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex);
                    ASSERT(freeStorage >= largestFreeRegion);
                }
            }

            storage_report_t report;
            report.totalFreeSpace    = freeStorage;
            report.largestFreeRegion = largestFreeRegion;
            return report;
        }

        full_storage_report_t storageReportFull(allocator_t* allocator)
        {
            full_storage_report_t report;
            for (u32 i = 0; i < NUM_LEAF_BINS; i++)
            {
                u32 count     = 0;
                u32 nodeIndex = allocator->m_binIndices[i];
                while (nodeIndex != node_t::NIL)
                {
                    nodeIndex = allocator->m_nodes[nodeIndex].binListNext;
                    count++;
                }
                report.freeRegions[i].size  = nfloat::F32ToU32(i);
                report.freeRegions[i].count = count;
            }
            return report;
        }
    } // namespace noffset

} // namespace ncore
