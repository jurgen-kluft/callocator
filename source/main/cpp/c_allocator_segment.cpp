#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_limits.h"
#include "ccore/c_arena.h"

#include "callocator/c_allocator_segment.h"

namespace ncore
{
    // Notes:
    // Can manage segments of sizes between min_size and max_size, where both sizes are a power of 2.
    // The difference between min_size_shift and max_size_shift can at most be 32 but such difference
    // should be avoided since it would require a lot of memory for the binmaps.
    // Also this specific implementation can only handle 3 level (u64) binmaps, meaning that the maximum
    // number of bits for the full level is 64 * 64 * 64 = 262144 bits = 256 K bits.
    // So (total_size / min_size) must be <= (256*1024).

    static inline u64 s_set_level_bit(s32 bit, u64* level)
    {
        const s32 bo = (bit >> 6);
        const s32 bi = (bit & (64 - 1));
        const u64 lc = level[bo];
        level[bo] |= ((u64)1 << bi);
        return lc;
    }

    static inline u64 s_clr_level_bit(s32 bit, u64* level)
    {
        const s32 bo = (bit >> 6);
        const s32 bi = (bit & (64 - 1));
        u64&      lv = level[bo];
        lv           = lv & ~((u64)1 << bi);
        return lv;
    }

    inline s8 clr_bit(segment_alloc_t* sa, s8 size_index, s32 bit)
    {
        ASSERT(size_index >= 0 && size_index < sa->m_num_sizes);
        segment_alloc_t::level_t& level = sa->m_levels[size_index];

        level.m_count--;
        u64 state = s_clr_level_bit(bit, level.m_bin0);
        if (level.m_bin1 != nullptr && state == 0)
        {
            bit   = bit >> 6; // To level 1
            state = s_clr_level_bit(bit, level.m_bin1);
            if (level.m_bin2 != nullptr && state == 0)
            {
                bit   = bit >> 6; // To level 2
                state = s_clr_level_bit(bit, level.m_bin2);
            }
        }
        return level.m_count == 0 ? 0 : 1;
    }

    s32 find_bit(segment_alloc_t* sa, s8 size_index)
    {
        ASSERT(size_index >= 0 && size_index < sa->m_num_sizes);
        segment_alloc_t::level_t& level = sa->m_levels[size_index];
        if (level.m_count == 0)
            return -1;

        s32 bit = 0;
        if (level.m_bin2 != nullptr)
        {
            bit = math::findFirstBit(level.m_bin2[0]);
        }
        if (level.m_bin1 != nullptr)
        {
            bit = (bit << 6) + math::findFirstBit(level.m_bin1[bit]);
        }
        bit = (bit << 6) + math::findFirstBit(level.m_bin0[bit]);
        return (bit < level.m_size) ? bit : -1;
    }

    u64 set_level0_bit(segment_alloc_t::level_t& level, s32 bit)
    {
        u64&      l0 = level.m_bin0[(bit >> 6)];
        const u32 bi = (bit & (64 - 1));
        u64       lc = l0;
        l0           = l0 | ((u64)1 << bi);
        return lc;
    }

    inline bool get_level0_bit(segment_alloc_t::level_t& level, s32 bit)
    {
        u64       l0 = level.m_bin0[(bit >> 6)];
        const u32 bi = (bit & (64 - 1));
        return (l0 & ((u64)1 << bi)) != 0;
    }

    inline u64 set_level0_pair(segment_alloc_t::level_t& level, s32 bit)
    {
        const u32 bo = (bit >> 6);
        const u32 bi = (bit & (64 - 1));
        u64&      l0 = level.m_bin0[bo];
        const u64 lc = l0;
        l0           = l0 | ((u64)3 << bi);
        return lc;
    }

    // returns 0 bits where cleared
    // returns 1 bits where set
    s8 set_bit(segment_alloc_t* sa, s8 size_index, s32 bit)
    {
        ASSERT(size_index >= 0 && size_index < sa->m_num_sizes);
        segment_alloc_t::level_t& level = sa->m_levels[size_index];
        ASSERT(get_level0_bit(level, bit) == false);
        if (get_level0_bit(level, bit ^ 1))
        {
            clr_bit(sa, size_index, bit ^ 1);
            return 0;
        }

        level.m_count++;
        const u64 bits0 = set_level0_bit(level, bit);
        if (level.m_bin1 != nullptr && bits0 == 0)
        {
            bit             = bit >> 6; // To level 1
            const u64 bits1 = s_set_level_bit(bit, level.m_bin1);
            if (level.m_bin2 != nullptr && bits1 == 0)
            {
                bit = bit >> 6; // To level 2
                s_set_level_bit(bit, level.m_bin2);
            }
        }
        return 1;
    }

    void set_2bits(segment_alloc_t* sa, s8 size_index, s32 bit)
    {
        ASSERT(size_index >= 0 && size_index < sa->m_num_sizes);
        ASSERT(bit >= 0 && ((bit & 3) == 0 || (bit & 3) == 2));

        segment_alloc_t::level_t& level = sa->m_levels[size_index];
        level.m_count += 2;

        const u64 bits0 = set_level0_pair(level, bit);
        if (level.m_bin1 != nullptr && bits0 == 0)
        {
            bit             = bit >> 6;
            const u64 bits1 = s_set_level_bit(bit, level.m_bin1);
            if (level.m_bin2 != nullptr && bits1 == 0)
            {
                bit = bit >> 6;
                s_set_level_bit(bit, level.m_bin2);
            }
        }
    }

    inline s8 size_to_index(segment_alloc_t* sa, s64 size)
    {
        ASSERT(math::ispo2(size));
        ASSERT(size >= ((s64)1 << sa->m_min_size_shift) && size <= ((s64)1 << sa->m_max_size_shift));
        return math::ilog2(size) - sa->m_min_size_shift;
    }

    namespace nsegment
    {
        bool allocate(segment_alloc_t* sa, s64 size, s64& offset)
        {
            const s8 size_index = size_to_index(sa, size);

            // We can detect bits set in m_size_free directly
            const u32 size_free = (sa->m_size_free & (0xffffffff << size_index));
            if (size_free == 0)
            {
                // Fragmented / OOM ?
                offset = -1;
                return false;
            }

            // Which (highest) bit is set in size_free?
            s8  size_free_index = math::findFirstBit(size_free);
            s32 bit             = find_bit(sa, size_free_index);
            if (bit < 0)
            {
                // Fragmented / OOM ?
                offset = -1;
                return false;
            }

            // If necessary, split segments down to size_index
            while (size_free_index > size_index)
            {
                if (clr_bit(sa, size_free_index, bit) == 0)
                    sa->m_size_free &= ~(1 << size_free_index);

                size_free_index--;

                bit = (bit << 1);
                set_2bits(sa, size_free_index, bit);
                sa->m_size_free |= (1 << size_free_index);
            }

            // Clear the bit in the bitmap at size_index
            if (clr_bit(sa, size_index, bit) == 0)
                sa->m_size_free &= ~(1 << size_index);

            // Return the actual allocated offset
            offset = ((s64)bit << (sa->m_min_size_shift + size_index));
            return true;
        }

        bool deallocate(segment_alloc_t* sa, s64 ptr, s64 size)
        {
            if (ptr < 0 || size <= 0 || ptr >= ((s64)1 << sa->m_total_size_shift) || size > ((s64)1 << sa->m_max_size_shift))
                return false; // Invalid pointer or size

            s8  size_index = size_to_index(sa, size);                           // get the size index from the size
            s32 bit        = (s32)(ptr >> (sa->m_min_size_shift + size_index)); // the bit index of the bitmap at size_index
            while (size_index < sa->m_num_sizes)
            {
                if (set_bit(sa, size_index, bit) == 0)
                {
                    if (sa->m_levels[size_index].m_count == 0)
                        sa->m_size_free &= ~(1 << size_index);
                }
                else
                {
                    sa->m_size_free |= (1 << size_index);
                    break;
                }

                bit = (bit >> 1);
                size_index++;
            }
            return true;
        }

        void initialize(segment_alloc_t* sa, alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size)
        {
            ASSERT(math::ispo2(min_size) && math::ispo2(max_size) && math::ispo2(total_size));
            ASSERT(min_size < max_size && max_size <= total_size);
            const s8 min_size_shift = math::ilog2(min_size);
            const s8 max_size_shift = math::ilog2(max_size);
            const s8 tot_size_shift = math::ilog2(total_size);
            const s8 num_sizes      = 1 + max_size_shift - min_size_shift;

            // Allocate the count and offsets array's
            sa->m_levels = g_allocate_array_and_clear<segment_alloc_t::level_t>(allocator, num_sizes);

            s32 size_in_bits = 1 << (tot_size_shift - min_size_shift);
            ASSERT(size_in_bits <= (1 << 3 * 6));

            s32 i = 0;
            while (i < num_sizes)
            {
                segment_alloc_t::level_t& level = sa->m_levels[i++];
                level.m_count                   = 0;
                level.m_size                    = size_in_bits;

                const bool is_max_level = (i == num_sizes);

                const s32 level0_size_in_bits = size_in_bits;
                if (level0_size_in_bits > 0)
                {
                    const s32 level0_size_in_bits_aligned = math::alignUp(level0_size_in_bits, 64);
                    const s32 level0_size_in_u64s         = level0_size_in_bits_aligned >> 6;
                    if (is_max_level)
                    {
                        level.m_bin0 = g_allocate_array_and_memset<u64>(allocator, level0_size_in_u64s, 0xFFFFFFFF);
                        // Make sure we don't have bits set beyond number_of_bits
                        const s32 excess_bits = (level0_size_in_bits_aligned - level0_size_in_bits);
                        if (excess_bits > 0)
                        {
                            const s32 excess_u64_index = level0_size_in_u64s - 1;
                            const u64 mask             = (~((u64)0)) >> excess_bits;
                            level.m_bin0[excess_u64_index] &= mask;
                        }
                    }
                    else
                    {
                        level.m_bin0 = g_allocate_array_and_clear<u64>(allocator, level0_size_in_u64s);
                    }
                }

                const s32 level1_size_in_bits = size_in_bits >> 6;
                if (level1_size_in_bits > 0)
                {
                    const s32 level1_size_in_bits_aligned = math::alignUp(level1_size_in_bits, 64);
                    const s32 level1_size_in_u64s         = level1_size_in_bits_aligned >> 6;
                    if (is_max_level)
                    {
                        level.m_bin1 = g_allocate_array_and_memset<u64>(allocator, level1_size_in_u64s, 0xFFFFFFFF);
                        // Make sure we don't have bits set beyond number_of_bits
                        const s32 excess_bits = (level1_size_in_bits_aligned - level1_size_in_bits);
                        if (excess_bits > 0)
                        {
                            const s32 excess_u64_index = level1_size_in_u64s - 1;
                            const u64 mask             = (~((u64)0)) >> excess_bits;
                            level.m_bin1[excess_u64_index] &= mask;
                        }
                    }
                    else
                    {
                        level.m_bin1 = g_allocate_array_and_clear<u64>(allocator, level1_size_in_u64s);
                    }
                }

                const s32 level2_size_in_bits = size_in_bits >> 12;
                if (level2_size_in_bits > 0)
                {
                    const s32 level2_size_in_bits_aligned = math::alignUp(level2_size_in_bits, 64);
                    const s32 level2_size_in_u64s         = level2_size_in_bits_aligned >> 6;
                    if (is_max_level)
                    {
                        level.m_bin2 = g_allocate_array_and_memset<u64>(allocator, level2_size_in_u64s, 0xFFFFFFFF);
                        // Make sure we don't have bits set beyond number_of_bits
                        const s32 excess_bits = (level2_size_in_bits_aligned - level2_size_in_bits);
                        if (excess_bits > 0)
                        {
                            const s32 excess_u64_index = level2_size_in_u64s - 1;
                            const u64 mask             = (~((u64)0)) >> excess_bits;
                            level.m_bin2[excess_u64_index] &= mask;
                        }
                    }
                    else
                    {
                        level.m_bin2 = g_allocate_array_and_clear<u64>(allocator, level2_size_in_u64s);
                    }
                }
                size_in_bits = math::max(size_in_bits >> 1, (s32)1);
            }

            sa->m_min_size_shift   = min_size_shift;
            sa->m_max_size_shift   = max_size_shift;
            sa->m_total_size_shift = tot_size_shift;
            sa->m_num_sizes        = num_sizes;

            // The number of free segments on the maximum level is the full size
            sa->m_levels[sa->m_num_sizes - 1].m_count = sa->m_levels[sa->m_num_sizes - 1].m_size;

            // Initialize the size_free bitmap and the highest size bitmap
            sa->m_size_free = (1 << (sa->m_num_sizes - 1));
        }

        void teardown(segment_alloc_t* sa, alloc_t* allocator)
        {
            for (s8 i = 0; i < sa->m_num_sizes; ++i)
            {
                segment_alloc_t::level_t& level = sa->m_levels[i];
                if (level.m_bin0 != nullptr)
                    g_deallocate_array(allocator, level.m_bin0);
                if (level.m_bin1 != nullptr)
                    g_deallocate_array(allocator, level.m_bin1);
                if (level.m_bin2 != nullptr)
                    g_deallocate_array(allocator, level.m_bin2);
            }
            g_deallocate_array(allocator, sa->m_levels);
        }
    } // namespace nsegment
}; // namespace ncore
