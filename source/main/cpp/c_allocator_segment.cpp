#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_limits.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_segment.h"

namespace ncore
{
    namespace nsegmented
    {
        struct segment_bitmap_alloc_t
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            s8   m_min_size_shift;   // The minimum size of a segment in log2
            s8   m_max_size_shift;   // The maximum size of a segment in log2
            s8   m_total_size_shift; // The total size of the segment in log2
            s8   m_num_sizes;        // The number of sizes available
            u32  m_size_free;        // One bit per size, indicating if there is one or more free segments at that size.
            u64* m_level0;           // Level 0 for each size
            s32* m_offsets_0;        // Offsets for level 0, one per size
            u64* m_level1;           // Level 1
            s32* m_offsets_1;        // Offsets for level 0, one per size (-1 means no level 1 bitmap)
            u64* m_level2;           // Level 2
            s32* m_offsets_2;        // Offsets for level 0, one per size (-1 means no level 2 bitmap)

            // note: remove 'count' if we don't need it
            void initialize(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size)
            {
                ASSERT(min_size < max_size && math::g_ispo2(min_size) && math::g_ispo2(max_size));

                const s8 min_size_shift = math::g_ilog2(min_size);
                const s8 max_size_shift = math::g_ilog2(max_size);
                const s8 tot_size_shift = math::g_ilog2(total_size);
                const s8 num_sizes      = 1 + max_size_shift - min_size_shift;

                // Allocate the count and offsets array's
                m_offsets_0 = g_allocate_array_and_clear<s32>(allocator, num_sizes);
                m_offsets_1 = g_allocate_array_and_clear<s32>(allocator, num_sizes);
                m_offsets_2 = g_allocate_array_and_clear<s32>(allocator, num_sizes);

                // Determine the size and offsets for level 0
                s32 level0_size_in_bits = 0;
                s32 level1_size_in_bits = 0;
                s32 level2_size_in_bits = 0;

                // Note: Offset is the number of u64
                s32 offset0 = 0;
                s32 offset1 = 0;
                s32 offset2 = 0;

                s32 size_in_bits = 1 << (tot_size_shift - min_size_shift);
                ASSERT(size_in_bits <= (1 << 3*6));

                s32 count0 = size_in_bits;
                s32 count1 = size_in_bits >> 6;
                s32 count2 = size_in_bits >> 12;

                s32 i = 0;
                while (true)
                {
                    m_offsets_0[i] = offset0;
                    m_offsets_1[i] = count1 > 0 ? offset1 : -1;
                    m_offsets_2[i] = count2 > 0 ? offset2 : -1;

                    level0_size_in_bits += count0;
                    level0_size_in_bits = math::g_alignUp(level0_size_in_bits, 64);

                    level1_size_in_bits += count1;
                    level1_size_in_bits = math::g_alignUp(level1_size_in_bits, 64);

                    level2_size_in_bits += count2;
                    level2_size_in_bits = math::g_alignUp(level2_size_in_bits, 64);

                    i += 1;
                    if (i == num_sizes)
                        break;

                    size_in_bits = math::g_max(size_in_bits >> 1, (s32)1);

                    offset0 = level0_size_in_bits >> 6;
                    offset1 = level1_size_in_bits >> 6;
                    offset2 = level2_size_in_bits >> 6;
                    count0 = size_in_bits;
                    count1 = size_in_bits >> 6;
                    count2 = size_in_bits >> 12;
                }

                m_level0 = g_allocate_array_and_clear<u64>(allocator, level0_size_in_bits >> 6);
                m_level1 = g_allocate_array_and_clear<u64>(allocator, level1_size_in_bits >> 6);
                m_level2 = g_allocate_array_and_clear<u64>(allocator, level2_size_in_bits >> 6);

                if (count2 > 0)
                {
                    m_level2[offset2] = (count2 == 64) ? u64_max : ~(u64_max << count2);
                }
                if (count1 > 0)
                {
                    s32 offset = offset1;
                    while (count1 > 64)
                    {
                        m_level1[offset++] = u64_max;
                        count1 -= 64;
                    }
                    m_level1[offset] = (count1 == 64) ? u64_max : ~(u64_max << count1);
                }
                if (count0 > 0)
                {
                    s32 offset = offset0;
                    while (count0 > 64)
                    {
                        m_level0[offset++] = u64_max;
                        count0 -= 64;
                    }
                    m_level0[offset] = (count0 == 64) ? u64_max : ~(u64_max << count0);
                }

                m_min_size_shift   = min_size_shift;
                m_max_size_shift   = max_size_shift;
                m_total_size_shift = tot_size_shift;
                m_num_sizes        = num_sizes;

                // Initialize the size_free bitmap and the highest size bitmap
                m_size_free = (1 << (m_num_sizes - 1));
            }

            void teardown(alloc_t* allocator)
            {
                g_deallocate_array(allocator, m_level0);
                g_deallocate_array(allocator, m_level1);
                g_deallocate_array(allocator, m_level2);
                g_deallocate_array(allocator, m_offsets_0);
                g_deallocate_array(allocator, m_offsets_1);
                g_deallocate_array(allocator, m_offsets_2);
            }

            static inline u64 s_set_level_bit(s32 offset, s32 bit, u64* level)
            {
                const s32 bo = (bit >> 6);
                const s32 bi = (bit & (64 - 1));
                const u64 lc = level[offset + bo];
                level[offset + bo] |= ((u64)1 << bi);
                return lc;
            }

            static inline u64 s_clr_level_bit(s32 offset, s32 bit, u64* level)
            {
                const s32 bo = (bit >> 6);
                const s32 bi = (bit & (64 - 1));
                u64&      lv = level[offset + bo];
                lv           = lv & ~((u64)1 << bi);
                return lv;
            }

            inline u64 clr_bit(s8 size_index, s32 bit)
            {
                ASSERT(size_index >= 0 && size_index < m_num_sizes);

                const s32 offset0 = m_offsets_0[size_index];
                u64 state = s_clr_level_bit(offset0, bit, m_level0);

                const s32 offset1 = m_offsets_1[size_index];
                if (offset1 >= 0 && state == 0)
                {
                    bit   = bit >> 6; // To level 1
                    state = s_clr_level_bit(offset1, bit, m_level1);

                    const s32 offset2 = m_offsets_2[size_index];
                    if (offset2 >= 0 && state == 0)
                    {
                        bit   = bit >> 6; // To level 2
                        state = s_clr_level_bit(offset2, bit, m_level2);
                    }
                }

                return state;
            }

            s32 find_bit(s8 size_index) const
            {
                ASSERT(size_index >= 0 && size_index < m_num_sizes);
                s32 bit = 0;

                s32 offset = m_offsets_2[size_index];
                if (offset >= 0)
                {
                    bit = math::g_findFirstBit(m_level2[offset]);
                }
                offset = m_offsets_1[size_index];
                if (offset >= 0)
                {
                    bit = math::g_findFirstBit(m_level1[offset + bit]);
                }

                offset = m_offsets_0[size_index];
                return (bit << 6) + math::g_findFirstBit(m_level0[offset + bit]);
            }

            s32 find_and_clr_bit(s8 size_index)
            {
                const s32 bit = find_bit(size_index);
                if (clr_bit(size_index, bit) == 0)
                {
                    m_size_free &= ~(1 << size_index); // No more free elements at this size
                }
                return bit;
            }

            u64 set_level0_bit(s32 offset, s32 bit)
            {
                u64&      l0 = m_level0[offset + (bit >> 6)];
                const u32 bi = (bit & (64 - 1));
                u64       lc = l0;
                l0           = l0 | ((u64)1 << bi);
                return lc;
            }

            inline bool get_level0_bit(s32 offset, s32 bit) const
            {
                u64&      l0 = m_level0[offset + (bit >> 6)];
                const u32 bi = (bit & (64 - 1));
                return (l0 & ((u64)1 << bi)) != 0;
            }

            inline u64 set_level0_pair(s32 offset, s32 bit)
            {
                const u32 bo = (bit >> 6);
                const u32 bi = (bit & (64 - 1));
                u64&      l0 = m_level0[offset + bo];
                const u64 lc = l0;
                l0           = l0 | ((u64)3 << bi);
                return lc;
            }

            // returns 0 when setting the bit resulted in an odd+even pair of bits and clearing the other bit
            //           resulted in a bitmap that has no more bits set.
            // returns 2 when setting the bit resulted in an odd+even pair of bits, but clearing the other bit
            //           resulted in a bitmap that still has bits set.
            // returns 1 when setting the bit resulted in a single bit set.
            s8 set_bit(s8 size_index, s32 bit)
            {
                ASSERT(size_index >= 0 && size_index < m_num_sizes);
                ASSERT(bit >= 0 && ((bit & 3) == 0 || (bit & 3) == 2 || (bit & 3) == 4 || (bit & 3) == 6));
                s32 offset = m_offsets_0[size_index];
                if (get_level0_bit(offset, bit ^ 1))
                {
                    if (clr_bit(size_index, bit ^ 1) == 0)
                        return 0;
                    return 2;
                }

                const u64 bits0 = set_level0_bit(offset, bit);
                offset = m_offsets_1[size_index];
                if (offset >= 0 && bits0 == 0)
                {
                    bit = bit >> 6; // To level 1
                    const u64 bits1 = s_set_level_bit(offset, bit, m_level1);

                    offset = m_offsets_2[size_index];
                    if (offset >= 0 && bits1 == 0)
                    {
                        bit = bit >> 6; // To level 2
                        s_set_level_bit(offset, bit, m_level2);
                    }
                }

                return 1;
            }

            void set_2bits(s8 size_index, s32 bit)
            {
                ASSERT(size_index >= 0 && size_index < m_num_sizes);
                ASSERT(bit >= 0 && ((bit & 3) == 0 || (bit & 3) == 2 || (bit & 3) == 4 || (bit & 3) == 6));

                s32 offset = m_offsets_0[size_index];
                const u64 bits0 = set_level0_pair(offset, bit);

                offset = m_offsets_1[size_index];
                if (offset >= 0 && bits0 == 0)
                {
                    bit = bit >> 6;
                    const u64 bits1 = s_set_level_bit(offset, bit, m_level1);

                    offset = m_offsets_2[size_index];
                    if (offset >= 0 && bits1 == 0)
                    {
                        bit = bit >> 6;
                        s_set_level_bit(offset, bit, m_level2);
                    }
                }
            }

            inline s8 size_to_index(s64 size) const
            {
                ASSERT(math::g_ispo2(size));
                ASSERT(size >= ((s64)1 << m_min_size_shift) && size <= ((s64)1 << m_max_size_shift));
                return math::g_ilog2(size) - m_min_size_shift;
            }

            bool allocate(s64 size, s64& offset)
            {
                const s8 size_index = size_to_index(size);

                // We can detect bits set in m_size_free directly
                const u32 size_free = (m_size_free & ~((1 << size_index) - 1));
                if (size_free == 0)
                {
                    // Fragmented or OOM ?
                    offset = -1;
                    return false;
                }

                // Which (highest) bit is set in size_free?
                s8  size_free_index = math::g_findFirstBit(size_free);
                s32 bit             = find_bit(size_free_index);

                // Note:
                // Size bitmap at index 0 has (1 << (m_min_size_shift)) bits
                // Size bitmap at (m_num_levels - 1) effectively only has 1 bit

                while (size_free_index > size_index)
                {
                    if (clr_bit(size_free_index, bit) == 0)
                    {
                        m_size_free &= ~(1 << size_free_index);
                    }

                    size_free_index--;

                    bit = (bit << 1);

                    // Set both bits
                    set_2bits(size_free_index, bit);

                    // Mark the bit in 'size_free bitmap' to indicate that we have a free element at this size
                    m_size_free |= (1 << size_free_index);
                }

                // Clear the bit in the bitmap at size_index
                if (clr_bit(size_index, bit) == 0)
                {
                    m_size_free &= ~(1 << size_index);
                }

                // Return the actual allocated offset
                offset = ((s64)1 << (m_min_size_shift + size_index)) * bit;
                return true;
            }

            bool deallocate(s64 ptr, s64 size)
            {
                if (ptr < 0 || size <= 0 || ptr >= ((s64)1 << m_total_size_shift) || size > ((s64)1 << m_max_size_shift))
                {
                    return false; // Invalid pointer or size
                }

                s8  size_index = size_to_index(size);                           // get the size index from the size
                s32 bit        = (s32)(ptr >> (m_min_size_shift + size_index)); // the bit index of the bitmap at size_index
                while (size_index >= 0)
                {
                    const s8 result = set_bit(size_index, bit);
                    if (result == 1)
                        break;
                    if (result == 0)
                        m_size_free &= ~(1 << size_index);
                    bit = (bit >> 1);
                    size_index--;
                }
                return true;
            }
        };

        // A segmented allocator that allocates memory in segments of 2^N
        template <typename T> struct segment_node_alloc_t
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            static const T c_null = ~0;
            typedef T      node_t;

            u8*     m_node_size;           // Size is '1 << (m_node_size[size_index] & 0x7F)', if the MSB is set the node is allocated
            node_t* m_node_next;           // Next node in the size list
            node_t* m_node_prev;           // Previous node in the size list
            u64     m_size_list_occupancy; // Which size list is non-empty
            node_t* m_size_lists;          // Every size has its own list
            u8      m_min_size_shift;      // The minimum size of a segment in log2
            u8      m_max_size_shift;      // The maximum size of a segment in log2
            u8      m_total_size_shift;    // The total size of the segment in log2
            u8      m_node_count_shift;    // The number of sizes available

            void setup(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size);
            void teardown(alloc_t* allocator);

            // bool allocate(u32 size, node_t& out_node); // Returns false if the size is not available
            // void deallocate(node_t node);              // Deallocates the node
            bool allocate(s64 size, s64& out_ptr); // Returns false if the size is not available
            bool deallocate(s64 ptr, s64 size);    // Deallocates the node

            bool split(node_t node); // Splits the node into two nodes

            void add_size(u8 size_index, node_t node);
            void remove_size(u8 size_index, node_t node);
        };

        // template <typename T> void segment_node_alloc_t<T>::setup(alloc_t* allocator, u8 node_count_2log, u8 max_span_2log)
        template <typename T> void segment_node_alloc_t<T>::setup(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size)
        {
            ASSERT(math::g_ispo2(min_size) && math::g_ispo2(max_size) && math::g_ispo2(total_size));
            ASSERT(min_size < max_size && max_size <= total_size);

            const u8 min_size_shift = math::g_ilog2(min_size);
            const u8 max_size_shift = math::g_ilog2(max_size);
            const u8 tot_size_shift = math::g_ilog2(total_size);

            m_min_size_shift   = min_size_shift;                  // The minimum size of a segment in log2
            m_max_size_shift   = max_size_shift;                  // The maximum size of a segment in log2
            m_total_size_shift = tot_size_shift;                  // The total size of the segment in log2
            m_node_count_shift = tot_size_shift - min_size_shift; // The number of bits needed to represent the largest node span
            ASSERT(m_node_count_shift <= sizeof(node_t) * 8);     // Ensure that the maximum node index fits in a node_t

            u32 const max_node_count = 1 << m_node_count_shift;
            m_node_size              = g_allocate_array_and_clear<u8>(allocator, max_node_count);
            m_node_next              = g_allocate_array_and_clear<node_t>(allocator, max_node_count);
            m_node_prev              = g_allocate_array_and_clear<node_t>(allocator, max_node_count);
            m_size_lists             = g_allocate_array_and_memset<node_t>(allocator, m_node_count_shift + 1, 0xFFFFFFFF);

            // Split the full range into nodes of the maximum size
            u32 const node_span_shift = (max_size_shift - min_size_shift);
            u32 const node_count      = (1 << m_node_count_shift);
            u32 const node_span       = (1 << node_span_shift);
            u32       prev            = node_count - node_span;
            for (u32 n = 0; n < node_count; n += node_span)
            {
                m_node_size[n] = node_span_shift;

                m_node_next[prev] = n;
                m_node_prev[n]    = prev;

                prev = n;
            }

            m_size_list_occupancy |= ((u64)1 << node_span_shift);
            m_size_lists[node_span_shift] = 0;
        }

        template <typename T> void segment_node_alloc_t<T>::teardown(alloc_t* allocator)
        {
            allocator->deallocate(m_node_size);
            allocator->deallocate(m_node_next);
            allocator->deallocate(m_node_prev);
            allocator->deallocate(m_size_lists);
        }

        template <typename T> bool segment_node_alloc_t<T>::split(node_t node)
        {
            const u8 span = m_node_size[node];
            remove_size(span, node);

            // Split the node into two nodes
            node_t const insert = node + (1 << (span - 1));
            m_node_size[node]   = span - 1;
            m_node_size[insert] = span - 1;

            // Invalidate the next and previous pointers of the new node (debugging)
            m_node_next[insert] = (node_t)c_null;
            m_node_prev[insert] = (node_t)c_null;

            // Add node and insert into the size list that is (index - 1)
            add_size(span - 1, node);
            add_size(span - 1, insert);

            return true;
        }

        template <typename T> void segment_node_alloc_t<T>::add_size(u8 span, node_t node)
        {
            node_t& head = m_size_lists[span];
            if (head == (node_t)c_null)
            {
                m_node_next[node] = node;
                m_node_prev[node] = node;
            }
            else
            {
                node_t const prev = m_node_prev[head];
                m_node_next[prev] = node;
                m_node_prev[head] = node;
                m_node_next[node] = head;
                m_node_prev[node] = prev;
            }

            head = node;

            m_size_list_occupancy |= ((u64)1 << span);
        }

        template <typename T> void segment_node_alloc_t<T>::remove_size(u8 span, node_t node)
        {
            ASSERT(span >= 0 && span <= (m_max_size_shift - m_min_size_shift));
            node_t& head                   = m_size_lists[span];
            head                           = (head == node) ? m_node_next[node] : head;
            head                           = (head == node) ? (node_t)c_null : head;
            m_node_next[m_node_prev[node]] = m_node_next[node];
            m_node_prev[m_node_next[node]] = m_node_prev[node];
            m_node_next[node]              = (node_t)c_null;
            m_node_prev[node]              = (node_t)c_null;

            if (head == (node_t)c_null)
                m_size_list_occupancy &= ~((u64)1 << span);
        }

        template <typename T> bool segment_node_alloc_t<T>::allocate(s64 size, s64& out_ptr)
        {
            u8 const span = math::g_ilog2(size) - m_min_size_shift;

            node_t node = m_size_lists[span];
            if (node == (node_t)c_null)
            {
                // In the occupancy find a bit set that is above our size
                // Mask out the bits below size and then do a find first bit
                u64 const occupancy = m_size_list_occupancy & ~(((u64)1 << span) - 1);
                if (occupancy == 0) // There are no free sizes available
                    return false;

                u8 occ = math::g_findFirstBit(occupancy);
                node   = m_size_lists[occ];
                while (occ > span)
                {
                    split(node); // Split the node until we reach the size
                    occ -= 1;
                }
            }

            remove_size(span, node);

            m_node_size[node] |= 0x80; // Mark the node as allocated

            // out_node = node;
            // compute the pointer from 'node'
            out_ptr = ((s64)1 << m_min_size_shift) * node;

            return true;
        }

        template <typename T> bool segment_node_alloc_t<T>::deallocate(s64 ptr, s64 size)
        {
            node_t node = (node_t)(ptr >> m_min_size_shift);

            if (node == c_null || node >= (1 << m_node_count_shift))
                return false;

            m_node_size[node] &= ~0x80; // Mark the node as free

            u8 span = m_node_size[node];

            // Keep span smaller or equal to the largest span allowed
            while (span < (m_max_size_shift - m_min_size_shift))
            {
                const node_t sibling = ((node & ((2 << span) - 1)) == 0) ? node + (1 << span) : node - (1 << span);

                // Check if the sibling is free
                if ((m_node_size[sibling] & 0x80) == 0x80)
                    break;

                remove_size(span, sibling);                            // Sibling is being merged, remove it
                node_t const merged = node < sibling ? node : sibling; // Always take the left node as the merged node
                node_t const other  = node < sibling ? sibling : node; // Always take the right node as the other node
                m_node_size[merged] = span + 1;                        // Double the span (size) of the node
                m_node_size[other]  = 0;                               // Invalidate the node that is merged into 'merged'

                node = merged;
                span = m_node_size[node] & 0x7F;
            }

            add_size(span, node); // Add the merged node to the size list
            return true;
        }

        class segment_alloc_imp_t : public segment_alloc_t
        {
        public:
            void release(alloc_t* allocator) { v_release(allocator); }

        protected:
            virtual void v_release(alloc_t* allocator) = 0;
        };

        class segment_bitmap_allocator_t : public segment_alloc_imp_t
        {
        public:
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            segment_bitmap_alloc_t m_allocator;

            virtual void v_release(alloc_t* allocator) final
            {
                m_allocator.teardown(allocator);
                allocator->deallocate(this);
            }

            virtual bool v_allocate(s64 size, s64& out_ptr) final { return m_allocator.allocate(size, out_ptr); }
            virtual bool v_deallocate(s64 ptr, s64 size) final { return m_allocator.deallocate(ptr, size); }
        };

        segment_alloc_t* g_create_segment_b_allocator(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size)
        {
            segment_bitmap_allocator_t* alloc = g_construct<segment_bitmap_allocator_t>(allocator);
            if (alloc == nullptr)
                return nullptr;
            alloc->m_allocator.initialize(allocator, min_size, max_size, total_size);
            return alloc;
        }

        class segment_node16_allocator_t : public segment_alloc_imp_t
        {
        public:
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            segment_node_alloc_t<u16> m_allocator;

            virtual void v_release(alloc_t* allocator) final
            {
                m_allocator.teardown(allocator);
                allocator->deallocate(this);
            }

            virtual bool v_allocate(s64 size, s64& out_ptr) final { return m_allocator.allocate(size, out_ptr); }
            virtual bool v_deallocate(s64 ptr, s64 size) final { return m_allocator.deallocate(ptr, size); }
        };

        segment_alloc_t* g_create_segment_n_allocator(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size)
        {
            // Figure out from the min_size, max_size and total_size which type we need for the
            // segment node allocator, u8, u16 or u32.
            segment_node16_allocator_t* alloc = g_construct<segment_node16_allocator_t>(allocator);
            if (alloc == nullptr)
                return nullptr;
            alloc->m_allocator.setup(allocator, min_size, max_size, total_size);
            return alloc;
        }

        void g_teardown(alloc_t* alloc, segment_alloc_t* allocator)
        {
            segment_alloc_imp_t* imp = static_cast<segment_alloc_imp_t*>(allocator);
            imp->release(alloc);
        }
    }; // namespace nsegmented
}; // namespace ncore
