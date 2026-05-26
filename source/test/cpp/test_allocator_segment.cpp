#include "cbase/c_allocator.h"
#include "cbase/c_context.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"
#include "ccore/c_random.h"

#include "callocator/c_allocator_segment.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(segmented)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(allocate)
        {
            const u8 s_min_size_shift = 16;
            const u8 s_max_size_shift = 21;
            const u8 s_tot_size_shift = 31; // 256 GB

            const int_t s_min_size   = (int_t)1 << s_min_size_shift;
            const int_t s_max_size   = (int_t)1 << s_max_size_shift;
            const int_t s_total_size = (int_t)1 << s_tot_size_shift;

            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, s_min_size, s_max_size, s_total_size);

            s64 ptr[7];
            s64 sizes[7];
            sizes[0] = s_min_size;
            sizes[1] = s_min_size;
            sizes[2] = 2 * s_min_size;
            sizes[3] = 4 * s_min_size;
            sizes[4] = 8 * s_min_size;
            sizes[5] = 16 * s_min_size;
            sizes[6] = 32 * s_min_size;

            CHECK_TRUE(nsegment::allocate(&range, sizes[0], ptr[0]));
            CHECK_EQUAL(0, ptr[0]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[1], ptr[1]));
            CHECK_EQUAL(sizes[1], ptr[1]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[2], ptr[2]));
            CHECK_EQUAL(sizes[2], ptr[2]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[3], ptr[3]));
            CHECK_EQUAL(sizes[3], ptr[3]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[4], ptr[4]));
            CHECK_EQUAL(sizes[4], ptr[4]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[5], ptr[5]));
            CHECK_EQUAL(sizes[5], ptr[5]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[6], ptr[6]));
            CHECK_EQUAL(sizes[6], ptr[6]);

            for (int_t i = 0; i < 7; ++i)
            {
                CHECK_TRUE(nsegment::deallocate(&range, ptr[i], sizes[i]));
            }

            // All free again
            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }

        // Allocate and deallocate randomly many different sizes and lifetimes
        UNITTEST_TEST(stress_test)
        {
            const u8 s_min_size_shift = 16;
            const u8 s_max_size_shift = 21;
            const u8 s_tot_size_shift = 31; // 256 GB

            const int_t s_min_size   = (int_t)1 << s_min_size_shift;
            const int_t s_max_size   = (int_t)1 << s_max_size_shift;
            const int_t s_total_size = (int_t)1 << s_tot_size_shift;

            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, s_min_size, s_max_size, s_total_size);

            const int_t num_allocs = 900;
            s64         ptrs[num_allocs];
            s64         sizes[num_allocs];

            ncore::xor_random_t rng;
            rng.reset(12345);

            for (i32 i = 0; i < num_allocs; ++i)
            {
                // Random size between min_size and max_size
                s32 random_shift = g_random_u32_max(&rng, s_max_size_shift - s_min_size_shift + 1);
                sizes[i]         = ((s64)1 << (s_min_size_shift + random_shift));
                ptrs[i]          = -1;
            }

            // Allocate all
            for (i32 i = 0; i < num_allocs; ++i)
            {
                CHECK_TRUE(nsegment::allocate(&range, sizes[i], ptrs[i]));
            }

            // Shuffle
            i32 random_indices[num_allocs];
            for (i32 i = 0; i < num_allocs; ++i)
            {
                random_indices[i] = i;
            }
            for (i32 i = 0; i < num_allocs; ++i)
            {
                i32 idx1             = (i32)(rng.rand32() % num_allocs);
                i32 tmp              = random_indices[i];
                random_indices[i]    = random_indices[idx1];
                random_indices[idx1] = tmp;
            }

            // Deallocate in random order
            for (i32 i = 0; i < num_allocs; ++i)
            {
                i32 idx = random_indices[i];
                CHECK_TRUE(nsegment::deallocate(&range, ptrs[idx], sizes[idx]));
            }

            // All free again
            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }

        UNITTEST_TEST(stress_test2)
        {
            const u64       c_total_address_space = 256 * cGB;
            static const s8 s_section_size_min    = 26; // 64 MB
            static const s8 s_section_size_max    = 30;

            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, (1 << s_section_size_min), (1 << s_section_size_max), c_total_address_space);

            const int_t num_allocs = 500;
            s64         ptrs[num_allocs];
            s64         sizes[num_allocs];

            ncore::xor_random_t rng;
            rng.reset(12345);

            for (i32 i = 0; i < num_allocs; ++i)
            {
                // Random size between min_size and max_size
                s32 random_shift = g_random_u32_max(&rng, s_section_size_max - s_section_size_min + 1);
                sizes[i]         = ((s64)1 << (s_section_size_min + random_shift));
                ptrs[i]          = -1;
            }

            // Allocate all
            for (i32 i = 0; i < num_allocs; ++i)
            {
                CHECK_TRUE(nsegment::allocate(&range, sizes[i], ptrs[i]));
            }

            // Shuffle
            i32 random_indices[num_allocs];
            for (i32 i = 0; i < num_allocs; ++i)
            {
                random_indices[i] = i;
            }
            for (i32 i = 0; i < num_allocs; ++i)
            {
                i32 idx1             = (i32)(rng.rand32() % num_allocs);
                i32 tmp              = random_indices[i];
                random_indices[i]    = random_indices[idx1];
                random_indices[idx1] = tmp;
            }

            // Deallocate in random order
            for (i32 i = 0; i < num_allocs; ++i)
            {
                i32 idx = random_indices[i];
                CHECK_TRUE(nsegment::deallocate(&range, ptrs[idx], sizes[idx]));
            }

            // All free again
            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }

        UNITTEST_TEST(maximum_24bit_binmap)
        {
            // total_size / min_size = 2^24, which exercises the 4th binmap level.
            const s8 s_min_size_shift = 16;
            const s8 s_max_size_shift = 21;
            const s8 s_tot_size_shift = s_min_size_shift + 24;

            const int_t s_min_size   = ((int_t)1 << s_min_size_shift);
            const int_t s_max_size   = ((int_t)1 << s_max_size_shift);
            const int_t s_total_size = ((int_t)1 << s_tot_size_shift);

            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, s_min_size, s_max_size, s_total_size);

            CHECK_TRUE(range.m_levels[0].m_bin3 != nullptr);

            s64 ptr = -1;
            CHECK_TRUE(nsegment::allocate(&range, s_min_size, ptr));
            CHECK_EQUAL(0, ptr);
            CHECK_TRUE(nsegment::deallocate(&range, ptr, s_min_size));

            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }

        UNITTEST_TEST(deterministic_random_churn)
        {
            const u64       c_total_address_space = 256 * cGB; // = 2^38 = 256 GB
            static const s8 s_section_size_min    = 16;        // 64 KB
            static const s8 s_section_size_max    = 30;        // 1 GB

            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, (1 << s_section_size_min), (1 << s_section_size_max), c_total_address_space);

            ncore::xor_random_t rng;
            rng.reset(0x12345678); // fixed seed => deterministic

            const int_t max_live_allocs = 4096;
            const int_t num_operations  = 25000;

            s64 live_ptrs[max_live_allocs];
            s64 live_sizes[max_live_allocs];
            i32 live_count = 0;

            for (i32 op = 0; op < num_operations; ++op)
            {
                const bool try_allocate = (live_count == 0) || (live_count < max_live_allocs && ((rng.rand32() % 100) < 60));

                if (try_allocate)
                {
                    s32 random_shift = g_random_u32_max(&rng, s_section_size_max - s_section_size_min + 1);
                    s64 size         = ((s64)1 << (s_section_size_min + random_shift));
                    s64 ptr          = -1;

                    if (nsegment::allocate(&range, size, ptr))
                    {
                        live_ptrs[live_count]  = ptr;
                        live_sizes[live_count] = size;
                        live_count++;
                    }
                    else if (live_count > 0)
                    {
                        // deterministic recovery on allocation failure
                        i32 idx = (i32)(rng.rand32() % live_count);
                        CHECK_TRUE(nsegment::deallocate(&range, live_ptrs[idx], live_sizes[idx]));
                        live_count--;
                        live_ptrs[idx]  = live_ptrs[live_count];
                        live_sizes[idx] = live_sizes[live_count];
                    }
                }
                else
                {
                    i32 idx = (i32)(rng.rand32() % live_count);
                    CHECK_TRUE(nsegment::deallocate(&range, live_ptrs[idx], live_sizes[idx]));
                    live_count--;
                    live_ptrs[idx]  = live_ptrs[live_count];
                    live_sizes[idx] = live_sizes[live_count];
                }
            }

            while (live_count > 0)
            {
                live_count--;
                CHECK_TRUE(nsegment::deallocate(&range, live_ptrs[live_count], live_sizes[live_count]));
            }

            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);
            nsegment::teardown(&range, Allocator);
        }
    }
}
