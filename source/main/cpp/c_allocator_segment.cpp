#include "ccore/c_target.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    // Note: What about a bitset/bitmap per size ?
    //
    // So from a memory usage point of view, the bitmap approach is 1/8th to 1/16th
    // the size of the node approach.
    //
    // This is significant and might even allow us to track free chunks instead of sections.
    //
    // Allocate(size int_t) (node node_t):
    // - Compute index from size
    // - Compute bitmap offset and count from size
    // - Check m_size_free[index], if the bit is set it means at that size we have free elements
    // - If not, check every next higher size-bitmap, if this has a free element
    //   - If we found a free element at a higher size-bitmap, we can split it into two elements of
    //     the lower level and do this until we reach the size-bitmap of the requested size.
    //   - Let's see how many bits and size-bitmaps we need to touch:
    //     - Top size-bitmap, set 1 bit, take 1 bit to next size-bitmap
    //     - Next size-bitmap, set 1 bit, take 1 bit to next size-bitmap
    //     - etc..
    //     - So with 12 sizes, we need to set 12 bits, one at each size bitmap
    // Deallocate(node node_t):
    //   - Merging:
    //     - We now have 2 bits set at an even+odd bit position, so we need to up one size-bitmap
    //       and set a bit there. When we again have 2 bits set at an even+odd bit position, we set
    //       the next size-bitmap bit, and so on. Again N size-bitmap to go up.
    //

    struct segment_alloc_t
    {
        s8   m_min_size_shift;    // The minimum size of a segment in log2
        s8   m_max_size_shift;    // The maximum size of a segment in log2
        s8   m_num_sizes;         // The number of sizes available
        u32  m_size_free;         // One bit per size, indicating if there is one or more free segments at that size.
        u64* m_level0;            // Level 0 for each size
        s32* m_countandoffsets_0; // Offsets for level 0, one per size
        u64* m_level1;            // Level 1
        s32* m_countandoffsets_1; // Offsets for level 0, one per size (-1 means no level 1 bitmap)
        u64* m_level2;            // Level 2
        s32* m_countandoffsets_2; // Offsets for level 0, one per size (-1 means no level 2 bitmap)

        // note: remove 'count' if we don't need it

        // size_shift is already normalized (0 <= size_shift < m_num_sizes)
        void get_offset0(s8 size_shift, u32& count, u32& offset) const
        {
            count  = m_countandoffsets_0[(size_shift << 1) + 0];
            offset = m_countandoffsets_0[(size_shift << 1) + 1];
        }
        bool get_offset1(s8 size_shift, u32& count, u32& offset) const
        {
            count  = m_countandoffsets_1[(size_shift << 1) + 0];
            offset = m_countandoffsets_1[(size_shift << 1) + 1];
            return count > 0;
        }
        bool get_offset2(s8 size_shift, u32& count, u32& offset) const
        {
            count  = m_countandoffsets_2[(size_shift << 1) + 0];
            offset = m_countandoffsets_2[(size_shift << 1) + 1];
            return count > 0;
        }

        u64 clr_bit(s8 size_shift, s32 bit)
        {
            ASSERT(bit >= 0 && ((bit & 3) == 0 || (bit & 3) == 2 || (bit & 3) == 4 || (bit & 3) == 6));

            // Clear the bit in the level 0 bitmap
            u32 count, offset;
            get_offset0(size_shift, count, offset);

            u32  bo = (bit >> 6);
            u64& l0 = m_level0[offset + bo];
            u32  bi = (bit & (64 - 1));
            l0 &= ~(1ULL << bi);

            u64 state = l0;

            if (get_offset1(size_shift, count, offset))
            {
                bit = bit >> 6; // To level 1

                // Clear the bit in the level 1 bitmap
                bo      = (bit >> 6);
                u64& l1 = m_level1[offset + bo];
                bi      = (bit & (64 - 1));
                l1 &= ~(1ULL << bi);

                state = l1;

                if (get_offset2(size_shift, count, offset))
                {
                    bit = bit >> 6; // To level 2

                    // Clear the bit in the level 2 bitmap
                    bo      = (bit >> 6);
                    u64& l2 = m_level2[offset + bo];
                    bi      = (bit & (64 - 1));
                    l2 &= ~(1ULL << bi);

                    state = l2;
                }
            }

            return state;
        }

        s32 find_bit(s8 size_shift) const
        {
            s32 bit = 0;
            u64 l;

            u32 count, offset;
            if (get_offset2(size_shift, count, offset))
            {
                l   = m_level2[offset];
                bit = math::g_findFirstBit(l);
            }
            if (get_offset1(size_shift, count, offset))
            {
                l   = m_level1[offset + bit];
                bit = math::g_findFirstBit(l);
            }
            get_offset0(size_shift, count, offset);

            l = m_level0[offset + bit];
            return (bit << 6) + math::g_findFirstBit(l);
        }

        s32 find_and_clr_bit(s8 size_shift)
        {
            const s32 bit = find_bit(size_shift);
            if (clr_bit(size_shift, bit) == 0)
            {
                m_size_free &= ~(1 << size_shift); // No more free elements at this size
            }

            return 0;
        }

        void set_bit(s8 size_shift, s32 bit)
        {
            ASSERT(bit >= 0 && ((bit & 3) == 0 || (bit & 3) == 2 || (bit & 3) == 4 || (bit & 3) == 6));

            // Set the bit in the level 0 bitmap
            u32 count, offset;
            get_offset0(size_shift, count, offset);

            u64* bits0 = &m_level0[offset];
            u32  bo    = (bit >> 6);
            u32  bi    = (bit & (64 - 1));
            bits0[bo] |= (1 << bi);

            if (get_offset1(size_shift, count, offset))
            {
                bit = bit >> 6; // To level 1

                // Set the bit in the level 1 bitmap
                u64* bits1 = &m_level1[offset];
                bo         = (bit >> 6);
                bi         = (bit & (64 - 1));
                bits1[bo] |= (1 << bi);

                if (get_offset2(size_shift, count, offset))
                {
                    bit = bit >> 6; // To level 2

                    // Set the bit in the level 2 bitmap
                    u64* bits2 = &m_level2[offset];
                    bo         = (bit >> 6);
                    bi         = (bit & (64 - 1));
                    bits2[bo] |= (1 << bi);
                }
            }
        }

        bool allocate(s32 size, s64& offset)
        {
            ASSERT(math::g_ispo2(size));
            ASSERT(size >= (1 << m_min_size_shift) && size <= (1 << m_max_size_shift));

            const s8 size_index = math::g_ilog2(size) - m_min_size_shift;

            // We can detect bits set in m_size_free directly
            const u32 size_with_free_elements_bits = (m_size_free & ((1 << (size_index + 1)) - 1));
            if (size_with_free_elements_bits == 0)
            {
                // OOM ?
                offset = -1;
                return false;
            }

            // Which (highest) bit is set in m_size_free?
            s8  size_with_free_element_index = math::g_findLastBit(size_with_free_elements_bits);
            s32 bit                          = find_bit(size_with_free_element_index);

            while (size_with_free_element_index > size_index)
            {
                clr_bit(size_with_free_element_index, bit);

                size_with_free_element_index--;

                bit = (bit << 1);

                set_bit(size_with_free_element_index, bit + 1);

                // Mark the bit in 'size_free bitmap' to indicate that we have a free element at this size
                m_size_free |= (1 << size_with_free_element_index);
            }

            // We now should be able to find a '1' bit in the bitmap at 'size_index'.
            bit = find_and_clr_bit(size_index);

            // Return the actual allocated offset
            offset = ((s64)1 << (m_min_size_shift + size_index)) * bit;
            return true;
        }

        void deallocate(s64 offset, s32 size)
        {
            // compute the bit from the offset and size

        }
    };

}; // namespace ncore
