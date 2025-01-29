#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_offset.h"

namespace ncore
{
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

            // Bin sizes follow floating point (exponent + mantissa) distribution (piecewise linear log approx)
            // This ensures that for each size class, the average overhead percentage stays the same
            u32 uintToFloatRoundUp(u32 size)
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

                return (exp << MANTISSA_BITS) + mantissa;  // + allows mantissa->exp overflow for round up
            }

            u32 uintToFloatRoundDown(u32 size)
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

            u32 floatToUint(u32 floatValue)
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
        }  // namespace nfloat

        // Utility functions
        u32 findLowestSetBitAfter(u32 bitMask, u32 startBitIndex)
        {
            u32 maskBeforeStartIndex = (1 << startBitIndex) - 1;
            u32 maskAfterStartIndex  = ~maskBeforeStartIndex;
            u32 bitsAfter            = bitMask & maskAfterStartIndex;
            if (bitsAfter == 0)
                return allocation_t::NO_SPACE;
            return tzcnt_nonzero(bitsAfter);
        }

        allocator_t::allocator_t(alloc_t* allocator, u32 size, u32 maxAllocs)
            : m_allocator(allocator)
            , m_size(size)
            , m_maxAllocs(maxAllocs)
            , m_freeStorage(0)
            , m_usedBinsTop(0)
            , m_nodes(nullptr)
            , m_neighbors(nullptr)
            , m_used(nullptr)
            , m_freeIndex(0)
            , m_freeListHead(node_t::NIL)
            , m_freeOffset(maxAllocs - 1)
        {
            ASSERT(m_size < 0x80000000);  // Size must be less than 2^31
        }

        allocator_t::allocator_t(allocator_t&& other)
            : m_allocator(other.m_allocator)
            , m_size(other.m_size)
            , m_maxAllocs(other.m_maxAllocs)
            , m_freeStorage(other.m_freeStorage)
            , m_usedBinsTop(other.m_usedBinsTop)
            , m_nodes(other.m_nodes)
            , m_neighbors(other.m_neighbors)
            , m_used(other.m_used)
            , m_freeIndex(other.m_freeIndex)
            , m_freeListHead(other.m_freeListHead)
            , m_freeOffset(other.m_freeOffset)
        {
            nmem::memcpy(m_usedBins, other.m_usedBins, sizeof(u8) * NUM_TOP_BINS);
            nmem::memcpy(m_binIndices, other.m_binIndices, sizeof(u32) * NUM_LEAF_BINS);

            other.m_allocator    = nullptr;
            other.m_nodes        = nullptr;
            other.m_neighbors    = nullptr;
            other.m_used         = nullptr;
            other.m_freeIndex    = 0;
            other.m_freeListHead = node_t::NIL;
            other.m_freeOffset   = 0;
            other.m_maxAllocs    = 0;
            other.m_usedBinsTop  = 0;
        }

        void allocator_t::setup()
        {
            m_nodes     = (node_t*)m_allocator->allocate(sizeof(node_t) * m_maxAllocs);
            m_neighbors = (neighbor_t*)m_allocator->allocate(sizeof(neighbor_t) * m_maxAllocs);
            m_used      = (u32*)m_allocator->allocate((m_maxAllocs >> 5) * sizeof(u32));

            reset();
        }

        void allocator_t::teardown()
        {
            if (m_nodes)
                m_allocator->deallocate(m_nodes);
            if (m_neighbors)
                m_allocator->deallocate(m_neighbors);
            if (m_used)
                m_allocator->deallocate(m_used);

            m_freeStorage  = 0;
            m_usedBinsTop  = 0;
            m_freeOffset   = m_maxAllocs - 1;
            m_nodes        = nullptr;
            m_neighbors    = nullptr;
            m_used         = nullptr;
            m_freeIndex    = 0;
            m_freeListHead = node_t::NIL;
        }

        void allocator_t::reset()
        {
            m_freeStorage = 0;
            m_usedBinsTop = 0;
            m_freeOffset  = m_maxAllocs - 1;

            for (u32 i = 0; i < NUM_TOP_BINS; i++)
                m_usedBins[i] = 0;

            for (u32 i = 0; i < NUM_LEAF_BINS; i++)
                m_binIndices[i] = node_t::NIL;

            m_freeIndex    = 0;
            m_freeListHead = node_t::NIL;

            // Start state: Whole storage as one big node
            // Algorithm will split remainders and push them back as smaller nodes
            insertNodeIntoBin(m_size, 0);
        }

        allocator_t::~allocator_t()
        {
            if (m_nodes)
                m_allocator->deallocate(m_nodes);
            if (m_neighbors)
                m_allocator->deallocate(m_neighbors);
            if (m_used)
                m_allocator->deallocate(m_used);
        }

        allocation_t allocator_t::allocate(u32 size)
        {
            // Out of allocations?
            if (m_freeOffset == 0)
            {
                //return {.offset = allocation_t::NO_SPACE, .metadata = allocation_t::NO_SPACE};
                allocation_t a;
                a.offset = allocation_t::NO_SPACE;
                a.metadata = allocation_t::NO_SPACE;
                return a;
            }

            // Round up to bin index to ensure that alloc >= bin
            // Gives us min bin index that fits the size
            const u32 minBinIndex = nfloat::uintToFloatRoundUp(size);

            const u32 minTopBinIndex  = minBinIndex >> TOP_BINS_INDEX_SHIFT;
            const u32 minLeafBinIndex = minBinIndex & LEAF_BINS_INDEX_MASK;

            u32 topBinIndex  = minTopBinIndex;
            u32 leafBinIndex = allocation_t::NO_SPACE;

            // If top bin exists, scan its leaf bin. This can fail (NO_SPACE).
            if (m_usedBinsTop & (1 << topBinIndex))
            {
                leafBinIndex = findLowestSetBitAfter(m_usedBins[topBinIndex], minLeafBinIndex);
            }

            // If we didn't find space in top bin, we search top bin from +1
            if (leafBinIndex == allocation_t::NO_SPACE)
            {
                topBinIndex = findLowestSetBitAfter(m_usedBinsTop, minTopBinIndex + 1);

                // Out of space?
                if (topBinIndex == allocation_t::NO_SPACE)
                {
                    //return {.offset = allocation_t::NO_SPACE, .metadata = allocation_t::NO_SPACE};
                    allocation_t a;
                    a.offset = allocation_t::NO_SPACE;
                    a.metadata = allocation_t::NO_SPACE;
                    return a;
                }

                // All leaf bins here fit the alloc, since the top bin was rounded up. Start leaf search from bit 0.
                // NOTE: This search can't fail since at least one leaf bit was set because the top bit was set.
                leafBinIndex = tzcnt_nonzero(m_usedBins[topBinIndex]);
            }

            const u32 binIndex = (topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex;

            // Pop the top node of the bin. Bin top = node.next.
            const u32   nodeIndex     = m_binIndices[binIndex];
            node_t&     node          = m_nodes[nodeIndex];
            neighbor_t& neighbor      = m_neighbors[nodeIndex];
            const u32   nodeTotalSize = node.dataSize;
            node.dataSize             = size;
            setUsed(nodeIndex);
            m_binIndices[binIndex] = node.binListNext;
            if (node.binListNext != node_t::NIL)
                m_nodes[node.binListNext].binListPrev = node_t::NIL;

            m_freeStorage -= nodeTotalSize;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (-%u) (allocate)\n", m_freeStorage, nodeTotalSize);
#endif

            // Bin empty?
            if (m_binIndices[binIndex] == node_t::NIL)
            {
                m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);  // Remove a leaf bin mask bit

                // All leaf bins empty?
                if (m_usedBins[topBinIndex] == 0)
                {
                    m_usedBinsTop &= ~(1 << topBinIndex);  // Remove a top bin mask bit
                }
            }

            // Push back remaining N elements to a lower bin
            const u32 reminderSize = nodeTotalSize - size;
            if (reminderSize > 0)
            {
                const u32 newNodeIndex = insertNodeIntoBin(reminderSize, node.dataOffset + size);

                // Link new node after the current node so that we can merge them later if both are free
                // And update the old next neighbor to point to the new node (in middle)
                if (neighbor.next != node_t::NIL)
                    m_neighbors[neighbor.next].prev = newNodeIndex;
                m_neighbors[newNodeIndex].prev = nodeIndex;
                m_neighbors[newNodeIndex].next = neighbor.next;
                neighbor.next                  = newNodeIndex;
            }

            allocation_t a;
            a.offset = node.dataOffset;
            a.metadata = nodeIndex;
            return a;
        }

        void allocator_t::free(allocation_t allocation)
        {
            ASSERT(allocation.metadata != allocation_t::NO_SPACE);
            if (!m_nodes)
                return;

            const u32   nodeIndex = allocation.metadata;
            node_t&     node      = m_nodes[nodeIndex];
            neighbor_t& neighbor  = m_neighbors[nodeIndex];

            // Double delete check
            ASSERT(isUsed(nodeIndex));

            // Merge with neighbors...
            u32 offset = node.dataOffset;
            u32 size   = node.dataSize;

            if ((neighbor.prev != node_t::NIL) && (isUsed(neighbor.prev) == false))
            {
                // Previous (contiguous) free node: Change offset to previous node offset. Sum sizes
                node_t&     prevNode     = m_nodes[neighbor.prev];
                neighbor_t& prevNeighbor = m_neighbors[neighbor.prev];
                offset                   = prevNode.dataOffset;
                size += prevNode.dataSize;

                // Remove node from the bin linked list and put it in the freelist
                removeNodeFromBin(neighbor.prev);

                ASSERT(prevNeighbor.next == nodeIndex);
                neighbor.prev = prevNeighbor.prev;
            }

            if ((neighbor.next != node_t::NIL) && (isUsed(neighbor.next) == false))
            {
                // Next (contiguous) free node: Offset remains the same. Sum sizes.
                neighbor_t& nextNeighbor = m_neighbors[neighbor.next];
                node_t&     nextNode     = m_nodes[neighbor.next];
                size += nextNode.dataSize;

                // Remove node from the bin linked list and put it in the freelist
                removeNodeFromBin(neighbor.next);

                ASSERT(nextNeighbor.prev == nodeIndex);
                neighbor.next = (nextNeighbor.next);
            }

            const u32 nodeNext = neighbor.next;
            const u32 nodePrev = neighbor.prev;

            // Insert the removed node to freelist
#ifdef DEBUG_VERBOSE
            printf("Putting node %u into freelist[%u] (free)\n", nodeIndex, m_freeOffset + 1);
#endif
            // m_freeListHead is the head of the freelist. node.binListNext is the next node in the bin.
            if (m_freeListHead == node_t::NIL)
            {
                node.binListPrev = node_t::NIL;
                node.binListNext = node_t::NIL;
                m_freeListHead   = nodeIndex;
            }
            else
            {
                node.binListPrev                    = node_t::NIL;
                node.binListNext                    = m_freeListHead;
                m_nodes[m_freeListHead].binListPrev = nodeIndex;
                m_freeListHead                      = nodeIndex;
            }

            // Insert the (combined) free node to bin
            const u32 combinedNodeIndex = insertNodeIntoBin(size, offset);

            // Connect neighbors with the new combined node
            if (nodeNext != node_t::NIL)
            {
                m_neighbors[combinedNodeIndex].next = (nodeNext);
                m_neighbors[nodeNext].prev          = combinedNodeIndex;
            }
            if (nodePrev != node_t::NIL)
            {
                m_neighbors[combinedNodeIndex].prev = nodePrev;
                m_neighbors[nodePrev].next          = (combinedNodeIndex);
            }
        }

        u32 allocator_t::insertNodeIntoBin(u32 size, u32 dataOffset)
        {
            // Round down to bin index to ensure that bin >= alloc
            u32 binIndex = nfloat::uintToFloatRoundDown(size);

            u32 topBinIndex  = binIndex >> TOP_BINS_INDEX_SHIFT;
            u32 leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;

            // Bin was empty before?
            if (m_binIndices[binIndex] == node_t::NIL)
            {
                // Set bin mask bits
                m_usedBins[topBinIndex] |= 1 << leafBinIndex;
                m_usedBinsTop |= 1 << topBinIndex;
            }

            // Take a freelist node and insert on top of the bin linked list (next = old top)
            const u32 topNodeIndex = m_binIndices[binIndex];
            u32       nodeIndex    = node_t::NIL;
            if (m_freeListHead != node_t::NIL)
            {
                nodeIndex      = m_freeListHead;
                m_freeListHead = m_nodes[nodeIndex].binListNext;
                if (m_freeListHead != node_t::NIL)
                    m_nodes[m_freeListHead].binListPrev = node_t::NIL;
            }
            else if (m_freeIndex < m_maxAllocs)
            {
                nodeIndex = m_freeIndex++;
            }
            else
            {
                // Out of allocations
                return node_t::NIL;
            }

#ifdef DEBUG_VERBOSE
            printf("Getting node %u from freelist[%u]\n", nodeIndex, m_freeOffset + 1);
#endif
            m_nodes[nodeIndex].dataOffset  = dataOffset;
            m_nodes[nodeIndex].dataSize    = size;
            m_nodes[nodeIndex].binListNext = topNodeIndex;
            m_nodes[nodeIndex].binListPrev = node_t::NIL;

            m_neighbors[nodeIndex].prev = node_t::NIL;
            m_neighbors[nodeIndex].next = node_t::NIL;
            setUnused(nodeIndex);

            if (topNodeIndex != node_t::NIL)
                m_nodes[topNodeIndex].binListPrev = nodeIndex;
            m_binIndices[binIndex] = nodeIndex;

            m_freeStorage += size;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (+%u) (insertNodeIntoBin)\n", m_freeStorage, size);
#endif

            return nodeIndex;
        }

        void allocator_t::removeNodeFromBin(u32 nodeIndex)
        {
            node_t& node = m_nodes[nodeIndex];

            if (node.binListPrev != node_t::NIL)
            {
                // Easy case: We have previous node. Just remove this node from the middle of the list.
                m_nodes[node.binListPrev].binListNext = node.binListNext;
                if (node.binListNext != node_t::NIL)
                    m_nodes[node.binListNext].binListPrev = node.binListPrev;
            }
            else
            {
                // Hard case: We are the first node in a bin. Find the bin.

                // Round down to bin index to ensure that bin >= alloc
                u32 binIndex = nfloat::uintToFloatRoundDown(node.dataSize);

                u32 topBinIndex  = binIndex >> TOP_BINS_INDEX_SHIFT;
                u32 leafBinIndex = binIndex & LEAF_BINS_INDEX_MASK;

                m_binIndices[binIndex] = node.binListNext;
                if (node.binListNext != node_t::NIL)
                    m_nodes[node.binListNext].binListPrev = node_t::NIL;

                // Bin empty?
                if (m_binIndices[binIndex] == node_t::NIL)
                {
                    // Remove a leaf bin mask bit
                    m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);

                    // All leaf bins empty?
                    if (m_usedBins[topBinIndex] == 0)
                    {
                        // Remove a top bin mask bit
                        m_usedBinsTop &= ~(1 << topBinIndex);
                    }
                }
            }

            // Insert the node to freelist
#ifdef DEBUG_VERBOSE
            printf("Putting node %u into freelist[%u] (removeNodeFromBin)\n", nodeIndex, m_freeOffset + 1);
#endif
            if (m_freeListHead == node_t::NIL)
            {
                node.binListPrev = node_t::NIL;
                node.binListNext = node_t::NIL;
                m_freeListHead   = nodeIndex;
            }
            else
            {
                node.binListPrev                    = node_t::NIL;
                node.binListNext                    = m_freeListHead;
                m_nodes[m_freeListHead].binListPrev = nodeIndex;
                m_freeListHead                      = nodeIndex;
            }

            m_freeStorage -= node.dataSize;
#ifdef DEBUG_VERBOSE
            printf("Free storage: %u (-%u) (removeNodeFromBin)\n", m_freeStorage, node.getDataSize());
#endif
        }

        u32 allocator_t::allocationSize(allocation_t allocation) const
        {
            if (allocation.metadata == allocation_t::NO_SPACE || !m_nodes)
                return 0;

            return m_nodes[allocation.metadata].dataSize;
        }

        storage_report_t allocator_t::storageReport() const
        {
            u32 largestFreeRegion = 0;
            u32 freeStorage       = 0;

            // Out of allocations? -> Zero free space
            if (m_freeOffset > 0)
            {
                freeStorage = m_freeStorage;
                if (m_usedBinsTop)
                {
                    u32 topBinIndex   = 31 - lzcnt_nonzero(m_usedBinsTop);
                    u32 leafBinIndex  = 31 - lzcnt_nonzero(m_usedBins[topBinIndex]);
                    largestFreeRegion = nfloat::floatToUint((topBinIndex << TOP_BINS_INDEX_SHIFT) | leafBinIndex);
                    ASSERT(freeStorage >= largestFreeRegion);
                }
            }

            storage_report_t report;
            report.totalFreeSpace = freeStorage;
            report.largestFreeRegion = largestFreeRegion;
            return report;
        }

        full_storage_report_t allocator_t::storageReportFull() const
        {
            full_storage_report_t report;
            for (u32 i = 0; i < NUM_LEAF_BINS; i++)
            {
                u32 count     = 0;
                u32 nodeIndex = m_binIndices[i];
                while (nodeIndex != node_t::NIL)
                {
                    nodeIndex = m_nodes[nodeIndex].binListNext;
                    count++;
                }
                report.freeRegions[i].size = nfloat::floatToUint(i);
                report.freeRegions[i].count = count;
            }
            return report;
        }
    }  // namespace ngfx

}  // namespace ncore
